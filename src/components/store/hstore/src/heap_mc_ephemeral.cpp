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

#include "heap_mc_ephemeral.h"

#include "as_pin.h"
#include "as_emplace.h"
#include "as_extend.h"
#include "mm_plugin_itf.h"
#include <common/errors.h> /* S_OK */
#include <cassert>
#include <cstdlib> /* getenv */
#include <memory> /* make_unique */
#include <numeric> /* accumulate */
#include <utility>

constexpr unsigned heap_mc_ephemeral::log_min_alignment;
constexpr unsigned heap_mc_ephemeral::hist_report_upper_bound;

heap_mc_shim::heap_mc_shim(common::string_view path)
	: _mm(std::string(path), "", nullptr)
{
}

bool heap_mc_shim::reconstitute(
	ccpm::region_span // regions
	, ccpm::ownership_callback_t // resolver
	, bool // force_init
)
{
	return true; /* No reconstitute support, so every use must be "initialize". */
}

status_t heap_mc_shim::allocate(
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

status_t heap_mc_shim::free(
	void * & ptr
	, std::size_t bytes
)
{
	return
		bytes
		? _mm.deallocate(ptr, bytes)
		: _mm.deallocate_without_size(ptr)
		;
}

status_t heap_mc_shim::remaining(
	std::size_t & // out_size
) const
{
	throw std::runtime_error("remaining not supported");
}

ccpm::region_vector_t heap_mc_shim::get_regions() const
{
	ccpm::region_vector_t r{};
	status_t rc;
	unsigned region_id = 0;
	do
	{
		void *region_base;
		std::size_t region_size;
		rc = const_cast<MM_plugin_wrapper &>(_mm).query_managed_region(region_id, &region_base, &region_size);
		if ( rc != E_INVAL )
		{
			r.push_back(common::make_byte_span(static_cast<common::byte *>(region_base), region_size));
		}
		++region_id;
	} while ( rc == S_MORE );
	return r;
}

void heap_mc_shim::add_regions(ccpm::region_span regions)
{
	for ( auto r : regions )
	{
		_mm.add_managed_region(::base(r), ::size(r));
	}
	return;
}

bool heap_mc_shim::includes(
	const void * // ptr
) const
{
	throw std::runtime_error("remaining not supported");
}

heap_mc_ephemeral::heap_mc_ephemeral(
	unsigned debug_level_
	, impl::allocation_state_emplace *ase_
	, impl::allocation_state_pin *aspd_
	, impl::allocation_state_pin *aspk_
	, impl::allocation_state_extend *asx_
	, std::unique_ptr<ccpm::IHeap_expandable> p_
	, string_view id_
	, string_view backing_file_
	, const std::vector<byte_span> rv_full_
	, const byte_span pool0_heap_
)
	: common::log_source(debug_level_)
	, _heap(std::move(p_))
	, _managed_regions((_heap->add_regions(ccpm::region_span(&*ccpm::region_vector_t(pool0_heap_).begin(), 1)), id_), backing_file_, rv_full_)
	, _capacity(
		::size(pool0_heap_)
		+
		::size(
			std::accumulate(
/* Note: rv_full_ must contain at least the first element, representing pool 0 */
				rv_full_.begin() + 1
				, rv_full_.end()
				, byte_span{}
				, [] (const auto &a, const auto &b) -> byte_span
					{
						return {nullptr, ::size(a) + ::size(b)};
					}
			)
		)
	)
#if 0
	, _allocated(
		[this] ()
		{
			std::size_t r;
			auto rc = _heap->remaining(r);
			return _capacity - (rc == S_OK ? r : 0);
		} ()
	)
#endif
	, _ase(ase_)
	, _aspd(aspd_)
	, _aspk(aspk_)
	, _asx(asx_)
	, _hist_alloc()
	, _hist_inject()
	, _hist_free()
{

  for ( const auto &r : rv_full_ )
  {
    CPLOG(2, "%s : %p.%zx", __func__, ::base(r), ::size(r));
  }
  CPLOG(2, "%s : pool0_heap: %p.%zx", __func__, ::base(pool0_heap_), ::size(pool0_heap_));
}

heap_mc_ephemeral::heap_mc_ephemeral(
	unsigned debug_level_
	, common::string_view plugin_path_
	, impl::allocation_state_emplace *ase_
	, impl::allocation_state_pin *aspd_
	, impl::allocation_state_pin *aspk_
	, impl::allocation_state_extend *asx_
	, string_view id_
	, string_view backing_file_
	, const std::vector<byte_span> rv_full_
	, const byte_span pool0_heap_
)
	: heap_mc_ephemeral(
		debug_level_
		, ase_, aspd_, aspk_, asx_
		, std::make_unique<heap_mc_shim>(plugin_path_)
		, id_
		, backing_file_
		, rv_full_
		, pool0_heap_
	)
{
}

heap_mc_ephemeral::heap_mc_ephemeral(
	unsigned debug_level_
	, common::string_view plugin_path_
	, impl::allocation_state_emplace *ase_
	, impl::allocation_state_pin *aspd_
	, impl::allocation_state_pin *aspk_
	, impl::allocation_state_extend *asx_
	, string_view id_
	, string_view backing_file_
	, const std::vector<byte_span> rv_full_
	, const byte_span pool0_heap_
	, ccpm::ownership_callback_t
)
	: heap_mc_ephemeral(
		debug_level_
		, ase_, aspd_, aspk_, asx_
		, std::make_unique<heap_mc_shim>(plugin_path_)
		, id_
		, backing_file_
		, rv_full_
		, pool0_heap_
		)
{
}

void heap_mc_ephemeral::add_managed_region(
	const byte_span r_full
	, const byte_span r_heap
	, const unsigned // numa_node
)
{
	CPLOG(0, "%s before IHeap::add_regions size %zu", __func__, _heap->get_regions().size());
	for ( const auto &r : _heap->get_regions() )
	{
		CPLOG(0, "%s IHeap regions: %p.%zx", __func__, ::base(r), ::size(r));
	}
	ccpm::region_span::value_type rs[1] { r_heap };
	_heap->add_regions(rs);
	CPLOG(0, "%s : %p.%zx", __func__, ::base(r_heap), ::size(r_heap));
	_managed_regions.address_map_push_back(r_full);
	_capacity += ::size(r_heap);
	CPLOG(0, "%s after IHeap::add_regions size %zu", __func__, _heap->get_regions().size());
	for ( const auto &r : _heap->get_regions() )
	{
		CPLOG(0, "%s IHeap regions: %p.%zx", __func__, ::base(r), ::size(r));
	}
}

std::size_t heap_mc_ephemeral::free(persistent_t<void *> *p_, std::size_t sz_)
{
	/* Our free does not know the true size, because alignment is not known.
	 * But the pool free will know, as it can see how much has been allocated.
	 *
	 * The free, however, does not return a size. Pretend that it does.
	 */
#if USE_CC_HEAP == 4
	/* Note: order of testing is important. An extend arm+allocate) can occur while
	 * emplace is armed, but not vice-versa
	 */
	if ( _asx->is_armed() )
	{
		CPLOG(1, PREFIX "unexpected segment deallocation of %p of %zu", LOCATION, persistent_ref(*p_), sz_);
		abort();
#if 0
		_asx->record_deallocation(&persistent_ref(*p_), persister_nupm());
#endif
	}
	else if ( _ase->is_armed() )
	{
		_ase->record_deallocation(&persistent_ref(*p_), persister_nupm());
	}
	else
	{
		CPLOG(1, PREFIX "leaky deallocation of %p of %zu", LOCATION, persistent_ref(*p_), sz_);
	}
#endif
	/* IHeap interface does not support abstract pointers. Cast to regular pointer */
	auto sz = (_heap->free(*reinterpret_cast<void **>(p_), sz_), sz_);
	/* We would like to carry the persistent_t through to the crash-conssitent allocator,
	 * but for now just assume that the allocator has modifed p_, and call tick to indicate that.
	 */
	perishable::tick();
#if 0
	assert(sz <= _allocated);
	_allocated -= sz;
#endif
	_hist_free.enter(sz);
	return sz;
}
