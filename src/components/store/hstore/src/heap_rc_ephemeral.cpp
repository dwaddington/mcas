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

#include "heap_rc_ephemeral.h"

#include "hstore_config.h"

constexpr unsigned heap_rc_ephemeral::log_min_alignment;
constexpr unsigned heap_rc_ephemeral::hist_report_upper_bound;

heap_rc_ephemeral::heap_rc_ephemeral(
	unsigned debug_level_
	, const std::string & id_
	, const std::string & backing_file_
)
	: common::log_source(debug_level_)
	, _heap()
	, _managed_regions(id_, backing_file_, {})
	, _allocated(0)
	, _capacity(0)
	, _reconstituted()
	, _hist_alloc()
	, _hist_inject()
	, _hist_free()
{}

void heap_rc_ephemeral::add_managed_region(const ::iovec &r_full, const ::iovec &r_heap, const unsigned numa_node)
{
	_heap.add_managed_region(r_heap.iov_base, r_heap.iov_len, int(numa_node));
	CPLOG(2, "%s : %p.%zx", __func__, r_heap.iov_base, r_heap.iov_len);
	_managed_regions.address_map.push_back(r_full);
	_capacity += r_heap.iov_len;
}

void heap_rc_ephemeral::inject_allocation(void *p_, std::size_t sz_, unsigned numa_node_)
{
	_heap.inject_allocation(p_, sz_, int(numa_node_));
	{
		auto pc = static_cast<alloc_set_t::element_type>(p_);
		_reconstituted.add(alloc_set_t::segment_type(pc, pc + sz_));
	}
	_allocated += sz_;
	_hist_alloc.enter(sz_);
}

void *heap_rc_ephemeral::allocate(std::size_t sz_, unsigned _numa_node_, std::size_t alignment_)
{
	auto p = _heap.alloc(sz_, int(_numa_node_), alignment_);
	_allocated += sz_;
	_hist_alloc.enter(sz_);
	return p;
}

void heap_rc_ephemeral::free(void *p_, std::size_t sz_, unsigned numa_node_)
{
	_heap.free(p_, int(numa_node_), sz_);
	_allocated -= sz_;
	_hist_free.enter(sz_);
}

bool heap_rc_ephemeral::is_reconstituted(const void * p_) const
{
	return contains(_reconstituted, static_cast<alloc_set_t::element_type>(p_));
}
