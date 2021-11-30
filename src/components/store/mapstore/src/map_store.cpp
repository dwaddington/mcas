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

#include "map_store.h"

#include <api/kvstore_itf.h>
#include <city.h>
#include <common/exceptions.h>
#include <common/cycles.h>
#include <common/env.h>
#include <common/less_getter.h>
#include <common/rwlock.h>
#include <common/to_string.h>
#include <common/utils.h>
#include <common/memory.h>
#include <common/str_utils.h>
#include <fcntl.h>
#include <gsl/pointers>
#include <nupm/region_descriptor.h>
#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cerrno>
#include <cmath>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>

#define DEFAULT_ALIGNMENT 8
#define SINGLE_THREADED
#define MIN_POOL (1ULL << DM_REGION_LOG_GRAIN_SIZE)

#include "mm_plugin_itf.h"

/*
 * Outside of "factory", there are two uses of new/delete: RWLock and Iteratora.
 * These exist because they
 */
using namespace component;
using namespace common;

struct Value_type {
  Value_type() : _ptr(nullptr), _length(0), _value_lock(nullptr), _tsc() {
  }

  Value_type(void* ptr, size_t length, common::RWLock * value_lock) :
    _ptr(ptr), _length(length), _value_lock(value_lock), _tsc() {
  }
  void * _ptr;
  size_t _length;
  common::RWLock * _value_lock; /*< read write lock */
  common::tsc_time_t _tsc;
};


class Key_hash;

/* allocator types using MM_plugin_cxx_allocator (see mm_plugin_itf.h) */
using aac_t = MM_plugin_cxx_allocator<char>;
using string_t = std::basic_string<common::byte, std::char_traits<common::byte>, aac_t>;
using aam_t = MM_plugin_cxx_allocator<std::pair<string_t, Value_type>>;
using aal_t = MM_plugin_cxx_allocator<common::RWLock>;
using map_t = std::unordered_map<string_t, Value_type, Key_hash, std::equal_to<string_t>, aam_t>;


static size_t choose_alignment(size_t size)
{
  if((size >= 4096) && (size % 4096 == 0)) return 4096;
  if((size >= 64) && (size % 64 == 0)) return 64;
  if((size >= 16) && (size % 16 == 0)) return 16;
  if((size >= 8) && (size % 8 == 0)) return 8;
  if((size >= 4) && (size % 4 == 0)) return 4;
  return 1;
}

class Key_hash {
public:
  size_t operator()(Map_store::string_view_key k) const {
    return CityHash64(common::pointer_cast<char>(k.data()), k.size());
  }
};

namespace
{
int init_map_lock_mask()
{
  /* env variable USE_ODP to indicate On Demand Paging may be used
     and therefore mapped memory need not be pinned */
  bool odp = common::env_value("USE_ODP", true);
  return odp ? 0 : MAP_LOCKED;
}

const int effective_map_locked = init_map_lock_mask();
}

struct region_memory
  : private ::iovec
{
private:
  int _debug_level;
public:
  region_memory(unsigned debug_level_, void *p, std::size_t size)
    : ::iovec{p, size}
    , _debug_level(debug_level_)
  {}
  unsigned debug_level() const { return _debug_level; }
  virtual ~region_memory() {}
  using ::iovec::iov_base;
  using ::iovec::iov_len;
};

struct region_memory_mmap
  : public region_memory
{
  region_memory_mmap(unsigned debug_level_, void *p, std::size_t size)
    : region_memory(debug_level_, p, size)
  {}
  ~region_memory_mmap() override
  {
    CFLOGM(1, "freeing region memory ({},{})", iov_base, iov_len);
    if(::munmap(iov_base, iov_len))
    {
      FLOGM("munmap of region memory {}.{} failed", iov_base, iov_len);
    }
  }
};

struct region_memory_numa
  : public region_memory
{
  region_memory_numa(unsigned debug_level_, void *p, std::size_t size)
    : region_memory(debug_level_, p, size)
  {}
  ~region_memory_numa() override
  {
    CFLOGM(1, "freeing region memory ({},{})", iov_base, iov_len);
    numa_free(iov_base, iov_len);
  }
};

namespace
{
  int open_region_file(const string_view pool_name)
  {
    char * backing_store_dir = ::getenv("MAPSTORE_BACKING_STORE_DIR");
    if ( backing_store_dir )
    {
      /* create file */
      struct stat st;
      if (::stat(backing_store_dir,&st) == 0) {
        if (st.st_mode & (S_IFDIR != 0)) {
          using namespace std::string_literals;
          std::string filename = backing_store_dir + "/mapstore_backing_"s + std::string(pool_name) + ".dat";

          ::mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
          FINF("backing file ({}))", filename);
          return ::open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, mode);
        }
      }
    }
    return -1;
  }
}

/* allow find of unique_ptr by ptr value in a set or map */
template<typename T>
	struct compare_unique_ptr
	{
		using is_transparent = void;

		bool operator()(const std::unique_ptr<T> &a, const std::unique_ptr<T> &b) const
		{
			return a.get() < b.get();
		}

		bool operator()(const T *a, const std::unique_ptr<T> &b) const
		{
			return a < b.get();
		}

		bool operator()(const std::unique_ptr<T> &a, const T *b) const
		{
			return a.get() < b;
		}
	};

/**
 * Pool instance class
 *
 */
class Pool_instance {

private:

  unsigned debug_level() const { return _debug_level; }

  std::unique_ptr<region_memory> allocate_region_memory(size_t size);

  static const Pool_instance *checked_pool(const Pool_instance * pool)
  {
    if ( pool == nullptr )
      throw Logic_exception("checked_pool bad param");

    return pool;
  }

  class Iterator {
  public:
    explicit Iterator(const Pool_instance * pool)
      : _pool(checked_pool(pool)),
        _mark(_pool->writes()),
        _iter(_pool->_map->begin()),
        _end(_pool->_map->end())
    {}

    bool is_end() const { return _iter == _end; }
    bool check_mark(uint32_t writes) const { return _mark == writes; }

    const Pool_instance * _pool;
    uint32_t              _mark;
    map_t::const_iterator _iter;
    map_t::const_iterator _end;
  };

  using string_view = Map_store::string_view;
  using string_view_key = Map_store::string_view_key;
  using string_view_value = Map_store::string_view_value;
public:
  Pool_instance(const unsigned debug_level,
                const common::string_view mm_plugin_path,
                const common::string_view name_,
                size_t nsize,
                int numa_node_,
                unsigned flags_)
    : _debug_level(debug_level),
      _ref_mutex{},
      _nsize(0),
      _numa_node(numa_node_),
      _name(name_),
      _fdout(open_region_file(_name)),
      _regions{},
      _mm_plugin_mutex{},
      _mm_plugin(mm_plugin_path), /* plugin path for heap allocator */
      _map_lock{},
      _map(std::make_unique<map_t>(aam_t(_mm_plugin))),
      _flags{flags_},
      _iterators{},
      _writes{}
  {
    grow_pool(nsize < MIN_POOL ? MIN_POOL : nsize, _nsize);
    CFLOGM(1, "new pool instance");
  }
  Pool_instance(const Pool_instance &) = delete;
  Pool_instance &operator=(const Pool_instance &) = delete;

  ~Pool_instance()
  {
    CFLOGM(1, "freeing regions for pool ({})", _name);

    if ( 0 <= _fdout )
    {
      syncfs(_fdout);
      close(_fdout);
    }
  }
  const std::string& name() const { return _name; }

private:

  unsigned                   _debug_level;
  std::mutex                 _ref_mutex;
  size_t                     _nsize; /*< order important */
  int                        _numa_node;
  std::string                _name; /*< pool name */
  int                        _fdout;
  std::vector<std::unique_ptr<region_memory>> _regions; /*< regions supporting pool */
  std::mutex                 _mm_plugin_mutex;
  MM_plugin_wrapper          _mm_plugin;
  /* use a pointer so we can make sure it gets stored before memory is freed */
  common::RWLock             _map_lock; /*< read write lock */
  std::unique_ptr<map_t>     _map; /*< hash table based map */
  unsigned int               _flags;
  /* Note: Using Iterator * as a comparable is a slight cheat, because pointers
   * from separate allocations are, strictly speaking, not comparable.
   */
  std::set<std::unique_ptr<Iterator>, less_getter<std::unique_ptr<Iterator>>> _iterators;
  /*
    We use this counter to see if new writes have come in
    during an iteration.  This is essentially an optmistic
    locking strategy.
  */
  uint32_t _writes __attribute__((aligned(4)));

  inline void write_touch() { _writes++; }
  inline uint32_t writes() const { return _writes; }

  /* allocator adapters over reconstituting allocator */
  aac_t aac{_mm_plugin}; /* for keys */
  aal_t aal{_mm_plugin}; /* for locks */

public:
  status_t put(string_view_key key, const void *value,
               const size_t value_len, unsigned int flags);

  status_t get(string_view_key key, void *&out_value, size_t &out_value_len);

  status_t get_direct(string_view_key key, void *out_value,
                      size_t &out_value_len);

  status_t get_attribute(const IKVStore::Attribute attr,
                         std::vector<uint64_t> &out_attr,
                         const string_view_key key);

  status_t swap_keys(const string_view_key key0,
                     const string_view_key key1);

  status_t resize_value(string_view_key key,
                        const size_t new_size,
                        const size_t alignment);

  status_t lock(string_view_key key,
                IKVStore::lock_type_t type,
                void *&out_value,
                size_t &inout_value_len,
                size_t alignment,
                IKVStore::key_t& out_key,
                const char ** out_key_ptr);

  status_t unlock(IKVStore::key_t key_handle);

  status_t erase(string_view_key key);

  size_t count();

  status_t map(std::function<int(string_view_key key,
                                 string_view_value value)> function);

  status_t map(std::function<int(string_view_key key,
                                 string_view_value value,
                                 const common::tsc_time_t timestamp)> function,
                                 const common::epoch_time_t t_begin,
                                 const common::epoch_time_t t_end);

  status_t map_keys(std::function<int(string_view_key key)> function);

  status_t get_pool_regions(nupm::region_descriptor::address_map_t &out_regions);

  status_t grow_pool(const size_t increment_size, size_t &reconfigured_size);

  status_t free_pool_memory(const void *addr, const size_t size = 0);

  status_t allocate_pool_memory(const size_t size,
                                const size_t alignment,
                                void *&out_addr);

  IKVStore::pool_iterator_t open_pool_iterator();

  status_t deref_pool_iterator(IKVStore::pool_iterator_t iter,
                               const common::epoch_time_t t_begin,
                               const common::epoch_time_t t_end,
                               IKVStore::pool_reference_t& ref,
                               bool& time_match,
                               bool increment = true);

  status_t close_pool_iterator(IKVStore::pool_iterator_t iter);

};

struct Pool_session {
  Pool_session(gsl::not_null<std::shared_ptr<Pool_instance>> ph) : pool(ph) {
  }

  ~Pool_session() {
  }

  bool check() const { return canary == 0x45450101; }
  gsl::not_null<std::shared_ptr<Pool_instance>> pool;
  const unsigned canary = 0x45450101;
};

struct tls_cache_t {
  Pool_session *session;
};

std::mutex _pool_sessions_lock; /* guards both _pool_sessions and _pools */
/* ERROR?: should Pool_sessions really shared across Map_store instances? */
using pools_type = std::map<std::string, std::shared_ptr<Pool_instance>, std::less<>>;
pools_type _pools; /*< existing pools */
std::set<std::unique_ptr<Pool_session>> _pool_sessions;

static __thread tls_cache_t tls_cache = {nullptr};

using Std_lock_guard = std::lock_guard<std::mutex>;

std::set<std::unique_ptr<Pool_session>>::iterator find_session(Pool_session *session)
{
  return
    std::find_if(
      _pool_sessions.begin(), _pool_sessions.end()
      , [session] (const std::unique_ptr<Pool_session> &i) { return i.get() == session; }
    );
}

Pool_session *get_session(const IKVStore::pool_t pid)
{
  auto session = reinterpret_cast<Pool_session *>(pid);
  assert(session);

  if (session != tls_cache.session) {
    Std_lock_guard g(_pool_sessions_lock);

    auto it = find_session(session);
    if (it == _pool_sessions.end()) return nullptr;

    tls_cache.session = session;
  }

  return session;
}

status_t Pool_instance::put(string_view_key key,
			    const void *value,
			    const size_t value_len,
			    unsigned int flags)
{
  if (!value || !value_len || value_len > _nsize) {
    PWRN("Map_store: invalid parameters (value=%p, value_len=%lu)", value, value_len);
    return E_INVAL;
  }

#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock, RWLock_guard::WRITE);
#endif

  write_touch(); /* this could be early, but over-conservative is ok */

  std::lock_guard g{_mm_plugin_mutex}; /* aac, _mm_plugin.aligned_allocate, aal */
  string_t k(key.data(), key.length(), aac);

  auto i = _map->find(k);

  if (i != _map->end()) {

    if (flags & IKVStore::FLAGS_DONT_STOMP) {
      PWRN("put refuses to stomp (%*.s)", int(key.size()), common::pointer_cast<char>(common::pointer_cast<char>(key.data())));
      return IKVStore::E_KEY_EXISTS;
    }

    /* take lock */
    int rc;
    if((rc = (*_map)[k]._value_lock->write_trylock()) != 0) {
      PWRN("put refuses, already locked (%d)",rc);
      assert(rc == EBUSY);
      return E_LOCKED;
    }

    auto &p = i->second;

    if (p._length == value_len) {
      memcpy(p._ptr, value, value_len);
    }
    else {
      /* different size, reallocate */
      auto p_to_free = p._ptr;
      auto len_to_free = p._length;

      CFLOGM(3, "allocating {} bytes alignment {}", value_len, choose_alignment(value_len));

      if(_mm_plugin.aligned_allocate(value_len, choose_alignment(value_len),&p._ptr) != S_OK)
        throw General_exception("plugin aligned_allocate failed");

      memcpy(p._ptr, value, value_len);

      /* update entry */
      i->second._length = value_len;
      i->second._ptr = p._ptr;

      /* release old memory*/
      try {  _mm_plugin.deallocate(&p_to_free, len_to_free);      }
      catch(...) {  throw Logic_exception("unable to release old value memory");   }
    }

    wmb();
    i->second._tsc.update(); /* update timestamp */

    /* release lock */
    (*_map)[k]._value_lock->unlock();
  }
  else { /* key does not already exist */

    CFLOGM(3, "allocating {} bytes alignment {}", value_len, choose_alignment(value_len));

    void * buffer = nullptr;
    if(_mm_plugin.aligned_allocate(value_len, choose_alignment(value_len), &buffer) != S_OK)
      throw General_exception("memory plugin aligned_allocate failed");

    memcpy(buffer, value, value_len);
    //    common::RWLock * p = new (aal.allocate(1, DEFAULT_ALIGNMENT)) common::RWLock();
    common::RWLock * p = new (aal.allocate(1)) common::RWLock();

    /* create map entry */
    _map->emplace(k, Value_type{buffer, value_len, p});
  }

  return S_OK;
}

status_t Pool_instance::get(const string_view_key key,
                            void *&out_value,
                            size_t &out_value_len)
{
  common::string_view key_svc(common::pointer_cast<char>(key.data()), key.size());
  CFLOGM(1, "get({},{},{})", key_svc, out_value, out_value_len);

#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock);
#endif
  std::lock_guard g{_mm_plugin_mutex}; /* aac */
  string_t k(key.data(), aac);
  auto i = _map->find(k);

  if (i == _map->end()) return IKVStore::E_KEY_NOT_FOUND;

  out_value_len = i->second._length;

  /* result memory allocated with ::malloc */
  out_value = malloc(out_value_len);

  if ( out_value == nullptr )  {
    PWRN("Map_store: malloc failed");
    return IKVStore::E_TOO_LARGE;
  }

  memcpy(out_value, i->second._ptr, i->second._length);
  return S_OK;
}

status_t Pool_instance::get_direct(const string_view_key key,
                                   void *out_value,
                                   size_t &out_value_len)
{
  common::string_view key_svc(common::pointer_cast<char>(key.data()), key.size());
  CFLOGM(1, " key=({}) ", key_svc);

  if (out_value == nullptr || out_value_len == 0)
    throw API_exception("invalid parameter");

#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock);
#endif
  std::lock_guard g{_mm_plugin_mutex}; /* aac */
  string_t k(key.data(), key.size(), aac);
  auto i = _map->find(k);

  if (i == _map->end()) {
    if (debug_level()) PERR("Map_store: error key not found");
    return IKVStore::E_KEY_NOT_FOUND;
  }

  if (out_value_len < i->second._length) {
    if (debug_level()) PERR("Map_store: error insufficient buffer");

    return E_INSUFFICIENT_BUFFER;
  }

  out_value_len = i->second._length; /* update length */
  memcpy(out_value, i->second._ptr, i->second._length);

  return S_OK;
}

status_t Pool_instance::get_attribute(const IKVStore::Attribute attr,
                                      std::vector<uint64_t> &out_attr,
                                      const string_view_key key)
{
  switch (attr) {
  case IKVStore::Attribute::MEMORY_TYPE: {
    out_attr.push_back(IKVStore::MEMORY_TYPE_DRAM);
    break;
  }
  case IKVStore::Attribute::VALUE_LEN: {
    if (key.data() == nullptr) return E_INVALID_ARG;
#ifndef SINGLE_THREADED
    RWLock_guard guard(map_lock);
#endif
    std::lock_guard g{_mm_plugin_mutex}; /* aac */
    string_t k(key.data(), key.size(), aac);
    auto i = _map->find(k);
    if (i == _map->end()) return IKVStore::E_KEY_NOT_FOUND;
    out_attr.push_back(i->second._length);
    break;
  }
  case IKVStore::Attribute::WRITE_EPOCH_TIME: {
#ifndef SINGLE_THREADED
    RWLock_guard guard(map_lock);
#endif
    std::lock_guard g{_mm_plugin_mutex}; /* aac */
    string_t k(key.data(), key.size(), aac);
    auto i = _map->find(k);
    if (i == _map->end()) return IKVStore::E_KEY_NOT_FOUND;
    out_attr.push_back(boost::numeric_cast<uint64_t>(i->second._tsc.to_epoch().seconds()));
    break;
  }
  case IKVStore::Attribute::COUNT: {
    out_attr.push_back(_map->size());
    break;
  }
  default:
    return E_INVALID_ARG;
  }

  return S_OK;
}



status_t Pool_instance::swap_keys(const string_view_key key0,
                                  const string_view_key key1)
{
  std::lock_guard g{_mm_plugin_mutex}; /* aac, twice */
  string_t k0(key0.data(), key0.length(), aac);
  auto i0 = _map->find(k0);
  if(i0 == _map->end()) return IKVStore::E_KEY_NOT_FOUND;

  string_t k1(key1.data(), key1.length(), aac);
  auto i1 = _map->find(k1);
  if(i1 == _map->end()) return IKVStore::E_KEY_NOT_FOUND;

  /* lock both k-v pairs */
  auto& left = i0->second;
  if(left._value_lock->write_trylock() != 0)
    return E_LOCKED;

  auto& right = i1->second;
  if(right._value_lock->write_trylock() != 0) {
    left._value_lock->unlock();
    return E_LOCKED;
  }

  /* swap keys */
  auto tmp_ptr = left._ptr;
  auto tmp_len = left._length;
  left._ptr = right._ptr;
  left._length = right._length;
  right._ptr = tmp_ptr;
  right._length = tmp_len;

  /* release locks */
  left._value_lock->unlock();
  right._value_lock->unlock();

  return S_OK;
}

status_t Pool_instance::lock(const string_view_key key,
                             IKVStore::lock_type_t type,
                             void *&out_value,
                             size_t &inout_value_len,
                             size_t alignment,
                             IKVStore::key_t& out_key,
                             const char ** out_key_ptr)
{

  void *buffer = nullptr;
  std::lock_guard g{_mm_plugin_mutex}; /* aac, and later _mm_plugin.aligned_allocate, aal */
  string_t k(key.data(), key.size(), aac);
  bool created = false;
  common::string_view key_svc(common::pointer_cast<char>(key.data()), key.size());

  auto i = _map->find(k);

  CFLOGM(1, "lock looking for key:({})", key_svc);

  if (i == _map->end()) { /* create value */

    write_touch();

    /* lock API has semantics of create on demand */
    if (inout_value_len == 0) {
      out_key = IKVStore::KEY_NONE;
      CFLOGM(1, "could not on-demand allocate without length:({}) {}", key_svc, inout_value_len);
      return IKVStore::E_KEY_NOT_FOUND;
    }


    CFLOGM(1, "is on-demand allocating:({}) {}", key_svc, inout_value_len);

    if(alignment == 0)
      alignment = choose_alignment(inout_value_len);

    if(_mm_plugin.aligned_allocate(inout_value_len, alignment, &buffer) != S_OK)
      throw General_exception("memory plugin alloc failed");

    if (buffer == nullptr)
      throw General_exception("Pool_instance::lock on-demand create allocate_memory failed (len=%lu)",
                              inout_value_len);
    created = true;

    CFLOGM(1, "creating on demand key=({}) len={}",
          key_svc, inout_value_len);

    common::RWLock * p = new (aal.allocate(1)) common::RWLock();

    CFLOGM(2, "created RWLock at {}", p);
    _map->emplace(k, Value_type{buffer, inout_value_len, p});
  }

  CFLOGM(1, "lock call has got key");

  if (type == IKVStore::STORE_LOCK_READ) {
    if((*_map)[k]._value_lock->read_trylock() != 0) {
      if(debug_level())
        FWRNM("key ({}) unable to take read lock", key_svc);

      out_key = IKVStore::KEY_NONE;
      return E_LOCKED;
    }
  }
  else if (type == IKVStore::STORE_LOCK_WRITE) {

    write_touch();

    if((*_map)[k]._value_lock->write_trylock() != 0) {
      if(debug_level())
        FWRNM("Map_store: key ({}) unable to take write lock", key_svc);

      out_key = IKVStore::KEY_NONE;
      return E_LOCKED;
    }

  }
  else throw API_exception("invalid lock type");

  out_value = (*_map)[k]._ptr;
  inout_value_len = (*_map)[k]._length;

  out_key = reinterpret_cast<IKVStore::key_t>((*_map)[k]._value_lock);

  /* C++11 standard: § 23.2.5/8

     The elements of an unordered associative container are organized
     into buckets. Keys with the same hash code appear in the same
     bucket. The number of buckets is automatically increased as
     elements are added to an unordered associative container, so that
     the average number of elements per bucket is kept below a
     bound. Rehashing invalidates iterators, changes ordering between
     elements, and changes which buckets elements appear in, but does
     not invalidate pointers or references to elements. For
     unordered_multiset and unordered_multimap, rehashing preserves
     the relative ordering of equivalent elements.
  */
  if(out_key_ptr) {
    auto element = _map->find(k);
    *out_key_ptr = common::pointer_cast<char>(element->first.data());
  }

  return created ? S_OK_CREATED : S_OK;
}

status_t Pool_instance::unlock(IKVStore::key_t key_handle)
{
  if(key_handle == nullptr) {
    PWRN("Map_store: unlock argument key handle invalid (%p)",
         reinterpret_cast<void*>(key_handle));
    return E_INVAL;
  }

  /* TODO: how do we know key_handle is valid? */
  if(reinterpret_cast<common::RWLock *>(key_handle)->unlock() != 0) {
    PWRN("Map_store: bad parameter to unlock");
    return E_INVAL;
  }

  CFLOGM(2, "unlocked key (handle={})", key_handle);
  return S_OK;
}

status_t Pool_instance::erase(const string_view_key key)
{
  const common::string_view key_svc(common::pointer_cast<char>(key.data()), key.size());
#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock, RWLock_guard::WRITE);
#endif
  std::lock_guard g{_mm_plugin_mutex}; /* aac, _mm_plugin.deallocate, aal */
  string_t k(key.data(), key.size(), aac);
  auto i = _map->find(k);

  if (i == _map->end()) return IKVStore::E_KEY_NOT_FOUND;

  if ( i->second._value_lock->write_trylock() != 0 ) { /* check pair is not locked */
    if(debug_level())
      FWRNM("key ({}) unable to take write lock", key_svc);

    return E_LOCKED;
  }

  write_touch();
  _map->erase(i);

  _mm_plugin.deallocate(&i->second._ptr, i->second._length);
  i->second._value_lock->unlock();
  i->second._value_lock->~RWLock();
  aal.deallocate(i->second._value_lock, 1); //, DEFAULT_ALIGNMENT);

  return S_OK;
}

size_t Pool_instance::count() {
#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock);
#endif
  return _map->size();
}

status_t Pool_instance::map(std::function<int(const string_view_key key,
                                              string_view_value value)> function)
{
#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock);
#endif

  for (auto &pair : *_map) {
    auto val = pair.second;
    function(pair.first, string_view_value(static_cast<string_view_value::value_type *>(val._ptr), val._length));
  }

  return S_OK;
}

status_t Pool_instance::map(std::function<int(string_view_key key,
                                              string_view_value value,
                                              const common::tsc_time_t timestamp)> function,
                                              const common::epoch_time_t t_begin,
                                              const common::epoch_time_t t_end)
{
#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock);
#endif

  common::tsc_time_t begin_tsc(t_begin);
  common::tsc_time_t end_tsc(t_end);

  for (auto &pair : *_map) {
    auto val = pair.second;

    if(val._tsc >= begin_tsc && (end_tsc == 0 || val._tsc <= end_tsc)) {
      if(function(pair.first,
                  string_view_value(static_cast<string_view_value::value_type *>(val._ptr), val._length),
                  val._tsc) < 0) {
        return S_MORE; /* break out of the loop if function returns < 0 */
      }
    }
  }

  return S_OK;
}


status_t Pool_instance::map_keys(std::function<int(string_view_key key)> function)
{
#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock);
#endif

  for (auto &pair : *_map) function(pair.first);

  return S_OK;
}

status_t Pool_instance::resize_value(const string_view_key key,
                                     const size_t new_size,
                                     const size_t alignment)
{
  const common::string_view key_svc(common::pointer_cast<char>(key.data()), key.size());

  CFLOGM(1, "resize_value (key={}, new_size={}, align={}",
        key_svc, new_size, alignment);

  if (new_size == 0) return E_INVAL;

#ifndef SINGLE_THREADED
  RWLock_guard guard(map_lock);
#endif

  std::lock_guard g{_mm_plugin_mutex}; /* aac, _mm_plugin.aligned_allocate */
  auto i = _map->find(string_t(key.data(), key.size(), aac));

  if (i == _map->end()) return IKVStore::E_KEY_NOT_FOUND;
  if (i->second._length == new_size) {
    CFLOGM(2, "resize_value request for same size!");
    return E_INVAL;
  }

  write_touch();

  /* perform resize */
  void * buffer = nullptr;
  if(_mm_plugin.aligned_allocate(new_size, alignment, &buffer) != S_OK)
    throw General_exception("memory plufin aligned_allocate failed");

  /* lock KV-pair */
  void *out_value;
  size_t inout_value_len;
  IKVStore::key_t out_key_handle = IKVStore::KEY_NONE;

  status_t s = lock(key,
                    IKVStore::STORE_LOCK_WRITE,
                    out_value,
                    inout_value_len,
                    alignment,
                    out_key_handle,
                    nullptr);

  if (out_key_handle == IKVStore::KEY_NONE) {
    CFLOGM(2, "bad lock result");
    return E_INVAL;
  }

  CFLOGM(2, "resize_value locked key-value pair");

  size_t size_to_copy = std::min<size_t>(new_size, boost::numeric_cast<size_t>(i->second._length));

  memcpy(buffer, i->second._ptr, size_to_copy);

  /* free previous memory */
  _mm_plugin.deallocate(&i->second._ptr, i->second._length);

  i->second._ptr = buffer;
  i->second._length = new_size;

  /* release lock */
  if(unlock(out_key_handle) != S_OK)
    throw General_exception("unlock in resize failed");

  CFLOGM(2, "resize_value re-unlocked key-value pair");
  return s;
}

status_t Pool_instance::get_pool_regions(nupm::region_descriptor::address_map_t &out_regions)
{
  if (_regions.empty())
    return E_INVAL;

  for (const auto &region : _regions)
    out_regions.push_back(nupm::region_descriptor::address_map_t::value_type
                          (common::make_byte_span(region->iov_base, region->iov_len)));
  return S_OK;
}

status_t Pool_instance::grow_pool(const size_t increment_size,
                                  size_t &reconfigured_size)
{
  if (increment_size <= 0)
    return E_INVAL;

  size_t rounded_increment_size = round_up_page(increment_size);

  auto new_region = allocate_region_memory(rounded_increment_size);
  std::lock_guard g{_mm_plugin_mutex};
  _mm_plugin.add_managed_region(new_region->iov_base, new_region->iov_len);
  _regions.push_back(std::move(new_region));
  reconfigured_size = _nsize;
  return S_OK;
}

status_t Pool_instance::free_pool_memory(const void *addr, const size_t size) {

  if (!addr || _regions.empty())
    return E_INVAL;

  std::lock_guard g{_mm_plugin_mutex};
  if(size)
    _mm_plugin.deallocate(const_cast<void **>(&addr), size);
  else
    _mm_plugin.deallocate_without_size(const_cast<void **>(&addr));

  /* the region memory is not freed, only memory in region */
  return S_OK;
}

status_t Pool_instance::allocate_pool_memory(const size_t size,
                                             const size_t alignment,
                                             void *&out_addr) {

  if (size == 0 || size > _nsize || _regions.empty()) {
    PWRN("Map_store: invalid allocate_pool_memory request");
    return E_INVAL;
  }

  try {
    /* we can't fully support alignment choice */
    out_addr = 0;

    std::lock_guard g{_mm_plugin_mutex};
    if( _mm_plugin.aligned_allocate(size, (alignment > 0) && (size % alignment == 0) ?
                                    alignment : choose_alignment(size), &out_addr) != S_OK)
      throw General_exception("memory plugin aligned_allocate failed");

    CFLOGM(1, "allocated pool memory ({} {})", out_addr, size);
  }
  catch(...) {
    PWRN("Map_store: unable to allocate (%lu) bytes aligned by %lu", size, choose_alignment(size));
    return E_INVAL;
  }

  return S_OK;
}


IKVStore::pool_iterator_t Pool_instance::open_pool_iterator()
{
  auto it = _iterators.insert(std::make_unique<Iterator>(this));
  return reinterpret_cast<IKVStore::pool_iterator_t>(it.first->get());
}

status_t Pool_instance::deref_pool_iterator(IKVStore::pool_iterator_t iter,
                                            const common::epoch_time_t t_begin,
                                            const common::epoch_time_t t_end,
                                            IKVStore::pool_reference_t& ref,
                                            bool& time_match,
                                            bool increment)
{
  const auto i = reinterpret_cast<Iterator*>(iter);
  if(_iterators.count(i) != 1) return E_INVAL;
  if(i->is_end()) return E_OUT_OF_BOUNDS;
  if(!i->check_mark(_writes)) return E_ITERATOR_DISTURBED;

  common::tsc_time_t begin_tsc(t_begin);
  common::tsc_time_t end_tsc(t_end);

  auto r = i->_iter;
  ref.key = r->first.data();
  ref.key_len = r->first.length();
  ref.value = r->second._ptr;
  ref.value_len = r->second._length;

  ref.timestamp = r->second._tsc.to_epoch();

  /* leave condition in timestamp cycles for better accuracy */
  time_match = (r->second._tsc >= begin_tsc) && (end_tsc == 0 || r->second._tsc <= end_tsc);

  if(increment) {
    try {
      i->_iter++;
    }
    catch(...) {
      return E_ITERATOR_DISTURBED;
    }
  }

  return S_OK;
}

status_t Pool_instance::close_pool_iterator(IKVStore::pool_iterator_t iter)
{
  const auto it = _iterators.find(reinterpret_cast<Iterator *>(iter));
  if (it == _iterators.end()) return E_INVAL;
  _iterators.erase(it);
  return S_OK;
}

std::unique_ptr<region_memory> Pool_instance::allocate_region_memory(size_t size)
{
  std::unique_ptr<region_memory> rm;
  assert(size > 0);

  assert(size % PAGE_SIZE == 0);

  auto prot = PROT_READ | PROT_WRITE;
  auto flags = MAP_SHARED;
  /* create space in file */
  if ( 0 <= _fdout && ftruncate(_fdout, _nsize + size) == 0 )
  {
    auto p = mmap(reinterpret_cast<char *>(0xff00000000) + _nsize, /* help debugging */
             size,
             prot,
             flags, /* paging means no MAP_LOCKED */
             _fdout, /* file */
             _nsize /* offset */);
    if (p != MAP_FAILED)
    {
      FINF("using backing file for {} MiB", REDUCE_MB(size));
      rm = std::make_unique<region_memory_mmap>(debug_level(), p, size);
    }
  }

  if (! rm) {
    if ( _numa_node == -1 )
    {
    auto addr = reinterpret_cast<char *>(0x800000000) + _nsize; /* help debugging */
    /* memory to be freed with munmap */
      auto p = ::mmap(addr,
             size,
             prot,
             flags | MAP_ANONYMOUS | effective_map_locked,
             -1, /* file */
             0 /* offset */);

      if ( p == MAP_FAILED ) {
        auto e = errno;
        std::ostringstream msg;
        msg << __FILE__ << " allocate_region_memory mmap failed on DRAM for region allocation"
            << " size=" << std::dec << size << " :" << strerror(e);
        throw General_exception("%s", msg.str().c_str());
      }
      rm = std::make_unique<region_memory_mmap>(debug_level(), p, size);
    }
    else
    {
      auto p = numa_alloc_onnode(size, _numa_node);
      rm = std::make_unique<region_memory_numa>(debug_level(), p, size);
    }
  }

#if 0
  if(madvise(p, size, MADV_DONTFORK) != 0)
    throw General_exception("madvise 'don't fork' failed unexpectedly (%p %lu)", p, size);
#endif

  CFLOGM(1, "allocated_region_memory ({},{})", rm->iov_base, size);
  _nsize += size;
  return rm;
}

/** Main class */

Map_store::Map_store(const unsigned debug_level,
                     const common::string_view mm_plugin_path,
                     const common::string_view owner,
                     const common::string_view name)
  : Map_store(debug_level, mm_plugin_path, owner, name, -1)
{
}

Map_store::Map_store(const unsigned debug_level,
                     const common::string_view mm_plugin_path,
                     const common::string_view /* owner */,
                     const common::string_view /* name */,
                     const int numa_node_)
  : _debug_level(debug_level),
    _mm_plugin_path(mm_plugin_path),
    _numa_node(numa_node_)
{
  CFLOGM(1, "mm_plugin_path ({})", mm_plugin_path);
}

Map_store::~Map_store() {
}

IKVStore::pool_t Map_store::create_pool(const common::string_view name_,
                                        const size_t nsize,
                                        const flags_t flags,
                                        uint64_t /*args*/,
                                        IKVStore::Addr /*base addr unused */
)
{
  if (flags & IKVStore::FLAGS_READ_ONLY)
    throw API_exception("read only create_pool not supported on map-store component");

  Std_lock_guard g(_pool_sessions_lock);

  auto iter = _pools.find(name_);

  if (flags & IKVStore::FLAGS_CREATE_ONLY) {
    if (iter != _pools.end()) {
      return POOL_ERROR;
    }
  }

  std::shared_ptr<Pool_instance> handle;

  if(iter != _pools.end()) {
    handle = iter->second;
    CFLOGM(1, "using existing pool instance");
  }
  else {
    handle = _pools.insert(pools_type::value_type(name_, std::make_shared<Pool_instance>(debug_level(), _mm_plugin_path, name_, nsize, _numa_node, flags))).first->second;
    CFLOGM(1, "creating new pool instance");
  }
  auto session = _pool_sessions.emplace(std::make_unique<Pool_session>(handle)).first->get(); /* create a session too */
  CFLOGM(1, "adding new session ({})", session);

  CFLOGM(1, "created pool OK: {}", name_);

  return reinterpret_cast<IKVStore::pool_t>(session);
}

IKVStore::pool_t Map_store::open_pool(string_view name,
                                      const flags_t /*flags*/,
                                      component::IKVStore::Addr /* base_addr_unused */)
{
  std::shared_ptr<Pool_instance> ph;
  Std_lock_guard g(_pool_sessions_lock);
  /* see if a pool exists that matches the key */
  auto it = _pools.find(name);

  if (it == _pools.end())
    return component::IKVStore::POOL_ERROR;
  auto session = _pool_sessions.emplace(std::make_unique<Pool_session>(it->second)).first->get();
  CFLOGM(1, "opened pool({})", session);

  return reinterpret_cast<IKVStore::pool_t>(session);
}

status_t Map_store::close_pool(const pool_t pid)
{
  auto session = reinterpret_cast<Pool_session *>(pid);
  CFLOGM(1, "close_pool ({})", session);

  Std_lock_guard g(_pool_sessions_lock);
  auto it = find_session(session);
  if ( debug_level() && it == _pool_sessions.end() ) FWRNM("close pool on invalid handle");
  if ( it == _pool_sessions.end() ) return IKVStore::E_POOL_NOT_FOUND;

  tls_cache.session = nullptr;

  _pool_sessions.erase(it);
  CFLOGM(1, "closed pool ({})", pid);
  CFLOGM(1, "erased session {}", session);

  return S_OK;
}

status_t Map_store::delete_pool(const common::string_view poolname_)
{
  Std_lock_guard g(_pool_sessions_lock);
  /* see if a pool exists that matches the poolname */
  auto it = _pools.find(poolname_);

  if (it == _pools.end()) {
    CFWRNM(1, "({}) pool not found", poolname_);
    return E_POOL_NOT_FOUND;
  }

  for (auto &s : _pool_sessions) {
    if (s->pool->name() == poolname_) {
      FWRNM("({}) pool delete failed because pool still open ({})",
           poolname_, common::p_fmt(s.get()));
      return E_ALREADY_OPEN;
    }
  }

  _pools.erase(it);

  return S_OK;
}

status_t Map_store::get_pool_names(std::list<std::string>& inout_pool_names)
{
  Std_lock_guard g(_pool_sessions_lock);
  for (auto &h : _pools) {
    assert(h.second);
    inout_pool_names.push_back(h.second->name());
  }
  return S_OK;
}

status_t Map_store::put(IKVStore::pool_t pid, string_view_key key,
                        const void *value, size_t value_len,
                        const flags_t flags)
{
  auto session = get_session(pid);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->put(key, value, value_len, flags);
}

status_t Map_store::get(const pool_t pid, string_view_key key,
                        void *&out_value, size_t &out_value_len)
{
  auto session = get_session(pid);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->get(key, out_value, out_value_len);
}

status_t Map_store::get_direct(const pool_t pid, string_view_key key,
                               void *out_value, size_t &out_value_len,
                               component::IKVStore::memory_handle_t /*handle*/)
{
  auto session = get_session(pid);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->get_direct(key, out_value, out_value_len);
}

status_t Map_store::put_direct(const pool_t pid, string_view_key key,
                               const void *value, const size_t value_len,
                               memory_handle_t /*memory_handle*/,
                               const flags_t flags)
{
  return Map_store::put(pid, key, value, value_len, flags);
}

status_t Map_store::resize_value(const pool_t pool,
                                 string_view_key key,
                                 const size_t new_size,
                                 const size_t alignment)
{
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->resize_value(key, new_size, alignment);
}

status_t Map_store::get_attribute(const pool_t pool,
                                  const IKVStore::Attribute attr,
                                  std::vector<uint64_t> &out_attr,
                                  const string_view_key key)
{
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->get_attribute(attr, out_attr, key);
}

status_t Map_store::swap_keys(const pool_t           pool,
                              const string_view_key key0,
                              const string_view_key key1)
{
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->swap_keys(key0, key1);
}


status_t Map_store::lock(const pool_t pid,
                         string_view_key key,
                         lock_type_t type,
                         void *&out_value,
                         size_t &inout_value_len,
                         size_t alignment,
                         IKVStore::key_t &out_key,
                         const char ** out_key_ptr)
{
  auto session = get_session(pid);
  if (!session) {
    out_key = IKVStore::KEY_NONE;
    FWRNM("invalid pool id ({})", pid);
    return E_FAIL; /* same as hstore, but should be E_INVAL; */
  }

  auto rc = session->pool->lock(key, type, out_value, inout_value_len, alignment, out_key, out_key_ptr);

  const common::string_view key_svc(common::pointer_cast<char>(key.data()), key.size());
  CFLOGM(1, "lock({}, {}) rc={}", key_svc, out_key, rc);

  return rc;
}

status_t Map_store::unlock(const pool_t pid,
                           key_t key_handle,
                           IKVStore::unlock_flags_t /* flags not used */)
{
  auto session = get_session(pid);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  CFLOGM(1, "(key-handle={})", key_handle);

  session->pool->unlock(key_handle);
  return S_OK;
}

status_t Map_store::erase(const pool_t pid, const string_view_key key)
{
  auto session = get_session(pid);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->erase(key);
}

size_t Map_store::count(const pool_t pid)
{
  auto session = get_session(pid);
  if (!session) return pool_t(IKVStore::E_POOL_NOT_FOUND);

  return session->pool->count();
}

status_t Map_store::free_memory(void *p)
{
  ::free(p);
  return S_OK;
}

void Map_store::debug(const pool_t, unsigned, uint64_t) {}

int Map_store::get_capability(Capability cap) const
{
  switch (cap) {
  case Capability::POOL_DELETE_CHECK:
    return 1;
  case Capability::POOL_THREAD_SAFE:
    return 1;
  case Capability::RWLOCK_PER_POOL:
    return 1;
  case Capability::WRITE_TIMESTAMPS:
    return 1;
  default:
    return -1;
  }
}

status_t Map_store::map(const IKVStore::pool_t pool,
                        std::function<int(string_view_key key,
                                          string_view_value value)>
                        function)
{
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->map(function);
}

status_t Map_store::map(const pool_t pool,
                        std::function<int(string_view_key key,
                                          string_view_value value,
                                          common::tsc_time_t timestamp)> function,
                        const common::epoch_time_t t_begin,
                        const common::epoch_time_t t_end)
{
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->map(function, t_begin, t_end);
}


status_t Map_store::map_keys(const IKVStore::pool_t pool,
                             std::function<int(string_view_key key)> function)
{
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->map_keys(function);
}

status_t Map_store::get_pool_regions(const pool_t pool,
                                     nupm::region_descriptor &out_regions)
{
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;
  nupm::region_descriptor::address_map_t addr_map;
  auto status = session->pool->get_pool_regions(addr_map);
  out_regions = std::move(nupm::region_descriptor(addr_map));
  return status;
}

status_t Map_store::grow_pool(const pool_t pool, const size_t increment_size,
                              size_t &reconfigured_size)
{
  PMAJOR("grow_pool (%zu)", increment_size);
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;
  return session->pool->grow_pool(increment_size, reconfigured_size);
}

status_t Map_store::free_pool_memory(const pool_t pool, const void *addr,
                                     const size_t size) {
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;

  return session->pool->free_pool_memory(addr, size);
}

status_t Map_store::allocate_pool_memory(const pool_t pool,
                                         const size_t size,
                                         const size_t alignment,
                                         void *&out_addr) {
  auto session = get_session(pool);
  if (!session) return IKVStore::E_POOL_NOT_FOUND;
  return session->pool->allocate_pool_memory(size, alignment > size ? size : alignment, out_addr);
}

IKVStore::pool_iterator_t Map_store::open_pool_iterator(const pool_t pool)
{
  auto session = get_session(pool);
  if (!session) return nullptr;
  auto i = session->pool->open_pool_iterator();
  return i;
}

status_t Map_store::deref_pool_iterator(const pool_t pool,
                                        IKVStore::pool_iterator_t iter,
                                        const common::epoch_time_t t_begin,
                                        const common::epoch_time_t t_end,
                                        pool_reference_t& ref,
                                        bool& time_match,
                                        bool increment)
{
  auto session = get_session(pool);
  if (!session) return E_INVAL;
  return session->pool->deref_pool_iterator(iter,
                                            t_begin,
                                            t_end,
                                            ref,
                                            time_match,
                                            increment);
}

status_t Map_store::close_pool_iterator(const pool_t pool,
                                        IKVStore::pool_iterator_t iter)
{
  auto session = get_session(pool);
  if (!session) return E_INVAL;
  return session->pool->close_pool_iterator(iter);
}


/**
 * Factory entry point
 *
 */
extern "C" void *factory_createInstance(component::uuid_t component_id) {
  if (component_id == Map_store_factory::component_id()) {
    return static_cast<void *>(new Map_store_factory());
  }
  else {
    return NULL;
  }
}

Map_store_factory::~Map_store_factory() {
}

void *Map_store_factory::query_interface(component::uuid_t &itf_uuid)
{
  if (itf_uuid == component::IKVStore_factory::iid()) {
    return static_cast<component::IKVStore_factory *>(this);
  }
  else return NULL;  // we don't support this interface
}

void Map_store_factory::unload() { delete this; }

component::IKVStore *Map_store_factory::create(unsigned debug_level,
                                    const IKVStore_factory::map_create &mc)
{
  auto owner_it = mc.find(+component::IKVStore_factory::k_owner);
  auto name_it = mc.find(+component::IKVStore_factory::k_name);
  auto mm_plugin_path_it = mc.find(+component::IKVStore_factory::k_mm_plugin_path);

  std::string checked_mm_plugin_path;
  if(mm_plugin_path_it == mc.end()) {
    checked_mm_plugin_path = DEFAULT_MM_PLUGIN_PATH;
  }
  else {
    std::string path = mm_plugin_path_it->second;

    if(access(path.c_str(), F_OK) != 0) {
      path = DEFAULT_MM_PLUGIN_LOCATION + path;
      if(access(path.c_str(), F_OK) != 0) {
        PERR("inaccessible plugin path (%s) and (%s)", mm_plugin_path_it->second.c_str(), path.c_str());
        throw General_exception("unable to open mm_plugin");
      }
      checked_mm_plugin_path = path;
    }
    else {
      checked_mm_plugin_path = path;
    }
  }

  component::IKVStore *obj =
    static_cast<component::IKVStore *>
    (new Map_store(debug_level,
                   checked_mm_plugin_path,
                   owner_it == mc.end() ? "owner" : owner_it->second,
                   name_it == mc.end() ? "name" : name_it->second));
  assert(obj);
  obj->add_ref();
  return obj;
}
