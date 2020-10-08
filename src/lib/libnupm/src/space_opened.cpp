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

#include "space_opened.h"
#include "dax_manager.h"
#include "arena.h"
#include "arena_dev.h"
#include "arena_fs.h"
#include "arena_none.h"
#include "dax_data.h"
#include "nd_utils.h"

#include <common/exceptions.h>
#include <common/fd_locked.h>
#include <common/memory_mapped.h>
#include <common/utils.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h> /* ::open, ::lockf */
#include <boost/scope_exit.hpp>
#include <boost/icl/split_interval_map.hpp>
#include <gsl/pointers>
#include <experimental/filesystem>
#include <cinttypes>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>

//#define REGION_NAME "mcas-dax-pool"
#define DEBUG_PREFIX "dax_manager: "

static constexpr unsigned MAP_LOG_GRAIN = 21U;
static constexpr std::size_t MAP_GRAIN = std::size_t(1) << MAP_LOG_GRAIN;
static constexpr int MAP_HUGE = MAP_LOG_GRAIN << MAP_HUGE_SHIFT;

#ifndef MAP_SYNC
#define MAP_SYNC 0x80000
#endif

#ifndef MAP_SHARED_VALIDATE
#define MAP_SHARED_VALIDATE 0x03
#endif

#include <thread>
#include <sstream>

namespace fs = std::experimental::filesystem;

std::vector<common::memory_mapped> nupm::range_use::address_coverage_check(std::vector<common::memory_mapped> &&iovm_)
{
	using AC = boost::icl::interval_set<char *>;
	AC this_coverage;
	for ( const auto &e : iovm_ )
	{
		auto c = static_cast<char *>(e.iov_base);
		auto i = boost::icl::interval<char *>::right_open(c, c+e.iov_len);
		if ( intersects(_dm->_address_coverage, i) )
		{
			const void *end = c+e.iov_len;
			std::ostringstream o;
			o << "range " << e.iov_base << ".." << end << " overlaps existing mapped storage";
			PLOG("%s: %s", __func__, o.str().c_str());
			throw std::domain_error(o.str().c_str());
		}
		this_coverage.insert(i);
	}
	_dm->_address_coverage += this_coverage;
	_dm->_address_fs_available -= this_coverage;

	return std::move(iovm_);
}

nupm::range_use::range_use(dax_manager *dm_, std::vector<common::memory_mapped> &&iovm_)
  : _dm(dm_)
  , _iovm(address_coverage_check(std::move(iovm_)))
{
}

nupm::range_use::~range_use()
{
	if ( bool(_dm) )
	{
		for ( const auto &e : _iovm )
		{
			auto c = static_cast<char *>(e.iov_base);
			auto i = boost::icl::interval<char *>::right_open(c, c+e.iov_len);
			_dm->_address_coverage.erase(i);
			_dm->_address_fs_available.insert(i);
		}
	}
}

std::vector<common::memory_mapped> nupm::space_opened::map_dev(int fd, const addr_t base_addr)
{
  /* cannot map if the map grain exceeds the region grain */
  assert(base_addr);
  assert(check_aligned(base_addr, MAP_GRAIN));

  const auto base_ptr = reinterpret_cast<void *>(base_addr);

  std::size_t len;
  /* get length of device */
  {
    struct stat statbuf;
    int         rc = fstat(fd, &statbuf);
    if (rc == -1) throw ND_control_exception("fstat call failed");
    if ( S_ISREG(statbuf.st_mode) )
    {
      len = size_t(statbuf.st_size);
    }
    else if ( S_ISCHR(statbuf.st_mode) )
    {
      len = get_dax_device_size(statbuf);
    }
    else
    {
      throw General_exception("dax_map excpects a regular file or a char device; file %s is neither");
    }
  }

  PLOG(DEBUG_PREFIX "fd %i size=%lu", fd, len);

  /* mmap it in */
  common::memory_mapped iovm(
    base_ptr
    , len /* length = 0 means whole device (contrary to man 3 mmap??) */
    , PROT_READ | PROT_WRITE
    , MAP_SHARED_VALIDATE | MAP_FIXED | MAP_SYNC | MAP_HUGE | dax_manager::effective_map_locked
    , fd
  );
  CPLOG(1, "%s: %p = mmap(%p, 0x%zx, %s", __func__, iovm.iov_base, base_ptr, iovm.iov_len, dax_manager::effective_map_locked ? "MAP_SYNC|locked" : "MAP_SYNC|not locked");

  if ( ! iovm ) {
    iovm =
      common::memory_mapped(
        base_ptr
        , len /* length = 0 means whole device (contrary to man 3 mmap??) */
        , PROT_READ | PROT_WRITE
        , MAP_SHARED_VALIDATE | MAP_FIXED | MAP_HUGE | dax_manager::effective_map_locked
        , fd
      );

    CPLOG(1, "%s: %p = mmap(%p, 0x%zx, %s", __func__, iovm.iov_base, base_ptr, iovm.iov_len, dax_manager::effective_map_locked ? "locked" : "not locked");
  }

  if ( ! iovm ) {
    throw General_exception("mmap failed on fd %i (request %p): %s", fd, base_ptr, ::strerror(errno));
  }
  if (iovm.iov_base != base_ptr) {
    throw General_exception("mmap failed on fd %i (request %p, got %p)", fd, base_ptr, iovm.iov_base);
  }

  /* ERROR: throw after resource acquired */
  if ( madvise(iovm.iov_base, iovm.iov_len, MADV_DONTFORK) != 0 )
  {
    auto e = errno;
    throw General_exception("%s: madvise 'don't fork' failed unexpectedly (%p %lu) : %s",
        iovm.iov_base, iovm.iov_len, ::strerror(e));
  }
  std::vector<common::memory_mapped> v;
  v.push_back(std::move(iovm));
  return v;
}

std::vector<common::memory_mapped> nupm::space_opened::map_fs(int fd, const std::vector<::iovec> &mapping)
{
  return arena_fs::fd_mmap(fd, mapping, MAP_SHARED_VALIDATE | MAP_FIXED | MAP_SYNC | MAP_HUGE);
}

/* space_opened constructor for devdax: filename, single address, unknown size */
nupm::space_opened::space_opened(
  const common::log_source & ls_
  , dax_manager * dm_
  , const std::string &p
  , const addr_t base_addr
)
try
  : common::log_source((PLOG("%s: %d", __func__, __LINE__), ls_))
  , _fd_locked(::open(p.c_str(), O_RDWR, 0666))
  , _range(dm_, map_dev(_fd_locked.fd(), base_addr))
{
}
catch ( std::exception &e )
{
	PLOG("%s: path %s exception %s", __func__, p.c_str(), e.what());
	throw;
}

/* space_opened constructor for fsdax: filename, multiple mappings, unknown size */
nupm::space_opened::space_opened(
  const common::log_source & ls_
  , dax_manager * dm_
  , const std::string &p
  , const std::vector<::iovec> &mapping
)
try
  : common::log_source((PLOG("%s: %d", __func__, __LINE__), ls_))
  , _fd_locked(::open(p.c_str(), O_RDWR, 0666))
  , _range(dm_, map_fs(_fd_locked.fd(), mapping))
{
}
catch ( std::exception &e )
{
	PLOG("%s: path %s exception %s", __func__, p.c_str(), e.what());
	throw;
}

/* space_opened constructor for fsdax: filename, multiple mappings, unknown size */
nupm::space_opened::space_opened(
  const common::log_source & ls_
  , dax_manager * dm_
  , common::fd_locked &&fd_
  , const std::string &p
  , const std::vector<::iovec> &mapping
)
try
  : common::log_source((PLOG("%s: %d", __func__, __LINE__), ls_))
  , _fd_locked(std::move(fd_))
  , _range(dm_, map_fs(_fd_locked.fd(), mapping))
{
}
catch ( std::exception &e )
{
	PLOG("%s: path %s exception %s", __func__, p.c_str(), e.what());
	throw;
}
