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

#ifndef __EXAMPLE_VERSIONING_PLUGIN_H__
#define __EXAMPLE_VERSIONING_PLUGIN_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include <common/cycles.h>
#include <api/ado_itf.h>
#include <example_versioning_proto_generated.h>
#include <libpmem.h>

/** 
    Simple versioning root structure (fixed length).
 */
class ADO_example_versioning_plugin_root
{
public:
  static constexpr int MAX_VERSIONS = 3;
  
  ADO_example_versioning_plugin_root() {}

  void init() {
    pmem_memset_persist(this, 0, sizeof(ADO_example_versioning_plugin_root));
  }

  void check_recovery() {
    /* check for undo; this does not deal with failure on recovery */
    if(_undo.mid_tx()) {
      _current_slot = _undo.slot;
      _values[_current_slot] = _undo.value;
      _value_lengths[_current_slot] = _undo.value_len;
      _timestamps[_current_slot] = _undo.timestamp;
      pmem_persist(this, sizeof(*this));
      _undo.clear();
    }
  }
  
  void * add_version(void * value,
                     size_t value_len,
                     size_t& rv_len) {
    assert(value);
    void * rv = _values[_current_slot];
    rv_len = _value_lengths[_current_slot];

    /* create undo log for transaction */
    _undo = { _current_slot,
              _values[_current_slot],
              _value_lengths[_current_slot],
              _timestamps[_current_slot] };
    pmem_persist(&_undo, sizeof(_undo));

    /* perform transaction */
    _values[_current_slot] = value;
    _value_lengths[_current_slot] = value_len;
    _timestamps[_current_slot] = rdtsc();
    _current_slot++;    
    if(_current_slot == MAX_VERSIONS) _current_slot = 0;
    pmem_persist(&_current_slot, sizeof(_current_slot));

    /* reset undo log */
    _undo.clear();
               
    return rv; /* return value to be deleted */
  }

  void get_version(int version_index,
                   void*& out_value,
                   size_t& out_value_len,
                   cpu_time_t& out_time_stamp) const {
    int slot = _current_slot - 1;
    assert(abs(version_index) < MAX_VERSIONS);
    while(version_index < 0) {
      slot--;
      if(slot == -1) slot = MAX_VERSIONS - 1;
      version_index++;
    }
    out_value = _values[slot];
    out_value_len = _value_lengths[slot];
    out_time_stamp = _timestamps[slot];
  }
  
private:
  alignas(8) void *     _values[MAX_VERSIONS];
  alignas(8) size_t     _value_lengths[MAX_VERSIONS];
  alignas(8) int        _current_slot = 0;
  alignas(8) cpu_time_t _timestamps[MAX_VERSIONS];

  struct alignas(8) {
    int        slot;
    void *     value;
    size_t     value_len;
    cpu_time_t timestamp;

    void clear() {
      pmem_memset_persist(this, 0, sizeof(*this));
    }

    bool mid_tx() const {
      return timestamp > 0;
    }
    
  } _undo;
};
  

class ADO_example_versioning_plugin : public component::IADO_plugin
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
  ADO_example_versioning_plugin() {}

  /** 
   * Destructor
   * 
   */
  virtual ~ADO_example_versioning_plugin() {}

  /** 
   * Component/interface management
   * 
   */
  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0x84307b41,0x84f1,0x496d,0xbfbf,0xb5,0xe4,0xcf,0x40,0x70,0xe3);
  
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
                   const char * key,
                   size_t key_len,
                   IADO_plugin::value_space_t& values,
                   const void * in_work_request, /* don't use iovec because of non-const */
                   const size_t in_work_request_len,
                   bool new_root,
                   response_buffer_vector_t& response_buffers) override;
  
  status_t shutdown() override;

};


#pragma GCC diagnostic pop

#endif
