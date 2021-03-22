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


#ifndef _FABRIC_CLIENT_GROUPED_H_
#define _FABRIC_CLIENT_GROUPED_H_

#include <api/fabric_itf.h> /* component::IFabric_client_grouped */
#include "fabric_connection_client.h"

#include "fabric_generic_grouped.h"
#include "fabric_endpoint.h" /* fi_cq_entry_t */
#include "fabric_types.h" /* addr_ep_t */

#include "rdma-fi_domain.h" /* fi_cq_err_entry */

#include <cstdint> /* uint{16,32,64}_t */
#include <mutex> /* unique_lock */
#include <set>
#include <vector>

struct fi_info;
struct fi_cq_err_entry;
struct event_producer;
class Fabric;
class Fabric_comm_grouped;

#pragma GCC diagnostic push
#if defined __GNUC__ && 6 < __GNUC__ && __cplusplus < 201703L
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

class Fabric_client_grouped
  : public component::IFabric_client_grouped
  , public Fabric_connection_client
{
  Fabric_generic_grouped _g;

  /* BEGIN component::IFabric_client_grouped (IFabric_connection) */
  /**
   * @throw std::range_error - address already registered
   * @throw std::logic_error - inconsistent memory address tables
   */
  memory_region_t register_memory(
    const_byte_span contig
    , std::uint64_t key
    , std::uint64_t flags
  ) override
  {
    return aep()->register_memory(contig, key, flags);
  }
  /**
   * @throw std::range_error - address not registered
   * @throw std::logic_error - inconsistent memory address tables
   */
  void deregister_memory(
    const memory_region_t memory_region
  ) override
  {
    return aep()->deregister_memory(memory_region);
  };
  std::uint64_t get_memory_remote_key(
    const memory_region_t memory_region
  ) const noexcept override
  {
    return aep()->get_memory_remote_key(memory_region);
  };
  void *get_memory_descriptor(
    const memory_region_t memory_region
  ) const noexcept override
  {
    return aep()->get_memory_descriptor(memory_region);
  };
  std::string get_peer_addr() override { return Fabric_connection_client::get_peer_addr(); }
  std::string get_local_addr() override { return Fabric_connection_client::get_local_addr(); }

  /* END component::IFabric_client_grouped (IFabric_connection) */
  component::IFabric_group *allocate_group() override { return _g.allocate_group(); }

public:
  /*
   * @throw bad_dest_addr_alloc : std::bad_alloc
   * @throw fabric_runtime_error : std::runtime_error : ::fi_domain fail
   * @throw fabric_bad_alloc : std::bad_alloc - fabric allocation out of memory
   * @throw fabric_runtime_error : std::runtime_error : ::fi_connect fail
   *
   * @throw fabric_bad_alloc : std::bad_alloc - out of memory
   * @throw fabric_runtime_error : std::runtime_error : ::fi_ep_bind fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_enable fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_ep_bind fail (event registration)
   *
   * @throw std::logic_error : socket initialized with a negative value (from ::socket) in Fd_control
   * @throw std::logic_error : unexpected event
   * @throw std::system_error (receiving fabric server name)
   * @throw std::system_error : pselect fail (expecting event)
   * @throw std::system_error : resolving address
   *
   * @throw std::system_error : read error on event pipe
   * @throw std::system_error : pselect fail
   * @throw std::system_error : read error on event pipe
   * @throw fabric_bad_alloc : std::bad_alloc - libfabric out of memory (creating a new server)
   * @throw std::system_error - writing event pipe (normal callback)
   * @throw std::system_error - writing event pipe (readerr_eq)
   * @throw std::system_error - receiving data on socket
   */
  explicit Fabric_client_grouped(
    component::IFabric_endpoint_unconnected *aep
    , event_producer &ep
    , fabric_types::addr_ep_t peer_addr
  );

  ~Fabric_client_grouped();
  void forget_group(Fabric_comm_grouped *g) { return _g.forget_group(g); }

  /* BEGIN IFabric_client_grouped (IFabric_op_completer) */
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_old &completion_callback) override
  {
    return _g.poll_completions(completion_callback);
  }
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_param_definite &completion_callback, void *callback_param) override
  {
    return _g.poll_completions(completion_callback, callback_param);
  }
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative &completion_callback, void *callback_param) override
  {
    return _g.poll_completions_tentative(completion_callback, callback_param);
  }
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_definite &completion_callback) override
  {
    return _g.poll_completions(completion_callback);
  }
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_tentative &completion_callback) override
  {
    return _g.poll_completions_tentative(completion_callback);
  }
  /**
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_param_definite_ptr_noexcept completion_callback, void *callback_param) override
  {
    return _g.poll_completions(completion_callback, callback_param);
  }
  /**
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative_ptr_noexcept completion_callback, void *callback_param) override
  {
    return _g.poll_completions_tentative(completion_callback, callback_param);
  }

  std::size_t stalled_completion_count() override { return _g.stalled_completion_count(); }
  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_control fail
   * @throw std::system_error : pselect fail
   */
  void wait_for_next_completion(unsigned polls_limit) override { return _g.wait_for_next_completion(polls_limit); }
  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_control fail
   * @throw std::system_error : pselect fail
   */
  void wait_for_next_completion(std::chrono::milliseconds timeout) override { return _g.wait_for_next_completion(timeout); }
  void unblock_completions() override { return _g.unblock_completions(); }
  /* END IFabric_client_grouped (IFabric_op_completer) */

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_recvv fail
   */
  void post_recv(gsl::span<const ::iovec> buffers, void **desc, void *context) override { return aep()->post_recv(buffers, desc, context); }
  void post_recv(gsl::span<const ::iovec> buffers, void *context) override { return aep()->post_recv(buffers, context); }

  fabric_types::addr_ep_t get_name() const { return _g.get_name(); }

  /*
   * @throw std::logic_error : unexpected event
   * @throw std::system_error : read error on event pipe
  */
  std::size_t max_message_size() const noexcept override { return Fabric_connection_client::max_message_size(); }
  std::size_t max_inject_size() const noexcept override { return Fabric_connection_client::max_inject_size(); }
};

#pragma GCC diagnostic pop

#endif
