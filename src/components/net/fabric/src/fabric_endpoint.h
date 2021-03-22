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


#ifndef _FABRIC_ENDPOINT_ACTIVE_H_
#define _FABRIC_ENDPOINT_ACTIVE_H_

#include <api/fabric_itf.h> /* component::IFabric_op_completer, ::status_t */
#include "event_consumer.h"

#include "fabric_cq.h"
#include "fabric_ptr.h" /* fid_unique_ptr */
#include "fabric_types.h" /* addr_ep_t */
#include "fd_pair.h"

#include "rdma-fi_domain.h" /* fi_cq_attr, fi_cq_err_entry, fi_cq_data_entry */

#include <atomic>
#include <cstdint> /* uint{64,64}_t */
#include <map> /* multimap */
#include <memory> /* shared_ptr, unique_ptr */
#include <mutex>
#include <set>
#include <vector>

struct fi_info;
struct fi_cq_err_entry;
struct fid_cq;
struct fid_domain;
struct fid_ep;
struct fid_mr;
struct event_producer;
struct event_registration;
class Fabric;
class fabric_connection;

struct mr_and_address;

struct ru_flt_counter
{
private:
  bool _report;
  long _ru_flt_start;
public:
  ru_flt_counter(bool report);
  ~ru_flt_counter();
};
#pragma GCC diagnostic push
#if defined __GNUC__ && 6 < __GNUC__ && __cplusplus < 201703L
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

struct fabric_endpoint
  : public component::IFabric_endpoint_unconnected
  , public component::IFabric_endpoint_connected
  , public event_consumer
{
private:
  using byte_span = common::byte_span;
  Fabric &_fabric;
  std::shared_ptr<::fi_info> _domain_info;
	std::uint16_t _port;
	fabric_types::addr_ep_t _peer_addr;
  std::shared_ptr<::fid_domain> _domain;
  std::mutex _m; /* protects _mr_addr_to_desc, _mr_desc_to_addr */
  /*
   * Map of [starts of] registered memory regions to mr_and_address objects.
   * The map is maintained because no other layer provides fi_mr values for
   * the addresses in an iovec.
   */
  using map_addr_to_mra = std::multimap<const void *, std::unique_ptr<mr_and_address>>;
  map_addr_to_mra _mr_addr_to_mra;
  bool _paging_test;
  ru_flt_counter _fault_counter;

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_mr_reg fail
   */
  ::fid_mr *make_fid_mr_reg_ptr(
    const_byte_span buf
    , std::uint64_t access
    , std::uint64_t key
    , std::uint64_t flags
  ) const;

  ::fid_mr *covering_mr(byte_span v);
#if CAN_USE_WAIT_SETS
  ::fi_wait_attr _wait_attr;
  fid_unique_ptr<::fid_wait> _wait_set; /* make_fid_wait(fid_fabric &fabric, fi_wait_attr &attr) */
#endif
  std::mutex _m_fd_unblock_set;
  std::set<int> _fd_unblock_set;
  /* pingpong example used separate tx and rx completion queues.
   * Not sure why; perhaps it was for accounting.
   */
  ::fi_cq_attr _cq_attr;
  Fabric_cq _rxcq;
  Fabric_cq _txcq;

  std::shared_ptr<::fi_info> _ep_info;
  std::shared_ptr<::fid_ep> _ep;

  /* Events tagged for _ep, demultiplexed from the shared event queue to this pipe.
   * Perhaps we should provide a separate event queue for every connection, but not
   * sure if hardware would support that.
   */
  Fd_pair _event_pipe;
  std::unique_ptr<event_registration> _event_registration;

  /* true after an FI_SHUTDOWN event has been observed */
  std::atomic<bool> _shut_down;

  /* BEGIN component::IFabric_op_completer */

  /* END IFabric_op_completer */

  /* BEGIN event_consumer */
  /*
   * @throw std::system_error - writing event pipe
   */
  void cb(std::uint32_t event, ::fi_eq_cm_entry &entry) noexcept override;
  /*
   * @throw std::system_error - writing event pipe
   */
  void err(::fid_eq *eq, ::fi_eq_err_entry &entry) noexcept override;
  /* END event_consumer */

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_endpoint fail (make_fid_aep)
   */
  std::shared_ptr<::fid_ep> make_fid_aep(::fi_info &info, void *context) const;

  fid_mr *make_fid_mr_reg_ptr(
    const void *buf
    , std::size_t len
    , std::uint64_t access
    , std::uint64_t key
    , std::uint64_t flags
  ) const;

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_cq_open fail (make_fid_cq)
   */
  fid_unique_ptr<::fid_cq> make_fid_cq(::fi_cq_attr &attr, void *context) const;

	void sendmsg(
		gsl::span<const ::iovec> buffers_
		, void **desc_
		, ::fi_addr_t addr_
		, void *context_
		, std::uint64_t flags
	);

public:
  const ::fi_info &ep_info() const { return *_ep_info; }
  ::fi_info &modifiable_ep_info() const { return *_ep_info; }
  Fabric_cq &rxcq() { return _rxcq; }
  Fabric_cq &txcq() { return _txcq; }
  ::fid_ep &ep() { return *_ep; }
  /*
   * @throw std::system_error : pselect fail
   * @throw fabric_bad_alloc : std::bad_alloc - libfabric out of memory (creating a new server)
   * @throw std::system_error - writing event pipe (normal callback)
   * @throw std::system_error - writing event pipe (readerr_eq)
   */
  void ensure_event(const fabric_connection *cnxn) const;

	component::IFabric_client *make_open_client() override;
	component::IFabric_client_grouped *make_open_client_grouped() override;

  /*
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(const component::IFabric_op_completer::complete_old &completion_callback) override;
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
  /**
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions(component::IFabric_op_completer::complete_param_definite_ptr_noexcept completion_callback, void *callback_param) override;
  /**
   * @throw fabric_runtime_error : std::runtime_error - cq_read unhandled error
   * @throw std::logic_error - called on closed connection
   */
  std::size_t poll_completions_tentative(component::IFabric_op_completer::complete_param_tentative_ptr_noexcept completion_callback, void *callback_param) override;

  std::size_t stalled_completion_count() override
  {
    return _rxcq.stalled_completion_count() + _txcq.stalled_completion_count();
  }
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
#if 0
  std::string get_local_addr() override;
#endif
  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_sendv fail
   */
  void post_send(
    gsl::span<const ::iovec> buffers
    , void **desc
    , void *context
  );

  void post_send(
    gsl::span<const ::iovec> buffers
    , void *context
  );

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_recvv fail
   */
  void post_recv(
    gsl::span<const ::iovec> buffers
    , void **desc
    , void *context
  );

  void post_recv(
    gsl::span<const ::iovec> buffers
    , void *context
  );

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_readv fail
   */
  void post_read(
    gsl::span<const ::iovec> buffers
    , void **desc
    , std::uint64_t remote_addr
    , std::uint64_t key
    , void *context
  );

  void post_read(
    gsl::span<const ::iovec> buffers
    , std::uint64_t remote_addr
    , std::uint64_t key
    , void *context
  );

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_writev fail
   */
  void post_write(
    gsl::span<const ::iovec> buffers
    , void **desc
    , std::uint64_t remote_addr
    , std::uint64_t key
    , void *context
  );

  void post_write(
    gsl::span<const ::iovec> buffers
    , std::uint64_t remote_addr
    , std::uint64_t key
    , void *context
  );

  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_inject fail
   */
  void inject_send(const void *buf, std::size_t len);

public:
  /*
   * @throw fabric_bad_alloc : std::bad_alloc - out of memory
   * @throw fabric_runtime_error : std::runtime_error : ::fi_domain fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_ep_bind fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_enable fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_ep_bind fail (event registration)
   * @throw fabric_runtime_error : std::runtime_error : ::fi_endpoint fail (make_fid_aep)
   * @throw fabric_runtime_error : std::runtime_error : ::fi_wait_open fail
   * @throw fabric_runtime_error : std::runtime_error : ::fi_cq_open fail (make_fid_cq)
   * @throw bad_dest_addr_alloc
   * @throw std::system_error (receiving fabric server name)
   * @throw std::system_error - creating event pipe fd pair
   */
  explicit fabric_endpoint(
    Fabric &fabric
    , event_producer &ev
    , ::fi_info &info
    , common::string_view remote_addr_
    , std::uint16_t port
  );
  explicit fabric_endpoint(
    Fabric &fabric
    , event_producer &ev
    , ::fi_info &info
  );

  ~fabric_endpoint();

  fabric_types::addr_ep_t get_name() const;

  /*
   * @throw std::logic_error : unexpected event
   * @throw std::system_error : read error on event pipe
   */
  void expect_event(std::uint32_t) const;
  bool is_shut_down() const { return _shut_down; }
#if 0
  std::size_t max_message_size() const noexcept override;
  std::size_t max_inject_size() const noexcept override;
#endif

  Fabric &fabric() const { return _fabric; }
  ::fi_info &domain_info() const { return *_domain_info; }
  ::fid_domain &domain() const { return *_domain; }

  /* BEGIN component::IFabric_memory_control */
  /**
   * @contig_addr - the address of memory to be registered for RDMA. Restrictions
   * in "man fi_verbs" apply: the memory must be page aligned. The ibverbs layer
   * will execute an madvise(MADV_DONTFORK) syscall against the region. Any error
   * returned from that syscal will cause the register_memory function to fail.
   *
   * @throw std::range_error - address already registered
   * @throw std::logic_error - inconsistent memory address tables
   * @throw fabric_runtime_error : std::runtime_error : ::fi_mr_reg fail
   */
  memory_region_t register_memory(const_byte_span contig_addr, std::uint64_t key, std::uint64_t flags) override;
  /**
   * @throw std::range_error - address not registered
   * @throw std::logic_error - inconsistent memory address tables
   */
  void deregister_memory(const memory_region_t memory_region) override;
  std::uint64_t get_memory_remote_key(const memory_region_t memory_region) const noexcept override;
  void *get_memory_descriptor(const memory_region_t memory_region) const noexcept override;
  /* END component::IFabric_memory_control */

  std::vector<void *> populated_desc(gsl::span<const ::iovec> buffers);
};

#pragma GCC diagnostic pop

#endif
