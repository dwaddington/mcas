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

#include "dax_map.h"
#include "dax_data.h"
#include "nd_utils.h"

#include <common/exceptions.h>
#include <common/utils.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h> /* ::open, ::lockf */
#include <boost/icl/split_interval_map.hpp>
#include <cinttypes>
#include <mutex>
#include <set>

//#define REGION_NAME "mcas-dax-pool"
#define DEBUG_PREFIX "Devdax_manager: "

static constexpr unsigned MAP_LOG_GRAIN = 21U;
static constexpr std::size_t MAP_GRAIN = std::size_t(1) << MAP_LOG_GRAIN;
static constexpr std::uint64_t MAP_HUGE = MAP_LOG_GRAIN << MAP_HUGE_SHIFT;

#ifndef MAP_SYNC
#define MAP_SYNC 0x80000
#endif

#ifndef MAP_SHARED_VALIDATE
#define MAP_SHARED_VALIDATE 0x03
#endif

#include <thread>
#include <sstream>


namespace dax_map
{
  int init_map_lock_mask()
  {
    /* env variable USE_ODP to indicate On Demand Paging may be used and therefore mapped memory need not be pinned */
    char* p = getenv("USE_ODP");
    bool odp = false;
    if ( p != nullptr )
    {
      errno = 0;
      odp = bool(std::strtoul(p,nullptr,10));

      auto e = errno;
      if ( e == 0 )
      {
        PLOG("USE_ODP=%d (%s on-demand paging)", int(odp), odp ? "using" : "not using");
      }
      else
      {
        PLOG("USE_ODP specification %s failed to parse: %s", p, ::strerror(e));
      }
    }
    return odp ? 0 : MAP_LOCKED;
  }

  const int effective_map_locked = init_map_lock_mask();
}

static std::set<std::string> nupm_devdax_manager_mapped;
static std::mutex nupm_devdax_manager_mapped_lock;

static bool register_instance(const std::string& path)
{
  std::lock_guard<std::mutex> g(nupm_devdax_manager_mapped_lock);
  bool inserted = nupm_devdax_manager_mapped.insert(path).second;
  if ( inserted )
  {
    PLOG("Registered dax mgr instance: %s", path.c_str());
  }
  return inserted;
}

static void unregister_instance(const std::string& path)
{
  std::lock_guard<std::mutex> g(nupm_devdax_manager_mapped_lock);
  nupm_devdax_manager_mapped.erase(path);
  PLOG("Unregistered dax mgr instance: %s", path.c_str());
}

void nupm::Devdax_manager::register_range(const void *begin, std::size_t size)
{
  auto c = static_cast<const char *>(begin);
  auto i = boost::icl::interval<const char *>::right_open(c, c+size);
  if ( intersects(_address_coverage, i) )
  {
    const void *end = c+size;
    std::ostringstream o;
    o << "range " << begin << ".." << end << " overlaps existing mapped storage";
    PLOG("%s: %s", __func__, o.str().c_str());
    throw std::domain_error(o.str().c_str());
  }
  _address_coverage.insert(i);
}

void nupm::Devdax_manager::deregister_range(const void *begin, std::size_t size)
{
  auto c = static_cast<const char *>(begin);
  auto i = boost::icl::interval<const char *>::right_open(c, c+size);
  _address_coverage.erase(i);
}

namespace nupm
{

Devdax_manager::Devdax_manager(const std::vector<config_t>& dax_configs,
                               bool force_reset)
  : _dax_configs(dax_configs)
  , _nd()
  , _mapped_regions()
  , _region_hdrs()
  , _reentrant_lock()
  , _address_coverage()
{
  /* set up each configuration */
  for(const auto& config: dax_configs) {

    CPLOG(0, DEBUG_PREFIX "region (%s,%lx)", config.path.c_str(), config.addr);

    /* Protection against using the same /dev/dax file in a single process */
    if(register_instance(config.path) == false) /*< only one instance of this class per dax path */
      throw Constructor_exception("Devdax_manager instance already managing path (%s)", config.path.c_str());

    auto mr = map_region(config.path, config.addr);

    /* Protection against using the same address twice in a single process */
    register_range(mr.iov.iov_base, mr.iov.iov_len); /*< only one instance of this class per dax path */

    auto itb = _mapped_regions.insert(mapped_regions::value_type(config.path, std::move(mr)));
    assert( itb.second );
    recover_metadata(config.path,
                     itb.first->second.iov.iov_base,
                     itb.first->second.iov.iov_len,
                     force_reset);
  }
}

Devdax_manager::~Devdax_manager()
{
  CPLOG(0, "%s::%s", _cname, __func__);
  /* EXCEPTION UNSAFE */
  for (auto &i : _mapped_regions) {
    auto j = ::munmap(i.second.iov.iov_base, i.second.iov.iov_len);
    CPLOG(2, "%s:%s: %d = munmap(%p, 0x%zx", _cname, __func__, j, i.second.iov.iov_base, i.second.iov.iov_len);
    unregister_instance(i.first);
    deregister_range(i.second.iov.iov_base, i.second.iov.iov_len);
  }
}

const char * Devdax_manager::lookup_dax_device(unsigned region_id)
{
  for(auto& config: _dax_configs) {
    if(config.region_id == region_id) return config.path.c_str();
  }
  throw Logic_exception("lookup_dax_device could not find path for region (%d)",
                        region_id);
  return nullptr;
}


void Devdax_manager::debug_dump(unsigned region_id)
{
  guard_t g(_reentrant_lock);
  _region_hdrs[lookup_dax_device(region_id)]->debug_dump();
}

void *Devdax_manager::open_region(uint64_t uuid,
                                  unsigned region_id,
                                  size_t * out_length)
{
  guard_t           g(_reentrant_lock);
  const char *      device = lookup_dax_device(region_id);
  DM_region_header *hdr    = _region_hdrs[device];
  if (hdr == nullptr)
    throw General_exception("no region header for device (%s)", device);

  return hdr->get_region(uuid, out_length);
}

void *Devdax_manager::create_region(uint64_t uuid, unsigned region_id, const size_t size)
{
  guard_t           g(_reentrant_lock);
  const char *      device = lookup_dax_device(region_id);

  DM_region_header *hdr    = _region_hdrs[device];
  if (hdr == nullptr)
    throw General_exception("no region header for device (%s)", device);

  auto size_in_grains = boost::numeric_cast<DM_region::grain_offset_t>(div_round_up(size, hdr->grain_size()));

  PLOG("Devdax_manager::create_region rounding up to %" PRIu32 " grains (%" PRIu64 " MiB)",
       size_in_grains, REDUCE_MiB((1UL << DM_REGION_LOG_GRAIN_SIZE)*size_in_grains));

  return hdr->allocate_region(uuid, size_in_grains); /* allocates n grains */
}

void Devdax_manager::erase_region(uint64_t uuid, unsigned region_id)
{
  guard_t           g(_reentrant_lock);
  const char *      device = lookup_dax_device(region_id);
  DM_region_header *hdr    = _region_hdrs[device];
  if (hdr == nullptr)
    throw General_exception("no region header for device (%s)", device);

  hdr->erase_region(uuid);
}

size_t Devdax_manager::get_max_available(unsigned region_id)
{
  guard_t           g(_reentrant_lock);
  const char *      device = lookup_dax_device(region_id);
  DM_region_header *hdr    = _region_hdrs[device];
  if (hdr == nullptr)
    throw General_exception("no region header for device (%s)", device);

  return hdr->get_max_available();
}

void Devdax_manager::recover_metadata(const std::string &device_path,
                                      void *      p,
                                      size_t      p_len,
                                      bool        force_rebuild)
{
  assert(p);
  DM_region_header *rh = static_cast<DM_region_header *>(p);

  bool rebuild = force_rebuild;
  if (!rh->check_magic()) rebuild = true;

  if (rebuild) {
    PLOG("Devdax_manager: rebuilding.");
    rh = new (p) DM_region_header(p_len);
    PLOG("Devdax_manager: rebuilt.");
  }
  else {
    PLOG("Devdax_manager: no rebuild.");
    rh->check_undo_logs();
  }

  _region_hdrs[device_path] = rh;
}

#if 0
void *Devdax_manager::get_devdax_region(const std::string &device_path,
                                        size_t *    out_length)
{
  auto it = _mapped_regions.find(device_path);
  if ( it != _mapped_regions.end() )
  {
    auto r = it->second.iov;
    if (out_length) *out_length = r.iov_len;
    return r.iov_base;
  }
  else
  {
    return nullptr;
  }
}
#endif

auto Devdax_manager::map_region(const std::string &path, const addr_t base_addr) -> Opened_region
{
  const auto base_ptr = reinterpret_cast<void *>(base_addr);
  /* cannot map if the map grain exceeds the region grain */
  assert(base_addr);
  assert(check_aligned(base_addr, MAP_GRAIN));

  /* open device */
  common::Fd_open fd_open(::open(path.c_str(), O_RDWR, 0666));

  if (fd_open.fd() == -1) throw General_exception("%s: could not open devdax path %s (%s): %s", __func__, path.c_str(), ::strerror(errno));

  std::ostringstream o;
  o << std::hex << std::showbase << std::this_thread::get_id();

  /* Protection against using the same /dev/dax file in different processes */
  if ( ::lockf(fd_open.fd(), F_TLOCK, 0) != 0 )
  {
    auto e = errno;
    CPLOG(0, DEBUG_PREFIX "thread %s region (%s) exclusive lock failed %s", o.str().c_str(), path.c_str(), ::strerror(errno));
    throw std::runtime_error(__func__ + std::string(" cannot exclusive-lock ") + path + ": " + ::strerror(e));
  }

  CPLOG(0, DEBUG_PREFIX "thread %s region (%s) opened ok", o.str().c_str(), path.c_str());

  /* get length of device */
  size_t size = 0;
  {
    struct stat statbuf;
    int         rc = fstat(fd_open.fd(), &statbuf);
    if (rc == -1) throw ND_control_exception("fstat call failed");
    if ( S_ISREG(statbuf.st_mode) )
    {
      size = size_t(statbuf.st_size);
    }
    else if ( S_ISCHR(statbuf.st_mode) )
    {
      size = get_dax_device_size(statbuf);
    }
    else
    {
      throw General_exception("dax_map excpects a regular file or a char device; file %s is neither");
    }
  }

  PLOG(DEBUG_PREFIX "%s size=%lu", path.c_str(), size);

  /* mmap it in */
  void *p;
  p = ::mmap(base_ptr,
       size, /* length = 0 means whole device  (contrary to man 3 mmap??) */
       PROT_READ | PROT_WRITE,
       int(MAP_SHARED_VALIDATE | MAP_FIXED | MAP_SYNC | MAP_HUGE) | dax_map::effective_map_locked,
       fd_open.fd(), 0 /* offset */);
  CPLOG(2, "%s:%s: %p = mmap(%p, 0x%zx, %s", _cname, __func__, p, base_ptr, size, dax_map::effective_map_locked ? "MAP_SYNC|locked" : "MAP_SYNC|not locked");

  if ( p == MAP_FAILED ) {
    p = ::mmap(base_ptr,
             size, /* length = 0 means whole device  (contrary to man 3 mmap??) */
             PROT_READ | PROT_WRITE,
             int(MAP_SHARED_VALIDATE | MAP_FIXED | MAP_HUGE) | dax_map::effective_map_locked,
             fd_open.fd(), 0 /* offset */);
    CPLOG(2, "%s:%s: %p = mmap(%p, 0x%zx, %s", _cname, __func__, p, base_ptr, size, dax_map::effective_map_locked ? "locked" : "not locked");
  }

  if ( p == MAP_FAILED ) {
    throw General_exception("mmap failed on %s (request %p): %s", path.c_str(), base_ptr, ::strerror(errno));
  }
  if (p != base_ptr) {
    throw General_exception("mmap failed on %s (request %p, got %p)", path.c_str(), base_ptr, p);
  }

  if(madvise(p, size, MADV_DONTFORK) != 0)
    throw General_exception("madvise 'don't fork' failed unexpectedly (%p %lu)",
		base_ptr, size);

  return Opened_region{::iovec{p, size}, std::move(fd_open)};
}
}  // namespace nupm
