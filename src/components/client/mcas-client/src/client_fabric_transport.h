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
#ifndef __CLIENT_FABRIC_TRANSPORT_H__
#define __CLIENT_FABRIC_TRANSPORT_H__

#include <api/fabric_itf.h>
#include <common/cycles.h>
#include <common/exceptions.h>
#include <common/utils.h>

#include "mcas_client_config.h"

namespace mcas
{
namespace Client
{
static constexpr size_t NUM_BUFFERS = 64; /* defines max outstanding */

class Fabric_transport {
  friend class mcas_client;

  const bool option_DEBUG = mcas::Global::debug_level > 3;

 public:
  using Transport       = Component::IFabric_client;
  using buffer_t        = Buffer_manager<Transport>::buffer_t;
  using memory_region_t = Component::IFabric_memory_region *;

  double cycles_per_second = Common::get_rdtsc_frequency_mhz() * 1000000.0;

  Fabric_transport(Component::IFabric_client *fabric_connection)
      : _transport(fabric_connection),
      _max_inject_size(_transport->max_inject_size()),
      _bm(fabric_connection, NUM_BUFFERS)
  {
  }

  Fabric_transport(const Fabric_transport &) = delete;
  Fabric_transport &operator=(const Fabric_transport &) = delete;

  ~Fabric_transport() {}

  static Component::IFabric_op_completer::cb_acceptance completion_callback(void *        context,
                                                                            status_t      st,
                                                                            std::uint64_t completion_flags,
                                                                            std::size_t   len,
                                                                            void *        error_data,
                                                                            void *        param)
  {
    if (UNLIKELY(st != S_OK))
      throw Program_exception("poll_completions failed unexpectedly (st=%d) (cf=%lx)", st, completion_flags);

    if (*(static_cast<void **>(param)) == context) {
      *static_cast<void **>(param) = nullptr; /* signals completion */
      return Component::IFabric_op_completer::cb_acceptance::ACCEPT;
    }
    else {
      return Component::IFabric_op_completer::cb_acceptance::DEFER;
    }
  }

  /**
   * Wait for completion of a IO buffer posting
   *
   * @param iob IO buffer to wait for completion of
   */
  void wait_for_completion(void *wr)
  {
    auto start_time = rdtsc();
    // currently setting time out to 1 min...
    while (wr && static_cast<double>(rdtsc() - start_time) / cycles_per_second <= 60) {
      try {
        _transport->poll_completions_tentative(completion_callback, &wr);
      }
      catch (...) {
        break;
      }
    }
    if (wr) {
      throw Program_exception("time out: waiting for completion");
    }
  }

  /**
   * Test completion of work request
   *
   * @param Work request
   *
   * @return True iff complete
   */
  inline bool test_completion(void *wr)
  {
    _transport->poll_completions_tentative(completion_callback, &wr);
    return (wr == nullptr);
  }

  /**
   * Forwarders that allow us to avoid exposing _transport and _bm
   *
   */
  inline memory_region_t register_memory(void *base, size_t len)
  {
    return _transport->register_memory(base, len, 0, 0);
  }

  inline void *get_memory_descriptor(memory_region_t region) { return _transport->get_memory_descriptor(region); }

  inline void deregister_memory(memory_region_t region) { _transport->deregister_memory(region); }

  inline void post_send(const ::iovec *first, const ::iovec *last, void **descriptors, void *context)
  {
    _transport->post_send(first, last, descriptors, context);
  }

  inline void post_recv(const ::iovec *first, const ::iovec *last, void **descriptors, void *context)
  {
    _transport->post_recv(first, last, descriptors, context);
  }

  inline size_t max_message_size() const { return _transport->max_message_size(); }

  /**
   * Post send (one or two buffers) and wait for completion.
   *
   * @param iob First IO buffer
   * @param iob_extra Second IO buffer
   */
  void sync_send(buffer_t *iob, buffer_t *iob_extra = nullptr)
  {
    if (iob_extra) {
      iovec v[2]   = {*iob->iov, *iob_extra->iov};
      void *desc[] = {iob->desc, iob_extra->desc};

      post_send(&v[0], &v[2], desc, iob);
    }
    else {
      post_send(iob->iov, iob->iov + 1, &iob->desc, iob);
    }

    wait_for_completion(iob);
  }

  /**
   * Perform inject send (fast for small packets)
   *
   * @param iob Buffer to send
   */
  void sync_inject_send(buffer_t *iob)
  {
    auto len = iob->length();
    if (len <= _max_inject_size) {
      /* when this returns, iob is ready for immediate reuse */
      _transport->inject_send(iob->base(), iob->length());
    }
    else {
      /* too big for inject, do plain send */
      post_send(iob->iov, iob->iov + 1, &iob->desc, iob);
      wait_for_completion(iob);
    }
  }

  /**
   * Post send (one or two buffers) and wait for completion.
   *
   * @param iob First IO buffer
   * @param iob_extra Second IO buffer
   */
  void post_send(buffer_t *iob, buffer_t *iob_extra = nullptr)
  {
    if (iob_extra) {
      iovec v[2]   = {*iob->iov, *iob_extra->iov};
      void *desc[] = {iob->desc, iob_extra->desc};

      post_send(&v[0], &v[2], desc, iob);
    }
    else {
      post_send(iob->iov, iob->iov + 1, &iob->desc, iob);
    }
  }

  /**
   * Post receive then wait for completion before returning.
   * Use after post_send may lead to poor performance if the response
   * arrives before the receive buffer is posted.
   *
   * @param iob IO buffer
   */
  void sync_recv(buffer_t *iob)
  {
    post_recv(iob);
    wait_for_completion(iob);
  }

  void post_recv(buffer_t *iob)
  {
    if (option_DEBUG)
      PLOG("%s: (%p, %p, base=%p, len=%lu)", __func__, static_cast<const void *>(iob), iob->desc, iob->iov->iov_base,
           iob->iov->iov_len);

    post_recv(iob->iov, iob->iov + 1, &iob->desc, iob);
  }

  Component::IKVStore::memory_handle_t register_direct_memory(void *region, size_t region_len)
  {
    // if (!check_aligned(region, 64))
    //   throw API_exception("register_direct_memory: region should be
    //   aligned");

    auto mr        = register_memory(region, region_len);
    auto desc      = get_memory_descriptor(mr);
    auto buffer    = new buffer_t(region, region_len, mr, desc);
    if (option_DEBUG)
      PLOG("register_direct_memory (%p, %lu, mr=%p, desc=%p)", region, region_len, static_cast<const void *>(mr), desc);

    return reinterpret_cast<Component::IKVStore::memory_handle_t>(buffer);
  }

  status_t unregister_direct_memory(Component::IKVStore::memory_handle_t handle)
  {
    buffer_t *buffer = reinterpret_cast<buffer_t *>(handle);
    assert(buffer->check_magic());

    _transport->deregister_memory(buffer->region);
    return S_OK;
  }

  inline auto allocate() { return _bm.allocate(); }
  inline void free_buffer(buffer_t *buffer) { _bm.free(buffer); }

 protected:
  Transport *               _transport;
  size_t                    _max_inject_size;
  Buffer_manager<Transport> _bm; /*< IO buffer manager */
};                               // namespace Client

}  // namespace Client
}  // namespace mcas

#endif  //__CLIENT_FABRIC_TRANSPORT_H__
