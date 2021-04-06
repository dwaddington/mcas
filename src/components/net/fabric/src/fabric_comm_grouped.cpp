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


/*
 * Authors:
 *
 */

#include "fabric_comm_grouped.h"

#include "async_req_record.h"
#include "fabric_generic_grouped.h"
#include "fabric_endpoint.h" /* fi_cq_entry_t */
#include "fabric_runtime_error.h"

#include <sys/uio.h> /* struct iovec */

/**
 * Fabric/RDMA-based network component
 *
 */

/* Note: the info is owned by the caller, and must be copied if it is to be saved. */
Fabric_comm_grouped::Fabric_comm_grouped(Fabric_generic_grouped &conn_, Fabric_cq_generic_grouped &rx_, Fabric_cq_generic_grouped &tx_)
  : _conn( conn_ )
  , _rx(rx_)
  , _tx(tx_)
{
}

Fabric_comm_grouped::~Fabric_comm_grouped()
{
/* wait until all completions are reaped */
  _conn.forget_group(this);
}

/**
 * Asynchronously post a buffer to the connection
 *
 * @param connection Connection to send on
 * @param buffers Buffer vector (containing regions should be registered)
 *
 * @return Work (context) identifier
 */
void Fabric_comm_grouped::post_send(
  gsl::span<const ::iovec> buffers_
  , void **desc_
  , void *context_
)
{
  std::unique_ptr<async_req_record> gc{new async_req_record(&_tx, context_)};
  _conn.post_send(buffers_, desc_, &*gc);
  gc.release();
}

void Fabric_comm_grouped::post_send(
  gsl::span<const ::iovec> buffers_
  , void *context_
)
{
  std::unique_ptr<async_req_record> gc{new async_req_record(&_tx, context_)};
  _conn.post_send(buffers_, &*gc);
  gc.release();
}

/**
 * Asynchronously post a buffer to receive data
 *
 * @param connection Connection to post to
 * @param buffers Buffer vector (containing regions should be registered)
 *
 * @return Work (context) identifier
 */
void Fabric_comm_grouped::post_recv(
  gsl::span<const ::iovec> buffers_
  , void **desc_
  , void *context_
)
{
  std::unique_ptr<async_req_record> gc{new async_req_record(&_rx, context_)};
  _conn.post_send(buffers_, desc_, &*gc);
  gc.release();
}

void Fabric_comm_grouped::post_recv(
  gsl::span<const ::iovec> buffers_
  , void *context_
)
{
  std::unique_ptr<async_req_record> gc{new async_req_record(&_rx, context_)};
  _conn.post_recv(buffers_, &*gc);
  gc.release();
}

  /**
   * Post RDMA read operation
   *
   * @param connection Connection to read on
   * @param buffers Destination buffer vector
   * @param remote_addr Remote address
   * @param key Key for remote address
   * @param out_context
   *
   */
void Fabric_comm_grouped::post_read(
  gsl::span<const ::iovec> buffers_
  , void **desc_
  , uint64_t remote_addr_
  , uint64_t key_
  , void *context_
)
{
  /* ask for a read to buffer */
  std::unique_ptr<async_req_record> gc{new async_req_record(&_tx, context_)};
  _conn.post_read(buffers_, desc_, remote_addr_, key_, &*gc);
  gc.release();
}

void Fabric_comm_grouped::post_read(
  gsl::span<const ::iovec> buffers_,
  uint64_t remote_addr_,
  uint64_t key_,
  void *context_
)
{
  /* ask for a read to buffer */
  std::unique_ptr<async_req_record> gc{new async_req_record(&_tx, context_)};
  _conn.post_read(buffers_, remote_addr_, key_, &*gc);
  gc.release();
}

  /**
   * Post RDMA write operation
   *
   * @param connection Connection to write to
   * @param buffers Source buffer vector
   * @param remote_addr Remote address
   * @param key Key for remote address
   * @param out_context
   *
   */
void Fabric_comm_grouped::post_write(
  gsl::span<const ::iovec> buffers_
  , void **desc_
  , uint64_t remote_addr_
  , uint64_t key_
  , void *context_
)
{
  std::unique_ptr<async_req_record> gc{new async_req_record(&_tx, context_)};
  _conn.post_write(buffers_, desc_, remote_addr_, key_, &*gc);
  gc.release();
}

void Fabric_comm_grouped::post_write(
  gsl::span<const ::iovec> buffers_,
  uint64_t remote_addr_,
  uint64_t key_,
  void *context_
)
{
  std::unique_ptr<async_req_record> gc{new async_req_record(&_tx, context_)};
  _conn.post_write(buffers_, remote_addr_, key_, &*gc);
  gc.release();
}

  /**
   * Send message without completion
   *
   * @param connection Connection to inject on
   * @param buffers Buffer vector (containing regions should be registered)
   */
void Fabric_comm_grouped::inject_send(const void *buf_, const std::size_t len_)
{
  _conn.inject_send(buf_, len_);
}

#pragma GCC diagnostic push
#if defined __GNUC__ && 6 < __GNUC__ && __cplusplus < 201703L
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

std::size_t Fabric_comm_grouped::poll_completions(const component::IFabric_op_completer::complete_old &cb_)
{
  return _rx.poll_completions(cb_) + _tx.poll_completions(cb_);
}

std::size_t Fabric_comm_grouped::poll_completions(const component::IFabric_op_completer::complete_definite &cb_)
{
  return _rx.poll_completions(cb_) + _tx.poll_completions(cb_);
}

std::size_t Fabric_comm_grouped::poll_completions_tentative(const component::IFabric_op_completer::complete_tentative &cb_)
{
  return _rx.poll_completions_tentative(cb_) + _tx.poll_completions_tentative(cb_);
}

std::size_t Fabric_comm_grouped::poll_completions(const component::IFabric_op_completer::complete_param_definite &cb_, void *cb_param_)
{
  return _rx.poll_completions(cb_, cb_param_) + _tx.poll_completions(cb_, cb_param_);
}

std::size_t Fabric_comm_grouped::poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative &cb_, void *cb_param_)
{
  return _rx.poll_completions_tentative(cb_, cb_param_) + _tx.poll_completions_tentative(cb_, cb_param_);
}

std::size_t Fabric_comm_grouped::poll_completions(const component::IFabric_op_completer::complete_param_definite_ptr_noexcept cb_, void *cb_param_)
{
  return _rx.poll_completions(cb_, cb_param_) + _tx.poll_completions(cb_, cb_param_);
}

std::size_t Fabric_comm_grouped::poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative_ptr_noexcept cb_, void *cb_param_)
{
  return _rx.poll_completions_tentative(cb_, cb_param_) + _tx.poll_completions_tentative(cb_, cb_param_);
}

#pragma GCC diagnostic pop

std::size_t Fabric_comm_grouped::stalled_completion_count()
{
  return _rx.stalled_completion_count() + _tx.stalled_completion_count();
}

/**
 * Block and wait for next completion.
 *
 * @param polls_limit Maximum number of polls (throws exception on exceeding limit)
 *
 * @return Next completion context
 */
void Fabric_comm_grouped::wait_for_next_completion(std::chrono::milliseconds timeout)
{
  return _conn.wait_for_next_completion(timeout);
}

void Fabric_comm_grouped::wait_for_next_completion(unsigned polls_limit)
{
  return _conn.wait_for_next_completion(polls_limit);
}

/**
 * Unblock any threads waiting on completions
 *
 */
void Fabric_comm_grouped::unblock_completions()
{
  return _conn.unblock_completions();
}

auto Fabric_comm_grouped::register_memory(const_byte_span contig_,
                                        std::uint64_t key,
                                        std::uint64_t flags) -> memory_region_t
{
  return _conn.register_memory(contig_, key, flags);
}
void Fabric_comm_grouped::deregister_memory(memory_region_t memory_region)
{
  return _conn.deregister_memory(memory_region);
}
std::uint64_t Fabric_comm_grouped::get_memory_remote_key(memory_region_t m) const noexcept
{
  return _conn.get_memory_remote_key(m);
}
void * Fabric_comm_grouped::get_memory_descriptor(memory_region_t m) const noexcept
{
  return _conn.get_memory_descriptor(m);
}
