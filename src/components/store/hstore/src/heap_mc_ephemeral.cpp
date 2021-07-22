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
#include "heap_mc_shim.h"
#include "mm_plugin_itf.h"
#include <common/errors.h> /* S_OK */
#include <cassert>
#include <cstdlib> /* getenv */
#include <memory> /* make_unique */
#include <numeric> /* accumulate */
#include <utility>

constexpr unsigned heap_mc_ephemeral::log_min_alignment;
constexpr unsigned heap_mc_ephemeral::hist_report_upper_bound;

namespace
{
	struct persister final
		: public ccpm::persister
	{
		void persist(common::byte_span s) override
		{
			::pmem_persist(::base(s), ::size(s));
		}
	};
	persister p_cc{};
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
	: heap_mm_ephemeral(
		debug_level_
		, nupm::region_descriptor((p_->add_regions(ccpm::region_span(&*ccpm::region_vector_t(pool0_heap_).begin(), 1)), id_), backing_file_, rv_full_)
		, ::size(pool0_heap_)
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
	, _heap(std::move(p_))
	, _ase(ase_)
	, _aspd(aspd_)
	, _aspk(aspk_)
	, _asx(asx_)
{

  for ( const auto &r : rv_full_ )
  {
    CPLOG(2, "%s : %p.%zx", __func__, ::base(r), ::size(r));
  }
  CPLOG(2, "%s : pool0_heap: %p.%zx", __func__, ::base(pool0_heap_), ::size(pool0_heap_));
}

/* initial */
	/* heap_mc version */
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
		, std::make_unique<heap_mc_shim>(
			plugin_path_
			, &p_cc
		)
		, id_
		, backing_file_
		, rv_full_
		, pool0_heap_
	)
{
}
	/* heap_mm version */
heap_mc_ephemeral::heap_mc_ephemeral(
	unsigned debug_level_
	, MM_plugin_wrapper &&pw_
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
		, std::make_unique<heap_mc_shim>(
			std::move(pw_)
		)
		, id_
		, backing_file_
		, rv_full_
		, pool0_heap_
	)
{
}

/* crash-consistent reconstitute */
	/* heap_mc version */
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
	, ccpm::ownership_callback_t ownership_callback_
)
	: heap_mc_ephemeral(
		debug_level_
		, ase_, aspd_, aspk_, asx_
		, std::make_unique<heap_mc_shim>(
			plugin_path_
			, &p_cc
			, ccpm::region_span(&*ccpm::region_vector_t(pool0_heap_).begin(), 1)
			, ownership_callback_
		)
		, id_
		, backing_file_
		, rv_full_
		, pool0_heap_
	)
{
}
	/* heap_mm version idefined above, same as initial create */

void heap_mc_ephemeral::add_managed_region_to_heap(byte_span r_heap)
{
	ccpm::region_span::value_type rs[1] { r_heap };
	_heap->add_regions(rs);
}

void heap_mc_ephemeral::allocate(
	persistent_t<void *> &p_
	, std::size_t sz_
	, std::size_t alignment_
)
{
	if ( S_OK != _heap->allocate(*reinterpret_cast<void **>(&p_), sz_, alignment_) )
	{
		throw std::bad_alloc{};
	}
#if 0
	_allocated += sz_;
#endif
	_hist_alloc.enter(sz_);
}

std::size_t heap_mc_ephemeral::free(persistent_t<void *> &p_, std::size_t sz_)
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
		CPLOG(1, PREFIX "unexpected segment deallocation of %p of %zu", LOCATION, persistent_ref(p_), sz_);
		abort();
#if 0
		_asx->record_deallocation(&persistent_ref(p_), persister_nupm());
#endif
	}
	else if ( _ase->is_armed() )
	{
		_ase->record_deallocation(&persistent_ref(p_), persister_nupm());
	}
	else
	{
		CPLOG(1, PREFIX "leaky deallocation of %p of %zu", LOCATION, persistent_ref(p_), sz_);
	}
#endif
	/* IHeap interface does not support abstract pointers. Cast to regular pointer */
	auto sz = (_heap->free(*reinterpret_cast<void **>(&p_), sz_), sz_);
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

void heap_mc_ephemeral::free_tracked(const void *p_, std::size_t sz_)
{
	_heap->free(const_cast<void *&>(p_), sz_);
#if 0
	_allocated -= sz_;
#endif
	_hist_free.enter(sz_);
}

bool heap_mc_ephemeral::is_crash_consistent() const { return true; }
bool heap_mc_ephemeral::can_reconstitute() const { return false; }
