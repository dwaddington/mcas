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
#ifndef __SHARD_CONNECTION_H__
#define __SHARD_CONNECTION_H__

#ifdef __cplusplus

#include "tls_session.h"
#include "buffer_manager.h"
#include "connection_state.h"
#include "fabric_connection_base.h"  // default to fabric transport
#include "mcas_config.h"
#include "pool_manager.h"
#include "protocol.h"
#include "protocol_ostream.h"
#include "range.h"
#include "region_manager.h"
#if CW_TEST
#include <cw/cw_common.h>
#include <cw/test_data.h>
#include <cw/cw_fabric_test.h>
#endif

#include <api/components.h>
#include <api/fabric_itf.h>
#include <api/kvstore_itf.h>
#include <common/byte_span.h>
#include <common/exceptions.h>
#include <common/logging.h>
#include <gsl/pointers>

#include <cassert>
#include <map>
#include <queue>
#include <utility> /* swap */
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"

namespace mcas
{
using Connection_base = Fabric_connection_base;

	struct space_lock_info_t
	{
		using transport_t = mcas::Connection_base;
		memory_registered<transport_t> mr;
		unsigned count;

		explicit space_lock_info_t(memory_registered<transport_t> &&mr_) noexcept
			: mr(std::move(mr_))
			, count(0)
		{}
  };

/**
 * Connection handler is instantiated for each "connected" client
 */
class Connection_handler
  : public Connection_base,
    public Region_manager,
    private Connection_TLS_session
{
  friend class Connection_TLS_session;
  friend struct TLS_transport;
	using byte_span = common::byte_span;

  struct stats {
    uint64_t response_count;
    uint64_t recv_msg_count;
    uint64_t send_msg_count;
    uint64_t wait_send_value_misses;
    uint64_t wait_msg_recv_misses;
    uint64_t wait_respond_complete_misses;
    uint64_t last_count;
    uint64_t next_stamp;
    stats()
      : response_count(0),
        recv_msg_count(0),
        send_msg_count(0),
        wait_send_value_misses(0),
        wait_msg_recv_misses(0),
        wait_respond_complete_misses(0),
        last_count(0),
        next_stamp(0)
    {
    }
  };

public:
  enum class tick_type {
        TICK_RESPONSE_CONTINUE        = 0,
        TICK_RESPONSE_BOOTSTRAP_SPAWN = 1,
        TICK_RESPONSE_WAIT_SECURITY   = 2,
        TICK_RESPONSE_CLOSE           = 0xFF,
  };

private:
  /* adaptor point for different transports */
  using Preconnection = component::IFabric_endpoint_unconnected_server;
  using Connection = component::IFabric_server;
  using Factory    = component::IFabric_server_factory;

  Connection_state _state;
  unsigned         _handshakes;
  unsigned         _resp_handshakes;

  /* list of pre-registered memory regions; normally one region */
  std::vector<component::IKVStore::memory_handle_t> _mr_vector;

  uint64_t                            _tick_count alignas(8);
  uint64_t                            _auth_id;
  std::queue<buffer_t *>              _pending_msgs;
  std::queue<action_t>                _pending_actions;
  Pool_manager                        _pool_manager; /* per-connection */
  using locked_value_map_t = std::map<const void *, memory_registered<Connection_base>>;
  locked_value_map_t                                _locked_values;
  using spaces_shared_map_t = std::map<range<std::uint64_t>, space_lock_info_t>;
  spaces_shared_map_t _spaces_shared;
#if CW_TEST
	cw::registered_memory _rm;
#endif

  stats _stats alignas(8);

  common::Byte_buffer _tls_buffer;
#if CW_TEST
	cw::test_data _test_data;
#if 0
	byte_span _scratchpad;
#endif
#endif

  void dump_stats()
  {
    PINF("-----------------------------------------");
    PINF("| Connection Handler Statistics         |");
    PINF("-----------------------------------------");
    PINF("Ticks                       : %lu", _tick_count);
    PINF("Open pools                  : %lu", _pool_manager.open_pool_count());
    PINF("NEW_MSG_RECV misses         : %lu", _stats.wait_msg_recv_misses);
    PINF("Recv message count          : %lu", _stats.recv_msg_count);
    PINF("Send message count          : %lu", _stats.send_msg_count);
    PINF("Response count              : %lu", _stats.response_count);
    PINF("WAIT_SEND_VALUE misses      : %lu", _stats.wait_send_value_misses);
    PINF("WAIT_RESPOND_COMPLETE misses: %lu", _stats.wait_respond_complete_misses);
    PINF("-----------------------------------------");
  }

	unsigned debug_level() const { return Connection_base::debug_level(); }
  /**
   * Change state in FSM
   *
   * @param s
   */
  inline void set_state(Connection_state s)
  {
    CFLOGM(2, "state {} -> {}", _state, s);
    _state = s; /* we could add transition checking later */
  }

  static void static_send_callback2(void *cnxn, buffer_t *iob) noexcept
  {
    auto base = static_cast<Fabric_connection_base *>(cnxn);
    static_cast<Connection_handler *>(base)->send_callback2(iob);
  }

  void send_callback2(buffer_t *iob) noexcept
  {
    assert(iob->value_adjunct);
    CFLOGM(2, "Completed send2 (value_adjunct {})", iob->value_adjunct);
/*
 * Requires that value_adjunct (the value in the second buffer of the pair) have a
 * reservation which can be undone by ACTION_RELEASE_VALUE_LOCK_SHARED, which is _i_kvstore->unlock()
 */
    _deferred_unlock.push(action_t{action_type::ACTION_RELEASE_VALUE_LOCK_SHARED, iob->value_adjunct});
    send_callback(iob);
  }

  /**
   * Send handshake response
   *
   * @param start_tls Set if a TLS handshake needs to be started
   */
  void respond_to_handshake(bool start_tls
#if CW_TEST && 0
		, uint64_t scratchpad_base_
		, uint64_t scratchpad_size_
#endif
	);


public:

  explicit Connection_handler(unsigned debug_level,
                              gsl::not_null<Factory *> factory,
                              std::unique_ptr<Preconnection, uc_destructor> && preconnection
    , unsigned buffer_count
#if CW_TEST && 0
		, byte_span scratchpad
#endif
	);

  virtual ~Connection_handler();

  inline bool client_connected() { return _state != Connection_state::CLIENT_DISCONNECTED; }

  auto allocate_send() { return allocate(static_send_callback); }
  auto allocate_recv() { return allocate(static_recv_callback); }

  /**
   * Initialize security session
   *
   * @param bind_ipaddr
   * @param bind_port
   */
  void configure_security(const std::string& bind_ipaddr,
                          const unsigned bind_port,
                          const std::string& cert_file,
                          const std::string& key_file);


  /**
   * State machine transition tick.  It is really important that this tick
   * execution duration is small, so that other connections are not impacted by
   * the thread not returning to them.
   *
   */
  tick_type tick();
  /**
   * Check for network completions
   *
   */
  Fabric_connection_base::Completion_state check_network_completions()
  {
    auto state = poll_completions();

    while (!_deferred_unlock.empty()) {
      //      void *deferred_unlock = nullptr;
      //???     std::swap(deferred_unlock, _deferred_unlock.front());
      auto deferred_unlock = _deferred_unlock.front();
      CFLOG(2, "adding action for deferred unlocking value @ {}", deferred_unlock.parm);
      add_pending_action(deferred_unlock);
      _deferred_unlock.pop();
    }

    return state;
  }

  /**
   * Peek at pending message from the connection. Used when the resources
   * required to process a pending nessage may depend on the content of the
   * message.
   *
   * @param msg [out] Pointer to base protocol message
   *
   * @return Pointer to buffer holding the message or null if there are none
   */
  inline mcas::protocol::Message *peek_pending_msg() const
  {
    return _pending_msgs.empty() ? nullptr
      : static_cast<mcas::protocol::Message *>(_pending_msgs.front()->base().get());
  }

  /**
   * Discard a pending message from the connection. Used along with tick
   * (which adds pending messages) and peek (which peeks at them).
   *
   * @param msg [out] Pointer to base protocol message
   *
   * @return Pointer to buffer holding the message or null if there are none
   */
  inline buffer_t *pop_pending_msg()
  {
    assert(!_pending_msgs.empty());
    auto iob = _pending_msgs.front();
    _pending_msgs.pop();
    return iob;
  }

  /**
   * Get deferred actions
   *
   * @param action [out] Action
   *
   * @return True if action
   */
  inline bool get_pending_action(action_t &action)
  {
    if (_pending_actions.empty()) return false;

    action = _pending_actions.front();

    CFLOGM(2, "Connection_handler: popped pending action ({}, {})", int(action.op), action.parm);

    _pending_actions.pop();
    return true;
  }

  template <typename MT>
  void msg_log(const unsigned level_, const MT *msg_, const char *desc_, const char *direction_)
  {
    if (level_ < Connection_base::debug_level()) {
      std::ostringstream m;
      m << *msg_;
      PLOG("%s (%s) %s", direction_, desc_, m.str().c_str());
    }
  }

  template <typename MT>
  void msg_recv_log(const MT *m, const char *desc)
  {
    msg_recv_log(2, m, desc);
  }

  template <typename MT>
  void msg_recv_log(const unsigned level, const MT *msg, const char *desc)
  {
    msg_log(level, msg, desc, "RECV");
  }

  template <typename MT>
  void msg_send_log(const MT *m, const char *desc)
  {
    msg_send_log(2, m, desc);
  }

  template <typename MT>
  void msg_send_log(const unsigned level, const MT *msg, const char *desc)
  {
    msg_log(level, msg, desc, "SEND");
  }

  /**
   * Add an action to the pending queue
   *
   * @param action Action to add
   */
  inline void add_pending_action(const action_t& action)
  {
    _pending_actions.push(action);
  }

  template <typename Msg>
  void post_send_buffer(gsl::not_null<buffer_t *> buffer, Msg *msg, const char *desc)
  {
    msg_send_log(msg, desc);
    Connection_base::post_send_buffer(buffer);
  }

  template <typename Msg>
  void post_send_buffer_with_value(gsl::not_null<buffer_t *> buffer,
                         const ::iovec &           val_iov,
                         void *                    val_desc,
                         Msg *                     msg,
                         const char *              func_name)
  {
    msg_send_log(msg, func_name);
    Connection_base::post_send_buffer2(buffer, val_iov, val_desc);
  }

  /**
   * Post a response
   *
   * @param iob IO buffer to post
   */
  template <typename Msg>
  inline void post_response_with_value(gsl::not_null<buffer_t *> iob,
                             const ::iovec &           val_iov,
                             void *                    val_desc,
                             Msg *                     msg,
                             const char *              func_name)
  {
    iob->set_completion(static_send_callback2, val_iov.iov_base);
    post_send_buffer_with_value(iob, val_iov, val_desc, msg, func_name);
    /* don't wait for this, let it be picked up in the check_completions cycle
     */

    _stats.response_count++;
  }

  /**
   * Post a response
   *
   * @param iob IO buffer to post
   */
  template <typename Msg>
  inline void post_response(gsl::not_null<buffer_t *> iob, Msg *msg, const char *desc)
  {
    post_send_buffer(iob, msg, desc);
    /* don't wait for this, let it be picked up in the check_completions cycle
     */

    _stats.response_count++;
  }

  /**
   * Set up for pending value send/recv
   *
   * @param target
   * @param target_len
   * @param region
   */
  void add_memory_handle(component::IKVStore::memory_handle_t handle) { _mr_vector.push_back(handle); }

  component::IKVStore::memory_handle_t pop_memory_handle()
  {
    if (_mr_vector.empty()) return nullptr;
    auto mr = _mr_vector.back();
    _mr_vector.pop_back();
    return mr;
  }

	void add_locked_value(const void *target, memory_registered<Connection_base> &&mr);
	void release_locked_value(const void *target);
	void add_space_shared(const range<std::uint64_t> &range_, memory_registered<Connection_base> &&mr);
	void release_space_shared(const range<std::uint64_t> &range_);

  inline uint64_t       auth_id() const { return _auth_id; }
  inline void           set_auth_id(uint64_t id) { _auth_id = id; }
  inline size_t         max_message_size() const { return _max_message_size; }
  inline Pool_manager&  pool_manager() { return _pool_manager; }

private:
};

}  // namespace mcas

#pragma GCC diagnostic pop

#endif

#endif  // __CONNECTION_HANDLER_H__
