/*
   Copyright [2017-2019] [IBM Corporation]
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


#ifndef COMANCHE_HSTORE_NUPM_H
#define COMANCHE_HSTORE_NUPM_H

#include "pool_manager.h"

#include "hstore_nupm_types.h"
#include "hstore_open_pool.h"
#include "persister_nupm.h"
#include <gsl/pointers>

#include <cstring> /* strerror */

#include <cinttypes> /* PRIx64 */
#include <cstdlib> /* getenv */

struct dax_manager;

template <typename PersistData, typename Heap, typename HeapAllocator>
  struct region;

#pragma GCC diagnostic push
/* Note: making enable_shared_from_this private avoids the non-virtual-dtor error but
 * generates a different error with no error text (G++ 5.4.0)
 */
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"

/* Region is region<persist_data_t, heap_rc>, Table is hstore::table_t Allocator is table_t::allocator_type, LockType is hstore::locK_type_t */
template <typename Region, typename Table, typename Allocator, typename LockType>
  struct hstore_nupm
    : public pool_manager<::open_pool<non_owner<Region>>>
  {
    using region_type = Region;
  private:
    using table_t = Table;
    using allocator_t = Allocator;
    using lock_type_t = LockType;
  public:
    using open_pool_handle = ::open_pool<non_owner<region_type>>;
    using base = pool_manager<open_pool_handle>;
  private:
    std::unique_ptr<dax_manager> _dax_manager;
    unsigned _numa_node;

    void map_create(
      region_type *pop_
      , std::size_t size_
      , std::size_t expected_obj_count
    );

    static unsigned name_to_numa_node(const std::string &name);
  public:
    hstore_nupm(unsigned debug_level_, const std::string &, const std::string &name_, std::unique_ptr<dax_manager> mgr_);

    virtual ~hstore_nupm();

    const std::unique_ptr<dax_manager> & get_dax_manager() const override { return _dax_manager; }
    void pool_create_check(std::size_t) override;

    auto pool_create_1(
      const pool_path &path_
      , std::size_t size_
    ) -> nupm::region_descriptor override;

    auto pool_create_2(
      AK_FORMAL
      const nupm::region_descriptor &rac
      , component::IKVStore::flags_t flags
      , std::size_t expected_obj_count
    ) -> std::unique_ptr<open_pool_handle> override;

    nupm::region_descriptor pool_open_1(
      const pool_path &path_
    ) override;

    auto pool_open_2(
      AK_FORMAL
      const nupm::region_descriptor & v_
      , component::IKVStore::flags_t flags_
    ) -> std::unique_ptr<open_pool_handle> override;

    void pool_close_check(const std::string &) override;

    void pool_delete(const pool_path &path_) override;

    /* ERROR: want get_pool_regions(<proper type>, std::vector<::iovec>&) */
    nupm::region_descriptor pool_get_regions(const open_pool_handle &) const override;
  };
#pragma GCC diagnostic pop

#include "hstore_nupm.tcc"

#endif
