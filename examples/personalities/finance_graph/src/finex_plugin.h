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


/* 
 * Authors: 
 * 
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */

#ifndef __PERSONALITY_FINEX_H__
#define __PERSONALITY_FINEX_H__

#include <api/ado_itf.h>
#include "finex_proto_generated.h"

class Finex_plugin : public Component::IADO_plugin
{  
private:
  static constexpr bool option_DEBUG = true;

public:
  /** 
   * Constructor
   * 
   * @param block_device Block device interface
   * 
   */
  Finex_plugin() {}

  /** 
   * Destructor
   * 
   */
  virtual ~Finex_plugin() {}

  /** 
   * Component/interface management
   * 
   */
  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0x59564581,0x9e1b,0x4811,0xbdb2,0x19,0x57,0xa0,0xa6,0x84,0x57);
  
  void * query_interface(Component::uuid_t& itf_uuid) override {
    if(itf_uuid == Component::IADO_plugin::iid()) {
      return (void *) static_cast<Component::IADO_plugin*>(this);
    }
    else return NULL; // we don't support this interface
  }

  void unload() override {
    delete this;
  }

public:

  /* IADO_plugin */
  status_t register_mapped_memory(void * shard_vaddr,
                                  void * local_vaddr,
                                  size_t len) override;

  status_t do_work(const uint64_t work_key,
                   const char * key,
                   size_t key_len,
                   IADO_plugin::value_space_t& values,
                   const void * in_work_request,
                   const size_t in_work_request_len,
                   bool new_root,
                   response_buffer_vector_t& response_buffers) override;
  
  status_t shutdown() override;

private:
  status_t handle_transaction(const Graph_ADO_protocol::Transaction * transaction,
                              uint64_t work_key,
                              void * value,
                              size_t value_len);

  void * allocate_memory(const uint64_t work_id,
                         const size_t size,
                         const size_t alignment) {
    void * ptr = nullptr;
    status_t rc;
    if((rc=_cb.allocate_pool_memory(size, alignment, ptr)) != S_OK)
      throw General_exception("allocate memory callback failed (%d)", rc);
    return ptr;
  }

  void free_memory(const uint64_t work_id,
                   const size_t size,
                   void * addr) {
    if(_cb.free_pool_memory(size, addr) != S_OK)
      throw General_exception("free memory callback failed");
  }


};




#endif // __PERSONALITY_FINEX_H__
