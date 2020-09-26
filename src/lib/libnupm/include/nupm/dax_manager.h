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


/*
 * Authors:
 *
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */

#ifndef __NUPM_DAX_MANAGER_H__
#define __NUPM_DAX_MANAGER_H__

#include "nd_utils.h"
#include <common/fd_open.h>
#include <common/logging.h>
#include <common/memory_mapped.h>
#include <common/moveable_ptr.h>
#include <experimental/filesystem>
#include <experimental/string_view>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/right_open_interval.hpp>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>
#include <sys/uio.h>

struct iovec_owned
{
  common::moveable_ptr<void> iov_base;
  std::size_t iov_len;
  explicit iovec_owned(void *iov_base_, std::size_t iov_len_)
    : iov_base(iov_base_)
    , iov_len(iov_len_)
  {}
  iovec_owned(iovec_owned &&o_) noexcept = default;
  iovec_owned &operator=(iovec_owned &&) noexcept = default;
};

/* Control of the space behind a single dax_config_t entry.
 * The arena key is clled region_id in dax_config_t.
 * One per arena_id. Not called a region because
 * {create,open,erase}region functions use region to mean something else.
 */
struct arena;

namespace nupm
{
struct dax_manager;
class DM_region_header;

struct path_use
{
private:
  /* Would include a common::moveable_ptr<dax_manager>, except that the
   * registry is static, potentially covering multiple dax_manager instances.
   */
  std::string _path;
public:
  path_use(const std::string &path);
  path_use(const path_use &) = delete;
  path_use &operator=(const path_use &) = delete;
  path_use(path_use &&) noexcept;
  ~path_use();
};

struct range_use
{
private:
  common::moveable_ptr<dax_manager> _dm;
  /* Note: arena_fs may someday use multiple ranges */
  std::vector<common::memory_mapped> _iovm;

  std::vector<common::memory_mapped> address_coverage_check(std::vector<common::memory_mapped> &&iovm);
public:
  ::iovec iov(std::size_t i) { return _iovm[i].iov(); }
  range_use(dax_manager *dm_, std::vector<common::memory_mapped> &&);
  range_use(const range_use &) = delete;
  range_use &operator=(const range_use &) = delete;
  range_use(range_use &&) noexcept = default;
  ~range_use();
};

struct opened_space : private common::log_source
{
  common::Fd_open _fd_open;
  /* Note: arena_fs may someday use multiple ranges */
  range_use _range;

  /* owns the file mapping */
  std::vector<common::memory_mapped> map_dev(int fd, const addr_t base_addr);
  std::vector<common::memory_mapped> map_fs(int fd, const std::vector<::iovec> &mapping);
public:
  opened_space(const common::log_source &, dax_manager * dm_, const std::string &path, const addr_t base_addr);
  opened_space(const common::log_source &, dax_manager * dm_, const std::string &path, const std::vector<::iovec> &mapping);
  opened_space(opened_space &&) noexcept = default;
#if 0
  opened_space &operator=(opened_space &&) noexcept = default;
#endif
};

struct registered_opened_space
{
private:
  path_use _pu;
public:
  opened_space _or;
public:
  registered_opened_space(
    const common::log_source &ls_
    , dax_manager * dm_
    , const std::string &path_
    , addr_t base_addr_
  )
    : _pu(path_)
    , _or(ls_, dm_, path_, base_addr_)
  {}
  registered_opened_space(
    const common::log_source &ls_
    , dax_manager * dm_
    , const std::string &path_
    , const std::vector<::iovec> &mapping_
  )
    : _pu(path_)
    , _or(ls_, dm_, path_, mapping_)
  {}
  registered_opened_space(const registered_opened_space &) = delete;
  registered_opened_space &operator=(const registered_opened_space &) = delete;
  registered_opened_space(registered_opened_space &&) noexcept = default;
#if 0
  registered_opened_space &operator=(registered_opened_space &&) noexcept = default;
#endif
};

struct registry_memory_mapped
{
  virtual ~registry_memory_mapped() {}
  virtual bool enter(void *, std::vector<common::memory_mapped> &&) = 0;
  virtual void remove(void *) = 0;
  virtual void * allocate_address_range(std::size_t size) = 0;
};

/**
 * Lowest level persisent manager for devdax devices. See dax_map.cc for static
 * configuration.
 *
 */
struct dax_manager : protected common::log_source, private registry_memory_mapped {
 private:
  static constexpr const char *_cname = "dax_manager";

 public:
  using arena_id_t = unsigned;
  using string_view = std::experimental::string_view;
  static const int effective_map_locked;

  struct config_t {
    std::string path;
    addr_t addr;
    arena_id_t region_id;
    /* Through no fault of its own, config_t may begin life with no proper values */
    config_t() : path(), addr(0), region_id(0) {}
  };

  struct config_mapped
  {
    std::string path;
    addr_t addr;
  };

  /**
   * Constructor e.g.
     nupm::dax_manager ddm({{"/dev/dax0.3", 0x9000000000, 0},
                               {"/dev/dax1.3", 0xa000000000, 1}},
                                true);
   *
   * @param dax_config Vector of dax-path, address, arena_id tuples.
   * @param force_reset
   */
  dax_manager(const common::log_source &ls, const std::vector<config_t>& dax_config,
                 bool force_reset = false);

  /**
   * Destructor will not unmap memory/nor invalidate pointers?
   *
   */
  ~dax_manager();

  /**
   * Open a region of memory
   *
   * @param id Unique identifier
   * @param arena_id Arena identifier
   * @param out_length Out length of region in bytes
   *
   * @return (pointer, length) pairs to the mapped memory, or empty vector
   * if not found.
   * Until fsdax supports extending a region, the vector will not be more
    * than one element long.
   */
  std::vector<::iovec> open_region(string_view id, arena_id_t arena_id);

  /**
   * Create a new region of memory
   *
   * @param id Unique identifier
   * @param arena_id Arena identifier
   * @param size Size of the region requested in bytes
   *
   * @return Pointer to mapped memory
   */
  void *create_region(string_view id, arena_id_t arena_id, const size_t size);

  /**
   * Erase a previously allocated region
   *
   * @param id Unique region identifier
   * @param arena_id Arena identifier
   */
  void erase_region(string_view id, arena_id_t arena_id);

  /**
   * Get the maximum "hole" size.
   *
   *
   * @return Size in bytes of max hole
   */
  size_t get_max_available(arena_id_t arena_id);

  /**
   * Debugging information
   *
   * @param arena_id Arena identifier
   */
  void debug_dump(arena_id_t arena_id);

  void register_range(const void *begin, std::size_t size);
  void deregister_range(const void *begin, std::size_t size);
  void * allocate_address_range(std::size_t size) override;
 private:
  opened_space map_space(const std::string &path, addr_t base_addr);
  DM_region_header *recover_metadata(
                         ::iovec iov,
                         bool    force_rebuild = false);
  arena *lookup_arena(arena_id_t arena_id);
  /* callback for arena_dax to register mapped memory */
  bool enter(void *, std::vector<common::memory_mapped> &&) override;
  void remove(void *) override;
  using path = std::experimental::filesystem::path;
  using directory_entry = std::experimental::filesystem::directory_entry;
  void data_map_remove(const directory_entry &e);
  void map_register(const directory_entry &e);
  void files_scan(const path &p, void (dax_manager::*action)(const directory_entry &));
  std::unique_ptr<arena> make_arena_fs(const path &p, addr_t base, bool force_reset);
  std::unique_ptr<arena> make_arena_dev(const path &p, addr_t base, bool force_reset);

 private:
  using guard_t = std::lock_guard<std::mutex>;
  using mapped_spaces = std::map<std::string, registered_opened_space>;

  ND_control                                _nd;
  using AC = boost::icl::interval_set<char *>;
  AC                                        _address_coverage;
  AC                                        _address_fs_available;
  mapped_spaces                             _mapped_spaces;
  std::map<arena_id_t, std::unique_ptr<arena>> _arenas;
  std::mutex                                _reentrant_lock;
  using memory_mapped = std::map<void *, std::vector<common::memory_mapped>>;
  memory_mapped _memory_mapped;
 public:
  friend struct nupm::range_use; /* access to _address_coverage */
};
}  // namespace nupm

#endif
