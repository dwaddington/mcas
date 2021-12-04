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
#ifndef __BUFFER_MGR_H__
#define __BUFFER_MGR_H__

#ifdef __cplusplus

#include <common/chksum.h>
#include <common/delete_copy.h>
#include <common/logging.h>
#include <common/moveable_ptr.h>
#include <common/utils.h> /* MiB, UNLIKELY */
#include <api/components.h>
#include <api/fabric_itf.h>
#include <api/kvstore_itf.h>
#include <api/registrar_memory_direct.h> /* Opaque_memory_region */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include <rdma/fabric.h> /* fi_context2 */
#pragma GCC diagnostic pop
#include <sys/mman.h>

#include "free_deleter.h"
#include "mcas_config.h"
#include "memory_registered.h"
#include <gsl/pointers> /* not_null */
#include <cassert>
#include <cstring>      /* memset */
#include <memory>       /* unique_ptr */

namespace
{
  inline auto alloc_base(std::size_t len) -> gsl::not_null<void *>
  {
    auto b = ::aligned_alloc(MiB(2), len);
    if (b == nullptr) {
      throw std::bad_alloc();
    }
    ::memset(b, 0, len);
    return gsl::not_null<void *>(b);
  }
}

namespace mcas
{
/* Buffer_manager is always specialized as Buffer_manager<IFabric_memory_control>,
 * so the template is not necessary unless one anticipates specializing from
 * something which works kind of, but not exactly, like IFabric_memory_control.
 */
template <class Memory>
class Buffer_manager : private common::log_source {

public:
  static constexpr size_t BUFFER_LEN           = MiB(2); /* corresponds to huge page see below */
  using memory_registered_t                    = memory_registered<Memory>;

  using memory_region_t = typename Memory::memory_region_t;

  struct iov_mem_lock
  {
  private:
    common::moveable_ptr<::iovec> _iov;
  public:
    iov_mem_lock(::iovec *iov_)
      : _iov(iov_)
    {
      ::madvise(_iov->iov_base, _iov->iov_len, MADV_HUGEPAGE);
      ::mlock(_iov->iov_base, _iov->iov_len);
    }
    iov_mem_lock(iov_mem_lock &&) noexcept = default;
    ~iov_mem_lock()
    {
      if ( _iov )
      {
        ::munlock(_iov->iov_base, _iov->iov_len);
      }
    }
  };

  /* Although client tried to hide it with a reinterpret_cast, buffer_base is
   used as a component::memory_region_t */

  struct buffer_base : public component::Registrar_memory_direct::Opaque_memory_region, private common::log_source
  {
  private:
    common::byte_span _span;
    memory_registered_t _region;
    const unsigned magic;

  public:
    buffer_base(unsigned    debug_level_,
             Memory * transport_,
             common::byte_span span_
    )
      : common::log_source(debug_level_)
      , _span(span_)
      , _region(memory_registered_t(debug_level_, transport_, common::make_const_byte_span(span_), 0 /* proposed key */, 0 /* flags */))
      , magic(0xC0FFEE)
    {
      if ( size(span_) <= 1 )
      {
        throw std::domain_error("buffer length too small");
      }
      CFLOGM(1, "{} transport {} region {}", common::p_fmt(this),
            common::p_fmt(transport()), common::p_fmt(region()));
    }

    void *get_desc() const { return _region.desc(); }

    DELETE_COPY(buffer_base);

  protected:
    ~buffer_base()
    {
      CFLOGM(1, "{} transport {} region {}", common::p_fmt(this),
            common::p_fmt(transport()), common::p_fmt(region()));
    }

  public:
    size_t original_length() const { return ::size(_span); }
    inline gsl::not_null<void *> base() const { return ::base(_span); }
    auto region() const { return _region.mr(); }
    Memory * transport() const { return _region.memory_control(); }

#if 0
    /* unused */
    inline bool check_magic() const { return magic == 0xC0FFEE; }
    unsigned int crc32() const { return common::chksum32(_base, _length); }
#endif
  };

  struct buffer_internal
    : protected fi_context2
    , private std::unique_ptr<void, free_deleter>
    , public buffer_base
  {
    fi_context2 *to_context() { return this; }
    static buffer_internal *to_buffer(fi_context2 *c) { return static_cast<buffer_internal *>(c); }
    ::iovec iov[2];
    void *desc[2];
    iov_mem_lock _ml;
    using completion_t = void (*)(void *, buffer_internal *);
    completion_t   completion_cb;
    void *         value_adjunct;
    inline void set_completion(completion_t completion_) { completion_cb = completion_; }
    void set_completion(completion_t completion_, void *value_adjunct_)
    {
      completion_cb = completion_;
      value_adjunct = value_adjunct_;
    }
    buffer_internal(unsigned debug_level_,
             Memory *         transport_,
             size_t              length_
    )
      : std::unique_ptr<void, free_deleter>(alloc_base(length_))
      , buffer_base(debug_level_, transport_, common::make_byte_span(std::unique_ptr<void, free_deleter>::get(), length_))
      , iov{{this->base(), this->original_length()}, {nullptr, 0}}
      , desc{this->get_desc(), nullptr}
      , _ml(&this->iov[0])
      , completion_cb(nullptr)
      , value_adjunct(nullptr)
    {
#if 0
FLOG("ctl {} buffer {}.{:x}", static_cast<void*>(this), static_cast<void *>(get()), length_);
#endif
    }

    DELETE_COPY(buffer_internal);

    void set_length(size_t s) { iov[0].iov_len = s; }

    inline void reset_length()
    {
      assert(this->original_length() > 1);
      this->iov[0].iov_len = this->original_length();
      value_adjunct = nullptr;
      completion_cb = nullptr;
    }

    size_t length() const { return iov[0].iov_len; }

    unsigned int crc32() const { return common::chksum32(iov[0].iov_base, iov[0].iov_len); }
  };

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Buffer_manager(unsigned debug_level_,
                 Memory *transport,
			size_t buffer_count
		)
    : common::log_source(debug_level_),
      _buffer_count(buffer_count),
      _transport(transport)
  {
    for (unsigned i = 0; i < _buffer_count; i++) {
      const auto len  = BUFFER_LEN;
      _buffers.emplace_back(std::make_unique<buffer_internal>(debug_level_, _transport, len));
      _free.push_back(_buffers.back().get());
    }
    PLOG("%s %p allocated %lu buffers", __func__, common::p_fmt(this), buffer_count);
  }
#pragma GCC diagnostic pop

  Buffer_manager(Buffer_manager &&) noexcept = default;

  ~Buffer_manager()
  {
  }

  using completion_t = void (*)(void *, buffer_internal *);

  gsl::not_null<buffer_internal *> allocate(completion_t completion_)
  {
    if (UNLIKELY(_free.empty())) throw Program_exception("Buffer_manager: no shard buffers remaining");
    gsl::not_null<buffer_internal *> iob = _free.back();
    _free.pop_back();
    CFLOGM(3, "{} ({} free)", common::p_fmt(iob), _free.size());
    iob->reset_length();
    iob->set_completion(completion_);
    return iob;
  }

  void free(gsl::not_null<buffer_internal *> iob)
  {
    CFLOG(3, "{} ({} free)", common::p_fmt(iob), _free.size());
    iob->reset_length();
    iob->set_completion(nullptr);
    _free.push_back(iob);
  }

private:
  // static constexpr size_t buffer_len() { return BUFFER_LEN; }

  using pool_t = component::IKVStore::pool_t;
  using key_t  = std::uint64_t;

  const size_t                           _buffer_count;
  gsl::not_null<Memory *>             _transport;
  std::vector<std::unique_ptr<buffer_internal>> _buffers;
  std::vector<buffer_internal *>        _free;
};
}  // namespace mcas

#endif
#endif
