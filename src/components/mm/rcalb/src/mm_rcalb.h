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


#ifndef __MM_RCALB_COMPONENT_H__
#define __MM_RCALB_COMPONENT_H__

#include <string>
#include <set>
#include <memory>
#include <common/logging.h>
#include <api/mm_itf.h>


class Rca_LB_memory_manager : public component::IMemory_manager_volatile_reconstituting
{
 public:
  Rca_LB_memory_manager(const unsigned debug_level) {
    if(debug_level > 0)
      PLOG("Rca_LB_memory_manager: ctor");
  }

  virtual ~Rca_LB_memory_manager() {
  }
  
  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0xf811cb01,0x0911,0x4bb5,0x93f6,0x79,0xd6,0x11,0xba,0x9a,0xd4);
  
  void* query_interface(component::uuid_t& itf_uuid) override
  {
    if (itf_uuid == component::IMemory_manager_volatile_reconstituting::iid()) {
      return static_cast<component::IMemory_manager_volatile_reconstituting*>(this);
    }
    else return NULL;  // we don't support this interface
  }

  void unload() override { delete this; }

 public:
  virtual void print_info() override {
    PINF("MM: RCA LB");
  }

  

};


/** 
 * Factory
 * 
 */
class Rca_LB_memory_manager_factory : public component::IMemory_manager_factory {
 public:
  DECLARE_VERSION(0.1f);

  /* index_factory - see components.h */
  DECLARE_COMPONENT_UUID(0xfac1cb01, 0x0911, 0x4bb5, 0x93f6, 0x79, 0xd6, 0x11, 0xba, 0x9a, 0xd4);

  void* query_interface(component::uuid_t& itf_uuid) override
  {
    if (itf_uuid == component::IMemory_manager_factory::iid())
      return static_cast<component::IMemory_manager_factory*>(this);
    else return NULL;  // we don't support this interface
  }

  void unload() override { delete this; }

  virtual component::IMemory_manager_volatile * create_mm_volatile(const unsigned /*debug_level*/) override
  {
    return nullptr;
  }
  
  virtual component::IMemory_manager_volatile_reconstituting * create_mm_volatile_reconstituting(const unsigned debug_level) override
  {
    
    auto new_instance = static_cast<component::IMemory_manager_volatile_reconstituting *>
      (new Rca_LB_memory_manager(debug_level));
    
    new_instance->add_ref();
    return new_instance;
  }
  
  virtual component::IMemory_manager_persistent * create_mm_persistent(const unsigned /*debug_level*/) override {
    return nullptr;
  }
  
};




#endif // __MM_RCALB_COMPONENT_H__
