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


#include "fabric_server_factory.h"

#if 0
#include "fabric_memory_control.h"
#include "fabric_op_control.h"
#endif
#include "fabric_server.h"

#include <algorithm> /* transform */
#include <iterator> /* back_inserter */
#include <memory> /* make_shared, static_pointer_cast */

Fabric_server_factory::Fabric_server_factory(Fabric &fabric_, event_producer &eq_, ::fi_info &info_, std::uint32_t addr_, std::uint16_t port_)
  : Fabric_server_generic_factory(fabric_, eq_, info_, addr_, port_)
{}

Fabric_server_factory::~Fabric_server_factory()
{}

std::shared_ptr<event_expecter> Fabric_server_factory::new_server(Fabric &fabric_, event_producer &eq_, ::fi_info &info_)
{
	auto conn = std::make_shared<Fabric_server>(fabric_, eq_, info_);
	return std::static_pointer_cast<event_expecter>(conn);
#if 0
  return
    std::static_pointer_cast<fabric_endpoint>(
      std::static_pointer_cast<Fabric_op_control>(conn)
    );
#endif
}

component::IFabric_server * Fabric_server_factory::get_new_connection()
{
#if 0
  return static_cast<component::IFabric_server *>(Fabric_server_generic_factory::get_new_connection());
#else
  return static_cast<Fabric_server *>(Fabric_server_generic_factory::get_new_connection());
#endif
}

std::vector<component::IFabric_server *> Fabric_server_factory::connections()
{
#if 1
  // std::vector<event_expecter *>
  auto g = Fabric_server_generic_factory::connections();
  std::vector<component::IFabric_server *> v;
  std::transform(
    g.begin()
    , g.end()
    , std::back_inserter(v)
    , [] (event_expecter *v_)
      {
#if 0
        return static_cast<Fabric_server *>(static_cast<Fabric_connection_server *>(&*v_));
#else
        return static_cast<Fabric_server *>(&*v_);
#endif
      }
  );
  return v;
#else
	return Fabric_server_generic_factory::connections();
#endif
}

void Fabric_server_factory::close_connection(component::IFabric_server * cnxn_)
{
#if 0
  return Fabric_server_generic_factory::close_connection(static_cast<Fabric_connection_server *>(static_cast<Fabric_server *>(cnxn_)));
#else
  return Fabric_server_generic_factory::close_connection(static_cast<Fabric_server *>(cnxn_));
#endif
}
