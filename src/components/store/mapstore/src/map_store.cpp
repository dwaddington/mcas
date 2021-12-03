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

#include "pool_instance.h"
#include "pool_session.h"

#include <common/exceptions.h>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <memory> /* shared_ptr, unique_ptr */

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

namespace
{
struct tls_cache_t {
  pool_session *session;
};

std::mutex _pool_sessions_lock; /* guards both _pool_sessions and _pools */
/* ERROR?: should pool_sessions really shared across Map_store instances? */
using pools_type = std::map<std::string, std::shared_ptr<Pool_instance>, std::less<>>;
pools_type _pools; /*< existing pools */
std::set<std::unique_ptr<pool_session>> _pool_sessions;

using Std_lock_guard = std::lock_guard<std::mutex>;

std::set<std::unique_ptr<pool_session>>::iterator find_session(pool_session *session)
{
  return
    std::find_if(
      _pool_sessions.begin(), _pool_sessions.end()
      , [session] (const std::unique_ptr<pool_session> &i) { return i.get() == session; }
    );
}

  __thread tls_cache_t tls_cache = {nullptr};

  pool_session *get_session(const component::IKVStore::pool_t pid)
  {
    auto session = reinterpret_cast<pool_session *>(pid);
    assert(session);

    if (session != tls_cache.session) {
      Std_lock_guard g(_pool_sessions_lock);

      auto it = find_session(session);
      if (it == _pool_sessions.end()) return nullptr;

      tls_cache.session = session;
    }

    return session;
  }
}

/** Main class */

Map_store::Map_store(const unsigned debug_level,
                     const common::string_view mm_plugin_path,
                     const common::string_view owner,
                     const common::string_view name)
  : Map_store(debug_level, mm_plugin_path, owner, name, "")
{
}

Map_store::Map_store(const unsigned debug_level,
                     const common::string_view mm_plugin_path,
                     const common::string_view /* owner */,
                     const common::string_view /* name */,
                     const common::string_view numa_node_mask_)
  : _debug_level(debug_level),
    _mm_plugin_path(mm_plugin_path),
    _numa_node_mask(numa_node_mask_)
{
  CFLOGM(1, "mm_plugin_path ({})", mm_plugin_path);
}

Map_store::~Map_store() {
}

auto Map_store::create_pool(const common::string_view name_,
                                        const size_t nsize,
                                        const flags_t flags,
                                        uint64_t /*args*/,
                                        IKVStore::Addr /*base addr unused */
) -> IKVStore::pool_t
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
    handle = _pools.insert(pools_type::value_type(name_, std::make_shared<Pool_instance>(debug_level(), _mm_plugin_path, name_, nsize, _numa_node_mask.get(), flags))).first->second;
    CFLOGM(1, "creating new pool instance");
  }
  auto session = _pool_sessions.emplace(std::make_unique<pool_session>(handle)).first->get(); /* create a session too */
  CFLOGM(1, "adding new session ({})", session);

  CFLOGM(1, "created pool OK: {}", name_);

  return reinterpret_cast<IKVStore::pool_t>(session);
}

auto Map_store::open_pool(string_view name,
                                      const flags_t /*flags*/,
                                      component::IKVStore::Addr /* base_addr_unused */) -> IKVStore::pool_t
{
  std::shared_ptr<Pool_instance> ph;
  Std_lock_guard g(_pool_sessions_lock);
  /* see if a pool exists that matches the key */
  auto it = _pools.find(name);

  if (it == _pools.end())
    return component::IKVStore::POOL_ERROR;
  auto session = _pool_sessions.emplace(std::make_unique<pool_session>(it->second)).first->get();
  CFLOGM(1, "opened pool({})", session);

  return reinterpret_cast<IKVStore::pool_t>(session);
}

status_t Map_store::close_pool(const pool_t pid)
{
  auto session = reinterpret_cast<pool_session *>(pid);
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

auto Map_store::open_pool_iterator(const pool_t pool) -> IKVStore::pool_iterator_t
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
