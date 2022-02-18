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
#ifndef __MCAS_POOL_MANAGER_H__
#define __MCAS_POOL_MANAGER_H__

#include "fabric_connection_base.h"

#include <api/kvstore_itf.h>
#include <common/logging.h> /* log_source */
#include <common/to_string.h>

#include <string>
#include <cassert>
#include <map>

/* Access control lists is a compile-time option */
#ifdef FEATURE_POOL_ACL
#endif


namespace mcas
{

#ifdef FEATURE_POOL_ACL
/* attributes of metadata pools */
static constexpr const char * METADATA_POOL_PREFIX = "__metadata_";
static constexpr size_t METADATA_POOL_PREFIX_LEN = 10;
static constexpr size_t METADATA_POOL_INITIAL_SIZE = 4096*2;
static constexpr size_t METADATA_POOL_EXPECTED_OBJECTS = 100;
#endif

/**
   Pool_manager tracks open pool handles on a per-connection basis.    
*/
class Pool_manager : common::log_source {
 public:
  using pool_t = component::IKVStore::pool_t;

private:
  const void *to_ptr(component::IKVStore::pool_t p) {
    return reinterpret_cast<const void *>(p);
  }

 public:

  /** 
   * Constructor
   * 
   */
  Pool_manager()
    : common::log_source(mcas::global::debug_level),
      _map_n2p{},
      _map_p2n{},
      _open_pools{},
      _pool_info{}
  {
  }
  
  /**
   * Determine if pool is open and valid
   *
   * @param pool Pool name
   *
   * @return true of pool is open
   */
  bool check_for_open_pool(const std::string& pool_name, pool_t& out_pool) const
  {
    auto i = _map_n2p.find(pool_name);
    if (i == _map_n2p.end()) {
      CPLOG(1, "check_for_open_pool (%s) false", pool_name.c_str());
      out_pool = 0;
      return false;
    }

    auto j = _open_pools.find(i->second);
    if (j != _open_pools.end()) {
      out_pool = i->second;
      CPLOG(1, "check_for_open_pool (%s) true", pool_name.c_str());
      return true;
    }
    out_pool = 0;
    CPLOG(1, "check_for_open_pool (%s) false", pool_name.c_str());
    return false;
  }

private:
   
#ifdef FEATURE_POOL_ACL
   /** 
   * Derive the name of the metadata pool from the pool name
   * 
   * @param pool_name Pool name
   * 
   * @return 
   */
  std::string derive_metadata_pool_name(const std::string& pool_name) {
    std::string mpn = METADATA_POOL_PREFIX + pool_name;
    return mpn;
  }
#endif

public:
  
  /**
   * Open a pool and register it
   *
   * @param kvstore Handle to kvstore component interface
   * @param pool Pool identifier
   *
   * @return Pool handle
   */
  pool_t open_and_register_pool(component::IKVStore * kvstore,
                                const std::string&    pool_name)
  {
#ifdef FEATURE_POOL_ACL
    /* refuse to open pools that have metadata pool prefix */
    if(pool_name.compare(0, METADATA_POOL_PREFIX_LEN, METADATA_POOL_PREFIX)) {
      FWRNM("{}", "opening metadata pool directly disallowed");
      return component::IKVStore::POOL_ERROR;
    }
    
    auto metadata_pool_name = derive_metadata_pool_name(pool_name);
    auto metadata_pool = kvstore->open_pool(metadata_pool_name);
    if(metadata_pool == component::IKVStore::POOL_ERROR) return metadata_pool;

#endif
    
    assert(kvstore);
    pool_t pool = kvstore->open_pool(pool_name);

    if(pool == component::IKVStore::POOL_ERROR) {
#ifdef FEATURE_POOL_ACL
      PWRN("Pool_manager: %s metadata pool without owning pool", __func__);
      kvstore->close_pool(metadata_pool);
#endif
      return pool;
    }

    _open_pools[pool]   = 1;
    _map_n2p[pool_name] = pool;
    _map_p2n[pool]      = pool_name;
    _pool_info[pool]    = {
#ifdef FEATURE_POOL_ACL                           
                           metadata_pool,
#endif
                           0, /* expected_obj_count */
                           0, /* size */ // TODO get from store get_attribute
                           0 }; /* flags */

    CPLOG(1, "(+) registered pool (%p) ref:%u pm=%p", to_ptr(pool), _open_pools[pool],
          static_cast<void*>(this));

    return pool;
  }

  /** 
   * Create a pool and then register
   * 
   * @param kvstore Handle to kvstore component interface
   * @param pool_name Pool name
   * @param pool_size Size of pool in bytes (will be rounded up to page alignment)
   * @param expected_object_count Expected object count (intial hash table size)
   * @param flags Creation flags
   * 
   * @return Pool handle
   */
  pool_t create_and_register_pool(component::IKVStore * kvstore,
                                  const std::string&    pool_name,
                                  const size_t          pool_size,
                                  const uint64_t        expected_object_count,
                                  const unsigned int    flags)
  {
    using namespace component;

    assert(kvstore);
    assert(pool_name.empty() == false);
    
#ifdef FEATURE_POOL_ACL
    /* refuse to open pools that have metadata pool prefix */
    if(pool_name.compare(0, METADATA_POOL_PREFIX_LEN, METADATA_POOL_PREFIX)) {
      FWRNM("{}", "creating metadata pool directly disallowed");
      return component::IKVStore::POOL_ERROR;
    }
    
    auto metadata_pool_name = derive_metadata_pool_name(pool_name);
    auto metadata_pool = kvstore->create_pool(metadata_pool_name,
                                              METADATA_POOL_INITIAL_SIZE,
                                              0, /* flags */
                                              METADATA_POOL_EXPECTED_OBJECTS);

    if(metadata_pool == component::IKVStore::POOL_ERROR) return metadata_pool;

#endif
    
    IKVStore::pool_t pool = 0;
    
    size_t modified_pool_size = round_up_page(pool_size);
    
    /* call backend to create pool */
    pool = kvstore->create_pool(pool_name,
                                modified_pool_size,
                                flags,
                                expected_object_count);

    if (pool == IKVStore::POOL_ERROR) {
#ifdef FEATURE_POOL_ACL
      PWRN("Pool_manager:%s metadata pool without owning pool", __func__);
      kvstore->close_pool(metadata_pool);
#endif      
      return pool;
    }
    
    _open_pools[pool]   = 1;
    _map_n2p[pool_name] = pool;
    _map_p2n[pool]      = pool_name;
    _pool_info[pool]    = {
#ifdef FEATURE_POOL_ACL                           
                           metadata_pool,
#endif
                           expected_object_count, modified_pool_size, flags };

    CPLOG(1, "(+) registered pool (%p) ref:%u pm=%p",
          to_ptr(pool), _open_pools[pool],
          static_cast<void*>(this));

    return pool;
  }

  /**
   * Get pool information
   *
   * @param pool Pool identifier
   * @param expected_obj_count Expected object count
   * @param size Size of pool
   * @param flags Creation flags
   */
  void get_pool_info(const pool_t  pool,
                     uint64_t&     expected_obj_count,
                     size_t&       size,
                     unsigned int& flags)
  {
    auto i = _pool_info.find(pool);
    if (i != _pool_info.end()) {
      expected_obj_count = i->second.expected_obj_count;
      size               = i->second.size;
      flags              = i->second.flags;
    }
  }

  /**
   * Add reference count to open pool
   *
   * @param pool Pool identifier
   */
  void add_reference(pool_t pool)
  {
    if (_open_pools.find(pool) == _open_pools.end())
      throw Logic_exception("add reference to pool that is not open");

    _open_pools[pool] += 1;
    CPLOG(1, "(+) inc pool (%p) ref:%u", to_ptr(pool), _open_pools[pool]);
  }

  /**
   * Release open pool (and metadata pool).
   *
   * @param kvstore Handle to kvstore component interface
   * @param pool Pool identifier
   *
   * @return Returns true if reference becomes 0
   */
  bool release_pool_reference(component::IKVStore * kvstore, pool_t pool)
  {
    assert(kvstore);
    
    auto i = _open_pools.find(pool);
    if (i == _open_pools.end()) {
      throw std::invalid_argument(common::to_string(std::showbase, std::setbase(16),
                                                    "invalid pool handle ", pool));
    }

    i->second -= 1;  // _open_pools[pool]

    CPLOG(1, "(-) release pool (%p) ref:%u", to_ptr(pool), _open_pools[pool]);

    if (i->second == 0) {
#ifdef FEATURE_POOL_ACL
      {
        auto pool_info = _pool_info.find(pool);
        if(pool_info == _pool_info.end())
          throw Logic_exception("%s pool_info not found", __func__);

        auto& mdp = pool_info->second.metadata_pool;
        if(mdp == 0)
          throw Logic_exception("%s pool_info metadata_pool field 0", __func__);
        kvstore->close_pool(mdp);
      }
#else
      (void) kvstore;
#endif
      
      /* zero reference count; erase entries */
      _open_pools.erase(i);
      _map_n2p.erase(_map_p2n[pool]);
      _map_p2n.erase(pool);
      _pool_info.erase(pool);
      return true;
    }
    return false;
  }

  /**
   * Get current reference count for a pool
   *
   * @param pool Pool identifier
   *
   * @return Reference count
   */
  auto pool_reference_count(pool_t pool)
  {
    auto i = _open_pools.find(pool);
    if (i == _open_pools.end())
      throw std::invalid_argument("invalid pool handle");

    return i->second;
  }

  /**
   * Look up pool name
   *
   * @param pool Pool identifier
   *
   * @return Pool name string
   */
  auto pool_name(pool_t pool) { return _map_p2n[pool]; }

  /**
   * Determine if pool is open and valid
   *
   * @param pool Pool identifier
   */
  bool is_pool_open(pool_t pool) const
  {
    auto i = _open_pools.find(pool);

    if (i != _open_pools.end())
      return i->second > 0;
    else
      return false;
  }

  inline const std::map<pool_t, unsigned>& open_pool_set() const { return _open_pools; }

  /** 
   * Get open pool count
   * 
   * 
   * @return Number of pools open
   */
  inline size_t open_pool_count() const { return _open_pools.size(); }

 private:
  
  struct pool_info_t {
#ifdef FEATURE_POOL_ACL
    pool_t       metadata_pool;
#endif        
    uint64_t     expected_obj_count;
    size_t       size;
    unsigned int flags;
  };

  std::map<std::string, pool_t> _map_n2p;
  std::map<pool_t, std::string> _map_p2n;
  std::map<pool_t, unsigned>    _open_pools;
  std::map<pool_t, pool_info_t> _pool_info;

};
}  // namespace mcas

#endif  // __mcas_POOL_MANAGER_H__
