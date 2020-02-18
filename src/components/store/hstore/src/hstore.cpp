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

#include "hstore.h"

#include "atomic_controller.h"
#include "hop_hash.h"
#include "is_locked.h"
#include "key_not_found.h"
#include "logging.h"
#include "perishable.h"
#include "persist_fixed_string.h"
#include "pool_path.h"

#include "hstore_nupm_types.h"
#include "persister_nupm.h"

#include <common/errors.h>
#include <common/exceptions.h>
#include <common/utils.h>

#include <city.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <tbb/scalable_allocator.h> /* scalable_free */
#pragma GCC diagnostic pop

#include <algorithm>
#include <cassert>
#include <cerrno>
#include <cstring> /* strerror, memcmp, memcpy */
#include <memory> /* unique_ptr */
#include <new>
#include <map> /* session set */
#include <mutex> /* thread safe use of libpmempool/obj */
#include <set>
#include <stdexcept> /* domain_error */

/*
 * To run hstore without PM, use variables USE_DRAM and NO_CLFLUSHOPT:
 *   USE_DRAM=24 NO_CLFLUSHOPT=1 DAX_RESET=1 ./dist/bin/kvstore-perf --test put --component hstore --path pools --pool_name foo --device_name /tmp/ --elements 1000000 --size 5000000000 --devices 0.0
 */

template<typename T>
  struct type_number;

template<> struct type_number<char> { static constexpr uint64_t value = 2; };

namespace
{
  constexpr bool option_DEBUG = false;
  namespace type_num
  {
    constexpr uint64_t persist = 1U;
    constexpr uint64_t heap = 2U;
  }
}

#if USE_CC_HEAP == 3 /* reconstituting allocator */
template<> struct type_number<impl::mod_control> { static constexpr std::uint64_t value = 4; };
#endif /* USE_CC_HEAP */

/* globals */

thread_local std::set<hstore::open_pool_t *> tls_cache = {};

auto hstore::locate_session(const Component::IKVStore::pool_t pid) -> open_pool_t *
{
  auto *const s = reinterpret_cast<open_pool_t *>(pid);
  auto it = tls_cache.find(s);
  if ( it == tls_cache.end() )
  {
    std::unique_lock<std::mutex> sessions_lk(_pools_mutex);
    auto ps = _pools.find(s);
    if ( ps == _pools.end() )
    {
      return nullptr;
    }
    it = tls_cache.insert(ps->second.get()).first;
  }
  return *it;
}

auto hstore::move_pool(const Component::IKVStore::pool_t pid) -> std::unique_ptr<open_pool_t>
{
  auto *const s = reinterpret_cast<open_pool_t *>(pid);

  std::unique_lock<std::mutex> sessions_lk(_pools_mutex);
  auto ps = _pools.find(s);
  if ( ps == _pools.end() )
    {
      throw API_exception(PREFIX "invalid pool identifier %p", __func__, s);
    }

  tls_cache.erase(s);
  auto s2 = std::move(ps->second);
  _pools.erase(ps);
  return s2;
}

hstore::hstore(const std::string &owner, const std::string &name, std::unique_ptr<Devdax_manager> &&mgr_)
  : _pool_manager(std::make_shared<pm>(owner, name, std::move(mgr_), option_DEBUG))
  , _pools_mutex{}
  , _pools{}
{
}

hstore::~hstore()
{
}

auto hstore::thread_safety() const -> int
{
  return thread_model;
}

int hstore::get_capability(const Capability cap) const
{
  switch (cap)
  {
  case Capability::POOL_DELETE_CHECK: /*< checks if pool is open before allowing delete */
    return false;
  case Capability::RWLOCK_PER_POOL:   /*< pools are locked with RW-lock */
    return false;
  case Capability::POOL_THREAD_SAFE:  /*< pools can be shared across multiple client threads */
    return is_thread_safe;
  default:
    return -1;
  }
}

#include "session.h"

auto hstore::create_pool(const std::string & name_,
                         const std::size_t size_,
                         std::uint32_t flags_,
                         const uint64_t expected_obj_count_) -> pool_t
try
{
  if ( option_DEBUG )
  {
    PLOG(PREFIX "pool_name=%s size %zu", LOCATION, name_.c_str(), size_);
  }
  try
  {
    _pool_manager->pool_create_check(size_);
  }
  catch ( const std::exception & )
  {
    return E_FAIL;
  }

  auto path = pool_path(name_);

  auto s =
    std::unique_ptr<session_t>(
      static_cast<session_t *>(
        _pool_manager->pool_create(path, size_, flags_ & ~(FLAGS_CREATE_ONLY|FLAGS_SET_SIZE), expected_obj_count_).release()
      )
    );

  auto p = s.get();
  std::unique_lock<std::mutex> sessions_lk(_pools_mutex);
  _pools.emplace(p, std::move(s));

  return reinterpret_cast<IKVStore::pool_t>(p);
}
catch ( const pool_error & )
{
  return flags_ & FLAGS_CREATE_ONLY
    ? static_cast<IKVStore::pool_t>(POOL_ERROR)
    : open_pool(name_, flags_ & ~FLAGS_SET_SIZE)
    ;
}

auto hstore::open_pool(const std::string &name_,
                       std::uint32_t flags) -> pool_t
{
  auto path = pool_path(name_);
  try {
    auto s = _pool_manager->pool_open(path, flags);
    auto p = static_cast<session_t *>(s.get());
    std::unique_lock<std::mutex> sessions_lk(_pools_mutex);
    _pools.emplace(p, std::move(s));
    return reinterpret_cast<IKVStore::pool_t>(p);
  }
  catch( const pool_error & ) {
    return Component::IKVStore::POOL_ERROR;
  }
  catch( const std::invalid_argument & ) {
    return Component::IKVStore::POOL_ERROR;
  }
}

status_t hstore::close_pool(const pool_t pid)
{
  std::string path;
  try
  {
    auto pool = move_pool(pid);
    if ( option_DEBUG )
    {
      PLOG(PREFIX "closed pool (%" PRIxIKVSTORE_POOL_T ")", LOCATION, pid);
    }
    _pool_manager->pool_close_check(path);
  }
  catch ( const API_exception &e )
  {
    PLOG(PREFIX "exception %s", LOCATION, e.cause());
    return e.error_code();
  }
  return S_OK;
}

status_t hstore::delete_pool(const std::string &name_)
{
  auto path = pool_path(name_);

  _pool_manager->pool_delete(path);
  if ( option_DEBUG )
  {
    PLOG(PREFIX "pool deleted: %s", LOCATION, name_.c_str());
  }
  return S_OK;
}

auto hstore::grow_pool(
  const pool_t pool
  , const std::size_t increment_size
  , std::size_t & reconfigured_size ) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  if ( ! session )
  {
    return Component::IKVStore::E_POOL_NOT_FOUND;
  }
  try
  {
    reconfigured_size = session->pool_grow(_pool_manager->devdax_manager(), increment_size);
  }
  catch ( const std::bad_alloc & )
  {
    return E_TOO_LARGE; /* would be E_NO_MEM, if it were in the interface */
  }
  return S_OK;
}

auto hstore::put(const pool_t pool,
                 const std::string &key,
                 const void * value,
                 const std::size_t value_len,
                 std::uint32_t flags) -> status_t
{
  if ( option_DEBUG ) {
    PLOG(
         PREFIX "(key=%s) (value=%.*s)"
         , LOCATION
         , key.c_str()
         , int(value_len)
         , static_cast<const char*>(value)
         );
    assert(0 < value_len);
  }

  if ( (flags & ~FLAGS_DONT_STOMP) != 0 )
  {
    return E_BAD_PARAM;
  }
  if ( value == nullptr )
  {
    return E_BAD_PARAM;
  }

  const auto session = static_cast<session_t *>(locate_session(pool));

  if ( session )
  {
    try
    {
      auto i = session->insert(key, value, value_len);

      return
        i.second                   ? S_OK
        : flags & FLAGS_DONT_STOMP ? int(Component::IKVStore::E_KEY_EXISTS)
        : (
            session->update_by_issue_41(
              key
              , value
              , value_len
              , std::get<0>(i.first->second).data()
              , std::get<0>(i.first->second).size())
            , S_OK
          )
        ;
    }
    catch ( const std::bad_alloc & )
    {
      return Component::IKVStore::E_TOO_LARGE; /* would be E_NO_MEM, if it were in the interface */
    }
    catch ( const std::invalid_argument & )
    {
      return E_NOT_SUPPORTED;
    }
    catch ( const impl::is_locked & )
    {
      return E_LOCKED; /* ... and is locked, so cannot be updated */
    }
  }
  else
  {
    return Component::IKVStore::E_POOL_NOT_FOUND;
  }
}

auto hstore::get_pool_regions(const pool_t pool, std::vector<::iovec>& out_regions) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  if ( ! session )
  {
    return Component::IKVStore::E_POOL_NOT_FOUND;
  }
  out_regions = _pool_manager->pool_get_regions(session->handle());
  return S_OK;
}

auto hstore::put_direct(const pool_t pool,
                        const std::string& key,
                        const void * value,
                        const std::size_t value_len,
                        memory_handle_t,
                        std::uint32_t flags) -> status_t
{
  return put(pool, key, value, value_len, flags);
}

auto hstore::get(const pool_t pool,
                 const std::string &key,
                 void*& out_value,
                 std::size_t& out_value_len) -> status_t
{
  const auto session = static_cast<const session_t *>(locate_session(pool));
  if ( ! session )
  {
    return Component::IKVStore::E_POOL_NOT_FOUND;
  }

  try
  {
    /* Although not documented, assume that non-zero
     * out_value implies that out_value_len holds
     * the buffer's size.
     */
    if ( out_value )
    {
      auto buffer_size = out_value_len;
      out_value_len = session->get(key, out_value, buffer_size);
      /*
       * It might be reasonable to
       *  a) fill the buffer and/or
       *  b) return the necessary size in out_value_len,
       * but neither action is documented, so we do not.
       */
      if ( buffer_size < out_value_len )
      {
        return E_INSUFFICIENT_BUFFER;
      }
    }
    else
    {
      try
      {
        auto r = session->get_alloc(key);
        out_value = std::get<0>(r);
        out_value_len = std::get<1>(r);
      }
      catch ( const std::bad_alloc & )
      {
        return E_TOO_LARGE; /* would be E_NO_MEM, if it were in the interface */
      }
    }
    return S_OK;
  }
  catch ( const impl::key_not_found & )
  {
    return Component::IKVStore::E_KEY_NOT_FOUND;
  }
}

auto hstore::get_direct(const pool_t pool,
                        const std::string & key,
                        void* out_value,
                        std::size_t& out_value_len,
                        Component::IKVStore::memory_handle_t) -> status_t
{
  const auto session = static_cast<const session_t *>(locate_session(pool));
  if ( ! session )
  {
    return Component::IKVStore::E_POOL_NOT_FOUND;
  }

  try
  {
    const auto buffer_size = out_value_len;
    out_value_len = session->get(key, out_value, buffer_size);
    if ( buffer_size < out_value_len )
    {
      return E_INSUFFICIENT_BUFFER;
    }
    return S_OK;
  }
  catch ( const impl::key_not_found & )
  {
    return Component::IKVStore::E_KEY_NOT_FOUND;
  }
}

auto hstore::get_attribute(
  const pool_t pool,
  const Attribute attr,
  std::vector<uint64_t>& out_attr,
  const std::string* key) -> status_t
{
  const auto session = static_cast<const session_t *>(locate_session(pool));
  if ( ! session )
  {
    return Component::IKVStore::E_POOL_NOT_FOUND;
  }

  switch ( attr )
  {
  case VALUE_LEN:
    if ( ! key )
    {
      return E_BAD_PARAM;
    }
    try
    {
      /* interface does not say what we do to the out_attr vector;
       * push_back is at least non-destructive.
       */
      out_attr.push_back(session->get_value_len(*key));
      return S_OK;
    }
    catch ( const impl::key_not_found & )
    {
      return Component::IKVStore::E_KEY_NOT_FOUND;
    }
    catch ( const std::bad_alloc & )
    {
      return E_TOO_LARGE; /* would be E_NO_MEM, if it were in the interface */
    }
    break;
  case AUTO_HASHTABLE_EXPANSION:
    try
    {
      out_attr.push_back(session->get_auto_resize());
      return S_OK;
    }
    catch ( const std::bad_alloc & )
    {
      return E_TOO_LARGE; /* would be E_NO_MEM, if it were in the interface */
    }
    break;
  case PERCENT_USED:
    out_attr.push_back(session->percent_used());
    return S_OK;
    break;
#if ENABLE_TIMESTAMPS
  case IKVStore::Attribute::WRITE_EPOCH_TIME:
    if ( ! key )
    {
      return E_BAD_PARAM;
    }
    try
    {
      out_attr.push_back(session->get_write_epoch_time(*key));
      return S_OK;
    }
    catch ( const impl::key_not_found & )
    {
      return Component::IKVStore::E_KEY_NOT_FOUND;
    }
    break;
#endif
  default:
    return E_NOT_SUPPORTED;
  }
  assert(nullptr == "missing return");
}

auto hstore::set_attribute(
  const pool_t pool,
  const Attribute attr
  , const std::vector<uint64_t> & value
  , const std::string *) -> status_t
{
  auto session = static_cast<session_t *>(locate_session(pool));
  if ( ! session )
  {
    return Component::IKVStore::E_POOL_NOT_FOUND;
  }
  switch ( attr )
  {
  case AUTO_HASHTABLE_EXPANSION:
    if ( value.size() < 1 )
    {
      return E_BAD_PARAM;
    }
    session->set_auto_resize(bool(value[0]));
    return S_OK;
  default:
    return E_NOT_SUPPORTED;
  }
  assert(nullptr == "missing return");
}

auto hstore::resize_value(
  const pool_t pool
  , const std::string &key
  , const std::size_t new_value_len
  , const std::size_t alignment
) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  try
  {
    return
      session
      ? ( session->resize_mapped(key, new_value_len, alignment), S_OK )
      : E_FAIL
      ;
  }
  /* how might this fail? Out of memory, key not found, not locked, read locked */
  catch ( const std::invalid_argument & )
  {
    return E_BAD_ALIGNMENT; /* not properly locked */
  }
  catch ( const std::bad_alloc & )
  {
    return E_TOO_LARGE; /* would be E_NO_MEM, if it were in the interface */
  }
  catch ( const impl::key_not_found & )
  {
    return E_KEY_NOT_FOUND; /* key not found */
  }
  catch ( const impl::is_locked & )
  {
    return E_LOCKED; /* could not get unique lock (?) */
  }
}

auto hstore::lock(
  const pool_t pool
  , const std::string &key
  , lock_type_t type
  , void *& out_value
  , std::size_t & out_value_len
  , Component::IKVStore::key_t& out_key
  , const char ** out_key_ptr
) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  if(!session) return E_FAIL;

  auto r = session->lock(key, type, out_value, out_value_len);

  out_key = r.key;
  if ( out_key_ptr )
  {
    *out_key_ptr = r.key_ptr;
  }
  /* If lock valid, safe to provide access to the key */
  if ( r.key != Component::IKVStore::KEY_NONE )
  {
    out_value = r.value;
    out_value_len = r.value_len;
  }

  switch ( r.state )
  {
  case lock_result::e_state::created:
    /* Returns undocumented "E_LOCKED" if lock not held */
    return r.key == Component::IKVStore::KEY_NONE ? E_LOCKED : S_OK_CREATED;
  case lock_result::e_state::not_created:
    return E_KEY_NOT_FOUND;
  case lock_result::e_state::extant:
    /* Returns undocumented "E_LOCKED" if lock not held */
    return r.key == Component::IKVStore::KEY_NONE ? E_LOCKED : S_OK;
  case lock_result::e_state::creation_failed:
    /* should not happen. */
    return E_KEY_EXISTS;
  }
  return E_KEY_NOT_FOUND;
}


auto hstore::unlock(const pool_t pool,
                    Component::IKVStore::key_t key_) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  return
    session
    ? session->unlock(key_)
    : Component::IKVStore::E_POOL_NOT_FOUND
    ;
}

auto hstore::erase(const pool_t pool,
                   const std::string &key
                   ) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  return session
    ? session->erase(key)
    : Component::IKVStore::E_POOL_NOT_FOUND
    ;
}

std::size_t hstore::count(const pool_t pool)
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  if ( ! session )
  {
    return Component::IKVStore::E_POOL_NOT_FOUND;
  }

  return session->count();
}

void hstore::debug(const pool_t, const unsigned cmd, const uint64_t arg)
{
  switch ( cmd )
    {
    case 0:
      perishable::enable(bool(arg));
      break;
    case 1:
      perishable::reset(arg);
      break;
    case 2:
      {
      }
      break;
    default:
      break;
    };
}

auto hstore::map(
                 pool_t pool,
                 std::function
                 <
                   int(const void * key, std::size_t key_len,
                       const void * val, std::size_t val_len)
                 > f_
                 ) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));

  return session
    ? ( session->map(f_), S_OK )
    : int(Component::IKVStore::E_POOL_NOT_FOUND)
    ;
}

auto hstore::map(
  pool_t pool_,
  std::function<
    int(
      const void * key,
      std::size_t key_len,
      const void * value,
      std::size_t value_len,
      tsc_time_t timestamp
    )
  > f_,
  epoch_time_t t_begin_,
  epoch_time_t t_end_
) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool_));

  return session
    ? ( session->map(f_, t_begin_, t_end_) ? S_OK : E_NOT_SUPPORTED )
    : int(Component::IKVStore::E_POOL_NOT_FOUND)
    ;
}

auto hstore::map_keys(
                 pool_t pool,
                 std::function
                 <
                   int(const std::string &key)
                 > f_
                 ) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));

  return session
    ? ( session->map([&f_] (const void * key, std::size_t key_len,
                            const void *, std::size_t) -> int
                     {
                       f_(std::string(static_cast<const char*>(key), key_len));
                       return 0;
                     }), S_OK )
    : int(Component::IKVStore::E_POOL_NOT_FOUND)
    ;
}

auto hstore::free_memory(void * p) -> status_t
{
  scalable_free(p);
  return S_OK;
}

auto hstore::atomic_update(
                           const pool_t pool
                           , const std::string& key
                           , const std::vector<IKVStore::Operation *> &op_vector
                           , const bool take_lock) -> status_t
try
{
  const auto update_method = take_lock ? &session_t::lock_and_atomic_update : &session_t::atomic_update;
  const auto session = static_cast<session_t *>(locate_session(pool));
  return
    session
    ? ( (session->*update_method)(key, op_vector), S_OK )
    : int(Component::IKVStore::E_POOL_NOT_FOUND)
    ;
}
catch ( const std::bad_alloc & )
{
  return E_TOO_LARGE; /* would be E_NO_MEM, if it were in the interface */
}
catch ( const std::invalid_argument & )
{
  return E_NOT_SUPPORTED;
}
catch ( const impl::key_not_found & )
{
  return E_KEY_NOT_FOUND;
}
catch ( const impl::is_locked & )
{
  return E_LOCKED; /* ... is locked, so cannot be updated */
}
catch ( const std::system_error & )
{
  return E_FAIL;
}

auto hstore::swap_keys(
  const pool_t pool
  , const std::string key0
  , const std::string key1
) -> status_t
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  return
    session
    ? session->swap_keys(key0, key1)
    : int(Component::IKVStore::E_POOL_NOT_FOUND)
    ;
}

auto hstore::allocate_pool_memory(
  const pool_t pool,
  const size_t size,
  const size_t alignment,
  void * & out_addr
) -> status_t
try
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  return
    session
    ? ( out_addr = session->allocate_memory(size, alignment), S_OK )
    : int(Component::IKVStore::E_POOL_NOT_FOUND)
    ;
}
catch ( const std::invalid_argument & )
{
  return E_BAD_ALIGNMENT; /* ... probably */
}
catch ( const std::bad_alloc & )
{
  return E_TOO_LARGE; /* would be E_NO_MEM, if it were in the interface */
}

auto hstore::free_pool_memory(
  const pool_t pool,
  const void* const addr,
  const size_t size
) -> status_t
try
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  return
    session
    ? ( session->free_memory(addr, size), S_OK )
    : int(Component::IKVStore::E_POOL_NOT_FOUND)
    ;
}
catch ( const API_exception & ) /* bad pointer */
{
  return E_INVAL;
}
catch ( const std::exception & )
{
  return E_FAIL;
}

auto hstore::open_pool_iterator(pool_t pool) -> pool_iterator_t
{
  auto session = static_cast<session_t *>(locate_session(pool));
  return
    session
    ? session->open_iterator()
    : nullptr
    ;
}

status_t hstore::deref_pool_iterator(
  const pool_t pool
  , pool_iterator_t iter
  , const epoch_time_t t_begin
  , const epoch_time_t t_end
  , pool_reference_t & ref
  , bool & time_match
  , bool increment
)
{
  const auto session = static_cast<session_t *>(locate_session(pool));
  return
    session
    ? session->deref_iterator(
        iter
        , t_begin
        , t_end
        , ref
        , time_match
        , increment
      )
    : E_INVAL
    ;
}

status_t  hstore::close_pool_iterator(
  const pool_t pool
  , pool_iterator_t iter
)
{
  auto session = static_cast<session_t *>(locate_session(pool));
  return
    session
    ? session->close_iterator(iter)
    : E_INVAL
    ;
}

