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

#include "arena_dev.h"

#include <city.h> /* CityHash */
#include <nupm/dax_data.h>
#include <cinttypes>

namespace
{
	std::uint64_t make_uuid(const arena_dev::string_view id_)
	{
		return ::CityHash64(id_.begin(), id_.size());
	}
}

arena_dev::arena_dev(const common::log_source &ls_, gsl::not_null<nupm::DM_region_header *> hdr)
  : arena(ls_)
  , _hdr(hdr)
{}

void arena_dev::debug_dump() const
{
  _hdr->debug_dump();
}

auto arena_dev::region_get(const string_view id_) -> region_access
{
  ::iovec iov;
  iov.iov_base = _hdr->get_region(make_uuid(id_), &iov.iov_len);
  std::vector<::iovec> v;
  if ( iov.iov_base != nullptr )
  {
    v.push_back(iov);
  }
  return region_access{std::string(), v};
}

auto arena_dev::region_create(const string_view id_, gsl::not_null<nupm::registry_memory_mapped *>, const std::size_t size) -> region_access
{
  auto size_in_grains = boost::numeric_cast<nupm::DM_region::grain_offset_t>(div_round_up(size, _hdr->grain_size()));

  PLOG("%s::%s: rounding up to %" PRIu32 " grains (%" PRIu64 " MiB)", _cname, __func__,
       size_in_grains, REDUCE_MiB((1UL << DM_REGION_LOG_GRAIN_SIZE)*size_in_grains));

  return
    region_access(
      region_access::first_type()
      , region_access::second_type(1, ::iovec{_hdr->allocate_region(make_uuid(id_), size_in_grains), size_in_grains * _hdr->grain_size()})
    ); /* allocates n grains */
}

void arena_dev::region_erase(const string_view id_, gsl::not_null<nupm::registry_memory_mapped *>)
{
  _hdr->erase_region(make_uuid(id_));
}

std::size_t arena_dev::get_max_available() { return _hdr->get_max_available(); }
