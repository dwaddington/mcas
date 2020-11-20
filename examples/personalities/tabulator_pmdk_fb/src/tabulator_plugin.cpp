/*
   Copyright [2020] [IBM Corporation]
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

#include <libpmem.h>
#include <libpmemobj.h>

#include "tabulator_generated.h"
#include "tabulator_plugin.h"

using namespace flatbuffers;

constexpr const char * LAYOUT = "tabulator";
constexpr const uint64_t CANARY = 0xCAFEF001;
int debug_level = 3;


struct Record {
  uint64_t canary;
  float min;
  float max;
  float mean;
  size_t count;
};

POBJ_LAYOUT_BEGIN(record);
	POBJ_LAYOUT_TOID(record, struct Record);
POBJ_LAYOUT_END(record);

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

  if(value_len != sizeof(TOID(struct Record)))
     throw General_exception("unexpected value size");

  auto root = reinterpret_cast<TOID(struct Record)*>(value);
  
  /* new_root == true indicate this is a "fresh" key and therefore needs initializing */
  if(new_root) {
    TX_BEGIN(_pmemobj_pool) {

      TOID(struct Record) v = TX_NEW(struct Record);
      D_RW(v)->canary = CANARY;      
      D_RW(v)->min = -1.0f;
      D_RW(v)->max = -1.0f;
      D_RW(v)->count = 0;
        
      // TODO make this crash consistent       
      *root = v;
      pmem_persist(root, sizeof(TOID(struct Record)));
    }
    TX_ONABORT {
      throw General_exception("pmem transaction aborted");
    }
    TX_END
  }
  else {
    PNOTICE("canary check:%lx", D_RO(*root)->canary);
    if(D_RO(*root)->canary != CANARY) throw General_exception("value data corrupt");
    /* check integrity */
  }

  /* check ADO message */
  auto msg = Proto::GetMessage(in_work_request);
  if(msg->element_as_UpdateRequest()) {

    auto request = msg->element_as_UpdateRequest();
    PNOTICE("UpdateRequest! (sample=%f)", request->sample());
    auto sample = request->sample();
    auto tx_value = *reinterpret_cast<TOID(struct Record)*>(value);
    
    /* update data */    
    TX_BEGIN(_pmemobj_pool) {
      
      TX_ADD(tx_value);

      /* update max and min */
      if(D_RO(tx_value)->min == -1.0f) {
        D_RW(tx_value)->min = D_RW(tx_value)->max = sample;
      }
      else {
        if(sample < D_RO(tx_value)->min)
          D_RW(tx_value)->min = sample;
        if(sample > D_RO(tx_value)->max)
          D_RW(tx_value)->max = sample;
      }
      /* update running average */
      auto tmp = static_cast<float>(D_RO(tx_value)->count) * D_RO(tx_value)->mean;
      tmp += sample;
      D_RW(tx_value)->count ++;
      D_RW(tx_value)->mean = tmp / D_RO(tx_value)->count;
      
    } TX_END

    /* print status */
    PLOG("min:%f max:%f mean:%f", D_RO(tx_value)->min, D_RO(tx_value)->max, D_RO(tx_value)->mean);

  }
  else if(msg->element_as_QueryRequest()) {
    PNOTICE("QueryRequest!");    
  }
  else throw Logic_exception("unknown protocol message type");  
  


#if 0
  auto msg = GetMessage(in_work_request);
  auto txid = msg->transaction_id();

  // Put
  if(msg->element_as_PutRequest()) {
    auto pr = msg->element_as_PutRequest();

    if(debug_level > 0)
      PMAJOR("Got put request (%s,%s)",
             pr->key()->str().c_str(),
             pr->value()->str().c_str());

    size_t value_to_free_len = 0;
    auto value_to_free = root->add_version(detached_value,
                                           detached_value_len,
                                           value_to_free_len);
    if(value_to_free) {
      if(debug_level > 0)
        PMAJOR("freeing: value (%.*s)",
               (int) value_to_free_len, static_cast<char*>(value_to_free));
      cb_free_pool_memory(value_to_free_len, value_to_free);
    }

    /* create response message */
    FlatBufferBuilder fbb;
    auto req = CreateAck(fbb, S_OK);
    fbb.Finish(CreateMessage(fbb, txid, Element_Ack, req.Union()));
    response_buffers.emplace_back(copy_flat_buffer(fbb), fbb.GetSize(), response_buffer_t::alloc_type_malloc{});

    return S_OK;
  }
  // Get
  else if(msg->element_as_GetRequest()) {
    auto pr = msg->element_as_GetRequest();

    if(debug_level > 2)
      PMAJOR("Got get request (%s)",
             pr->key()->str().c_str());

    void * return_value = nullptr;
    size_t return_value_len = 0;
    cpu_time_t timestamp = 0;
    root->get_version(pr->version_index(), return_value, return_value_len, timestamp);

    if(debug_level > 0)
         PLOG("Picked version to return: @ %p (%.*s)", return_value,
              (int) return_value_len, (char *) return_value);

    FlatBufferBuilder fbb;
    auto req = CreateGetResponse(fbb, timestamp/* timestamp */, return_value_len);
    fbb.FinishSizePrefixed(CreateMessage(fbb, txid, Element_GetResponse, req.Union()));

    response_buffers.emplace_back(copy_flat_buffer(fbb), fbb.GetSize(),  response_buffer_t::alloc_type_malloc{});
    response_buffers.emplace_back(return_value, return_value_len, response_buffer_t::alloc_type_pool{});
  }
  else {
    PLOG("got something unrecognized!");
  }
#endif
  
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
  _pmem_filename = "/mnt/pmem0/"; // TODO carry this and pool size from params?
  _pmem_filename += pool_name;
  _pmem_filename += ".pmdk";
  
  /* open or create/open file for pmdk-pool (associated with this pool) */
  if((_pmemobj_pool = pmemobj_open(_pmem_filename.c_str(), LAYOUT)) == nullptr) {
    _pmemobj_pool = pmemobj_create(_pmem_filename.c_str(), LAYOUT, pool_size, 0666);
    if(_pmemobj_pool == nullptr)
      throw General_exception("unable to open or create PMDK file (%s)", ::strerror(errno));
  }  
}


/* called just before ADO shutdown */
status_t Tabulator_plugin::shutdown()
{
  assert(_pmemobj_pool);
  pmemobj_close(_pmemobj_pool);
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

