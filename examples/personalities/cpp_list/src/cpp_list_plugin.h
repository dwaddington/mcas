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

#ifndef __STRUCTURED_COMPONENT_H__
#define __STRUCTURED_COMPONENT_H__

#include <api/ado_itf.h>
#include <common/string_view.h>
#include <common/byte_span.h>
#include <cpp_list_proto_generated.h>
#include <ccpm/interfaces.h>

class ADO_structured_plugin : public component::IADO_plugin
{  
private:
  static constexpr bool option_DEBUG = true;
  using byte_span = common::byte_span;
  using string_view = common::string_view;
  using byte_string_view = common::basic_string_view<common::byte>;

public:
  /** 
   * Constructor
   * 
   * @param block_device Block device interface
   * 
   */
  ADO_structured_plugin() {}

  /** 
   * Destructor
   * 
   */
  virtual ~ADO_structured_plugin() {}

  /** 
   * Component/interface management
   * 
   */
  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0x59564581,0x9e1b,0x4811,0xbdb2,0x19,0x57,0xa0,0xa6,0x84,0x57);
  
  void * query_interface(component::uuid_t& itf_uuid) override {
    if(itf_uuid == component::IADO_plugin::iid()) {
      return (void *) static_cast<component::IADO_plugin*>(this);
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
                   byte_string_view key,
                   IADO_plugin::value_space_t& values,
                   byte_string_view in_work_request,
                   bool new_root,
                   response_buffer_vector_t& response_buffers) override;

  status_t shutdown() override;

private:

  status_t process_putvar_command(const structured_ADO_protocol::PutVariable * command,
                                  const ccpm::region_vector_t& regions);


  status_t process_invoke_command(const structured_ADO_protocol::Invoke * command,
                                  const ccpm::region_vector_t& regions,
                                  byte_span & out_work_response);

};




#endif
