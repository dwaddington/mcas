/*
   Copyright [2020] [IBM Corporation]
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

#ifndef _MCAS_NUPM_ARENA_FS_
#define _MCAS_NUPM_ARENA_FS_

#include "arena.h"

#include <common/logging.h>
#include <common/fd_locked.h>

#include <experimental/filesystem>
#include <string>

namespace common
{
	struct memory_mapped;
}

/* An arena implemented by an fsdax directory */
struct arena_fs
	: arena
{
private:
	constexpr static const char *_cname = "arena_fs";
	using path = std::experimental::filesystem::path;
	path _dir;

	void *region_create_inner(
		common::fd_locked &&fd
		, const path &path_
		, gsl::not_null<nupm::registry_memory_mapped *> mh
		, const std::vector<::iovec> &mapping
	);
	path path_data(string_view id) const
	{
		using namespace std::string_literals;
		return _dir / ( std::string(id) + ".data"s );
	}
	path path_map(string_view id) const
	{
		using namespace std::string_literals;
		return _dir / ( std::string(id) + ".map"s );
	}
	static std::vector<::iovec> get_mapping(const path &path_map, const std::size_t expected_size);
public:
	arena_fs(const common::log_source &ls, path dir);
	region_access region_get(string_view id) override;
	region_access region_create(string_view id, gsl::not_null<nupm::registry_memory_mapped *> mh, std::size_t size) override;
	void region_erase(string_view id, gsl::not_null<nupm::registry_memory_mapped *> mh) override;
	std::size_t get_max_available() override;
    bool is_file_backed() const override { return true; }
	void debug_dump() const override;
	static std::pair<std::vector<::iovec>, std::size_t> get_mapping(const path &path_map);
	static std::vector<common::memory_mapped> fd_mmap(int fd, const std::vector<::iovec> &map, int flags);
};

#endif
