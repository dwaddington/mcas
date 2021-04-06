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


#ifndef _FABRIC_CONNECTION_SERVER_H_
#define _FABRIC_CONNECTION_SERVER_H_

#include "fabric_connection.h"
#include "fabric_types.h"

struct fabric_endpoint;
struct event_producer;
struct fi_info;

#pragma GCC diagnostic push
#if defined __GNUC__ && 6 < __GNUC__ && __cplusplus < 201703L
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

class Fabric_connection_server
  : public fabric_connection
{
  /* BEGIN Fabric_op_control */
  void solicit_event() const override;
  void wait_event() const override;
  /* END Fabric_op_control */
public:
  /*
   * @throw fabric_bad_alloc : std::bad_alloc - out of memory
   * @throw fabric_runtime_error : std::runtime_error : ::fi_domain fail

   * @throw fabric_bad_alloc : std::bad_alloc - out of memory
   * @throw fabric_runtime_error : std::runtime_error : ::fi_domain fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_ep_bind fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_enable fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_ep_bind fail (event registration)
   */
  explicit Fabric_connection_server(
    component::IFabric_endpoint_unconnected_server *aep
  );
  Fabric_connection_server(const Fabric_connection_server &) = delete;
  Fabric_connection_server &operator=(const Fabric_connection_server &) = delete;
  ~Fabric_connection_server();

  /* TODO: Function shared with fabric_connection_client - combine */
  std::size_t max_message_size() const noexcept override;
  std::size_t max_inject_size() const noexcept override;
  /* END Function shared with fabric_connection_client */
};

#pragma GCC diagnostic pop

#endif
