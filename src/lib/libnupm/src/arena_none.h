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

#ifndef _MCAS_NUPM_ARENA_NONE_
#define _MCAS_NUPM_ARENA_NONE_

#include "arena.h"

#include <experimental/filesystem>

/* An unsupported arena */
struct arena_none
	: arena
{
private:
	constexpr static const char *_cname = "arena_none";
	using path = std::experimental::filesystem::path;
	path _dir;
public:
	arena_none(const common::log_source &ls, path dir_) : arena(ls), _dir{dir_} {}
	region_access region_get(
		string_view // id
	) override
	{ return region_access{}; }
	region_access region_create(
		string_view // id
		, gsl::not_null<nupm::registry_memory_mapped *> // mh
		, std::size_t // size
	) override
	{ return region_access{}; }
	void region_erase(
		string_view // id
		, gsl::not_null<nupm::registry_memory_mapped *> // mh
	) override {};
	std::size_t get_max_available() override { return 0; }
	bool is_file_backed() const override { return false; }
	void debug_dump() const override {}
};

#endif
