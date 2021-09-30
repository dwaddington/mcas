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

#include "fabric_endpoint.h"

#include "bad_dest_addr_alloc.h"
#include "event_registration.h"
#include "fabric.h" /* trywait() */
#include "fabric_check.h" /* CHECK_FI_ERR */
#include "fabric_client.h"
#include "fabric_client_grouped.h"
#include "fabric_connection.h"
#include "fabric_ptr.h" /* fid_unique_ptr */
#include "fabric_runtime_error.h"
#include "fabric_str.h" /* tostr */
#include "fabric_util.h" /* make_fi_infodup, get_event_name */
#include "fabric_enter_exit_trace.h"
#include "fd_control.h"
#include "fd_pair.h"
#include "fd_unblock_set_monitor.h"
#include "system_fail.h"

#include <common/env.h> /* env_value */
#include <common/logging.h> /* PLOG */
#include <common/to_string.h>

#include <rdma/fi_errno.h> /* fi_strerror */
#include "rdma-fi_rma.h" /* fi_{read,recv,send,write}v, fj_inject, fi_sendmsg */

#include <sys/select.h> /* pselect */
#include <sys/mman.h> /* madvise */
#include <sys/resource.h> /* rusage */
#include <sys/time.h> /* rusage */
#include <sys/uio.h> /* iovec */

#include <boost/io/ios_state.hpp>
#include <gsl/pointers>

#include <algorithm> /* min */
#include <chrono> /* milliseconds */
#include <limits> /* <int>::max */
#include <sstream> /* ostringstream */
#include <typeinfo> /* typeinfo::name */

namespace component
{
	struct IFabric_memory_region
	{
		using byte_span = common::byte_span;
		using const_byte_span = common::const_byte_span;
	private:
		std::shared_ptr<::fid_mr> _mr;
		byte_span _v;
		unsigned _count;
	public:
		IFabric_memory_region(gsl::not_null<::fid_mr *> mr_, const_byte_span contig_)
			: _mr(fid_ptr(mr_))
			, _v(common::make_byte_span(const_cast<void *>(::base(contig_)), ::size(contig_)))
			, _count(1)
		{
		}
		~IFabric_memory_region()
		{
		}
		std::shared_ptr<::fid_mr> mr() { return _mr; }
		::fid_mr *mr_unshared() const { return _mr.get(); }
		byte_span v() const { return _v; }
		void count_incr() { ++_count; }
		unsigned count_decr() { return --_count; }
		unsigned count() const { return _count; }
	};
}

namespace
{
	std::tuple<
		std::shared_ptr<::fi_info>
		, fabric_types::addr_ep_t
	> set_peer(std::unique_ptr<Fd_control> control_, const ::fi_info &info_)
	{
		std::shared_ptr<::fi_info> domain_info(make_fi_infodup(info_, "domain"));
		fabric_types::addr_ep_t peer_addr;
		if ( domain_info->ep_attr->type == FI_EP_MSG )
		{
			peer_addr = control_->recv_name();
			/* fi_connect, at least for verbs, ignores addr and uses dest_addr from the hints. */
			domain_info->dest_addrlen = peer_addr.size();
			if ( 0 != domain_info->dest_addrlen )
			{
				domain_info->dest_addr = ::malloc(domain_info->dest_addrlen);
				if ( ! domain_info->dest_addr )
				{
					throw bad_dest_addr_alloc(domain_info->dest_addrlen);
				}
				std::copy(peer_addr.begin(), peer_addr.end(), static_cast<char *>(domain_info->dest_addr));
			}
		}
		/* Other providers will look in addr: provide the name there as well. */
		return { domain_info, peer_addr };
	}
	std::tuple<
		std::shared_ptr<::fi_info>
		, fabric_types::addr_ep_t
	> set_no_peer(const ::fi_info &info_)
	{
		std::shared_ptr<::fi_info> domain_info(make_fi_infodup(info_, "domain"));
		return { domain_info, fabric_types::addr_ep_t() };
	}
}

/**
 * Fabric/RDMA-based network component
 *
 */

#define CAN_USE_WAIT_SETS 0

/* Note: the info is owned by the caller, and must be copied if it is to be saved. */
fabric_endpoint::fabric_endpoint(
    Fabric &fabric_
    , event_producer &ev_
		, std::tuple<
			std::shared_ptr<::fi_info>
			, fabric_types::addr_ep_t
		> domain_info_and_peer_addr_
  )
  : _fabric(fabric_)
  , _domain_info(std::get<0>(domain_info_and_peer_addr_))
	, _peer_addr(std::get<1>(domain_info_and_peer_addr_))
  /* NOTE: "this" is returned for context when domain-level events appear in the event queue bound to the domain
   * and not bound to a more specific entity (an endpoint, mr, av, pr scalable_ep).
   */
  , _domain(_fabric.make_fid_domain(*_domain_info, this))
  , _m{}
  , _mr_addr_to_mra{}
  , _paging_test(common::env_value<bool>("FABRIC_PAGING_TEST", false))
  , _fault_counter(_paging_test || common::env_value<bool>("FABRIC_PAGING_REPORT", false))
#if CAN_USE_WAIT_SETS
  /* verbs provider does not support wait sets */
  , _wait_attr{
    FI_WAIT_FD /* wait_obj type. verbs supports ony FI_WAIT_FD */
    , 0U /* flags, "must be set to 0 by the caller" */
  }
  , _wait_set(make_fid_wait(fabric(), _wait_attr))
#endif
  , _m_fd_unblock_set{}
  , _fd_unblock_set{}
#if CAN_USE_WAIT_SETS
  , _cq_attr{4096, 0U, Fabric_cq::fi_cq_format, FI_WAIT_SET, 0U, FI_CQ_COND_NONE, &*_wait_set}
#else
  , _cq_attr{4096, 0U, Fabric_cq::fi_cq_format, FI_WAIT_NONE, 0U, FI_CQ_COND_NONE, nullptr}
#endif
  , _rxcq(make_fid_cq(_cq_attr, this), "rx")
  , _txcq(make_fid_cq(_cq_attr, this), "tx")
  , _ep_info(make_fi_infodup(domain_info(), "endpoint construction"))
  , _ep(make_fid_aep(*_ep_info, this))
  /* events */
  , _event_pipe{}
  /* NOTE: the various tests for type (FI_EP_MSG) should perhaps
   * move to derived classses.
   *                      connection  message boundaries  reliable
   * FI_EP_MSG:               Y               Y              Y
   * FI_EP_SOCK_STREAM:       Y               N              Y
   * FI_EP_RDM:               N               Y              Y
   * FI_EP_DGRAM:             N               Y              N
   * FI_EP_SOCK_DGRAM:        N               N              N
   */
  , _event_registration( ep_info().ep_attr->type == FI_EP_MSG ? new event_registration(ev_, *this, ep()) : nullptr )
  , _shut_down(false)
{
/* ERROR (probably in libfabric verbs): closing an active endpoint prior to fi_ep_bind causes a SEGV,
 * as fi_ibv_msg_ep_close will call fi_ibv_cleanup_cq whether there is CQ state to clean up.
 */
  CHECK_FI_ERR(::fi_ep_bind(&*_ep, _txcq.fid(), FI_TRANSMIT));
  CHECK_FI_ERR(::fi_ep_bind(&*_ep, _rxcq.fid(), FI_RECV));
  CHECK_FI_ERR(::fi_enable(&*_ep));
}

/* Note: info_ is owned by the caller, and must be copied if it is to be saved. */
fabric_endpoint::fabric_endpoint(
    Fabric &fabric_
    , event_producer &ev_
    , const ::fi_info &info_
    , common::string_view remote_address_
    , std::uint16_t port_
  )
  : fabric_endpoint(fabric_, ev_
	/* Ask the server (over TCP) what address should be used for the fabric. Remember that address */
	, set_peer(std::make_unique<Fd_control>(remote_address_, fabric_.choose_port(port_)), info_)
	)
{}

/* Note: the info is owned by the caller, and must be copied if it is to be saved. */
fabric_endpoint::fabric_endpoint(
    Fabric &fabric_
    , event_producer &ev_
    , const ::fi_info &info_
  )
  : fabric_endpoint(fabric_, ev_, set_no_peer(info_))
{}

fabric_endpoint::~fabric_endpoint()
{
}

/**
 * Asynchronously post a buffer to the connection
 *
 * @param connection Connection to send on
 * @param buffers Buffer vector (containing regions should be registered)
 *
 * @return Work (context) identifier
 */
void fabric_endpoint::post_send(
  gsl::span<const ::iovec> buffers_
  , void **desc_
  , context_t context_
)
{
	ENTER_EXIT_TRACE_N
  CHECK_FI_EQ(
    ::fi_sendv(
      &ep()
      , &*buffers_.begin()
      , desc_
      , buffers_.size()
      , ::fi_addr_t{}
      , context_
    )
    , 0
  );
  _txcq.incr_inflight(__func__);
}

void fabric_endpoint::post_send(
  gsl::span<const ::iovec> buffers_
  , context_t context_
)
{
	ENTER_EXIT_TRACE1
  auto desc = populated_desc(buffers_);
  post_send(buffers_, &*desc.begin(), context_);
}

/**
 * Asynchronously post a buffer to receive data
 *
 * @param connection Connection to post to
 * @param buffers Buffer vector (containing regions should be registered)
 *
 * @return Work (context) identifier
 */
void fabric_endpoint::post_recv(
  gsl::span<const ::iovec> buffers_
  , void **desc_
  , context_t context_
)
{
	ENTER_EXIT_TRACE_N
  CHECK_FI_EQ(
    ::fi_recvv(
      &ep()
      , &*buffers_.begin()
      , desc_
      , buffers_.size()
      , ::fi_addr_t{}
      , context_
    )
    , 0
  );
  _rxcq.incr_inflight(__func__);
}

void fabric_endpoint::post_recv(
  gsl::span<const ::iovec> buffers_
  , context_t context_
)
{
	ENTER_EXIT_TRACE1
  auto desc = populated_desc(buffers_);
  post_recv(buffers_, &*desc.begin(), context_);
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
void fabric_endpoint::post_read(
  gsl::span<const ::iovec> buffers_
  , void **desc_
  , uint64_t remote_addr_
  , uint64_t key_
  , context_t context_
)
{
	ENTER_EXIT_TRACE_N
  CHECK_FI_EQ(
    ::fi_readv(
      &ep()
      , &*buffers_.begin()
      , desc_
      , buffers_.size()
      , ::fi_addr_t{}
      , remote_addr_
      , key_
      , context_
    )
    , 0
  );
  _txcq.incr_inflight(__func__);
}

void fabric_endpoint::post_read(
  gsl::span<const ::iovec> buffers_
  , uint64_t remote_addr_
  , uint64_t key_
  , context_t context_
)
{
	ENTER_EXIT_TRACE1
  auto desc = populated_desc(buffers_);
  post_read(buffers_, &*desc.begin(), remote_addr_, key_, context_);
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
void fabric_endpoint::post_write(
  gsl::span<const ::iovec> buffers_
  , void **desc_
  , uint64_t remote_addr_
  , uint64_t key_
  , context_t context_
)
{
	ENTER_EXIT_TRACE_N
  CHECK_FI_EQ(
    ::fi_writev(
      &ep()
      , &*buffers_.begin()
      , desc_
      , buffers_.size()
      , ::fi_addr_t{}
      , remote_addr_
      , key_
      , context_
      )
    , 0
    );
  _txcq.incr_inflight(__func__);
}

void fabric_endpoint::post_write(
  gsl::span<const ::iovec> buffers_
  , uint64_t remote_addr_
  , uint64_t key_
  , context_t context_
)
{
	ENTER_EXIT_TRACE1
  auto desc = populated_desc(buffers_);
  post_write(buffers_, &*desc.begin(), remote_addr_, key_, context_);
}

  /**
   * Send message
   *
   * @param connection Connection to inject on
   * @param buf_ start of data to send
   * @param len_ length of data to send (must not exceed max_inject_size())
   */
void fabric_endpoint::sendmsg(
	gsl::span<const ::iovec> buffers_
	, void **desc_
	, ::fi_addr_t addr_
	, void *context_
	, std::uint64_t flags
)
{
	ENTER_EXIT_TRACE1
	const ::fi_msg m {
		&*buffers_.begin(), desc_, buffers_.size(), addr_, context_, flags
	};
	CHECK_FI_EQ(::fi_sendmsg(&ep(), &m, flags), 0);

	/* Note: keeping track of the number of completions is dfficult due to the
	 * possibility of send requests with "unsignalled completions."
	 * Such send requests *may or may not* produce a completion, depending on
	 * whether the operation fails (will generate a completion) or succeeds
	 * (will not generate a completion).
	 */
	if ( flags & FI_COMPLETION )
	{
		_txcq.incr_inflight(__func__);
	}
}

  /**
   * Send message without completion
   *
   * @param connection Connection to inject on
   * @param buf_ start of data to send
   * @param len_ length of data to send (must not exceed max_inject_size())
   */
void fabric_endpoint::inject_send(const void *buf_, std::size_t len_)
{
	ENTER_EXIT_TRACE_N
  CHECK_FI_EQ(::fi_inject(&ep(), buf_, len_, ::fi_addr_t{}), 0);
}

/**
 * Poll completions (e.g., completions)
 *
 * @param completion_callback (context_t, ::status_t status, void* error_data)
 *
 * @return Number of completions processed
 */

#pragma GCC diagnostic push
#if defined __GNUC__ && 6 < __GNUC__ && __cplusplus < 201703L
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif

std::size_t fabric_endpoint::poll_completions(const component::IFabric_op_completer::complete_old &cb_)
{
	ENTER_EXIT_TRACE1
  std::size_t ct_total = 0;

  ct_total += _rxcq.poll_completions(cb_);
  ct_total += _txcq.poll_completions(cb_);

  if ( _shut_down && ct_total == 0 )
  {
    throw std::logic_error(__func__ + std::string(": Connection closed"));
  }
  return ct_total;
}

std::size_t fabric_endpoint::poll_completions(const component::IFabric_op_completer::complete_definite &cb_)
{
	ENTER_EXIT_TRACE_N
  std::size_t ct_total = 0;

  ct_total += _rxcq.poll_completions(cb_);
  ct_total += _txcq.poll_completions(cb_);

  if ( _shut_down && ct_total == 0 )
  {
    throw std::logic_error(__func__ + std::string(": Connection closed"));
  }
  return ct_total;
}

std::size_t fabric_endpoint::poll_completions_tentative(const component::IFabric_op_completer::complete_tentative &cb_)
{
	ENTER_EXIT_TRACE1
  std::size_t ct_total = 0;

  ct_total += _rxcq.poll_completions_tentative(cb_);
  ct_total += _txcq.poll_completions_tentative(cb_);

  if ( _shut_down && ct_total == 0 )
  {
    throw std::logic_error(__func__ + std::string(": Connection closed"));
  }
  return ct_total;
}

std::size_t fabric_endpoint::poll_completions(const component::IFabric_op_completer::complete_param_definite &cb_, void *cb_param_)
{
	ENTER_EXIT_TRACE1
  std::size_t ct_total = 0;

  ct_total += _rxcq.poll_completions(cb_, cb_param_);
  ct_total += _txcq.poll_completions(cb_, cb_param_);

  if ( _shut_down && ct_total == 0 )
  {
    throw std::logic_error(__func__ + std::string(": Connection closed"));
  }
  return ct_total;
}

std::size_t fabric_endpoint::poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative &cb_, void *cb_param_)
{
	ENTER_EXIT_TRACE_N
  std::size_t ct_total = 0;

  ct_total += _rxcq.poll_completions_tentative(cb_, cb_param_);
  ct_total += _txcq.poll_completions_tentative(cb_, cb_param_);

  if ( _shut_down && ct_total == 0 )
  {
    throw std::logic_error(__func__ + std::string(": Connection closed"));
  }
  return ct_total;
}

std::size_t fabric_endpoint::poll_completions(const component::IFabric_op_completer::complete_param_definite_ptr_noexcept cb_, void *cb_param_)
{
	ENTER_EXIT_TRACE_N
  std::size_t ct_total = 0;

  ct_total += _rxcq.poll_completions(cb_, cb_param_);
  ct_total += _txcq.poll_completions(cb_, cb_param_);

  if ( _shut_down && ct_total == 0 )
  {
    throw std::logic_error(__func__ + std::string(": Connection closed"));
  }
  return ct_total;
}

std::size_t fabric_endpoint::poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative_ptr_noexcept cb_, void *cb_param_)
{
	ENTER_EXIT_TRACE1
  std::size_t ct_total = 0;

  ct_total += _rxcq.poll_completions_tentative(cb_, cb_param_);
  ct_total += _txcq.poll_completions_tentative(cb_, cb_param_);

  if ( _shut_down && ct_total == 0 )
  {
    throw std::logic_error(__func__ + std::string(": Connection closed"));
  }
  return ct_total;
}

#pragma GCC diagnostic pop

/**
 * Block and wait for next completion.
 *
 * @param polls_limit Maximum number of polls (throws exception on exceeding limit)
 *
 * @return Next completion context
 * @throw std::system_error - creating fd pair
 */
void fabric_endpoint::wait_for_next_completion(std::chrono::milliseconds timeout)
{
	ENTER_EXIT_TRACE1
  Fd_pair fd_unblock;
  fd_unblock_set_monitor(_m_fd_unblock_set, _fd_unblock_set, fd_unblock.fd_write());
  /* Only block if we have not seen FI_SHUTDOWN */
  if ( ! _shut_down )
  {
/* verbs provider does not support wait sets */
#if USE_WAIT_SETS
    ::fi_wait(&*_wait_set, std::min(std::numeric_limits<int>::max(), int(timeout.count())));
#else
    static constexpr unsigned cq_count = 2;
    ::fid_t f[cq_count] = { _rxcq.fid(), _txcq.fid() };
    /* if fabric is in a state in which it can wait on the cqs ... */
    if ( fabric().trywait(f, cq_count) == FI_SUCCESS )
    {
      /* Wait sets: libfabric may notify any of read, write, except */
      fd_set fds_read;
      fd_set fds_write;
      fd_set fds_except;
      FD_ZERO(&fds_read);
      FD_ZERO(&fds_write);
      FD_ZERO(&fds_except);

      /* the fd through which an unblock call can end the wait */
      FD_SET(fd_unblock.fd_read(), &fds_read);

      auto fd_max = fd_unblock.fd_read(); /* max fd value */
      /* add the cq file descriptors to the wait set */
      for ( unsigned i = 0; i != cq_count; ++i )
      {
        int fd;
        CHECK_FI_ERR(::fi_control(f[i], FI_GETWAIT, &fd));
        FD_SET(fd, &fds_read);
        FD_SET(fd, &fds_write);
        FD_SET(fd, &fds_except);
        fd_max = std::max(fd_max, fd);
      }
      struct timespec ts {
        timeout.count() / 1000 /* seconds */
        , (timeout.count() % 1000) * 1000000 /* nanoseconds */
      };

      /* Wait until libfabric indicates a completion */
      auto ready = ::pselect(fd_max+1, &fds_read, &fds_write, &fds_except, &ts, nullptr);
      if ( -1 == ready )
      {
        switch ( auto e = errno )
        {
        case EINTR:
          break;
        default:
          system_fail(e, "wait_for_next_completion");
        }
      }
      /* Note: there is no reason to act on the fd's because either
       *  - the eventual completion will take care of them, or
       *  - the fd_unblock_set_monitor will take care of them.
       */
#endif
    }
  }
}

void fabric_endpoint::wait_for_next_completion(unsigned polls_limit)
{
	ENTER_EXIT_TRACE1
  for ( ; polls_limit != 0; --polls_limit )
  {
    try
    {
      return wait_for_next_completion(std::chrono::milliseconds(0));
    }
    catch ( const fabric_runtime_error &e )
    {
      if ( e.id() != FI_ETIMEDOUT )
      {
        throw;
      }
    }
  }
}

/**
 * Unblock any threads waiting on completions
 *
 */
void fabric_endpoint::unblock_completions()
{
	ENTER_EXIT_TRACE1
  std::lock_guard<std::mutex> g{_m_fd_unblock_set};
  for ( auto fd : _fd_unblock_set )
  {
    char c{};
    auto sz = ::write(fd, &c, 1);
    (void) sz;
  }
}

/* EVENTS */
void fabric_endpoint::cb(std::uint32_t event, ::fi_eq_cm_entry &) noexcept
try
{
  if ( event == FI_SHUTDOWN )
  {
    _shut_down = true;
    unblock_completions();
  }
  _event_pipe.write(&event, sizeof event);
}
catch ( const std::exception &e )
{
  FLOGM("{}", e.what());
}

void fabric_endpoint::err(::fid_eq *eq_, ::fi_eq_err_entry &e_) noexcept
try
{
  char pe_buffer[512];
  auto pe = ::fi_eq_strerror(eq_, e_.prov_errno, e_.err_data, pe_buffer, sizeof pe_buffer);
  /* An error event; not what we were expecting */
  std::ostringstream os;
  os << "Fabric error event:"
    << " fid " << e_.fid
    << " context " << e_.context
    << " data " << e_.data
    << " err " << e_.err
    << " prov_errno " << e_.prov_errno
    << " \"" << pe << "/" << pe_buffer << "\""
    << " err_data ";

  for ( auto i = static_cast<uint8_t *>(e_.err_data)
    ; i != static_cast<uint8_t *>(e_.err_data) + e_.err_data_size
    ; ++i
  )
  {
    os << std::setfill('0') << std::setw(2) << std::hex << *i;
  }
  PLOG("%s", os.str().c_str());

  std::uint32_t event = FI_NOTIFY;
  _event_pipe.write(&event, sizeof event);
}
catch ( const std::exception &e )
{
  FLOGM("{}", e.what());
}

void fabric_endpoint::ensure_event(const fabric_connection *cnxn_) const
{
  /* First, ensure that expect_event will see an event */
  for ( bool have_event = false
    ; ! have_event
    ;
      /* Make some effort to wait until the event queue is readable.
       * NOTE: Seems to block for too long, as fi_trywait is not
       * behaving as expected. See Fabric::wait_eq.
       */
      cnxn_->wait_event()
  )
  {
    cnxn_->solicit_event(); /* _ev.read_eq() in client, no-op in server */
    auto fd = _event_pipe.fd_read();
    fd_set fds_read;
    FD_ZERO(&fds_read);
    FD_SET(fd, &fds_read);
    struct timespec ts {
      0 /* seconds */
      , 0 /* nanoseconds */
    };

    auto ready = ::pselect(fd+1, &fds_read, nullptr, nullptr, &ts, nullptr);

    if ( -1 == ready )
    {
      switch ( auto e = errno )
      {
      case EINTR:
        break;
      default:
        system_fail(e, "expect_event_sync");
      }
    }
    /* Note: there is no reason to act on the fd because
     *  - expect_event will read it
     */
    else
    {
      have_event = FD_ISSET(fd, &fds_read);
#if 0
      if ( have_event )
      {
        FLOGM("ready count {}", ready);
      }
      else
      {
        FLOGM("timeout, ready count {}", ready);
      }
#endif
    }
  }
}

void fabric_endpoint::expect_event(std::uint32_t event_exp) const
{
  std::uint32_t event = 0;
  _event_pipe.read(&event, sizeof event);
  if ( event != event_exp )
  {
    throw std::logic_error(std::string("expected ") + get_event_name(event_exp) + " got " + get_event_name(event) );
  }
}

fid_unique_ptr<::fid_cq> fabric_endpoint::make_fid_cq(::fi_cq_attr &attr, void *context) const
{
  ::fid_cq *f = nullptr;
  CHECK_FI_ERR(::fi_cq_open(&domain(), &attr, &f, context));
  FABRIC_TRACE_FID(f);
  return fid_unique_ptr<::fid_cq>(f);
}

std::shared_ptr<::fid_ep> fabric_endpoint::make_fid_aep(::fi_info &info, void *context) const
try
{
  ::fid_ep *f;
  Fabric::fi_lock_guard g{Fabric::fi_tostr_mutex};
  CHECK_FI_ERR(::fi_endpoint(&domain(), &info, &f, context));
  static_assert(0 == FI_SUCCESS, "FI_SUCCESS not 0, which means that we need to distinguish between these types of \"successful\" returns");
  FABRIC_TRACE_FID(f);
  return fid_ptr(f);
}
catch ( const fabric_runtime_error &e )
{
  throw e.add(tostr(info));
}

namespace
{
  std::string while_in(const common::string_view where)
  {
    return " (while in " + std::string(where) + ")";
  }
}

component::IFabric_client * fabric_endpoint::make_open_client()
try
{
  return new Fabric_client(this, fabric(), _peer_addr);
}
catch ( const fabric_runtime_error &e )
{
  throw e.add(while_in(__func__));
}
catch ( const std::system_error &e )
{
  throw std::system_error(e.code(), e.what() + while_in(__func__));
}

component::IFabric_client_grouped * fabric_endpoint::make_open_client_grouped()
try
{
  return static_cast<component::IFabric_client_grouped *>(new Fabric_client_grouped(this, fabric(), _peer_addr));
}
catch ( const fabric_runtime_error &e )
{
  throw e.add(while_in(__func__));
}
catch ( const std::system_error &e )
{
  throw std::system_error(e.code(), e.what() + while_in(__func__));
}

/* memory control section */

using guard = std::unique_lock<std::mutex>;

namespace
{
  /* True if range of a is a superset of range of b */
  using common::byte_span;
  using common::const_byte_span;
  bool covers(const byte_span a, const const_byte_span b)
  {
    return ::base(a) <= ::base(b) && ::end(b) <= ::end(a);
  }
  std::ostream &operator<<(std::ostream &o, const byte_span v)
  {
    return o << "[" << ::base(v) << ".." << ::end(v) << ")";
  }
  std::ostream &operator<<(std::ostream &o, const const_byte_span v)
  {
    return o << "[" << ::base(v) << ".." << ::end(v) << ")";
  }

  long ru_flt()
  {
    rusage usage;
    auto rc = ::getrusage(RUSAGE_SELF, &usage);
    return rc == 0 ? usage.ru_minflt + usage.ru_majflt : 0;
  }
  bool mr_trace = common::env_value<bool>("FABRIC_MR_TRACE", false);
}

ru_flt_counter::ru_flt_counter(bool report_)
  : _report(report_)
  , _ru_flt_start(ru_flt())
{}

ru_flt_counter::~ru_flt_counter()
{
  if ( _report )
  {
    PLOG("fault count %li", ru_flt() - _ru_flt_start);
  }
}

void fabric_endpoint::print_registry() const
{
	unsigned i = 0;
	const unsigned limit = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
	for ( const auto &j : _mr_addr_to_mra )
	{
		if ( i < limit )
		{
			FLOG("Token {} mr {} refcount {} {}", &*j.second, j.second->mr_unshared(), j.second->count(), j.second->v());
		}
		else
		{
			FLOG("Token (suppressed {} more", _mr_addr_to_mra.size() - limit);
			break;
		}
		++i;
	}
#pragma GCC diagnostic pop
}

auto fabric_endpoint::register_memory(const_byte_span contig_, std::uint64_t key_, std::uint64_t flags_) -> memory_region_t
{
	ENTER_EXIT_TRACE_N
	guard g{_m};

	auto it = mr_covering_it(contig_);

	if ( it == _mr_addr_to_mra.rend() )
	{
		auto mra =
			std::make_unique<mr_and_address>(
				make_fid_mr_reg_ptr(
					contig_
					, std::uint64_t(FI_SEND|FI_RECV|FI_READ|FI_WRITE|FI_REMOTE_READ|FI_REMOTE_WRITE)
					, key_
					, flags_
				)
				, contig_
		);

		/* operations which access local memory will need the "mr." Record it here. */

		auto fit = _mr_addr_to_mra.emplace(::base(contig_), std::move(mra));
		++fit;
		it = map_addr_to_mra::reverse_iterator(fit);
	}
	else
	{
		it->second->count_incr();
	}

	if ( mr_trace )
	{
		FLOG("Registered token ref({}} mr {} {} desc {} key {}", it->second->count(), &*it->second, it->second->mr_unshared(), it->second->v(), get_memory_descriptor(&*it->second), get_memory_remote_key(&*it->second));
		print_registry();
	}

	return &*it->second;

	/*
	 * Operations which access remote memory will need the memory key.
	 * If the domain has FI_MR_PROV_KEY set, we need to return the actual key.
	 */
}

void fabric_endpoint::deregister_memory(const memory_region_t mr_)
{
	ENTER_EXIT_TRACE_N
	guard g{_m};

	auto lb = _mr_addr_to_mra.lower_bound(::data(mr_->v()));
	auto ub = _mr_addr_to_mra.upper_bound(::data(mr_->v()));

	map_addr_to_mra::size_type scan_count = 0;
	auto it =
		std::find_if(
			lb
			, ub
			, [&mr_, &scan_count] ( const map_addr_to_mra::value_type &m ) { ++scan_count; return &*m.second == mr_; }
	);

	if ( it == ub )
	{
		FLOG("Deregistered token {} mr {} failed, not in registry", mr_, mr_->mr_unshared());
		print_registry();
		throw std::logic_error(
			common::to_string(
				__func__, " token ", mr_, " mr ", mr_->mr_unshared(), " (with range ", mr_->v(), ")"
				, " not found in ", scan_count, " of ", _mr_addr_to_mra.size(), " registry entries"
			)
		);
	}
	if ( mr_trace )
	{
		FLOG("Deregistered token (before) ref({}) mr/sought {} mr/found {} {}", mr_->count(), mr_, mr_->mr_unshared(), it->second->mr_unshared(), it->second->v());
		print_registry();
	}

	if ( it->second->count_decr() == 0 )
	{
		_mr_addr_to_mra.erase(it);
	}
}

std::uint64_t fabric_endpoint::get_memory_remote_key(const memory_region_t mr_) const noexcept
{
	ENTER_EXIT_TRACE_N
  /* recover the memory region */
  auto mr = &*mr_->mr();
  /* ask fabric for the key */
  return ::fi_mr_key(mr);
}

void *fabric_endpoint::get_memory_descriptor(const memory_region_t mr_) const noexcept
{
	ENTER_EXIT_TRACE_N
  /* recover the memory region */
  auto mr = &*mr_->mr();
  /* ask fabric for the descriptor */
  return ::fi_mr_desc(mr);
}

/* find iiterator for registered memory region which covers the iovec range. Must hold lock for _mr_addr_to_mra */
auto fabric_endpoint::mr_covering_it(const const_byte_span v) -> map_addr_to_mra::reverse_iterator
{
	ENTER_EXIT_TRACE_N
	/* _mr_addr_to_mr is sorted by starting address.
	 * Find the last acceptable starting address, and iterate
	 * backwards through the map until we find a covering range
	 * or we reach the start of the table.
	 */
	auto ub = _mr_addr_to_mra.upper_bound(::data(v));

	return
		std::find_if(
			map_addr_to_mra::reverse_iterator(ub)
			, _mr_addr_to_mra.rend()
			, [&v] ( const map_addr_to_mra::value_type &m ) { return covers(m.second->v(), v); }
		);
}

/* find a registered memory region which covers the iovec range */
auto fabric_endpoint::mr_covering_throws(const const_byte_span v) -> memory_region_t
{

  guard g{_m};

  auto ub = _mr_addr_to_mra.upper_bound(::data(v));

  auto it =
    std::find_if(
      map_addr_to_mra::reverse_iterator(ub)
      , _mr_addr_to_mra.rend()
      , [&v] ( const map_addr_to_mra::value_type &m ) { return covers(m.second->v(), v); }
    );

  if ( it == _mr_addr_to_mra.rend() )
  {
    throw std::range_error(common::to_string("No mapped region covers ", v));
  }

#if 0
  FLOG("mr_covering( {}) found mr {} with range {}", v, it->second->mr_unshared(), it->second->v());
#endif
  return &*it->second;
}

/* find a registered memory region which covers the iovec range */
auto fabric_endpoint::mr_covering(const const_byte_span v) noexcept -> memory_region_t
try
{
	ENTER_EXIT_TRACE1
	return mr_covering_throws(v);
}
catch ( const std::exception &e )
{
	return nullptr;
}

/* If local keys are needed, one local key per buffer. */
std::vector<void *> fabric_endpoint::populated_desc(gsl::span<const ::iovec> buffers)
{
	std::vector<void *> desc;

	std::transform(
		buffers.begin()
		, buffers.end()
		, std::back_inserter(desc)
		, [this] (const ::iovec &v)
			{
			        auto mr = &*mr_covering_throws(common::make_const_byte_span(v.iov_base, v.iov_len))->mr();
				return ::fi_mr_desc(mr);
			}
	);

	return desc;
}

/* (no context, synchronous only) */
/*
 * ERROR: the sixth parameter is named "requested key" in fi_mr_reg doc, but
 * if the user asks for a key which is unavailable the error returned is
 * "Required key not available." The parameter name and the error disagree:
 * "requested" is not the same as "required."
 */
gsl::not_null<fid_mr *> fabric_endpoint::make_fid_mr_reg_ptr(
  const_byte_span buf
  , uint64_t access
  , uint64_t key
  , uint64_t flags
) const
{
  ::fid_mr *f;
  auto constexpr offset = 0U; /* "reserved and must be zero" */
  /* used iff the registration completes asynchronously
   * (in which case the domain has been bound to an event queue with FI_REG_MR)
   */
  auto constexpr context = nullptr;
  try
  {
    /* Note: this was once observed to return "Cannot allocate memory" when called from JNI code. */
    /* Note: this was once observed to return an error when the DAX persistent Apache Pass memory
     * seemed properly aligned. The work-around was to issue a pre-emptive madvise(MADV_DONTFORK)
     * against the entire memory space of the DAX device.
     */
    /* Note: this was once observed to return "Bad address" when the (GPU) memory seemed properly aligned. */
    CHECK_FI_ERR(::fi_mr_reg(&*_domain, ::data(buf), ::size(buf), access, offset, key, flags, &f, context));
    if ( _paging_test )
    {
      auto rc = ::madvise(const_cast<common::byte *>(::data(buf)), ::size(buf), MADV_DONTNEED);
      PLOG("Paging test madvisee(%p, 0x%zx, MADV_DONTNEED) %s", ::base(buf), ::size(buf), rc ? " refused" : " accepted");
    }
  }
  catch ( const fabric_runtime_error &e )
  {
		throw e.add(
			common::to_string(
				std::showbase, std::hex, " in ", __func__ ," calling ::fi_mr_reg(domain "
				, &*_domain, " buf ", ::base(buf), ", len ", ::size(buf), ", access ", access
				, ", offset ", offset, ", key ", key, ", flags ", flags, ", fid_mr ", &f
				, ", context ", common::p_fmt(context), ")"
			)
		);
  }
  FABRIC_TRACE_FID(f);
  return f;
}
