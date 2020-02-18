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

#include <api/components.h>
#include <api/fabric_itf.h>
#include <api/kvstore_itf.h>
#include <common/cpu.h>
#include <common/cycles.h>
#include <common/exceptions.h>
#include <common/logging.h>
#include <sys/mman.h>

#include <map>
#include <queue>
#include <set>
#include <thread>

#include "buffer_manager.h"
#include "fabric_connection_base.h"  // default to fabric transport
#include "mcas_config.h"
#include "pool_manager.h"
#include "protocol.h"
#include "region_manager.h"

namespace mcas
{
using Connection_base = Fabric_connection_base;

/**
 * Connection handler is instantiated for each "connected" client
 */
class Connection_handler
    : public Connection_base
    , public Region_manager {
 private:
  unsigned option_DEBUG = mcas::Global::debug_level;

  /* Adaptor point for different transports */
  using Connection = Component::IFabric_server;
  using Factory    = Component::IFabric_server_factory;

 public:
  enum {
    TICK_RESPONSE_CONTINUE        = 0,
    TICK_RESPONSE_BOOTSTRAP_SPAWN = 1,
    TICK_RESPONSE_CLOSE           = 0xFF,
  };

  enum {
    ACTION_NONE = 0,
    ACTION_RELEASE_VALUE_LOCK,
    ACTION_POOL_DELETE,
  };

 protected:
  enum State {
    INITIALIZE,
    POST_HANDSHAKE,
    WAIT_HANDSHAKE,
    WAIT_HANDSHAKE_RESPONSE_COMPLETION,
    POST_MSG_RECV,
    WAIT_NEW_MSG_RECV,
    WAIT_RECV_VALUE,
    WAIT_SEND_VALUE,
  };

  State _state = State::INITIALIZE;

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++" // many uninitialized/default initialized elements
  Connection_handler(Factory* factory, Connection* connection)
      : Connection_base(factory, connection), Region_manager(connection),
        _pending_msgs{},
        _pending_actions{},
        _freq_mhz(Common::get_rdtsc_frequency_mhz())
  {
    _pending_actions.reserve(Buffer_manager<Connection>::DEFAULT_BUFFER_COUNT);
    _pending_msgs.reserve(Buffer_manager<Connection>::DEFAULT_BUFFER_COUNT);
  }
#pragma GCC diagnostic pop

  ~Connection_handler()
  {
    dump_stats();
    //    exit(0); /* for profiler */
  }

  /**
   * State machine transition tick.  It is really important that this tick
   * execution duration is small, so that other connections are not impacted by
   * the thread not returning to them.
   *
   */
  int tick();

  /**
   * Change state in FSM
   *
   * @param s
   */
  inline void set_state(State s) { _state = s; /* we could add transition checking later */ }

  /**
   * Check for network completions
   *
   */
  Fabric_connection_base::Completion_state check_network_completions()
  {
    auto state = poll_completions();
    if (state == Fabric_connection_base::Completion_state::ADDED_DEFERRED_LOCK) {
      /* deferred unlocks */
      if (_deferred_unlock) {
        if (option_DEBUG > 2) PLOG("adding action for deferred unlocking value @ %p", _deferred_unlock);
        add_pending_action(action_t{ACTION_RELEASE_VALUE_LOCK, _deferred_unlock});
        _deferred_unlock = nullptr;
      }
    }
    return state;
  }

  /**
   * Get pending message from the connection
   *
   * @param msg [out] Pointer to base protocol message
   *
   * @return Pointer to buffer holding the message or null if there are none
   */
  inline buffer_t* get_pending_msg(mcas::Protocol::Message*& msg)
  {
    if (_pending_msgs.empty()) return nullptr;
    auto iob = _pending_msgs.back();
    assert(iob);
    _pending_msgs.pop_back();
    msg = static_cast<mcas::Protocol::Message*>(iob->base());
    return iob;
  }

  /**
   * Peek at pending message from the connection. Used when the resources required
   * to process a pending nessage may depend on the content of the message.
   *
   * @param msg [out] Pointer to base protocol message
   *
   * @return Pointer to buffer holding the message or null if there are none
   */
  inline mcas::Protocol::Message* peek_pending_msg() const
  {
    if (_pending_msgs.empty()) return nullptr;
    auto iob = _pending_msgs.back();
    assert(iob);
    return static_cast<mcas::Protocol::Message*>(iob->base());
  }

  /**
   * Discard a pending message from the connection. Used as a complement to
   * peek_pending_msg
   *
   * @param msg [out] Pointer to base protocol message
   *
   * @return Pointer to buffer holding the message or null if there are none
   */
  inline buffer_t* pop_pending_msg()
  {
    assert(!_pending_msgs.empty());
    auto iob = _pending_msgs.back();
    _pending_msgs.pop_back();
    return iob;
  }

  /**
   * Get deferrd action
   *
   * @param action [out] Action
   *
   * @return True if action
   */
  inline bool get_pending_action(action_t& action)
  {
    if (_pending_actions.empty()) return false;

    action = _pending_actions.back();

    if (option_DEBUG > 2) PLOG("Connection_handler: popped pending action (%u, %p)", action.op, action.parm);

    _pending_actions.pop_back();
    return true;
  }

  /**
   * Add an action to the pending queue
   *
   * @param action Action to add
   */
  inline void add_pending_action(const action_t action) { _pending_actions.push_back(action); }

  /**
   * Post a response
   *
   * @param iob IO buffer to post
   */
  inline void post_response(buffer_t* iob, buffer_t* val_iob = nullptr)
  {
    assert(iob);

    post_send_buffer(iob, val_iob);
    if (val_iob) {
      _posted_value_buffer = val_iob;
    }

    _stats.response_count++;

    set_state(POST_MSG_RECV); /* don't wait for this, let it be picked up in
                                 the check_completions cycle */
  }

  /**
   * Set up for pending value send/recv
   *
   * @param target
   * @param target_len
   * @param region
   */
  void set_pending_value(void* target, size_t target_len, Component::IFabric_connection::memory_region_t region);

  inline void set_state_wait_send_value() { set_state(State::WAIT_SEND_VALUE); }

  void add_memory_handle(Component::IKVStore::memory_handle_t handle) { _mr_vector.push_back(handle); }

  Component::IKVStore::memory_handle_t pop_memory_handle()
  {
    if (_mr_vector.empty()) return nullptr;
    auto mr = _mr_vector.back();
    _mr_vector.pop_back();
    return mr;
  }

  inline uint64_t auth_id() const { return _auth_id; }
  inline void     set_auth_id(uint64_t id) { _auth_id = id; }

  inline size_t max_message_size() const { return _max_message_size; }

  inline Pool_manager& pool_manager() { return _pool_manager; }

 private:
  struct {
    uint64_t response_count               = 0;
    uint64_t recv_msg_count               = 0;
    uint64_t send_msg_count               = 0;
    uint64_t wait_recv_value_misses       = 0;
    uint64_t wait_msg_recv_misses         = 0;
    uint64_t wait_respond_complete_misses = 0;
    uint64_t last_count                   = 0;
    uint64_t next_stamp                   = 0;
  } _stats alignas(8);

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
    PINF("WAIT_RECV_VALUE misses      : %lu", _stats.wait_recv_value_misses);
    PINF("WAIT_RESPOND_COMPLETE misses: %lu", _stats.wait_respond_complete_misses);
    PINF("-----------------------------------------");
  }

 private:
  /* list of pre-registered memory regions; normally one region */
  std::vector<Component::IKVStore::memory_handle_t> _mr_vector;

  uint64_t               _tick_count alignas(8) = 0;
  uint64_t               _auth_id               = 0;
  std::vector<buffer_t*> _pending_msgs;
  std::vector<action_t>  _pending_actions;
  float                  _freq_mhz;
  Pool_manager           _pool_manager; /* instance shared across connections */
};

}  // namespace mcas
#endif

#endif  // __SHARD_HPP__
