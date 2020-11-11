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


/*
 * Authors:
 *
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */

#ifndef __RUST_ADO_WRAPPER_COMPONENT_H__
#define __RUST_ADO_WRAPPER_COMPONENT_H__

#include <api/ado_itf.h>


class ADO_rust_wrapper_plugin : public component::IADO_plugin
{
public:
  /**
   * Constructor
   *
   * @param block_device Block device interface
   *
   */
  ADO_rust_wrapper_plugin() {}

  /**
   * Destructor
   *
   */
  virtual ~ADO_rust_wrapper_plugin() {}

  /**
   * Component/interface management
   *
   */
  DECLARE_VERSION(1.0f);
  DECLARE_COMPONENT_UUID(0xe1968476,0xfa83,0x489c,0x9b41,0xa7,0xbf,0x26,0x9d,0x65,0x39);

  void * query_interface(component::uuid_t& itf_uuid) override {
    if(itf_uuid == component::IADO_plugin::iid()) {
      return static_cast<component::IADO_plugin*>(this);
    }
    else return NULL; // we don't support this interface
  }

  void unload() override {
    delete this;
  }

public:

  /* IADO_plugin */
  void launch_event(const uint64_t auth_id,
                    const std::string& pool_name,
                    const size_t pool_size,
                    const unsigned int pool_flags,
                    const unsigned int memory_type,
                    const size_t expected_obj_count,
                    const std::vector<std::string>& params);

  status_t register_mapped_memory(void * shard_vaddr,
                                  void * local_vaddr,
                                  size_t len) override;

  void notify_op_event(component::ADO_op op) override;

  void cluster_event(const std::string& sender,
                     const std::string& type,
                     const std::string& message) override;

  status_t do_work(const uint64_t work_key,
                   const char * key,
                   size_t key_len,
                   IADO_plugin::value_space_t& values,
                   const void * in_work_request, /* don't use iovec because of non-const */
                   const size_t in_work_request_len,
                   bool new_root,
                   response_buffer_vector_t& response_buffers) override;

  status_t shutdown() override;

};




#endif
