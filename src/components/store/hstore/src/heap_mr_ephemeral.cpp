/*
   Copyright [2017-2021] [IBM Corporation]
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

#include "heap_mr_ephemeral.h"

#include "hstore_config.h"
#include "mm_plugin_itf.h"
#include <common/errors.h> /* S_OK, E_INVAL */

heap_mr_shim::heap_mr_shim(string_view path)
	: _mm(std::string(path), "", nullptr)
{
}

status_t heap_mr_shim::allocate(
	void * & ptr
	, std::size_t bytes
	, std::size_t alignment
)
{
	return
		alignment
		? _mm.aligned_allocate( bytes, alignment, &ptr)
		: _mm.allocate(bytes, &ptr)
		;
}

status_t heap_mr_shim::free(
	void * & ptr
	, std::size_t bytes
)
{
	return
		bytes
		? _mm.deallocate(&ptr, bytes)
		: _mm.deallocate_without_size(&ptr)
		;
}

status_t heap_mr_shim::inject_allocation(void *ptr, std::size_t size)
{
	return _mm.inject_allocation(ptr, size);
}

status_t heap_mr_shim::remaining(
	std::size_t & // out_size
) const
{
	throw std::runtime_error("remaining not supported");
}


void heap_mr_shim::add_managed_region(byte_span region_)
{
	_mm.add_managed_region(::base(region_), ::size(region_));
}

constexpr unsigned heap_mr_ephemeral::log_min_alignment;
constexpr unsigned heap_mr_ephemeral::hist_report_upper_bound;

heap_mr_ephemeral::heap_mr_ephemeral(
	unsigned debug_level_
	, const string_view plugin_path_
	, const string_view id_
	, const string_view backing_file_
)
	: common::log_source(debug_level_)
	, _heap(plugin_path_)
	, _managed_regions(id_, backing_file_, {})
	, _allocated(0)
	, _capacity(0)
	, _reconstituted()
	, _hist_alloc()
	, _hist_inject()
	, _hist_free()
{}

void heap_mr_ephemeral::add_managed_region(
	const byte_span &r_full
	, const byte_span &r_heap
	, const unsigned // numa_node
)
{
	_heap.add_managed_region(r_heap);
	CPLOG(2, "%s : %p.%zx", __func__, ::base(r_heap), ::size(r_heap));
	_managed_regions.address_map_push_back(r_full);
	_capacity += ::size(r_heap);
}

void heap_mr_ephemeral::inject_allocation(
	void *p_
	, std::size_t sz_
	, unsigned // numa_node_
)
{
	_heap.inject_allocation(p_, sz_);
	{
		auto pc = static_cast<alloc_set_t::element_type>(p_);
		_reconstituted.add(alloc_set_t::segment_type(pc, pc + sz_));
	}
	_allocated += sz_;
	_hist_alloc.enter(sz_);
}

void *heap_mr_ephemeral::allocate(
	std::size_t sz_
	, unsigned // _numa_node_
	, std::size_t alignment_
)
{
	void *p = 0;
	if ( S_OK != _heap.allocate(p, sz_, alignment_) )
	{
		throw std::bad_alloc{};
	}
	_allocated += sz_;
	_hist_alloc.enter(sz_);
	return p;
}

void heap_mr_ephemeral::free(
	void *p_
	, std::size_t sz_
	, unsigned // numa_node_
)
{
	_heap.free(p_, sz_);
	_allocated -= sz_;
	_hist_free.enter(sz_);
}

bool heap_mr_ephemeral::is_reconstituted(const void * p_) const
{
	return contains(_reconstituted, static_cast<alloc_set_t::element_type>(p_));
}
