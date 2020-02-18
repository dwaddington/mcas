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

#ifndef __PYTHON_NUMPY_PLUGIN_H__
#define __PYTHON_NUMPY_PLUGIN_H__

#include <common/cycles.h>
#include <api/ado_itf.h>


class ADO_python_numpy_plugin : public Component::IADO_plugin
{  
private:
  unsigned _debug_level = 0;

public:
  /** 
   * Constructor
   * 
   */
  ADO_python_numpy_plugin();

  /** 
   * Destructor
   * 
   */
  virtual ~ADO_python_numpy_plugin();

  /** 
   * Component/interface management
   * 
   */
  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0xe5edffcd,0xee74,0x4f92,0x8dc8,0x7e,0xfe,0xa6,0xea,0x45,0x4e);
  
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
                   const void * in_work_request, /* don't use iovec because of non-const */
                   const size_t in_work_request_len,
                   bool new_root,
                   response_buffer_vector_t& response_buffers) override;

  status_t set_debug_level(unsigned debug_level) {
    _debug_level = debug_level;
    PLOG("python_numpy_plugin: debug level set to %u", debug_level);
    return S_OK;
  }

  status_t shutdown();

};




#endif
