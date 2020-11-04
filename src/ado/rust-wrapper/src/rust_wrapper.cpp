/*
  Copyright [2017-2020] [IBM Corporation]
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

#include "rust_wrapper.h"
#include <libpmem.h>
#include <api/interfaces.h>
#include <common/logging.h>
#include <string>



extern "C"
{
  struct Value {
    void * data;
    size_t size;    
  };
  
  status_t ffi_register_mapped_memory(uint64_t shard_vaddr, uint64_t local_vaddr, size_t len);
  status_t ffi_do_work(uint64_t work_id,
                       const char * key,
                       const Value * attached_value,
                       const Value * detached_value,
                       const uint8_t * request,
                       const size_t request_len);
}

status_t ADO_go_wrapper_plugin::register_mapped_memory(void *shard_vaddr,
                                                       void *local_vaddr,
                                                       size_t len) {

  return ffi_register_mapped_memory(reinterpret_cast<uint64_t>(shard_vaddr),
                                    reinterpret_cast<uint64_t>(local_vaddr),
                                    len);
}

status_t ADO_go_wrapper_plugin::do_work(uint64_t work_key,
                                        const char * key,
                                        size_t key_len,
                                        IADO_plugin::value_space_t& values,
                                        const void *in_work_request, /* don't use iovec because of non-const */
                                        const size_t in_work_request_len,
                                        bool new_root,
                                        response_buffer_vector_t& response_buffers) {

  
  (void)key_len; // unused
  (void)values; // unused
  (void)in_work_request; // unused
  (void)in_work_request_len; // unused
  (void)new_root; // unused
  (void)response_buffers; // unused

  assert(values.size() > 0);
  Value attached_value{values[0].ptr, values[0].len};

  if(values.size() > 1) {
    Value detached_value{values[1].ptr, values[1].len};
    return ffi_do_work(work_key, key, &attached_value, &detached_value,
                       reinterpret_cast<const uint8_t*>(in_work_request), in_work_request_len);    
  }
  else {
    Value detached_value{nullptr, 0};
    return ffi_do_work(work_key, key, &attached_value, &detached_value,
                       reinterpret_cast<const uint8_t*>(in_work_request), in_work_request_len);
  }
}

status_t ADO_go_wrapper_plugin::shutdown() {
  /* here you would put graceful shutdown code if any */
  return S_OK;
}

/**
 * Factory-less entry point
 *
 */
extern "C" void *factory_createInstance(component::uuid_t interface_iid) {
  PLOG("instantiating ADO_go_wrapper_plugin");
  if (interface_iid == interface::ado_plugin)
    return static_cast<void *>(new ADO_go_wrapper_plugin());
  else
    return NULL;
}

#undef RESET_STATE
