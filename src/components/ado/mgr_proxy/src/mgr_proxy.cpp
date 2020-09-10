/*
   Copyright [2019-2020] [IBM Corporation]
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

#include <api/components.h>
#include <common/logging.h>
#include <common/utils.h>
#include <sys/types.h>
#include <cassert>
#include <threadipc/queue.h>
#include "mgr_proxy.h"

using namespace component;
using namespace common;
using namespace std;
using namespace threadipc;

ADO_manager_proxy::ADO_manager_proxy(unsigned    debug_level_,
                                     unsigned    shard_,
                                     std::string cores_,
                                     float       cpu_num_)
    : _shard(shard_), _debug_level(debug_level_), _cores(cores_), _cpu_num(cpu_num_)
{
  (void)_debug_level; // unused
}

ADO_manager_proxy::ADO_manager_proxy()
  : _shard(0),
    _debug_level(0),
    _cores(""),
    _cpu_num(1)
{}

ADO_manager_proxy::~ADO_manager_proxy() {}

IADO_proxy *ADO_manager_proxy::create(const uint64_t auth_id,
                                      const unsigned debug_level_,
                                      component::IKVStore * kvs,
                                      component::IKVStore::pool_t pool_id,
                                      const std::string &pool_name,
                                      const size_t pool_size,
                                      const unsigned int pool_flags,
                                      const uint64_t expected_obj_count,
                                      const std::string& filename,
                                      std::vector<std::string>& args,
                                      numa_node_t value_memory_numa_zone,
                                      SLA * sla)
{
  (void)sla; // unused
  Thread_ipc::instance()->schedule_to_mgr(_shard, _cores, _cpu_num,
                                          value_memory_numa_zone);

  struct threadipc::Message *msg = NULL;
  Thread_ipc::instance()->get_next_ado(msg);

  while (!msg || msg->shard_core != _shard || msg->op != Operation::schedule) {
    if (msg) Thread_ipc::instance()->add_to_ado(msg);
    Thread_ipc::instance()->get_next_ado(msg);
  }


  // TODO:ask manager what memory;
  int memory = MB(1);

  component::IBase *comp = component::load_component("libcomponent-adoproxy.so",
                                                     component::ado_proxy_factory);

  auto fact =
    make_itf_ref(
      static_cast<IADO_proxy_factory *>(
        comp->query_interface(IADO_proxy_factory::iid())
      )
    );

  assert(fact);

  auto ado =
    make_itf_ref(fact->create(auth_id, debug_level_,
			      kvs, pool_id, pool_name,
			      pool_size, pool_flags,
			      expected_obj_count, filename, args, msg->cores,
			      memory, msg->core_number, msg->numa_zone)
    );

  Thread_ipc::instance()->register_to_mgr(_shard, msg->cores,
                                          ado->ado_id());

  return ado.release();
}

bool ADO_manager_proxy::has_exited(IADO_proxy *ado)
{
  return ado->has_exited();
}

status_t ADO_manager_proxy::shutdown_ado(IADO_proxy *ado)
{
  auto ado_ref = make_itf_ref(ado);
  return S_OK;
}
/**
 * Factory entry point
 *
 */
extern "C" void *factory_createInstance(component::uuid_t component_id)
{
  if (component_id == ADO_manager_proxy_factory::component_id()) {
    return static_cast<void *>(new ADO_manager_proxy_factory());
  }
  else
    return NULL;
}
