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

#include "mcas_config.h"
#include "shard.h"

namespace mcas
{
int Connection_handler::tick()
{
  using namespace mcas::Protocol;

  auto response = TICK_RESPONSE_CONTINUE;
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

  if (check_network_completions() == Fabric_connection_base::Completion_state::CLIENT_DISCONNECT) {
    PMAJOR("Client disconnected.");
    return mcas::Connection_handler::TICK_RESPONSE_CLOSE;
  }

  switch (_state) {
    case POST_MSG_RECV: { /*< post buffer to receive new message */
      if (option_DEBUG > 2) PMAJOR("Shard State: %lu %p POST_MSG_RECV", _tick_count, static_cast<const void *>(this));
      post_recv_buffer(allocate());
      set_state(WAIT_NEW_MSG_RECV);

      break;
    }
    case WAIT_NEW_MSG_RECV: {
      if (check_for_posted_recv_complete()) { /*< check for recv completion */

        const auto iob = posted_recv();
        assert(iob);

        const Message *msg = mcas::Protocol::message_cast(iob->base());
        assert(msg);

        switch (msg->type_id) {
          case MSG_TYPE_IO_REQUEST: {
            if (option_DEBUG > 2) PMAJOR("Shard: IO_REQUEST");
            _pending_msgs.push_back(iob);
            set_state(POST_MSG_RECV);
            break;
          }
          case MSG_TYPE_PUT_ADO_REQUEST:
          case MSG_TYPE_ADO_REQUEST: {
            if (option_DEBUG > 2) PMAJOR("Shard: ADO_REQUEST");
            _pending_msgs.push_back(iob);
            set_state(POST_MSG_RECV);
            break;
          }
          case MSG_TYPE_CLOSE_SESSION: {
            if (option_DEBUG > 2) PMAJOR("Shard: CLOSE_SESSION");
            free_recv_buffer();
            response = TICK_RESPONSE_CLOSE;
            break;
          }
          case MSG_TYPE_POOL_REQUEST: {
            if (option_DEBUG > 2) PMAJOR("Shard: POOL_REQUEST");
            _pending_msgs.push_back(iob);
            set_state(POST_MSG_RECV); /* move state to new message recv */
            break;
          }
          case MSG_TYPE_INFO_REQUEST: {
            if (option_DEBUG > 2) PMAJOR("Shard: INFO_REQUEST");
            _pending_msgs.push_back(iob);
            set_state(POST_MSG_RECV); /* move state to new message recv */
            break;
          }
          default:
            throw Logic_exception("unhandled message (type:%x)", msg->type_id);
        }

        _stats.recv_msg_count++;

        if (option_DEBUG > 2) PMAJOR("Shard State: %lu %p WAIT_MSG_RECV complete", _tick_count, static_cast<const void *>(this));
      }
      else {
        _stats.wait_msg_recv_misses++;
      }

      break;
    }
    case WAIT_SEND_VALUE:
    case WAIT_RECV_VALUE: {
      if (check_for_posted_value_complete()) {
        if (option_DEBUG > 2) {
          PMAJOR("Shard State: %lu %p WAIT_SEND/RECV_VALUE ok", _tick_count, static_cast<const void *>(this));
        }

        if (_posted_value_buffer) {
          /* add action to release lock */
          add_pending_action(action_t{ACTION_RELEASE_VALUE_LOCK, _posted_value_buffer->base()});

          delete _posted_value_buffer; /* delete descriptor */
          _posted_value_buffer = nullptr;
        }

        if (option_DEBUG > 2) PMAJOR("Shard State: %lu %p WAIT_RECV_VALUE_COMPLETE", _tick_count, static_cast<const void *>(this));

        if (_state == WAIT_RECV_VALUE)
          _stats.recv_msg_count++;
        else
          _stats.send_msg_count++;

        set_state(POST_MSG_RECV);
      }
      else
        _stats.wait_recv_value_misses++;
      break;
    }
    case INITIALIZE: {
      set_state(POST_HANDSHAKE);
      break;
    }
    case POST_HANDSHAKE: {
      if (option_DEBUG > 2) PMAJOR("Shard State: %lu %p POST_HANDSHAKE", _tick_count, static_cast<const void *>(this));
      //    post_recv_buffer(allocate());

      for (size_t i = 0; i < NUM_SHARD_BUFFERS / 2; i++) post_recv_buffer(allocate());

      set_state(WAIT_HANDSHAKE);
      break;
    }
    case WAIT_HANDSHAKE: {
      if (check_for_posted_recv_complete()) {
        if (option_DEBUG > 2) PMAJOR("Shard State: %lu %p WAIT_HANDSHAKE complete", _tick_count, static_cast<const void *>(this));

        const auto iob = posted_recv();
        assert(iob);

        const auto msg = mcas::Protocol::message_cast(iob->base())->ptr_cast<Message_handshake>();

        if (msg->get_status() != S_OK) throw General_exception("handshake status != S_OK (%d)", msg->get_status());

        /* set authentication token ; TODO - verify token with AAA */
        set_auth_id(msg->auth_id);

        auto reply_iob = allocate();
        assert(reply_iob);

        auto reply_msg = new (reply_iob->base()) mcas::Protocol::Message_handshake_reply(
            iob->length(), auth_id(), 1 /* seq */, max_message_size(), reinterpret_cast<uint64_t>(this),
            nullptr,  // cert
            0);
        /* post response */
        reply_iob->set_length(reply_msg->msg_len);
        post_send_buffer(reply_iob);
        free_buffer(iob);
        set_state(WAIT_HANDSHAKE_RESPONSE_COMPLETION);
      }
      break;
    }
    case WAIT_HANDSHAKE_RESPONSE_COMPLETION: {
      if (check_for_posted_send_complete()) {
        if (option_DEBUG > 2)
          PMAJOR("Shard State: %lu %p WAIT_HANDSHAKE_RESPONSE_COMPLETION "
                 "complete.",
                 _tick_count, static_cast<const void *>(this));
        set_state(POST_MSG_RECV);
      }
      break;
    }

  }  // end switch
  return response;
}

void Connection_handler::set_pending_value(void *target, size_t target_len, memory_region_t region)
{
  assert(target);
  assert(target_len);

  if (option_DEBUG > 2) PLOG("set_pending_value (target=%p, target_len=%lu, handle=%p)", target, target_len, static_cast<const void *>(region));

  auto desc = get_memory_descriptor(region);
  assert(desc);
  _posted_value_buffer             = new buffer_t(target, target_len, region, desc); /* allocate buffer descriptor */
  _posted_value_buffer->flags      = Buffer_manager<Fabric_connection_base>::BUFFER_FLAGS_EXTERNAL;
  _posted_value_buffer_outstanding = true;

  post_recv_value_buffer(_posted_value_buffer);
  set_state(State::WAIT_RECV_VALUE);
}

}  // namespace mcas
