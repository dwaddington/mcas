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
#include "connection_handler.h"
#include "connection_state.h"
#include "security.h"
#include "protocol_ostream.h"
#include "mcas_config.h"

#include <common/to_string.h>
#include <sstream>
#if 0
  /* Convert stream arguments to a string */
  template <typename Args>
    // std::string to_string(Args&&... args)
    std::string to_string2(const Args& args)
    {
      std::ostringstream s;
      (s << args);
      return s.str();
    }
#endif
/*
 * Allocating a few extra receive buffers seems to improve performance.
 */
static constexpr unsigned extra_buffers = 3;

namespace mcas
{
Connection_handler::Connection_handler(unsigned debug_level_,
                                       gsl::not_null<Factory *> factory,
                                       std::unique_ptr<Preconnection, uc_destructor> && preconnection
                                       , unsigned buffer_count_
#if CW_TEST && 0
	, byte_span scratchpad_
#endif
)
  : Connection_base(debug_level_, factory, std::move(preconnection), buffer_count_),
    Region_manager(debug_level_, transport()),
    Connection_TLS_session(debug_level_, this),
    _state(Connection_state::INITIAL),
    _handshakes(0),
    _resp_handshakes(0),
    _mr_vector{},
    _tick_count(0),
    _auth_id(0),
    _pending_msgs{},
    _pending_actions(),
    _pool_manager(),
	_locked_values{}
	, _spaces_shared{}
#if CW_TEST
	, _rm()
#endif
	,
    _stats{},
    _tls_buffer()
#if CW_TEST
	, _test_data(__LINE__)
#endif
{
}

Connection_handler::~Connection_handler()
{
  dump_stats();
#if 0
  exit(0); /* for profiler, if not using --forced-exit */
#endif
}

auto Connection_handler::tick() -> tick_type
{
  using namespace protocol;

  auto response = tick_type::TICK_RESPONSE_CONTINUE;
  _tick_count++;

#if 0
  auto now = rdtsc();

  /* output IOPS */
  if (_stats.next_stamp == 0) {
    _stats.next_stamp = now + (_freq_mhz * 1000000.0);
    _stats.last_count = _stats.response_count;
  }
  if (now >= (_stats.next_stamp)) {
    PMAJOR("(%p) IOPS: %lu /s", this, _stats.response_count - _stats.last_count);
    _stats.next_stamp = now + (_freq_mhz * 1000000.0);
    _stats.last_count = _stats.response_count;
  }
#endif

  /* */
  if (check_network_completions() == Fabric_connection_base::Completion_state::CLIENT_DISCONNECT) {
    PMAJOR("Client disconnected.");
    _state = Connection_state::CLIENT_DISCONNECTED;
    return tick_type::TICK_RESPONSE_CLOSE;
  }

  if (_send_value_posted_count != 0) {
    ++_stats.wait_send_value_misses;
  }

  switch (_state) {
  case Connection_state::WAIT_NEW_MSG_RECV:
    if (check_for_posted_recv_complete()) /*< check for recv completion */
      {
        auto iob = posted_recv();
        assert(iob);

        const auto msg = protocol::message_cast(iob->base());
        msg_recv_log(msg, __func__);

        //        PNOTICE("Recv message: crc32 (id=%lx, len=%u)", msg->auth_id(), msg->msg_len());

        switch (msg->type_id()) {
        case MSG_TYPE::IO_REQUEST:
        case MSG_TYPE::PUT_ADO_REQUEST:
        case MSG_TYPE::ADO_REQUEST:
        case MSG_TYPE::POOL_REQUEST:
        case MSG_TYPE::INFO_REQUEST:
        case MSG_TYPE::NO_MSG:
        case MSG_TYPE::PING:
          post_recv_buffer(allocate_recv());
          CFLOGM(2, "{}", common_to_string(*msg));
          _pending_msgs.push(iob);
          break;

        case MSG_TYPE::CLOSE_SESSION:
          post_recv_buffer(allocate_recv()); /* Is this (and the subsequent free_recv_buffer) necessary? */
          CFLOGM(2, "{}", common_to_string(*msg));
          free_recv_buffer();
	  FLOGM("{}", "CLOSE_SESSION message");
          response = tick_type::TICK_RESPONSE_CLOSE;
          break;

        default:
          throw Logic_exception("unhandled message (type:%x)", int(msg->type_id()));
        }

        ++_stats.recv_msg_count;

        CFLOGM(2, "Shard {} State: WAIT_MSG_RECV complete ({} ticks)", this, _tick_count);
      }
    else {
      ++_stats.wait_msg_recv_misses;
    }

    break;

  case Connection_state::INITIAL: {
    ++_handshakes;
    CFLOGM(2, "Shard {} State: POST_HANDSHAKE ({} ticks, {} handshakes)", this, _tick_count, _handshakes);

    /* fabric connection has allocated the first receive buffer */

    /* allocating an extra buffer or three makes the hstore benchmark run about 10% faster */
    for (auto i = extra_buffers; i != 0; --i) {
      post_recv_buffer(allocate_recv());
    }

    set_state(Connection_state::WAIT_HANDSHAKE);
    return tick_type::TICK_RESPONSE_CONTINUE;
  }

  case Connection_state::CLIENT_DISCONNECTED: {
    break;
  }

  case Connection_state::CLOSE_CONNECTION: {
    PNOTICE("### closing connection");
    return tick_type::TICK_RESPONSE_CLOSE;
  }

  case Connection_state::WAIT_TLS_HANDSHAKE: {
    auto next = process_tls_session();

    if(next == Connection_state::WAIT_NEW_MSG_RECV) {
      /* authentication ID becomes that from TLS session */
      set_auth_id(tls_auth_id());
    }

    set_state(next);
    break;
  }

  case Connection_state::WAIT_HANDSHAKE:
    if (check_for_posted_recv_complete()) {
      ++_resp_handshakes;

      CFLOGM(2, "Shard {} State: WAIT_HANDSHAKE complete (tick count {}, resp_handshakes {})",
             this, _tick_count, _resp_handshakes);

      const auto iob = posted_recv();
      assert(iob);

      const auto msg = protocol::message_cast(iob->base())->ptr_cast<Message_handshake>();

      if (msg->get_status() != S_OK)
        throw General_exception("handshake status != S_OK (%d)", msg->get_status());

      set_auth_id(msg->auth_id());

#if CW_TEST
      _rm =
			cw::registered_memory(
				Connection_base::transport()
				, std::make_unique<cw::dram_memory>(std::max(std::uint64_t(100), msg->test_data_size))
				, 0
			);
#endif
      /* if security_tls_auth bit is set, then we need to establish a
         GNU TLS side-channel as part of this session. this is
         triggered by sending the TLS port for the client to accept
         on.
      */
      if(msg->is_tls_auth()) {
        PNOTICE("TLS is ON (auth)");
        set_security_options(true, false /* hmac */);
        respond_to_handshake(true);
        set_state(Connection_state::WAIT_TLS_HANDSHAKE);
        return tick_type::TICK_RESPONSE_WAIT_SECURITY;
      }
      else {
        respond_to_handshake(false);
        PNOTICE("TLS is OFF");
      }

      free_buffer(iob);

    }
    break;
  }  // end switch
  return response;
}


void Connection_handler::configure_security(const std::string& bind_ipaddr,
                                            const unsigned bind_port,
                                            const std::string& cert_file,
                                            const std::string& key_file)
{
  if(Connection_base::debug_level() > 1)
    PLOG("config security (addr:%s, port:%u)", bind_ipaddr.c_str(), bind_port);

  if(bind_ipaddr.empty() == false) {
    set_security_binding(bind_ipaddr, bind_port);
    set_security_params(cert_file, key_file);
  }
}

void Connection_handler::respond_to_handshake(
	bool start_tls
#if CW_TEST && 0
	, uint64_t scratchpad_base_
	, uint64_t scratchpad_size_
#endif
)
{
  auto reply_iob = allocate_send();
  assert(reply_iob);

  auto reply_msg = new (reply_iob->base()) protocol::Message_handshake_reply(reply_iob->length(),
                                                                             auth_id(),
                                                                             1 /* seq */,
                                                                             max_message_size(),
                                                                             reinterpret_cast<uint64_t>(this),
                                                                             start_tls
#if CW_TEST
		, reinterpret_cast<std::uint64_t>(&_rm[0]) /* vaddr */
		, _rm.key() /* key */
#if 0
		, reinterpret_cast<std::uint64_t>(::base(_scratchpad))
		, ::size(_scratchpad)
#endif
#endif
	);

  /* post response */
  reply_iob->set_length(reply_msg->msg_len());
  post_recv_buffer(allocate_recv());
  post_send_buffer(reply_iob, reply_msg, __func__);
#if CW_TEST
	{
		FLOG("TEST SERVER vaddr {} key {}", static_cast<void *>(&_rm[0]), _rm.key());

	}
#endif
  set_state(Connection_state::WAIT_NEW_MSG_RECV);
}

void Connection_handler::add_locked_value(
	const void *target
	, memory_registered<Connection_base> &&mr
)
{
	_locked_values.emplace(std::pair{target, std::move(mr)});
}


void Connection_handler::release_locked_value(const void *target)
{
	auto i = _locked_values.find(target); /* search by target address */
	if ( i == _locked_values.end() )
	{
		throw Logic_exception("%s: bad target; value never locked? (%p)", __func__, target);
	}

	_locked_values.erase(i);
}

void Connection_handler::add_space_shared(const range<std::uint64_t> &range_, memory_registered<Connection_base> &&mr_)
{
	auto i = _spaces_shared.emplace(std::pair{range_, std::move(mr_)}).first;
	++i->second.count;

	CPLOG(2, "%s: [0x%" PRIx64 "..0x%" PRIx64 ") count %u", __func__, range_.first, range_.second, i->second.count);
}

void Connection_handler::release_space_shared(const range<std::uint64_t> &range_)
{
  auto i = _spaces_shared.find(range_); /* search by max offset */
  if (i == _spaces_shared.end()) {
    throw Logic_exception("%s: bad target; space never located? (%" PRIx64 ":%" PRIx64 ")", __func__, range_.first,
                          range_.second);
  }

  CPLOG(2, "%s: [0x%" PRIx64 "..0x%" PRIx64 ") count %u", __func__, range_.first, range_.second, i->second.count);

  --i->second.count;

  if (i->second.count == 0) {
    _spaces_shared.erase(i);
  }
}



}  // namespace mcas
