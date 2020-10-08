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

#include "arena_fs.h"
#include <nupm/dax_manager.h>
#include <common/exceptions.h>
#include <common/fd_open.h>
#include <common/memory_mapped.h>
#include <common/utils.h>

#include <fcntl.h> /* posix_fallocate */
#include <boost/scope_exit.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h> /* ::open, ::lockf */
#include <experimental/filesystem>
#include <cinttypes>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>

static constexpr unsigned MAP_LOG_GRAIN = 21U;
static constexpr int MAP_HUGE = MAP_LOG_GRAIN << MAP_HUGE_SHIFT;

#ifndef MAP_SYNC
#define MAP_SYNC 0x80000
#endif

namespace fs = std::experimental::filesystem;

std::vector<common::memory_mapped> arena_fs::fd_mmap(int fd, const std::vector<::iovec> &map, int flags)
{
	std::vector<common::memory_mapped> mapped_elements;
	for ( const auto &e : map )
	{
		using namespace nupm;
		mapped_elements.emplace_back(
			e.iov_base
			, e.iov_len
			, PROT_READ | PROT_WRITE
			, flags
			, fd
		);

		if ( ! mapped_elements.back() )
		{
			mapped_elements.pop_back();
			flags &= ~MAP_SYNC;
			mapped_elements.emplace_back(
				e.iov_base
				, e.iov_len
				, PROT_READ | PROT_WRITE
				, flags
				, fd
			);
		}

		if ( ! mapped_elements.back().iov_base )
		{
			auto er = int(mapped_elements.back().iov_len);
			throw General_exception("%s: mmap failed for fsdax (request %p:0x%zu): %s", __func__, e.iov_base, e.iov_len, ::strerror(er));
		}

		if ( ::madvise(e.iov_base, e.iov_len, MADV_DONTFORK) != 0 )
		{
			auto er = errno;
			throw General_exception("%s: madvise 'don't fork' failed for fsdax (%p %lu): %s", __func__, e.iov_base, e.iov_len, ::strerror(er));
		}
	}

	return mapped_elements;
}

std::pair<std::vector<::iovec>, std::size_t> arena_fs::get_mapping(const fs::path &path_map)
{
	/* A region must always be mapped to the same address, as MCAS
	 * MCAS software uses absolute addresses. Current design is to
	 * save this in a file extended attribute, ahtough it could be
	 * saved in a specially-named file.
	 */
	std::vector<::iovec> m;
	std::ifstream f(path_map.c_str());
	std::size_t covered = 0;
	std::uint64_t addr;
	std::size_t size;
	f.unsetf(std::ios::dec|std::ios::hex|std::ios::oct);
	f >> addr >> size;
	while ( f.good() )
	{
		m.push_back(::iovec{reinterpret_cast<void *>(addr), size});
		covered += size;
		PLOG("%s %s: %p, 0x%zx", __func__, path_map.c_str(), m.back().iov_base, m.back().iov_len);
		f >> addr >> size;
	}
	return { m, covered };
}

std::vector<::iovec> arena_fs::get_mapping(const fs::path &path_map, const std::size_t expected_size)
{
	auto r = get_mapping(path_map);
	if ( r.second != expected_size )
	{
		std::ostringstream o;
		o << __func__ << ": map file " << path_map << std::hex << std::showbase << " expected to cover " << expected_size << " bytes, but covers " << r.second << " bytes";
		throw std::runtime_error(o.str());
	}
	return r.first;
}

arena_fs::arena_fs(const common::log_source &ls_, std::experimental::filesystem::path dir_)
  : arena(ls_)
  , _dir(dir_)
{
  PLOG("%s debug level %u", __func__, debug_level());
}

void arena_fs::debug_dump() const
{
  PLOG("%s::%s:i fsdax directory %s", _cname, __func__, _dir.c_str());
}

/* used only inside region_create */
void *arena_fs::region_create_inner(
	common::fd_locked &&fd
	, const path &path_
	, gsl::not_null<nupm::registry_memory_mapped *> const mh_
	, const std::vector<::iovec> &mapping_
)
try {
	auto entered = mh_->enter(std::move(fd), path_, mapping_);
	/* return the map key, or nullptr if already mapped */
	return entered ? mapping_.front().iov_base : nullptr;
}
catch ( const std::runtime_error &e )
{
	PLOG("%s: %s failed %s", __func__, path_.c_str(), e.what());
	return nullptr;
}

auto arena_fs::region_get(string_view id_) -> region_access
{
  return {path_data(id_), get_mapping(path_map(id_)).first};
}

auto arena_fs::region_create(
	const string_view id_
	, gsl::not_null<nupm::registry_memory_mapped *> const mh_
	, std::size_t size
) -> region_access
{
	/* A region is a file the the region_path directory.
	 * The file name is the id_.
	 */

	fs::create_directories(path_data(id_).remove_filename());

	try
	{
		common::fd_locked fd(::open(path_data(id_).c_str(), O_CREAT|O_EXCL|O_RDWR, 0666));

		if ( fd < 0 )
		{
			auto e = errno;
			PLOG("%s %i = open %s failed: %s", __func__, fd.fd(), path_data(id_).c_str(), ::strerror(e));
			return region_access();
		}
		CPLOG(1, "%s %i = open %s", __func__, fd.fd(), path_data(id_).c_str());

		auto path_data_local = path_data(id_);
		auto path_map_local = path_map(id_);

		/* file is created and opened */
		bool commit = false;

		BOOST_SCOPE_EXIT(&commit, &path_data_local) {
			if ( ! commit ) { ::unlink(path_data_local.c_str()); }
		} BOOST_SCOPE_EXIT_END

		/* Every region segment needs a unique address range. locate_free_address_range provides one.
		 */

		size = round_up_t(size, 1U<<21U);
		auto base_addr = mh_->locate_free_address_range(size);

		/* Extend the file to the specified size */
		auto e = ::posix_fallocate(fd.fd(), 0, ::off_t(size));
		if ( e != 0 )
		{
			PLOG("%s::%s posix_fallocate: %zu: %s", _cname, __func__, size, strerror(e));
			return region_access();
		}
		CPLOG(1, "%s posix_fallocate %i to %zu", __func__, fd.fd(), size);

		{
			std::ofstream f(path_map_local.c_str(), std::ofstream::trunc);
			CPLOG(1, "%s: write %s", __func__, path_map_local.c_str());
			f << std::showbase << std::hex << base_addr << " " << size << std::endl;
		}

		fs::path map_path_local = path_map(id_);

		BOOST_SCOPE_EXIT(&commit, &map_path_local) {
			if ( ! commit )
			{
				std::error_code ec;
				fs::remove(map_path_local, ec);
			}
		} BOOST_SCOPE_EXIT_END;

		using namespace nupm;

		auto v = region_create_inner(std::move(fd), path_data_local, mh_, std::vector<::iovec>{{base_addr, size}});
		if ( v )
		{
			commit = true;
		}
		return region_access(path_data_local, region_access::second_type(1, ::iovec{v, size}));
	}
	catch (const std::exception & e)
	{
		PLOG("%s: create %p failed: %s", __func__, id_.begin(), e.what());
		return region_access();
	}
}

void arena_fs::region_erase(string_view id_, gsl::not_null<nupm::registry_memory_mapped *> mh_)
{
	namespace fs = std::experimental::filesystem;
	auto path_data_local = path_data(id_);
	CPLOG(1, "%s remove %s", __func__, path_data_local.c_str());
	fs::remove(path_data(id_));
	CPLOG(1, "%s remove %s", __func__, path_map(id_).c_str());
	fs::remove(path_map(id_));
	mh_->remove(path_data_local);
}

std::size_t arena_fs::get_max_available()
{
  return 0; /* .. until someone needs an actual value */
}
