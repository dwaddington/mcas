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


#ifndef _FABRIC_CONNECTION_SERVER_H_
#define _FABRIC_CONNECTION_SERVER_H_

#if 0
#include <api/fabric_itf.h> /* component::IFabric_server */
#endif
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
    component::IFabric_endpoint_unconnected *aep
  );
  Fabric_connection_server(const Fabric_connection_server &) = delete;
  Fabric_connection_server &operator=(const Fabric_connection_server &) = delete;
  ~Fabric_connection_server();
#if 0
  /* BEGIN IFabric_op_control */
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_old &completion_callback) override;
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_param_definite &completion_callback, void *callback_param) override;
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative &completion_callback, void *callback_param) override;
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_definite &completion_callback) override;
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_tentative &completion_callback) override;
  /**
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_param_definite_ptr_noexcept completion_callback, void *callback_param) override;
  /**
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative_ptr_noexcept completion_callback, void *callback_param) override;

  std::size_t stalled_completion_count() override;
  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_control fail
   * @throw std::system_error : pselect fail
   */
  void wait_for_next_completion(unsigned polls_limit) override;
  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_control fail
   * @throw std::system_error : pselect fail
   */
  void wait_for_next_completion(std::chrono::milliseconds timeout) override;
  void unblock_completions() override;
  /* END IFabric_op_control */
#endif

#if 0
  using memory_region_t = component::IFabric_endpoint_unconnected::memory_region_t;
  using const_byte_span = component::IFabric_endpoint_unconnected::const_byte_span;

  /**
   * @throw std::range_error - address already registered
   * @throw std::logic_error - inconsistent memory address tables
   */
  memory_region_t register_memory(
    const_byte_span contig
    , std::uint64_t key
    , std::uint64_t flags
  ) override;
  /**
   * @throw std::range_error - address not registered
   * @throw std::logic_error - inconsistent memory address tables
   */
  void deregister_memory(
    const memory_region_t memory_region
  ) override;
  std::uint64_t get_memory_remote_key(
    const memory_region_t memory_region
  ) const noexcept override;
  void *get_memory_descriptor(
    const memory_region_t memory_region
  ) const noexcept override;
#endif
#if 0
  std::string get_peer_addr() override;
  std::string get_local_addr() override;
#endif
  /* TODO: Function shared with fabric_connection_client - combine */
  std::size_t max_message_size() const noexcept override;
  std::size_t max_inject_size() const noexcept override;
  /* END Function shared with fabric_connection_client */
};

#pragma GCC diagnostic pop

#endif
