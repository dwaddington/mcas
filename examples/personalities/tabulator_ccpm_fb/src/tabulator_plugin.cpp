/*
   Copyright [2020-2021] [IBM Corporation]
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
       http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#include <common/logging.h>
#include <common/utils.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <api/interfaces.h>
#include <string.h>
#include <flatbuffers/flatbuffers.h>

#include <ccpm/value_tracked.h>
#include <ccpm/container_cc.h>
#include <EASTL/iterator.h>
#include <EASTL/vector.h>

#include <libpmem.h>

#include "tabulator_generated.h"
#include "tabulator_plugin.h"

using namespace flatbuffers;

constexpr const uint64_t CANARY = 0xCAFEF001;
int debug_level = 3;

struct persister final
	: public ccpm::persister
{
	void persist(common::byte_span s) override
	{
		::pmem_persist(::base(s), ::size(s));
	}
};

namespace
{
	persister pe{};
}

struct Record {
  uint64_t canary;
  double min;
  double max;
  double mean;
  size_t count;
};

status_t Tabulator_plugin::register_mapped_memory(void * shard_vaddr,
                                                  void * local_vaddr,
                                                  size_t len)
{
  PLOG("Tabulator_plugin: register_mapped_memory (%p, %p, %lu)",
       shard_vaddr, local_vaddr, len);
  /* we would need a mapping if we are not using the same virtual
     addresses as the Shard process */

  return S_OK;
}

inline void * copy_flat_buffer(FlatBufferBuilder& fbb)
{
  auto fb_len = fbb.GetSize();
  void * ptr = ::malloc(fb_len);
  memcpy(ptr, fbb.GetBufferPointer(), fb_len);
  //  hexdump(ptr, fb_len);
  return ptr;
}

status_t Tabulator_plugin::do_work(const uint64_t work_key,
                                   const char * key,
                                   size_t key_len,
                                   IADO_plugin::value_space_t& values,
                                   const void *in_work_request,
                                   const size_t in_work_request_len,
                                   bool new_root,
                                   response_buffer_vector_t& response_buffers)
{
  assert(values.size() == 1);

  auto value = values[0].ptr;  
  auto value_len = values[0].len;

  /* define value type */
  using vt = double;

  /* define "tracking" type which tracks write modifications */
  using logged_vt = ccpm::value_tracked<vt, ccpm::tracker_log>;

  /* define crash-consistent vector */
  using cc_vector = ccpm::container_cc<eastl::vector<logged_vt, ccpm::allocator_tl>>;

  ccpm::cca * ccaptr;
  cc_vector * ccv;
  
  /* new_root == true indicates this is a "fresh" key and therefore needs initializing */
  ccpm::region_vector_t rv(common::make_byte_span(value, value_len));
  if(new_root) {
    ccaptr = new ccpm::cca(&pe, rv);
    ccv = new (ccaptr->allocate_root(sizeof(cc_vector))) cc_vector(&pe, *ccaptr);
    /* initialize vector */
    ccv->container->push_back(-1.0);
    ccv->container->push_back(-1.0);
    ccv->container->push_back(-1.0);
    ccv->container->push_back(0.0);
    ccv->commit();
  }
  else {
    ccaptr = new ccpm::cca(&pe, rv, ccpm::accept_all);
    void *root_base = ::base(ccaptr->get_root());
    /* reconstruct the cc_vector vft (see note on move constructor in ccpm/log.h */
    ccv = new (root_base) cc_vector(std::move(*static_cast<cc_vector *>(root_base)));
    ccv->rollback(); /* in case we're recovering from crash */
  }

  auto& min = (double&) ccv->container->at(0);
  auto& max = (double&) ccv->container->at(1);
  auto& mean = (double&) ccv->container->at(2);
  auto& count = (double&) ccv->container->at(3);
    
  /* check ADO message */
  auto msg = Proto::GetMessage(in_work_request);
  if(msg->element_as_UpdateRequest()) {

    auto request = msg->element_as_UpdateRequest();
    auto sample = request->sample();

    /* update min,max,mean,count with new sample */
    if(min == -1.0 || sample < min) {
      ccv->add(&min, sizeof(double)); /* add to transaction */
      min = sample;
    }

    if(max == -1.0 || sample > max) {
      ccv->add(&max, sizeof(double)); /* add to transaction */
      max = sample;
    }

    auto tmp = count * mean;
    tmp += sample;
    count += 1.0;
    mean = tmp / count;
    ccv->add(&count, sizeof(double)); /* add to transaction */
    ccv->add(&mean, sizeof(double)); /* add to transaction */
    ccv->commit();

    /* print status */
    PINF("key(%.*s) min:%g max:%g mean:%g count:%g",
         boost::numeric_cast<int>(key_len), key, min, max, mean, count);

    /* create response message */
    {
      using namespace Proto;
      FlatBufferBuilder fbb;
      auto req = CreateUpdateReply(fbb, S_OK);
      fbb.Finish(CreateMessage(fbb, Element_UpdateReply, req.Union()));
      response_buffers.emplace_back(copy_flat_buffer(fbb),
                                    fbb.GetSize(),
                                    response_buffer_t::alloc_type_malloc{});
    }

  }
  else if(msg->element_as_QueryRequest()) {
    PNOTICE("QueryRequest!");

    /* create response message */
    {
      using namespace Proto;
      FlatBufferBuilder fbb;
      
      Value v(min, max, mean);
      auto req = CreateQueryReply(fbb, S_OK, &v);
      fbb.Finish(CreateMessage(fbb, Element_UpdateReply, req.Union()));
      response_buffers.emplace_back(copy_flat_buffer(fbb),
                                    fbb.GetSize(),
                                    response_buffer_t::alloc_type_malloc{});
    }

  }
  else throw Logic_exception("unknown protocol message type");  

  /* clean up */
  delete ccaptr;
  
  return S_OK;
}

/* called when the pool is opened and the ADO is launched */
void Tabulator_plugin::launch_event(const uint64_t                  auth_id,
                                    const std::string&              pool_name,
                                    const size_t                    pool_size,
                                    const unsigned int              pool_flags,
                                    const unsigned int              memory_type,
                                    const size_t                    expected_obj_count,
                                    const std::vector<std::string>& params)
{
}


/* called just before ADO shutdown */
status_t Tabulator_plugin::shutdown()
{
  return S_OK;
}




/**
 * Factory-less entry point
 *
 */
extern "C" void * factory_createInstance(component::uuid_t interface_iid)
{
  if(interface_iid == interface::ado_plugin)
    return static_cast<void*>(new Tabulator_plugin());
  else return NULL;
}

