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

#include "hstore_config.h"
#include "persister_nupm.h"
#include "dax_manager.h"
#include "pool_path.h"
#include "region.h"
#include "session.h"

#include <city.h> /* CityHash */

#include <cinttypes> /* PRIx64 */
#include <cstdlib> /* getenv */
#include <cstring> /* strerror */

template <typename Region, typename Table, typename Allocator, typename LockType>
  unsigned hstore_nupm<Region, Table, Allocator, LockType>::name_to_numa_node(const std::string &name)
  {
    if ( 0 == name.size() )
    {
      throw std::domain_error("cannot determine numa node from null string");
    }
    auto c = name[name.size()-1];
    if ( ! std::isprint(c) )
    {
      throw std::domain_error("last character of name (unprintable) does not look like a numa node ID");
    }
    if ( c < '0' || '8' < c )
    {
#if 0
      throw std::domain_error(std::string("last character of name '") + name + "' does not look like a numa node ID");
#else
      /* current test cases do not always supply a node number - default to 0 */
      c = '0';
#endif
    }
    return unsigned(c - '0');
  }

template <typename Region, typename Table, typename Allocator, typename LockType>
  std::uint64_t hstore_nupm<Region, Table, Allocator, LockType>::dax_uuid_hash(const pool_path &p)
  {
    std::string s = p.str();
    return CityHash64(s.data(), s.size());
  }

template <typename Region, typename Table, typename Allocator, typename LockType>
  hstore_nupm<Region, Table, Allocator, LockType>::hstore_nupm(unsigned debug_level_, const std::string &, const std::string &name_, std::unique_ptr<dax_manager> mgr_)
    : pool_manager<::open_pool<non_owner<region_type>>>(debug_level_)
    , _dax_manager(std::move(mgr_))
    , _numa_node(name_to_numa_node(name_))
  {}

template <typename Region, typename Table, typename Allocator, typename LockType>
  hstore_nupm<Region, Table, Allocator, LockType>::~hstore_nupm()
  {
  }

template <typename Region, typename Table, typename Allocator, typename LockType>
  void hstore_nupm<Region, Table, Allocator, LockType>::pool_create_check(std::size_t)
  {
  }

template <typename Region, typename Table, typename Allocator, typename LockType>
  auto hstore_nupm<Region, Table, Allocator, LockType>::pool_create_1(
    const pool_path &path_
    , std::size_t size_
  ) -> region_access_create
  {
    auto size = size_;

#if USE_CC_HEAP == 3
    /* The first part of pool space is the header, which is described by a Region.
     * In order to give the heap a well-aligned space, the size actually allocated
     * to a heap may be as little as 3/4 of the area provided to the heap.
     * The constant 3/4 is embedded in the heap_rc_shared class.
     *
     * Ask for enough space to contain the header and to compensate for inefficiency
     * due to heap alignment.
     */
    size = sizeof(region_type) + size_ * 4 / 3;
#endif

#if defined HSTORE_LOG_GRAIN_SIZE
    /* _dax_manager will allocate a region of some granularity But there is no mechanism for it to
     * tell us that. Round request up to a grain size, if specified, to avoid wasting space.
     */
	const std::size_t hstore_grain_size = std::size_t(1) << (HSTORE_LOG_GRAIN_SIZE);
    size = round_up(size_,hstore_grain_size);
#endif
    /* Attempt to create a new pool. */
    try
    {
      PLOG(PREFIX "id %s: creating region length 0x%zx", LOCATION, path_.str().c_str(), size);
      auto v = _dax_manager->create_region(path_.str(), _numa_node, size);
      /* Guess that nullptr indicate a failure */
      if ( ! v.second.size() )
      {
        throw pool_error("create_region fail: " + path_.str(), pool_ec::region_fail);
      }
      PLOG(PREFIX "id %s: created region at %p:0x%zx", LOCATION, path_.str().c_str(), v.second.front().iov_base, v.second.front().iov_len);
      /* Note: uuid required for, well, something in the caller. Consider moving dax_uuid_hash computation to the caller */
      auto uuid = dax_uuid_hash(path_);
      return region_access_create(v.first, v.second.front(), uuid);
    }
    catch ( const General_exception &e )
    {
      throw pool_error("create_region fail: " + path_.str() + " " + e.cause(), pool_ec::region_fail_general_exception);
    }
    catch ( const std::bad_alloc& e)
    {
      throw pool_error("create_region fail: " + path_.str(), pool_ec::region_fail_general_exception);
    }
    catch ( const API_exception &e )
    {
      throw pool_error("create_region fail: " + path_.str() + " " + e.cause(), pool_ec::region_fail_api_exception);
    }
  }

template <typename Region, typename Table, typename Allocator, typename LockType>
  auto hstore_nupm<Region, Table, Allocator, LockType>::pool_create_2(
    AK_ACTUAL
    const region_access_create & rac_
    , component::IKVStore::flags_t flags_
    , std::size_t expected_obj_count_
  ) -> std::unique_ptr<open_pool_handle>
  {
    if ( flags_ != 0 )
    {
      throw pool_error("unsupported flags " + std::to_string(flags_), pool_ec::pool_unsupported_mode);
    }

    /* Attempt to create a new pool. */
    try
    {
      open_pool_handle h(
        new (std::get<1>(rac_).iov_base)
        region_type(
          AK_REF this->debug_level()
          , std::get<2>(rac_) // uuid
          , std::get<1>(rac_).iov_len
          , expected_obj_count_
          , _numa_node
          , std::get<0>(rac_) // backing file
        )
      );
      return std::make_unique<session<open_pool_handle, allocator_t, table_t, lock_type_t>>(AK_REF std::move(h), construction_mode::create);
    }
    catch ( const General_exception &e )
    {
      throw pool_error(std::string("create_region fail: ") + e.cause(), pool_ec::region_fail_general_exception);
    }
    catch ( const std::bad_alloc& e)
    {
      throw pool_error("create_region fail (bad alloc): ", pool_ec::region_fail_general_exception);
    }
    catch ( const API_exception &e )
    {
      throw pool_error(std::string("create_region fail: ") + e.cause(), pool_ec::region_fail_api_exception);
    }
  }

  template <typename Region, typename Table, typename Allocator, typename LockType>
  auto hstore_nupm<Region, Table, Allocator, LockType>::pool_open_1(
    const pool_path &path_
  ) -> region_access
  {
    auto iovs = _dax_manager->open_region(path_.str(), _numa_node);

    if ( iovs.second.size() != 1 )
    {
      throw pool_error("in Devdax_manger::open_region faili: " + path_.str(), pool_ec::region_fail);
    }

    return iovs;
  }

  template <typename Region, typename Table, typename Allocator, typename LockType>
  auto hstore_nupm<Region, Table, Allocator, LockType>::pool_open_2(
    AK_ACTUAL
    const region_access & ra_
    , component::IKVStore::flags_t flags_
  ) -> std::unique_ptr<open_pool_handle>
  {
    if ( flags_ != 0 )
    {
      throw pool_error("unsupported flags " + std::to_string(flags_), pool_ec::pool_unsupported_mode);
    }

    open_pool_handle h(new (ra_.second.front().iov_base) region_type(this->debug_level(), _dax_manager, ra_.first ));

    PLOG(PREFIX "in open_2 region at %p", LOCATION, ra_.second.front().iov_base);
    /* open_pool_handle is a managed region * */
    auto s = std::make_unique<session<open_pool_handle, allocator_t, table_t, lock_type_t>>(AK_REF std::move(h), construction_mode::reconstitute);
    return s;
  }

template <typename Region, typename Table, typename Allocator, typename LockType>
  void hstore_nupm<Region, Table, Allocator, LockType>::pool_close_check(const std::string &)
  {
  }

template <typename Region, typename Table, typename Allocator, typename LockType>
  void hstore_nupm<Region, Table, Allocator, LockType>::pool_delete(const pool_path &path_)
  {
    _dax_manager->erase_region(path_.str(), _numa_node);
  }

template <typename Region, typename Table, typename Allocator, typename LockType>
  auto hstore_nupm<Region, Table, Allocator, LockType>::pool_get_regions(const open_pool_handle & pool_) const
  -> std::pair<std::string, std::vector<::iovec>>
  {
    return pool_->get_regions();
  }
