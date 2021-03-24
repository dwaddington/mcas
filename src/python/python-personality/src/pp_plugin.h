/*
   Copyright [2021] [IBM Corporation]
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

#ifndef __PP_PLUGIN_H__
#define __PP_PLUGIN_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include <api/ado_itf.h>

constexpr unsigned DEBUG_LEVEL = 3;

/** 
 *  Python Personality plugin
 * 
 */
class Pp_plugin : public component::IADO_plugin,
                  private common::log_source
{  
public:

  /** 
   * Constructor
   * 
   */
  Pp_plugin();

  /** 
   * Destructor
   * 
   */
  virtual ~Pp_plugin() {}

  /** 
   * Component/interface management
   * 
   */
  DECLARE_VERSION(1.0f);
  DECLARE_COMPONENT_UUID(0xb8560b79,0x1287,0x4423,0x94,0x42,0x9c,0xfb,0xae,0x16,0xea);
  
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

  void launch_event(const uint64_t                  auth_id,
                    const std::string&              pool_name,
                    const size_t                    pool_size,
                    const unsigned int              pool_flags,
                    const unsigned int              memory_type,
                    const size_t                    expected_obj_count,
                    const std::vector<std::string>& params) override;

  status_t do_work(const uint64_t              work_key,
                   const char *                key,
                   size_t                      key_len,
                   IADO_plugin::value_space_t& values,
                   const void *                in_work_request, /* don't use iovec because of non-const */
                   const size_t                in_work_request_len,
                   bool                        new_root,
                   response_buffer_vector_t&   response_buffers) override;
  
  status_t shutdown() override;

private:

  status_t execute_python(const uint64_t              work_key,
                          const char *                key,
                          const size_t                key_len,
                          void *                      value,
                          const size_t                value_len,
                          const char *                code_string,
                          const char *                function_name,
                          response_buffer_vector_t&   response_buffers);

  void write_to_store(const uint64_t work_key,
                      const char *   varname,
                      PyObject *     object);

  status_t unwrap_nparray_from_data_descriptor(const char * key,
                                               const size_t key_len,
                                               void *       value,
                                               const size_t value_len,
                                               PyObject *&  out_ndarray);

  status_t unwrap_pickle_from_data_descriptor(const char * key,
                                              const size_t key_len,
                                              void *       value,
                                              const size_t value_len,
                                              PyObject *&  out_object);
};


#pragma GCC diagnostic pop

#endif
