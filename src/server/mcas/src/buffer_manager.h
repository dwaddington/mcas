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

#include <common/chksum.h>
#include <api/components.h>
#include <api/fabric_itf.h>
#include <api/kvstore_itf.h>
#include <sys/mman.h>

#include "mcas_config.h"
#include "memory_registered.h"
#include <gsl/pointers> /* not_null */
#include <cassert>
#include <cstring>      /* memset */
#include <memory>       /* unique_ptr */

namespace mcas
{
template <class Transport>
class Buffer_manager {

  static constexpr const char *_cname       = "Buffer_manager";

  unsigned debug_level() const { return _debug_level; }

public:
  static constexpr size_t DEFAULT_BUFFER_COUNT = NUM_SHARD_BUFFERS;
  static constexpr size_t BUFFER_LEN           = MiB(2); /* corresponds to huge page see below */
  using memory_registered_t                    = memory_registered<Transport>;

  enum {
    BUFFER_FLAGS_EXTERNAL = 1,
  };

  using memory_region_t = typename Transport::memory_region_t;

    /* Although client tried to hide it with a reinterpret_cast, buffer_t is
     used as a Component::memory_region_t */

  struct buffer_t : public component::IKVStore::Opaque_memory_region
  {
    using completion_t = void (*)(void *, buffer_t *);
    ::iovec iov[1];

  private:
    static constexpr const char *_cname = "buffer_t";
    memory_registered_t          _region; /* client-side buffers only */
    unsigned                     debug_level() const { return _debug_level; }

  public:
    void *         desc;
    completion_t   completion_cb;
    const size_t   original_length;
    void *         value_adjunct;
    unsigned       _debug_level;
    int            flags;
    const unsigned magic;

    buffer_t(unsigned              debug_level_,
             gsl::not_null<void *> base,
             size_t                length,
             memory_registered_t &&region_,
             int                   flags_)
      : iov{{base, length}},
        _region(std::move(region_)),
        desc(_region.desc()),
        completion_cb(nullptr),
        original_length(length),
        value_adjunct(nullptr),
        _debug_level(debug_level_),
        flags(flags_),
        magic(0xC0FFEE)
    {
      assert(length);
      CPLOG(2, "%s::%s %p transport %p region %p", _cname, __func__, static_cast<void *>(this),
            static_cast<void *>(transport()), static_cast<void *>(region()));
    }

    buffer_t(unsigned debug_level_, gsl::not_null<void *> base, size_t length, memory_registered_t region_)
      : buffer_t(debug_level_, base, length, std::move(region_), 0)
    {
    }

    buffer_t()                 = delete;
    buffer_t(const buffer_t &) = delete;
    buffer_t &operator=(const buffer_t &) = delete;

    virtual ~buffer_t()
    {
      CPLOG(2, "%s::%s %p transport %p region %p", _cname, __func__, static_cast<void *>(this),
            static_cast<void *>(transport()), static_cast<void *>(region()));
    }

    inline gsl::not_null<void *> base() const { return iov->iov_base; }

    inline size_t length() const { return iov->iov_len; }

    auto region() const { return _region.mr(); }
    auto transport() const { return _region.transport(); }

    inline void set_length(size_t s) { iov->iov_len = s; }
    inline void set_external() { flags |= BUFFER_FLAGS_EXTERNAL; }
    inline bool is_external() const { return flags & BUFFER_FLAGS_EXTERNAL; }
    inline void set_completion(completion_t completion_) { completion_cb = completion_; }
    inline void set_completion(completion_t completion_, void *value_adjunct_)
    {
      completion_cb = completion_;
      value_adjunct = value_adjunct_;
    }

    inline void reset_length()
    {
      assert(!is_external());
      assert(original_length > 1);
      iov->iov_len  = original_length;
      value_adjunct = nullptr;
      completion_cb = nullptr;
    }

    inline bool check_magic() const { return magic == 0xC0FFEE; }

    inline unsigned int crc32() const { return common::chksum32(iov->iov_base, iov->iov_len); }
  };

public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Buffer_manager(unsigned debug_level_,
                 Transport *transport,
                 size_t buffer_count = DEFAULT_BUFFER_COUNT)
    : _buffer_count(buffer_count),
      _transport(transport),
      _debug_level(debug_level_)
  {
    for (unsigned i = 0; i < _buffer_count; i++) {
      const auto len  = BUFFER_LEN;
      auto       base = alloc_base(len);
      assert(base);
      auto mr         = memory_registered_t(debug_level_, _transport, base, len, 0, 0);
      auto new_buffer = std::make_unique<buffer_t>(debug_level_, base, len, std::move(mr));
      _free.push_back(new_buffer.get());
      _buffers.emplace_back(std::move(new_buffer));
    }
    PLOG("%s %p allocated %lu buffers", __func__, static_cast<void *>(this), buffer_count);
  }
#pragma GCC diagnostic pop

  Buffer_manager()                       = delete;
  Buffer_manager(const Buffer_manager &) = delete;
  Buffer_manager &operator=(const Buffer_manager &) = delete;

  ~Buffer_manager()
  {
    /* EXCEPTION UNSAFE */
    PLOG("%s %p", __func__, static_cast<void *>(this));
    /* Note: A buffer_t does not own its iov->iov_base.
     */
    for (const auto &b : _buffers) {
      ::free(b->iov->iov_base);
    }
  }

  using completion_t = void (*)(void *, buffer_t *);
  buffer_t *allocate(completion_t completion_)
  {
    if (UNLIKELY(_free.empty())) throw Program_exception("Buffer_manager: no shard buffers remaining");
    auto iob = _free.back();
    assert(iob->flags == 0);
    _free.pop_back();
    CPLOG(3, "%s::%s %p (%lu free)", _cname, __func__, static_cast<const void *>(iob), _free.size());
    assert(iob);
    iob->reset_length();
    iob->set_completion(completion_);
    return iob;
  }

  void free(buffer_t *iob)
  {
    assert(iob);
    assert(iob->flags == 0);

    CPLOG(3, "%s::%s %p (%lu free)", _cname, __func__, static_cast<const void *>(iob), _free.size());
    iob->reset_length();
    iob->set_completion(nullptr);
    _free.push_back(iob);
  }

private:
  // static constexpr size_t buffer_len() { return BUFFER_LEN; }
  static auto alloc_base(std::size_t len) -> gsl::not_null<void *>
  {
    auto b = aligned_alloc(MiB(2), len);
    if (b == nullptr) {
      throw std::bad_alloc();
    }
    auto base = gsl::not_null<void *>(b);
    ::madvise(base, len, MADV_HUGEPAGE);
    ::mlock(base,len);
    std::memset(base, 0, len);
    return base;
  };

  using pool_t = component::IKVStore::pool_t;
  using key_t  = std::uint64_t;

  const size_t                           _buffer_count;
  gsl::not_null<Transport *>             _transport;
  std::vector<std::unique_ptr<buffer_t>> _buffers;
  std::vector<buffer_t *>                _free;
  unsigned                               _debug_level;
};

}  // namespace mcas

#endif
#endif
