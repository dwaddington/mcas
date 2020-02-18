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

#ifndef __EXAMPLE_FB_PLUGIN_H__
#define __EXAMPLE_FB_PLUGIN_H__

#include <common/cycles.h>
#include <api/ado_itf.h>
#include <example_fb_proto_generated.h>

/** 
    Simple versioning root structure (fixed length). This version
    is not crash consistent 
 */
class ADO_example_fb_plugin_root
{
public:
  static constexpr unsigned MAX_VERSIONS = 3;
  
  ADO_example_fb_plugin_root() {}

  void init() {
    memset(this, 0, sizeof(ADO_example_fb_plugin_root));
  }
  
  void * add_version(void * value,
                     size_t value_len,
                     size_t& rv_len) {
    assert(value);
    void * rv = _values[_current_slot];
    rv_len = _value_lengths[_current_slot];
    _values[_current_slot] = value;
    _value_lengths[_current_slot] = value_len;
    _timestamps[_current_slot] = rdtsc();
    if(++_current_slot == MAX_VERSIONS) _current_slot = 0;
    return rv; /* return value to be deleted */
  }

  void get_version(int version_index,
                   void*& out_value,
                   size_t& out_value_len,
                   cpu_time_t& out_time_stamp) const {
    int slot = _current_slot;
    assert(abs(version_index) < MAX_VERSIONS);
    while(version_index < 0) {
      slot--;
      if(slot == -1) slot = MAX_VERSIONS - 1;
      version_index++;
      PLOG("slot %d: %p", slot, _values[slot]);
    }
    out_value = _values[slot];
    out_value_len = _value_lengths[slot];
    out_time_stamp = _timestamps[slot];
  }
  
private:
  void *     _values[MAX_VERSIONS];
  size_t     _value_lengths[MAX_VERSIONS];
  int        _current_slot;
  cpu_time_t _timestamps[MAX_VERSIONS];
};
  

class ADO_example_fb_plugin : public Component::IADO_plugin
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
  ADO_example_fb_plugin() {}

  /** 
   * Destructor
   * 
   */
  virtual ~ADO_example_fb_plugin() {}

  /** 
   * Component/interface management
   * 
   */
  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0x84307b41,0x84f1,0x496d,0xbfbf,0xb5,0xe4,0xcf,0x40,0x70,0xe3);
  
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
  
  status_t shutdown() override;

};




#endif
