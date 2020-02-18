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
#ifndef __BUFFER_MGR_H__
#define __BUFFER_MGR_H__

#ifdef __cplusplus

#include <api/components.h>
#include <api/fabric_itf.h>
#include <api/kvstore_itf.h>
#include <sys/mman.h>

#include "mcas_config.h"
#include <cstring> /* memset */
#include <memory> /* unique_ptr */

namespace mcas
{
template <class Transport>
class Buffer_manager {
  unsigned option_DEBUG = mcas::Global::debug_level;

 public:
  static constexpr size_t DEFAULT_BUFFER_COUNT = NUM_SHARD_BUFFERS;
  static constexpr size_t BUFFER_LEN           = MiB(2);

  enum {
    BUFFER_FLAGS_EXTERNAL = 1,
  };

  using memory_region_t = typename Transport::memory_region_t;

  struct buffer_t {
    iovec *         iov;
    memory_region_t region;
    void *          desc;
    const size_t    original_length;
    int             flags;
    const unsigned  magic;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++" // missing initializers
    buffer_t(void *base, size_t length, memory_region_t region_, void * desc_)
      : iov(new ::iovec{base, length}),
        region(region_),
        desc(desc_),
        original_length(length), flags(0), magic(0xC0FFEE) {
    }
#pragma GCC diagnostic pop

    buffer_t(const buffer_t &) = delete;
    buffer_t &operator=(const buffer_t &) = delete;

    ~buffer_t()
    {
       delete iov;
    }

    inline void *base()
    {
      assert(iov);
      return iov->iov_base;
    }
    inline size_t length()
    {
      assert(iov);
      return iov->iov_len;
    }
    inline void set_length(size_t s) { iov->iov_len = s; }
    inline void set_external() { flags |= BUFFER_FLAGS_EXTERNAL; }
    inline bool is_external() const { return flags & BUFFER_FLAGS_EXTERNAL; }
    inline void reset_length()
    {
      assert(!is_external());
      assert(original_length > 1);
      iov->iov_len = original_length;
    }
    inline bool check_magic() const { return magic == 0xC0FFEE; }
  };

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++" // missing initializers
  Buffer_manager(Transport *transport, size_t buffer_count = DEFAULT_BUFFER_COUNT)
      : _buffer_count(buffer_count), _transport(transport)
  {
    init();
  }
#pragma GCC diagnostic pop

  Buffer_manager(const Buffer_manager &) = delete;
  Buffer_manager &operator=(const Buffer_manager &) = delete;

  ~Buffer_manager()
  {
    /* Note: A buffer_t does not own its iov->iov_base.
     */
    for (auto &b : _buffers) {
      ::free(b->iov->iov_base);
    }
  }

  buffer_t *allocate()
  {
    if (UNLIKELY(_free.empty())) throw Program_exception("buffer_manager.h: no shard buffers remaining");
    auto iob = _free.back();
    assert(iob->flags == 0);
    _free.pop_back();
    if (option_DEBUG > 3) PLOG("bm: allocate : %p %lu", static_cast<const void *>(iob), _free.size());
    assert(iob);
    iob->reset_length();
    return iob;
  }

  void free(buffer_t *iob)
  {
    assert(iob);
    assert(iob->flags == 0);

    if (option_DEBUG > 3) PLOG("bm: free     : %p", static_cast<const void *>(iob));
    iob->reset_length();
    _free.push_back(iob);
  }

 private:
  // static constexpr size_t buffer_len() { return BUFFER_LEN; }
  static auto alloc_base(std::size_t len) -> void * {
    auto base = aligned_alloc(MiB(2), len);
    assert(base);
    ::madvise(base, len, MADV_HUGEPAGE);
    std::memset(base, 0, len);
    return base;
  };
  void init()
  {
    for (unsigned i = 0; i < _buffer_count; i++) {
      const auto len = BUFFER_LEN;
      auto base = alloc_base(len);
      auto region = _transport->register_memory(base, len, 0, 0);
      auto desc   = _transport->get_memory_descriptor(region);
      auto new_buffer = std::make_unique<buffer_t>(base, len, region, desc);
      _free.push_back(new_buffer.get());
      _buffers.push_back(std::move(new_buffer));
    }
  }

  using pool_t = Component::IKVStore::pool_t;
  using key_t  = std::uint64_t;

  const size_t            _buffer_count;
  Transport *             _transport;
  std::vector<std::unique_ptr<buffer_t>> _buffers;
  std::vector<buffer_t *> _free;
};

}  // namespace mcas

#endif
#endif
