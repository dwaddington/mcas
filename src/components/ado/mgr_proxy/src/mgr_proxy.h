/*
   Copyright [2019] [IBM Corporation]
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
 * Luna Xu (xuluna@ibm.com)
 *
 */

#ifndef __ADOMGRPROX_COMPONENT_H__
#define __ADOMGRPROX_COMPONENT_H__

#include <api/ado_itf.h>
#include <threadipc/queue.h>

class ADO_manager_proxy : public Component::IADO_manager_proxy {
 public:
  ADO_manager_proxy(unsigned    debug_level,
                    int         shard,
                    std::string cores,
                    float       cpu_num);
  ADO_manager_proxy();
  virtual ~ADO_manager_proxy();

  DECLARE_VERSION(0.1f);
  // clang-format off
  DECLARE_COMPONENT_UUID(0x8a120985, 0x1253, 0x404d, 0x94d7, 0x77, 0x92, 0x75,
                         0x21, 0xa1, 0x91);
  // clang-format on

  void *query_interface(Component::uuid_t &itf_uuid) override
  {
    if (itf_uuid == Component::IADO_manager_proxy::iid())
      return (void *) static_cast<Component::IADO_manager_proxy *>(this);
    else
      return NULL;  // we don't support this interface
  }

  void unload() override { delete this; }

 public:
  
  virtual Component::IADO_proxy *create(const uint64_t auth_id,
                                        Component::IKVStore * kvs,
                                        Component::IKVStore::pool_t pool_id,
                                        const std::string &pool_name,
                                        const size_t pool_size,
                                        const unsigned int pool_flags,
                                        const uint64_t expected_obj_count,
                                        const std::string &         filename,
                                        std::vector<std::string> &  args,
                                        numa_node_t value_memory_numa_zone,
                                        Component::SLA *sla = nullptr) override;

  virtual bool has_exited(Component::IADO_proxy *ado_proxy) override;

  virtual status_t shutdown(Component::IADO_proxy *ado) override;

 private:
  int                   shard;
  unsigned              debug_level;
  shared_memory_token_t token;
  std::string           cores;
  float                 cpu_num;
};

class ADO_manager_proxy_factory : public Component::IADO_manager_proxy_factory {
 public:
  DECLARE_VERSION(0.1f);
  DECLARE_COMPONENT_UUID(0xfac20985,
                         0x1253,
                         0x404d,
                         0x94d7,
                         0x77,
                         0x92,
                         0x75,
                         0x21,
                         0xa1,
                         0x91);  //

  void *query_interface(Component::uuid_t &itf_uuid) override
  {
    if (itf_uuid == Component::IADO_manager_proxy_factory::iid()) {
      return (void *) static_cast<Component::IADO_manager_proxy_factory *>(
          this);
    }
    else
      return NULL;  // we don't support this interface
  }

  void unload() override { delete this; }

  virtual Component::IADO_manager_proxy *create(unsigned    debug_level,
                                                int         core,
                                                std::string cores,
                                                float       cpu_num) override
  {
    Component::IADO_manager_proxy *obj =
        static_cast<Component::IADO_manager_proxy *>(
            new ADO_manager_proxy(debug_level, core, cores, cpu_num));
    assert(obj);
    obj->add_ref();
    return obj;
  }
};

#endif
