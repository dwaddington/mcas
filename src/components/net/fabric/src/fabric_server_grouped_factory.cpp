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


#include "fabric_server_grouped_factory.h"

#if 0
#include "fabric_memory_control.h"
#endif
#include "fabric_endpoint.h"
#include "fabric_server_grouped.h"

#include <algorithm> /* transform */
#include <iterator> /* back_inserter */
#include <memory> /* make_shared */

Fabric_server_grouped_factory::Fabric_server_grouped_factory(Fabric &fabric_, event_producer &eq_, ::fi_info &info_, std::uint32_t addr_, std::uint16_t port_)
  : Fabric_server_generic_factory(fabric_, eq_, info_, addr_, port_)
{
}

Fabric_server_grouped_factory::~Fabric_server_grouped_factory()
{}

std::shared_ptr<event_expecter> Fabric_server_grouped_factory::new_server(Fabric &fabric_, event_producer &eq_, ::fi_info &info_)
{
  auto conn = std::make_shared<Fabric_server_grouped>(fabric_, eq_, info_);
  return std::static_pointer_cast<event_expecter>(conn);
#if 0
  return
    std::static_pointer_cast<Fabric_memory_control>(
      std::static_pointer_cast<Fabric_op_control>(conn)
    );
#endif
}

component::IFabric_server_grouped * Fabric_server_grouped_factory::get_new_connection()
{
  return static_cast<Fabric_server_grouped *>(Fabric_server_generic_factory::get_new_connection());
}

std::vector<component::IFabric_server_grouped *> Fabric_server_grouped_factory::connections()
{
  auto g = Fabric_server_generic_factory::connections();
  std::vector<component::IFabric_server_grouped *> v;
  std::transform(
    g.begin()
    , g.end()
    , std::back_inserter(v)
    , [] (event_expecter *v_)
      {
#if 0
        return static_cast<Fabric_server_grouped *>(static_cast<Fabric_op_control *>(&*v_));
#else
        return static_cast<Fabric_server_grouped *>(&*v_);
#endif
      }
  );
  return v;
}

void Fabric_server_grouped_factory::close_connection(component::IFabric_server_grouped * cnxn_)
{
  return Fabric_server_generic_factory::close_connection(static_cast<Fabric_server_grouped *>(cnxn_));
}
