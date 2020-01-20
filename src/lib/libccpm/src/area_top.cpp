/*
   Copyright [2019] [IBM Corporation]
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

#include "area_top.h"

#include "area_ctl.h"
#include "div.h"
#include <cassert>
#include <stdexcept>
#include <ostream>

auto ccpm::level_hints::find_free_ctl_ix(
	unsigned min_run_length_
) const -> free_ctls_t::size_type
{
	auto ix = tier_ix_from_run_length(min_run_length_);
	for ( ; ix != alloc_states_per_word && _free_ctls[ix].empty() ; ++ix )
	{
	}
	return ix;
}

auto ccpm::level_hints::tier_ix_from_run_length(
	unsigned run_length
) const -> free_ctls_t::size_type
{
	assert(run_length != 0 && run_length <= alloc_states_per_word);
	return run_length - 1;
}

auto ccpm::area_top::is_in_chain(
	const area_ctl *a
	, unsigned level_ix
	, unsigned run_length
) const -> bool
{
	assert(level_ix < _level.size());
	const auto &level = _level[level_ix];
	return run_length != 0 && level.tier_from_run_length(run_length)->contains(a);
}

ccpm::area_top::area_top(area_ctl *ctl_)
	: _ctl(ctl_)
	/* bytes_free does a full search of the existing tree.
	 * Consider doing a full chain restoration at the same time.
	 */
	, _bytes_free(_ctl->bytes_free())
	, _all_restored(false)
	, _level(ctl_->height())
{
	/* TODO: add _ctl to appropriate free_ctl chain */
}

auto ccpm::area_top::create(const ::iovec &iov_) -> area_top *
{
#if 0
	auto h = area_ctl::height(iov_.iov_len);
	std::size_t subdivision_size = area_ctl::min_alloc_size;
	/* subdivision size at height 1 is area_ctl::min_alloc_size. Increases by
	 * allloc_states_per_word for each additional level
	 */
	for ( auto i = h; i != 1; --i )
	{
		subdivision_size *= alloc_states_per_word;
	}
	auto c =
		area_ctl::commission(
			iov_.iov_base
			, subdivision_size
			, area_ctl::index_t(iov_.iov_len / subdivision_size)
		);
#else
	auto c = area_ctl::commission(iov_.iov_base, iov_.iov_len);
#endif
	auto t = new area_top(c);
	return t;
}

auto ccpm::area_top::restore(
	const ::iovec &iov_
	, ownership_callback_t resolver_
) -> area_top *
{
	/* The base if the region contains an array of area_ctls.
	 * The first area_ctl is probably not the root of the area_ctl tree,
	 * but it contains the height of the tree, and the last area_ctl
	 * in the array (at position height-1) *is* the root area_ctl.
	 */
	auto ctl0 = static_cast<area_ctl *>(iov_.iov_base);
	auto ctl = &ctl0[ctl0->full_height()-1];
	ctl->restore(resolver_);
	return new area_top(ctl);
}

auto ccpm::area_top::bytes_free() const -> std::size_t
{
	return _ctl ? _ctl->bytes_free() : 0;
}

void ccpm::area_top::restore_to_chain(
	area_ctl *a_
	, unsigned level_ix_
	, unsigned longest_run_
)
{
	if ( longest_run_ != 0 )
	{
		assert(level_ix_ < _level.size());
		auto &level = _level[level_ix_];
		auto free_ctl = level.tier_from_run_length(longest_run_);
		free_ctl->insert_after(a_);
	}
}

/*
 * Levels: areas are placed in levels according to the log of their size.
 * Levels form a hierarchy, with level 0 containing the smallest elements,
 * area_ctl::min_alloc_size bytes.
 *
 * Tier: within each level, areas are sorted according to the longest run
 * of free elements in the area. (A free run must be contained entirely
 * within a single atomic_word.) Tiers run from 0 to 32. Tier 0 will always
 * be empty, as there is no point in tracking fully allocated areas.
 * Tier 32 could also be "always empty", if we chose to undo subdivision
 * of entirely free areas. We do not do that yet.
 *
 * Allocate strategy:
 * Determine level L at which the allocation should occur
 *    (
 *      _sub_size(L) <= bytes_ <= _sub_size(L)*(32-1)
 *      ||
 *      _sub_size == area_ctl::min_alloc_size
 *    )
 *
 * 1. allocate from existing chains in area_top which are at L and have free
 *    runs at least long enough to hold bytes. Move the subdivided area from
 *    its current area_top chain to its new area_top chain or, of the area is
 *    entirely allocated, to no area_top chain.
 * (if none found)
 * 2. Starting after the rightmost location encountered in the previous search
 *    of this kind at L, conduct an exhaustive left-to-right search for subdivided
 *    areas, adding each area to its proper area_top chain if not already in the
 *    chain.
 *    If a suitable subdivided area is found, stop the search and allocate at that
 *    area. Add the subdivided area to its appropriate area_top chain.
 ( (if none found ...)
 * 3. Create a new subdivided region from L+1 (if any free at L+1), allocate from
 *    that region. If none free at L+1, first create subdivided region at L+2,
 *    then L+1, etc.) Add each new subdivided region to its appropriate area_top
 *    chain.
 * 4.
 *
 *
 * 1. Allocate from a chained area the appropriate level, and from any tier
 *    containing a long enough run of free elements. (The area from which
 *    elements are allocated may now have a shorter "longest run". Remove that
 *    area from its current tier. If the area still has a non-zero run, add it to
 *    a new tier.
 *
 * If the allocation fails, there are two possibilities: (a) subdivide a chained
 * area to produce an area at the right level, or (b) search for areas to add to
 * the chain.
 *
 * Attempting (a) first has this advantage: The new areas subdivided, if any, will
 * be new subdivisions and not an pre-existing subdivisions (which may or may not
 * be already present in a chain).
 * Attempting (a) first has this disadvantage: additional fragmentation.
 *
 * Attempting (b) first has this advantage: smaller, already-fragmented areas might
 * be discovered and used.
 * Attempting (b) first has this disadvantage: Any subdivision found would have
 * links of indeterminate state: we would not know whether the links were part
 * of a current chain, or left over from a previous generation of (non-persistent)
 * chains.
 *
 * 2. (restricted version of (b)): Move an unchained area to the chain and retry
 * the allocation. If we find such an area we will know that it is not already in
 * the chain; if it were it would have been found and used in Step 1.
 *
 * 3. Subdivide a chained area 2. (Chooses speed at the cost of fragmentation.)
 */

/* Part 1: allocate from an existing chain */
void ccpm::area_top::allocate_strategy_1(
	void * & ptr_
	, const size_t bytes_
	, const size_t alignment_
	, const unsigned level_ix_
	, const unsigned run_length_
)
{
	auto &level = _level[level_ix_];
	/* One each level there are separate ctl chains for the longest free run
	 * at each level (from 1 to substates_per_word). The index of each chain
	 * is one less then the longest free run.
	 */
	assert(run_length_ < level.size());
	auto tier_ptr = level.find_free_ctl(run_length_);
	if ( tier_ptr != level.tier_end() )
	{
		auto &free_ctl = *tier_ptr;

		/* A viable allocation exists at free_ctl. Use it. */
		auto viable = static_cast<area_ctl *>(free_ctl.next());
		viable->allocate(_ctl->get_doubt(), ptr_, bytes_, alignment_, run_length_);
		/* The ctl may have a new, shorter longest run.
		 * If so, move it to a new chain within the level object */
		auto longest_run = viable->el_max_free_run();
		viable->remove();
		if ( longest_run != 0 )
		{
			level.tier_from_run_length(longest_run)->insert_after(viable);
		}
	}
}

/* Recovery 1: rechain all existing (not chained) area_ctls. Such a
 * area_ctls could only exist after a crash or restart, as eligible area_ctls
 * are normally chained.
 */
bool ccpm::area_top::allocate_recovery_1()
{
	auto ct =
		_all_restored
		? 0U
		: _ctl->restore_all(
				this
				, unsigned(_level.size()-1)
			)
		;
	_all_restored = true;
	return ct != 0;
}

/* Recovery 2: allocate from a child in a new subdivision.
 * We would prefer to allocate only at (level_ix+1). But if no free space is
 * known at that level, we have to move higher, and allocate twice, or more.
 * Go up from (level_ix+1) until we find a run of ct_atomic_words free elements.
 * Allocate that run as a subdivision, and iteratively allocate new subdivision
 * until we have allocated a subdivision for the target level.
 */
bool ccpm::area_top::allocate_recovery_2(
	const unsigned level_ix
)
{
	auto parent_level_ix = level_ix + 1;
	list_item *subdivide_tier_ptr = nullptr;
	for (
		;
			(
				parent_level_ix != _level.size()
				&&
				(
					subdivide_tier_ptr =
						_level[parent_level_ix]
							.find_free_ctl(area_ctl::ct_atomic_words)
				)
				==
				_level[parent_level_ix].tier_end()
			)
		; ++parent_level_ix )
	{
	}

	if (
		parent_level_ix != _level.size()
		&&
		subdivide_tier_ptr != _level[parent_level_ix].tier_end()
	)
	{
		auto subdivide_level_ix = parent_level_ix;
		do
		{
			auto &level = _level[subdivide_level_ix];
			auto &free_ctl = *subdivide_tier_ptr;
			/* A viable allocation exists at free_ctl. Use it. */
			auto parent = static_cast<area_ctl *>(free_ctl.next());
			/* The first parent will exist because find_free_ctl found it,
			 * and subsequent parents will exist because new_subdivision
			 * just created them.
			 */
			assert(parent);
			/* carve out a new area_ptr from viable */
			auto child = parent->new_subdivision(1U);
			/* The parent may have a new, shorter longest run.
			 * If so, move it to a new chain within the level object */
			auto parent_longest_run = parent->el_max_free_run();
			parent->remove();
			if ( parent_longest_run != 0 )
			{
				level.
					tier_from_run_length(parent_longest_run)->
						insert_after(parent);
			}
			/* Link in the new area */
			--subdivide_level_ix;
			auto &child_level = _level[subdivide_level_ix];
			subdivide_tier_ptr =
				child_level.tier_from_run_length(child->el_max_free_run());
			subdivide_tier_ptr->insert_after(child);
		} while ( subdivide_level_ix != level_ix );

		/* A child with the maximum possible number of free elements now exists
		 * at the necessary level. Retry the allocation, which should now succeed.
		 */
		return true;
	}
	else
	{
		return false;
	}
}

void ccpm::area_top::allocate(
	void * & ptr_
	, std::size_t bytes_
	, std::size_t alignment_
)
{
	if ( _ctl )
	{
		const auto bytes = std::max(bytes_, area_ctl::min_alloc_size);
		/* If there is no chained area suitable for allocation we will try to add
		 * areas to the lists of chained areas. If any areas are added, we then
		 * restart the search here:
		 */
		const auto level_ix = area_ctl::size_to_level(bytes);
		/* If level_l is a feasible level ... */
		if ( level_ix < _level.size() )
		{
			/* TODO: consider whether it is possible to reduce the required
			 * run length by combining the run_length_for_use and
			 * run_length_for_alignment penalties.
			 */
			/* Number of elements necessary to contain bytes */
			const auto run_length_for_use =
				unsigned(
					div_round_up(bytes, area_ctl::level_to_element_size(level_ix))
				);

			/* Number of consecutive free elements sufficent to ensure run_length
			 * elements aligned to the alignment specification.
			 */
			const auto run_length_for_alignment =
				run_length_for_use
				+
				std::max(
					/* Slack needed because the alignment request exceeds the
					 * natural alignment of the level */
					unsigned(
						alignment_ / area_ctl::level_to_element_size(level_ix)
					)
					/* Slack needed because the whole region is poorly */
					, unsigned(
						reinterpret_cast<uintptr_t>(_ctl) % (area_ctl::level_to_element_size(level_ix)) != 0
					)
				)
				;
			/* Should have at most doubled the number of elements */
			assert(run_length_for_alignment <= run_length_for_use * 2);

			allocate_strategy_1(
				ptr_
				, bytes
				, alignment_
				, level_ix
				, run_length_for_alignment
			);

			while ( ptr_ == nullptr
				&&
				(
					/* Recovery 1: rechain an existing but unchained area_ctl
					 */
					allocate_recovery_1()
					||
					/* Recovery 2: make a new subdivision */
					allocate_recovery_2(level_ix)
				)
			)
			{
				allocate_strategy_1(
					ptr_
					, bytes
					, alignment_
					, level_ix
					, run_length_for_alignment
				);
			}
		}
	}
}

void ccpm::area_top::deallocate(void * & ptr_, std::size_t bytes_)
{
	if ( _ctl )
	{
		auto bytes = std::max(bytes_, area_ctl::min_alloc_size);
		_ctl->deallocate(this, ptr_, bytes);
	}

}

void ccpm::area_top::print(std::ostream &o_, unsigned level_) const
{
	const auto si = std::string(level_*2, ' ');
	if ( _ctl )
	{
		o_ << si << "area_top:\n";
		_ctl->print(o_, level_ + 1);
		o_ << si << "area_top end\n";
	}
	else
	{
		o_ << si << "area_top: not initialized\n";
	}
}

bool ccpm::area_top::contains(const void *p) const
{
	return _ctl && _ctl->contains(p);
}
