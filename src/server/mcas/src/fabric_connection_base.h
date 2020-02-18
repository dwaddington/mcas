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
#ifndef __FABRIC_CONNECTION_BASE_H__
#define __FABRIC_CONNECTION_BASE_H__

#include <list>

#include "mcas_config.h"

namespace mcas
{
class Fabric_connection_base {
  friend class Connection;
  friend class Shard;

 private:
  unsigned option_DEBUG = mcas::Global::debug_level;

 public:
  using memory_region_t = Component::IFabric_memory_region *;

 protected:
  using buffer_t = Buffer_manager<Component::IFabric_server>::buffer_t;
  using pool_t   = Component::IKVStore::pool_t;

  /* deferred actions */
  typedef struct {
    int   op;
    void *parm;
  } action_t;

  enum class Completion_state {
    ADDED_DEFERRED_LOCK,
    NO_DEFER,
    CLIENT_DISCONNECT,
    NONE,
  };

  /**
   * Ctor
   *
   * @param factory
   * @param fabric_connection
   */
  Fabric_connection_base(Component::IFabric_server_factory *factory, Component::IFabric_server *fabric_connection)
      : _bm(fabric_connection), _factory(factory),
        _transport(fabric_connection),
        _max_message_size((assert(_transport),
        _transport->max_message_size())),
        _registered_regions{},
        _completed_recv_buffers{}
  {
  }

  Fabric_connection_base(const Fabric_connection_base &) = delete;
  Fabric_connection_base &operator=(const Fabric_connection_base &) = delete;

  /**
   * Dtor
   *
   */
  ~Fabric_connection_base()
  {
    for (auto r : _registered_regions) {
      _transport->deregister_memory(r);
    }

    /* ERROR: RDMA and FABRIC disagree on the name (disconnect vs.
     * close_connection). Maybe RDMA's choice (disconnect) is better. One less
     * word and one less syllable. ERROR: in fabric, you cannot do anything with
     * a connection after you close it. In particular, you cannot deregister
     * memory. So close_connection is the *last" operation on "_transport."
     */
    // See BUG mcas-134
    //    _factory->close_connection(_transport);
  }

  static void completion_callback(void *   context,
                                  status_t st,
                                  std::uint64_t,  // completion_flags,
                                  std::size_t,    //   len,
                                  void *error_data,
                                  void *param) noexcept
  {
    Fabric_connection_base *pThis = static_cast<Fabric_connection_base *>(param);

    /* set callback debugging here */
    static constexpr bool option_DEBUG = false;

    if (UNLIKELY(st != S_OK)) {
      PERR("Fabric_connection_base: fabric operation failed st != S_OK (st=%d, context=%p)", st, context);
      PERR("Error: %s", static_cast<char *>(error_data));
      return;
    }

    //     if (context == pThis->_posted_recv_buffer) {
    //   assert(pThis->_posted_recv_buffer_outstanding);
    //   assert(pThis->_posted_recv_buffer);
    //   if (option_DEBUG) PLOG("Posted recv complete (%p).", context);
    //   pThis->_posted_recv_buffer_outstanding = false; /* signal recv completion */
    //   return;
    // }

    if (context == pThis->_posted_send_buffer) {
      assert(pThis->_posted_send_buffer_outstanding);
      if (option_DEBUG) PLOG("Posted send complete (%p).", context);
      pThis->_posted_send_buffer_outstanding = false; /* signal send completion */
      return;
    }
    else if (context == pThis->_posted_value_buffer) {
      assert(pThis->_posted_value_buffer_outstanding);
      char *p = static_cast<char *>(pThis->_posted_value_buffer->base());
      if (option_DEBUG) {
        PLOG("Posted value complete (%p) [%x %x %x...]", context, 0xff & int(p[0]), 0xff & int(p[1]),
             0xff & int(p[2]));
      }
      pThis->_posted_value_buffer_outstanding = false; /* signal value completion */
    }
    else { /* must be recv completion */
      pThis->_completed_recv_buffers.push_front(reinterpret_cast<buffer_t *>(context));
      pThis->_posted_recv_buffer_count--;
    }

    // else {
    //   throw Program_exception("unknown completion context (%p)", context);
    // }
  }

  bool check_for_posted_send_complete()
  {
    if (_posted_send_buffer_outstanding) return false;

    if (_posted_send_buffer) {
      if (option_DEBUG > 2) PLOG("Fabric_connection_base::freeing buffer (%p)", static_cast<const void *>(_posted_send_buffer));

      free_buffer(_posted_send_buffer);
      _posted_send_buffer = nullptr;
    }

    return true;
  }

  bool check_for_posted_recv_complete()
  {
    /* don't free buffer (such as above); it will be used for response */
    //    return _posted_recv_buffer_outstanding == false;
    return _completed_recv_buffers.size() > 0;
  }

  bool check_for_posted_value_complete(bool *added_deferred_unlock = nullptr)
  {
    if (_posted_value_buffer_outstanding) return false;

    if (_posted_value_buffer) { /* release 'value buffer' from get_direct */
      assert(_deferred_unlock == nullptr);
      _deferred_unlock = _posted_value_buffer->base();
      if (added_deferred_unlock) *added_deferred_unlock = true;
      _posted_value_buffer = nullptr;
    }

    return true;
  }

  void free_recv_buffer()
  {
    PLOG("freeing recv buffers");
    for (auto b : _completed_recv_buffers) {
      free_buffer(b);
    }
  }

  void post_recv_buffer(buffer_t *buffer)
  {
    // assert(buffer);
    // assert(_posted_recv_buffer_outstanding == false);
    // _posted_recv_buffer             = buffer;
    // _posted_recv_buffer_outstanding = true;

    // TODO add to posted vector
    _transport->post_recv(buffer->iov, buffer->iov + 1, &buffer->desc, buffer);
    _posted_recv_buffer_count++;
  }

  void post_send_buffer(buffer_t *buffer, buffer_t *val_buffer = nullptr)
  {
    assert(buffer);
    assert(_posted_send_buffer_outstanding == false);
    const auto iov = buffer->iov;

    if (!val_buffer) {
      /* if packet is small enough use inject */
      if (iov->iov_len <= _transport->max_inject_size()) {
        if (option_DEBUG > 2) PLOG("Fabric_connection_base: posting send with inject (%p)", iov->iov_base);

        _transport->inject_send(iov->iov_base, iov->iov_len);
        free_buffer(buffer); /* buffer can be immediately released; see fi_inject */
      }
      else {
        _posted_send_buffer             = buffer;
        _posted_send_buffer_outstanding = true;

        if (option_DEBUG > 2) PLOG("Fabric_connection_base: posting send (%p, %p)", static_cast<const void *>(buffer), iov->iov_base);

        _transport->post_send(iov, iov + 1, &_posted_send_buffer->desc, _posted_send_buffer);
      }
    }
    else {
      _posted_send_buffer             = buffer;
      _posted_send_buffer_outstanding = true;

      iovec v[2]   = {*buffer->iov, *val_buffer->iov};
      void *desc[] = {buffer->desc, val_buffer->desc};

      if (option_DEBUG)
        PLOG("posting .... value (%.*s) (len=%lu,ptr=%p)", int(val_buffer->iov->iov_len),
             static_cast<char *>(val_buffer->iov->iov_base), val_buffer->iov->iov_len, val_buffer->iov->iov_base);

      _transport->post_send(&v[0], &v[2], desc, buffer);
    }
  }

  void post_send_value_buffer(buffer_t *buffer)
  {
    if (buffer) {
      assert(!_posted_value_buffer);
      _posted_value_buffer = buffer;
    }
    else {
    } /* buffer already set up */
    assert(_posted_value_buffer);
    _posted_value_buffer_outstanding = true;
    _transport->post_send(_posted_value_buffer->iov, _posted_value_buffer->iov + 1, &_posted_value_buffer->desc,
                          _posted_value_buffer);

    if (option_DEBUG)
      PLOG("posting send value buffer (%p)(base=%p,len=%lu,desc=%p)", static_cast<const void *>(buffer), _posted_value_buffer->iov->iov_base,
           _posted_value_buffer->iov->iov_len, _posted_value_buffer->desc);
  }

  void post_recv_value_buffer(buffer_t *buffer)
  {
    assert(buffer);
    _posted_value_buffer = buffer;

    _posted_value_buffer_outstanding = true;
    _transport->post_recv(_posted_value_buffer->iov, _posted_value_buffer->iov + 1, &_posted_value_buffer->desc,
                          _posted_value_buffer);

    if (option_DEBUG)
      PLOG("posted recv value buffer (%p)(base=%p,len=%lu,desc=%p)", static_cast<const void *>(buffer),
           _posted_value_buffer->iov->iov_base,
           _posted_value_buffer->iov->iov_len, _posted_value_buffer->desc);
  }

  buffer_t *posted_recv()
  {
    if (_completed_recv_buffers.size() == 0) return nullptr;
    auto rb = _completed_recv_buffers.back();
    _completed_recv_buffers.pop_back();
    return rb;
  }

  inline buffer_t *posted_send() const { return _posted_send_buffer; }

  Completion_state poll_completions()
  {
    if (_posted_recv_buffer_count > 0 || _posted_send_buffer_outstanding || _posted_value_buffer_outstanding) {
      bool added_deferred_unlock = false;
      try {
        _transport->poll_completions(&Fabric_connection_base::completion_callback, this);
        /* Note: this test may be in error, as the function of
         * check_for_posted_send_complete is not to complete the
         * send but to free the buffer after the send completes. */
        //          if(_posted_send_buffer_outstanding)
        check_for_posted_send_complete();

        //          if(_posted_value_buffer_outstanding)
        check_for_posted_value_complete(&added_deferred_unlock);
      }
      catch (std::logic_error &e) {
        return Completion_state::CLIENT_DISCONNECT;
      }

      return added_deferred_unlock ? Completion_state::ADDED_DEFERRED_LOCK : Completion_state::NO_DEFER;
    }
    return Completion_state::NONE;
  }

  /**
   * Forwarders that allow us to avoid exposing _transport and _bm
   *
   */
  inline auto register_memory(const void *base, size_t len)
  {
    return _transport->register_memory(base, len, 0, 0); /* flags not supported for verbs */
  }

  inline void deregister_memory(memory_region_t region) { return _transport->deregister_memory(region); }

  inline void *get_memory_descriptor(memory_region_t region) { return _transport->get_memory_descriptor(region); }

  inline auto allocate() { return _bm.allocate(); }

  inline void free_buffer(buffer_t *buffer) { _bm.free(buffer); }

  inline size_t IO_buffer_size() const { return Buffer_manager<Component::IFabric_server>::BUFFER_LEN; }

  auto transport() const { return _transport; }

 private:
  Buffer_manager<Component::IFabric_server> _bm;

 protected:
  Component::IFabric_server_factory *_factory;
  Component::IFabric_server *        _transport;
  size_t                             _max_message_size;
  std::vector<memory_region_t>       _registered_regions;
  void *                             _deferred_unlock = nullptr;

  /* xx_buffer_outstanding is the signal for completion,
     xx_buffer is the buffer pointer that needs to be freed (and set to null)
  */
  std::list<buffer_t *> _completed_recv_buffers;
  unsigned              _posted_recv_buffer_count = 0;

  buffer_t *_posted_send_buffer             = nullptr;
  bool      _posted_send_buffer_outstanding = false;

  /* value for two-phase get & put - assumes get and put don't happen
     at the same time for the same FSM
   */
  buffer_t *_posted_value_buffer             = nullptr;
  bool      _posted_value_buffer_outstanding = false;
};

}  // namespace mcas

#endif  // __FABRIC_CONNECTION_BASE_H__
