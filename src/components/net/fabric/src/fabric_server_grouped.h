/*
   Copyright [2017-2020] [IBM Corporation]
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

#include <api/fabric_itf.h> /* component::IFabric_server_grouped */
#include "event_expecter.h"
#include "fabric_connection_server.h"

#include "fabric_generic_grouped.h"
#include "fabric_types.h" /* addr_ep_t */

#include <cstdint> /* uint{16,32,64}_t */
#include <functional> /* function */
#include <mutex> /* unique_lock */

struct fi_info;
struct fi_cq_err_entry;
struct event_producer;
class Fabric;
class Fabric_comm_grouped;

#pragma GCC diagnostic push
#if defined __GNUC__ && 6 < __GNUC__ && __cplusplus < 201703L
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

class Fabric_server_grouped
	: public component::IFabric_server_grouped
	, public component::IFabric_initiator /* for internal use */
	, public event_expecter
{
	fabric_endpoint _aep;
	Fabric_connection_server _srv;
	const fabric_endpoint *aep() const { return &_aep; }
	fabric_endpoint *aep() { return &_aep; }
#if 0
  Fabric_op_control &c() { return *this; }
#endif
  Fabric_generic_grouped _g;

  /* BEGIN component::IFabric_server_grouped (IFabric_connection) */
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
  std::string get_peer_addr() override { return _srv.get_peer_addr(); }
  std::string get_local_addr() override { return _srv.get_local_addr(); }
public:
	void expect_event(std::uint32_t ev) { aep()->expect_event(ev); }
  std::size_t max_message_size() const noexcept override { return _srv.max_message_size(); }
  std::size_t max_inject_size() const noexcept override { return _srv.max_inject_size(); }
  /* END component::IFabric_server_grouped (IFabric_connection) */
private:
  component::IFabric_group *allocate_group() override { return _g.allocate_group(); }

public:
  /*
   * @throw fabric_bad_alloc : std::bad_alloc - libfabric allocation out of memory
   *
   * @throw fabric_bad_alloc : std::bad_alloc - out of memory
   * @throw fabric_runtime_error : std::runtime_error : ::fi_domain fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_ep_bind fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_enable fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_ep_bind fail (event registration)
   */
  explicit Fabric_server_grouped(
    Fabric &fabric
    , event_producer &ep
    , ::fi_info & info
  );

  ~Fabric_server_grouped();

  /* BEGIN IFabric_server_grouped (IFabric_op_completer) */
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_old &completion_callback) override
  {
    return aep()->poll_completions(completion_callback);
  }
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_definite &completion_callback) override
  {
    return aep()->poll_completions(completion_callback);
  }
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_tentative &completion_callback) override
  {
    return aep()->poll_completions_tentative(completion_callback);
  }
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_param_definite &completion_callback, void *callback_param) override
  {
    return aep()->poll_completions(completion_callback, callback_param);
  }
  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative &completion_callback, void *callback_param) override
  {
    return aep()->poll_completions_tentative(completion_callback, callback_param);
  }
  /**
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_param_definite_ptr_noexcept completion_callback, void *callback_param) override
  {
    return aep()->poll_completions(completion_callback, callback_param);
  }
  /**
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative_ptr_noexcept completion_callback, void *callback_param) override
  {
    return aep()->poll_completions_tentative(completion_callback, callback_param);
  }

  std::size_t stalled_completion_count() override { return aep()->stalled_completion_count(); }
  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_control fail
   * @throw std::system_error : pselect fail
   */
  void wait_for_next_completion(unsigned polls_limit) override { return aep()->wait_for_next_completion(polls_limit); }
  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_control fail
   * @throw std::system_error : pselect fail
   */
  void wait_for_next_completion(std::chrono::milliseconds timeout) override { return aep()->wait_for_next_completion(timeout); }
  void unblock_completions() override { return aep()->unblock_completions(); }
  /* END IFabric_server_grouped (IFabric_op_completer) */

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_sendv fail
   */
  void post_send(gsl::span<const ::iovec> buffers, void **desc, void *context) override { return _g.post_send(buffers, desc, context); }
  void post_send(gsl::span<const ::iovec> buffers, void *context) override { return _g.post_send(buffers, context); }

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_recvv fail
   */
  void post_recv(gsl::span<const ::iovec> buffers, void **desc, void *context) override { return _g.post_recv(buffers, desc, context); }
  void post_recv(gsl::span<const ::iovec> buffers, void *context) override { return _g.post_recv(buffers, context); }

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_readv fail
   */
  void post_read(
    gsl::span<const ::iovec> buffers
    , void **desc
    , std::uint64_t remote_addr
    , std::uint64_t key
    , void *context
  ) override { return _g.post_read(buffers, desc, remote_addr, key, context); }
  void post_read(
    gsl::span<const ::iovec> buffers,
    std::uint64_t remote_addr,
    std::uint64_t key,
    void *context
  ) override { return _g.post_read(buffers, remote_addr, key, context); }

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_writev fail
   */
  void post_write(
    gsl::span<const ::iovec> buffers
    , void **desc
    , std::uint64_t remote_addr
    , std::uint64_t key
    , void *context
  ) override { return _g.post_write(buffers, desc, remote_addr, key, context); }
  void post_write(
    gsl::span<const ::iovec> buffers,
    std::uint64_t remote_addr,
    std::uint64_t key,
    void *context
  ) override { return _g.post_write(buffers, remote_addr, key, context); }
  void inject_send(const void *buf, std::size_t len) override { return _g.inject_send(buf, len); }

  fabric_types::addr_ep_t get_name() const;

  void forget_group(Fabric_comm_grouped *);
};

#pragma GCC diagnostic pop

#endif
