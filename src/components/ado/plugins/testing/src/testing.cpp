/*
  Copyright [2017-2019] [IBM Corporation]
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

#include "testing.h"
#include <libpmem.h>
#include <ccpm/immutable_allocator.h>
#include <api/interfaces.h>
#include <common/logging.h>
#include <common/cycles.h>
#include <common/utils.h>
#include <boost/numeric/conversion/cast.hpp>
#include <cstring>

status_t ADO_testing_plugin::register_mapped_memory(void *shard_vaddr,
                                                    void *local_vaddr,
                                                    size_t len)
{
  PLOG("ADO_testing_plugin: register_mapped_memory (%p, %p, %lu)", shard_vaddr,
       local_vaddr, len);
  /* we would need a mapping if we are not using the same virtual
     addresses as the Shard process */
  return S_OK;
}

#define ASSERT_OK(X, Y)                       \
  if (X != S_OK)                                     \
    {                                           \
      PERR(Y);                                  \
      return E_FAIL;                            \
    }

#define ASSERT_TRUE(X, Y)                       \
  if (!(X))                                     \
    {                                           \
      PERR(Y);                                  \
      return E_FAIL;                            \
    }

#define ASSERT_FALSE(X, Y)                      \
  if (X)                                        \
    {                                           \
      PERR(Y);                                  \
      return E_FAIL;                            \
    }

void ADO_testing_plugin::launch_event(const uint64_t auth_id,
                                      const std::string& pool_name,
                                      const size_t pool_size,
                                      const unsigned int pool_flags,
                                      const size_t expected_obj_count)
{
  PMAJOR("ADO_testing_plugin: launch event (auth_id=%lu, %s, %lu, %u, %lu)",
         auth_id, pool_name.c_str(),
         pool_size, pool_flags, expected_obj_count);
}

status_t ADO_testing_plugin::do_work(uint64_t work_key,
                                     const std::string &key,
                                     void *value,
                                     size_t value_len,
                                     void * detached_value,
                                     size_t detached_value_len,
                                     const void *in_work_request, /* don't use iovec because of non-const */
                                     const size_t in_work_request_len,
                                     bool new_root,
                                     response_buffer_vector_t& response_buffers)
{

  PLOG("ADO_testing_plugin: do_work (key=%s)", key.c_str());
  status_t rc = S_OK;

  ASSERT_TRUE(value_len != 0, "ADO_testing_plugin: value_len is 0");

  if (key == "BasicInvokeAdo")
    {
      PLOG("ADO_testing_plugin: running BasicInvokeAdo (key=%s)", key.c_str());
      std::string work(static_cast<const char *>(in_work_request), in_work_request_len);
      PLOG("ADO_testing_plugin: work=(%s)", work.c_str());
      ASSERT_TRUE(work == "RUN!TEST-BasicInvokeAdo", "ADO_testing_plugin: unexpected message");

      memset(value, 0xe, value_len);
      PLOG("value=%p value_len=%lu", value, value_len);
      ASSERT_TRUE(value_len == 4096, "ADO_testing_plugin: value bad length");

      /* resize value */
      void *new_addr = nullptr;
      rc = _cb.resize_value(work_key, key, KB(8), new_addr);
      ASSERT_TRUE(rc == S_OK,
                  "ADO_testing_plugin: resize_value callback failed");
      PLOG("value after resize=%p", new_addr);

      /* check old content is still there */
      void *tmp = malloc(value_len);
      memset(tmp, 0xe, value_len);

      PLOG("ADO_testing_plugin: content after resize OK.");
      memset(new_addr, 0xf, KB(8));

      /* create new value */
      void *new_value_addr = nullptr;
      rc = _cb.create_key(work_key, "newKey", 12, true, new_value_addr);
      memset(new_value_addr, 'N', 12);
      PLOG("new key created at %p", new_value_addr);
      ASSERT_TRUE(new_value_addr && rc == S_OK, "ADO_testing_plugin: create_key callback failed");

      /* allocate value memory */
      void *new_value_memory = nullptr;
      size_t s = 1200;
      rc = _cb.allocate_pool_memory(s, 4096 /* alignment */, new_value_memory);
      PLOG("new value: %p rc=%d", new_value_memory, rc);
      rc = _cb.free_pool_memory(s, new_value_memory);
      ASSERT_TRUE(rc == S_OK, "free_pool_memory callback failed");

      /* get pool info */
      std::string info;
      rc = _cb.get_pool_info(info);
      ASSERT_TRUE(rc == S_OK, "get_pool_info callback failed");
      PLOG("pool info:(%s)", info.c_str());
    }
  else if (key == "BasicInvokePutAdo")
    {
      std::string val(reinterpret_cast<char *>(value), value_len);
      PLOG("ADO_testing_plugin: (key=%s,value=%s)", key.c_str(), val.c_str());
      std::string request(reinterpret_cast<const char *>(in_work_request), in_work_request_len);
      PLOG("ADO_testing_plugin: (request=%s)", request.c_str());

      ASSERT_TRUE("RUN!TEST-BasicInvokePutAdo" == request, "ADO_testing_plugin: invoke_put_ado failed");
      ASSERT_TRUE("VALUE_TO_PUT" == val, "ADO_testing_plugin: invoke_put_ado failed");
    }
  else if (key == "InvokeAdoCreateOnDemand")
    {
      void *src = nullptr;
      void *dst = nullptr;
      size_t size = KB(64);
      rc = _cb.allocate_pool_memory(size, 4096 /* alignment */, src);
      rc |= _cb.allocate_pool_memory(size, 4096 /* alignment */, dst);
      ASSERT_TRUE(rc == S_OK, "InvokeAdoCreateOnDemand: failed");
      memset(src, 0, size);
      memset(dst, 0xA, size);
      auto start = rdtsc();
      memcpy(dst, src, size);
      pmem_persist(dst, size);
      auto stop = rdtsc();
      PLOG("%lu cycles for %lu (%f usec)", stop - start, size, Common::cycles_to_usec(stop - start));
    }
  else if (key == "FindKeyCallback")
    {
      std::string matched_key;
      offset_t matched_pos = 0;

      void *new_value_addr = nullptr;
      rc = _cb.create_key(work_key, "aKey", 12, true, new_value_addr);
      if(rc != S_OK) return rc;
      rc = _cb.create_key(work_key, "bKey", 13, true, new_value_addr);
      if(rc != S_OK) return rc;
      rc = _cb.create_key(work_key, "cKey", 14, true, new_value_addr);
      if(rc != S_OK) return rc;

      PLOG("issuing find key (pos=%lu)..", matched_pos);
      while ((rc = _cb.find_key(".*",
                                matched_pos,                         // begin pos
                                Component::IKVIndex::FIND_TYPE_NEXT, // find type
                                matched_pos,
                                matched_key)) == S_OK)
        {
          PLOG("match: (%s) @ %lu", matched_key.c_str(), matched_pos);
          matched_pos++;
          if(rc != S_OK) return rc;
        }
      rc = S_OK; // reset RC
    }
  else if (key == "BasicAllocatePoolMemory")
    {
      void *result = nullptr;
      size_t size = KB(64);

      rc = _cb.allocate_pool_memory(size, 4096 /* alignment */, result);
      ASSERT_TRUE(rc == S_OK, "BasicAllocatePoolMemory::allocate_pool_memory failed");
    }
  else if (key == "BasicDetachedMemory")
    {
      ASSERT_TRUE(detached_value != nullptr, "BasicDetachedMemory::invalid detached ptr");

      /* free detached memory */
      rc = _cb.free_pool_memory(detached_value_len, detached_value);
      ASSERT_TRUE(rc == S_OK, "BasicDetachedMemory::free_pool_memory failed");
    }
  else if (key == "GetReferenceVector") 
    {
      Component::IADO_plugin::Reference_vector v;
      rc = _cb.get_reference_vector(0,0,v);
      PMAJOR("rc (count=%lu, val=%p, val_len=%lu",
             v.count(), v.ref_array(), v.value_memory_size());

      ASSERT_TRUE(rc == S_OK, "get_reference_vector failed");
      ASSERT_TRUE(v.count() == 4, "GetReferenceVector:: unexpected vector size");
      
      auto ref = v.ref_array();
      for(size_t i=0;i<v.count();i++) {
        PLOG("kv: key=(%p, %lu) val=(%p, %lu)",
             ref[i].key, ref[i].key_len,
             ref[i].value, ref[i].value_len);
        ASSERT_FALSE(ref[i].key == nullptr, "GetReferenceVector::bad reference vector");
        ASSERT_TRUE(ref[i].key_len > 0, "GetReferenceVector::bad reference vector");
        ASSERT_FALSE(ref[i].value == nullptr, "GetReferenceVector::bad reference vector");
        ASSERT_TRUE(ref[i].value_len > 0, "GetReferenceVector::bad reference vector");
      }

      for(size_t i=0;i<v.count();i++) {
        PLOG("kv: key=(%s) val=(%s)",
             reinterpret_cast<char*>(ref[i].key), reinterpret_cast<char*>(ref[i].value));
      }

      rc = _cb.free_pool_memory(v.value_memory_size(), v.value_memory());

      ASSERT_TRUE(rc == S_OK, "GetReferenceVector::free_pool_memory failed");
    }
  else if (key == "Iterator")
    {
      Component::IKVStore::pool_iterator_t iterator = nullptr;
      Component::IKVStore::pool_reference_t r{nullptr, 0, nullptr, 0, 0};
      while((rc = _cb.iterate(0,0,iterator,r)) == S_OK) {
        //        PLOG("Iterator: ref (%p,%lu,%p,%lu,%lu)", r.key, r.key_len, r.value, r.value_len, r.timestamp);
        PLOG("Iterator: ref (%.*s,%.*s,%lu)",
             (int)r.key_len, r.key, (int) r.value_len,
             (char*) r.value, r.timestamp);
      }
      if(rc == E_NOT_IMPL)
        PLOG("Component does not support iterator.");
             
      ASSERT_TRUE(rc == E_NOT_IMPL || rc == E_OUT_OF_BOUNDS, "iterator failed");
      rc = S_OK;
    }
  else if (key == "GetReferenceVectorByTime") 
    {
      Component::IADO_plugin::Reference_vector v;
      rc = _cb.get_reference_vector(0, 0, v);
      PMAJOR("rc (count=%lu, val=%p, val_len=%lu",
             v.count(), v.ref_array(), v.value_memory_size());

      ASSERT_TRUE(rc == S_OK, "get_reference_vector failed");
      ASSERT_TRUE(v.count() == 21, "get_reference_vector failed");

      {
        auto ref = v.ref_array();
        for(size_t i=0;i<v.count();i++) {
          PLOG("kv: key=(%p, %lu) val=(%p, %lu)",
               ref[i].key, ref[i].key_len,
               ref[i].value, ref[i].value_len);
          ASSERT_FALSE(ref[i].key == nullptr, "GetReferenceVector::bad reference vector");
          ASSERT_TRUE(ref[i].key_len > 0, "GetReferenceVector::bad reference vector");
          ASSERT_FALSE(ref[i].value == nullptr, "GetReferenceVector::bad reference vector");
          ASSERT_TRUE(ref[i].value_len > 0, "GetReferenceVector::bad reference vector");
        }
      }

      rc = _cb.free_pool_memory(v.value_memory_size(), v.value_memory());
      ASSERT_TRUE(rc == S_OK, "GetReferenceVector::free_pool_memory failed");

      /* now do by time retrieval */
      {
        sleep(1);
        auto now = epoch_now();
        PLOG("epoch now: %lu", now);
        wmb();
        
        {
          void *new_value_addr = nullptr;
          ASSERT_OK(_cb.create_key(work_key, "aKey", 7, true, new_value_addr),
                    "create key failed GetReferenceVectorByTime");        
          strcpy((char*) new_value_addr, "Hello!");
        }

        wmb();
        {
          void *new_value_addr = nullptr;
          ASSERT_OK(_cb.create_key(work_key, "anotherKey", 7, true, new_value_addr),
                    "create key failed GetReferenceVectorByTime");        
          strcpy((char*) new_value_addr, "World!");
        }

        wmb();
        rc = _cb.get_reference_vector(now, 0, v);
        PMAJOR("by-time rc (count=%lu, val=%p, val_len=%lu",
               v.count(), v.ref_array(), v.value_memory_size());

        ASSERT_TRUE(v.count() == 2, "bad vector count GetReferenceVectorByTime");

        {
          auto ref = v.ref_array();
          for(size_t i=0;i<v.count();i++) {
            PLOG("kv: key=(%.*s) val=(%.*s)",
                 (int) ref[i].key_len, (char*) ref[i].key, 
                 (int) ref[i].value_len, (char*) ref[i].value);
            ASSERT_FALSE(ref[i].key == nullptr, "GetReferenceVector::bad reference vector");
            ASSERT_TRUE(ref[i].key_len > 0, "GetReferenceVector::bad reference vector");
            ASSERT_FALSE(ref[i].value == nullptr, "GetReferenceVector::bad reference vector");
            ASSERT_TRUE(ref[i].value_len > 0, "GetReferenceVector::bad reference vector");
          }
        }
        
        rc = _cb.free_pool_memory(v.value_memory_size(), v.value_memory());
        ASSERT_TRUE(rc == S_OK, "GetReferenceVector::free_pool_memory failed");
      }
    }  
  else
    {
      PMAJOR("unknown key:%s", key.c_str());
      rc = E_INVAL;
    }
  return rc;
}

status_t ADO_testing_plugin::shutdown()
{
  /* here you would put graceful shutdown code if any */
  return S_OK;
}

/**
 * Factory-less entry point
 *
 */
extern "C" void *factory_createInstance(Component::uuid_t &interface_iid)
{
  PLOG("instantiating ADO_testing_plugin");
  if (interface_iid == Interface::ado_plugin)
    return static_cast<void *>(new ADO_testing_plugin());
  else
    return NULL;
}

#undef RESET_STATE
