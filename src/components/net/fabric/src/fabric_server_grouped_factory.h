/*
   Copyright [2017-2021] [IBM Corporation]
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


#ifndef _FABRIC_SERVER_GROUPED_FACTORY_H_
#define _FABRIC_SERVER_GROUPED_FACTORY_H_

#include <api/fabric_itf.h> /* component::IFabric_server_grouped_factory */
#include "fabric_server_grouped.h" /* for covariant return */
#include "fabric_server_generic_factory.h"

#include <cstdint> /* uint16_t */
#include <memory> /* shared_ptr */
#include <thread>

struct fi_info;

class Fabric;
struct event_expecter;
struct event_producer;

class Fabric_server_grouped_factory
  : public component::IFabric_server_grouped_factory
  , public Fabric_server_generic_factory
{
public:
	component::IFabric_endpoint_unconnected_server * get_new_endpoint_unconnected() override { return Fabric_server_generic_factory::get_new_endpoint_unconnected(); }
	Fabric_server_grouped *open_connection(component::IFabric_endpoint_unconnected_server *) override;
  /**
   * Note: fi_info is not const because we reuse it when constructing the passize endpoint
   *
   * @throw std::system_error - ::setsockopt
   * @throw std::system_error - ::bind
   * @throw std::system_error - ::listen
   *
   * @throw fabric_runtime_error : std::runtime_error : ::fi_passive_ep fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_pep_bind fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_listen fail
   */
  explicit Fabric_server_grouped_factory(Fabric &fabric, event_producer &ev_pr, ::fi_info &info, std::uint32_t addr, std::uint16_t control_port);
  Fabric_server_grouped_factory(Fabric_server_grouped_factory &&) noexcept;
  ~Fabric_server_grouped_factory();

  /*
   * @throw std::logic_error : unexpected event
   * @throw std::system_error : read error on event pipe
   */
#if 0
  component::IFabric_server_grouped * get_new_connection() override;
#endif

  void close_connection(component::IFabric_server_grouped * connection) override;

  std::vector<component::IFabric_server_grouped *> connections() override;

  /**
   * @throw std::bad_alloc : fabric_bad_alloc - libfabric out of memory
   */
#if 0
  std::shared_ptr<event_expecter> new_server(Fabric &fabric, event_producer &eq, ::fi_info &entry) override;
#else
	/* Note: shared_ptr may be overkill */
#if 0
	auto open_connection(std::shared_ptr<component::IFabric_endpoint_unconnected> aep) -> Fabric_server_grouped * override;
#endif
#endif
  std::size_t max_message_size() const noexcept override { return Fabric_server_generic_factory::max_message_size(); }
  std::string get_provider_name() const override { return Fabric_server_generic_factory::get_provider_name(); }
  void cb(std::uint32_t event, ::fi_eq_cm_entry &entry) noexcept override { return Fabric_server_generic_factory::cb(event, entry); }
  void err(::fid_eq *eq, ::fi_eq_err_entry &entry) noexcept override { return Fabric_server_generic_factory::err(eq, entry); }
};

#endif
