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

#include "area_ctl.h"

#include "area_top.h"
#include "div.h"
#include <common/logging.h>
#include <libpmem.h>
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <limits>
#include <stdexcept>

#include <iomanip>
#include <ostream>
#include <sstream>

/*
 * TODO:
 *  1. fix loss of (most of) the initial element when sizeof area_ctl is much les than element size
 *  2. support grow (not in IHeap interface)
 *  3. finish crash consistency support on map side.
 */
template <typename P>
	void persist(
		const P & p
	)
	{
		::pmem_persist(&p, sizeof p);
	}

#define PERSIST(x) do { persist(x); } while (0)
#define PERSIST_N(p, ct) do { ::pmem_persist(p, (sizeof *p) * ct); } while (0)

void ccpm::doubt::set(const char *fn, void *p_, std::size_t bytes_)
{
	PLOG("set %s: doubt @ %p area %p.%zu", fn, static_cast<void *>(this), p_, bytes_);
	_bytes = bytes_;
	_in_doubt = p_;
	PERSIST(*this);
}

class verifier
{
	const ccpm::area_ctl *_a;
public:
	verifier(const ccpm::area_ctl *a_)
		: _a(a_)
	{
		assert(_a->is_valid());
	}
	verifier(const verifier &) = delete;
	verifier& operator=(const verifier &) = delete;
	~verifier()
	{
		assert(_a->is_valid());
	}
};

constexpr ccpm::area_ctl::index_t ccpm::area_ctl::ct_atomic_words;
constexpr std::size_t ccpm::area_ctl::min_alloc_size;

/* Constructr for just enough of an area_ctl to hold _full_height */
ccpm::area_ctl::area_ctl(level_ix_t full_height_)
	: _magic0(magic)
	, _full_height(full_height_)
	, _level()
	, _element_count()
	, _sub_size()
	, _dt()
	, _alloc_bits()
	, _element_state()
	, _magic1(magic)
{
}

ccpm::area_ctl::area_ctl(
	unsigned level_
	, std::size_t sub_size_
	, index_t element_count_
	, unsigned header_ct_
	, level_ix_t full_height_
)
	: _magic0(0U + magic)
	, _full_height(full_height_)
	, _level(level_)
	, _element_count(element_count_)
	, _sub_size(sub_size_)
	, _dt()
	, _alloc_bits()
	, _element_state()
	, _magic1(magic)
{
	assert(reinterpret_cast<uintptr_t>(this) % alignof(area_ctl) == 0);
	/* *** element_bitmap *** */

	/* These are available for allocation */
	el_fill_alloc_range(0, element_count_, false);

	/* These are permanently "allocated, reserved" because they are past end of
	 * storage */
	el_reserve_range(element_count_, max_elements);

	/* If we are at the lowest level, or allocation at a lower level would cause
	 * the space for area_ctls to exceed one atomic word, mark the bitmap elements
	 * covering area_ctl and the bitmap itself as allocated, to prevent their use
	 * by the allocator.
	 * Otherwise, add subdivision beneath.
	*/
	if (
		_level == 0
		||
		alloc_states_per_word < front_pad_count(header_ct_ + 1, _sub_size / alloc_states_per_word)
	)
	{
		/* These are permanently "allocated" to instances of area_ctl */
		el_reserve_range(0, front_pad_count(header_ct_ + _level));

		/* If not at lowest level, and in case we are constructing the initial
		 * area_ctl, the origin needs an area_ctl with at least _full_height field
		 * so that recovery an find the root area_ctl.
		 */
		if ( _level != 0 )
		{
			new (element_void(0)) area_ctl(full_height_);
		}
	}
	else
	{
		new_subdivision(header_ct_ + 1);
	}

	/* elements are uninitialized. The allocator perhaps ought to zero them on
	 * allacation
	 */

	_magic0 = magic;
	_magic1 = magic;
}

auto ccpm::area_ctl::commission(
	void *start_
	, std::size_t size_
) -> area_ctl *
{
	auto h = height(size_);
	auto top_level = h - 1;
	std::size_t subdivision_size = area_ctl::min_alloc_size;
	/* subdivision size at height 1 is area_ctl::min_alloc_size. Increases by
	 * alloc_states_per_word for each additional level
	 */
	for ( auto i = top_level; i != 0; --i )
	{
		subdivision_size *= alloc_states_per_word;
	}

	auto pos = static_cast<area_ctl *>(start_) + top_level;
	return
		new
			(pos)
			area_ctl(
				top_level
				, subdivision_size
				, index_t(size_ / subdivision_size)
				, 1
				, h
			)
		;
}

/* Precondition: this area has a run of ct_atomic_words empty slots */
auto ccpm::area_ctl::new_subdivision(unsigned header_ct_) -> area_ctl *
{
	verifier v(this);
	const auto ix = el_find_n_free(ct_atomic_words);
	/* caller should have checked that the area had at least ct_atomic_words
	 * empty slots */
	assert(ix != _element_count);
	const index_t element_count = max_elements;
	/* construct area */
	assert(_sub_size > element_count);
	/* Okay to construct the area before acquiring ownership, because the code
	 * is not multithreaded. A multithreaded version should add a "doubt" element
	 * for subdivision allocation, and modify state only *after* obtaining
	 * ownership by setting alloc bits, i.e.
	 *   1. persist doubt
	 *   2. set allocate bits (may fail if another thread exists)
	 *   3. set state bytes, commission (any order)
	 *   4. clear doubt
	 * Furthermore, recovery in a a multithreaded version needs to check the
	 * subdivision doubt(s), and clear the allocation bits where doubt exists
	 * (because the subdivision did not complete).
	 */

	const auto pac =
		new
			/* An area_ctl at _level is (_level * sizeof *this) bytes past
			 * the start of the first parent element
			 */
			(element_byte(ix) + (_level-1) * (sizeof *this))
			area_ctl(
				_level - 1
				, _sub_size / alloc_states_per_word
				, element_count
				, header_ct_
				, _full_height
			);
	auto &aw = el_allocate_n(ix, ct_atomic_words, sub_state::subdivision);
	PERSIST(aw);

	return pac;
}

unsigned ccpm::area_ctl::height(std::size_t bytes_)
{
	auto h = 1;

	std::size_t subdivision_size = min_alloc_size;
	while ( subdivision_size * ccpm::area_ctl::max_elements < bytes_ )
	{
		subdivision_size *= ccpm::alloc_states_per_word;
		++h;
	}
	return h;
}

/* functions prefixed "el" operate on one or more of all elements in the element
 * state array
 */
void ccpm::area_ctl::el_set_alloc(index_t ix, bool alloc_state)
{
	verifier v(this);
	const auto outer_offset = ix / alloc_states_per_word;
	const auto inner_offset = ix % alloc_states_per_word;
	auto &aw = element_alloc_map()[outer_offset];
	aw =
		( aw & ~(atomic_word(1U) << inner_offset) )
		| (atomic_word(alloc_state) << inner_offset)
		;
	PERSIST(aw);
}

auto ccpm::area_ctl::el_fill_state_range(
	const index_t first_, const index_t last_, const sub_state s
) -> index_t
{
	verifier v(this);
	for ( auto ix = first_ ; ix != last_; ++ix )
	{
		element_state(ix) = s;
	}
	PERSIST_N(&element_state(first_), last_ - first_);
	return last_;
}

auto ccpm::area_ctl::el_fill_alloc_range(index_t first_, index_t last_, bool alloc)
	-> index_t
{
	verifier v(this);
	for ( ; first_ != last_; ++first_ )
	{
		el_set_alloc(first_, alloc);
	}
	return last_;
}

auto ccpm::area_ctl::el_alloc_value(index_t ix) const -> bool
{
	verifier v(this);
	const auto outer_offset = ix / alloc_states_per_word;
	const auto inner_offset = ix % alloc_states_per_word;
	const atomic_word aw = element_alloc_map()[outer_offset];
	return (aw >> inner_offset) & 1;
}

auto ccpm::area_ctl::el_state_value(index_t ix) const -> sub_state
{
	verifier v(this);
	/* bad form to call this if not allocated, as the value is a dont care */
	assert(el_alloc_value(ix));
	return element_state(ix);
}

auto ccpm::area_ctl::el_find_n_free(index_t n) const -> index_t
{
	verifier v(this);
	for ( index_t outer_offset = 0; outer_offset != _element_count; ++outer_offset )
	{
		const auto pos = aw_find_n_free(element_alloc_map()[outer_offset], n);
		if ( pos + n <= alloc_states_per_word )
		{
			return outer_offset * alloc_states_per_word + pos;
		}
	}
	return _element_count;
}

auto ccpm::area_ctl::el_run_at(const index_t ix) const -> index_t
{
	verifier v(this);
	auto iy = ix + 1;
	for (
		; iy != _element_count && el_is_continued(iy)
		; ++iy
	)
	{
	}
	return index_t(iy - ix);
}

auto ccpm::area_ctl::el_client_run_at(const index_t ix) const -> index_t
{
	verifier v(this);
	assert(el_is_client_start(ix)); /* should have been verified by caller */
	auto n = el_run_at(ix);
	/* Error if the run crosses a word boundary */
	assert(ix/alloc_states_per_word == (ix+n-1)/alloc_states_per_word);
	return n;
}

auto ccpm::area_ctl::el_reserved_run_at(const index_t ix) const -> index_t
{
	verifier v(this);
	assert(el_is_reserved(ix)); /* should have been verified by caller */
	return el_run_at(ix);
}

auto ccpm::area_ctl::el_subdivision_run_at(const index_t ix) const -> index_t
{
	verifier v(this);
	assert(el_is_subdivision(ix)); /* should have been verified by caller */
	auto n = el_run_at(ix);
	/* Error if the run crosses a word boundary */
	assert(ix/alloc_states_per_word == (ix+n-1)/alloc_states_per_word);
	return n;
}

/* functions prefixed "aw" operate on only a single atomic_word, which is a subset
 * of the element state array
 */

/* in the atomic word aw, return the index of a start of a run of n free elements
* (or alloc_states_per_word-n+1, if there is no such run)
*/
auto ccpm::aw_find_n_free(atomic_word aw, const unsigned n) -> unsigned
{
	/* mask if 1's marks area to check */
	atomic_word mask = (atomic_word(1U) << n) - 1U;
	atomic_word desired = atomic_word(0);
	unsigned pos = 0;
	while ( pos + n <= alloc_states_per_word && (aw & mask) != desired )
	{
		aw >>= 1;
		++pos;
	}
	return pos;
}

/*
 * Precondition:
 *   (The process may crash
 *   *after* having modified the allocation bits and
 *   *before* the allocation has completed (element_state changed and the requestor
 *   having recorded the allocation).)
 *
 *   Therefore the caller must have persisted a record of "doubt" must have been
 *   persisted which recorded the address and length of the intended allocation.
 *
 *   The caller should invalidate the record of doubt after it has accepted
 *   ownership of the area.
 *
 *   Two callers use el_allocate_n:
 *    (1) Allocation by a client. In this case the caller will clear the doubt
 *        after it has (a) persisted the state bytes (in allocator-owned memory)
 *        and (b) persisted the allocation pointer in client-owned memory.
 *    (2) Allocation of a subdivision internally by area_ctl. In this case the
 *        subdivision builder will clear the doubt after it has persisted the
 *        state bytes (in allocator-owned memory).
 *
 * Starting with element ix, replace n 0s with the client allocated indicators
 * (Must affect not more than one single atomic_word.)
 */
auto ccpm::area_ctl::el_allocate_n(
	const index_t ix
	, const index_t n
	, const sub_state s
) -> atomic_word &
{
	verifier v(this);
	assert(0 < n);
	const auto outer_offset = ix / alloc_states_per_word;
	const auto inner_offset = ix % alloc_states_per_word;

	/* Since this allocation is single threaded, it is okay to write the states
	 * before changing alloc bits. If this were a thread-safe allocator, with
	 * alloc bits arbitrating ownership among threads, we would have to
	 * (1) mark the area "in doubt"i, and persist that
	 * (2) set and persist the alloc bits
	 * (3) set and persist the states
	 * (4) remove the "in doubt" marker.
	 */

	element_state(ix) = s;
	el_fill_state_range(ix+1, ix+n, sub_state::continued);
	/* the bit mask to add */
	atomic_word res = (atomic_word(1U) << n) - 1U;

	auto &aw = element_alloc_map()[outer_offset];
	aw |= (res << inner_offset);
	return aw;
}

void ccpm::area_ctl::el_reserve_range(
	index_t first_
	, const index_t last_
)
{
	verifier v(this);

	if ( first_ != last_ )
	{
		el_fill_state_range(first_, first_ + 1, sub_state::reserved);
		el_fill_state_range(first_ + 1, last_, sub_state::continued);
	}

	for ( ; first_ != last_; ++first_ )
	{
		const auto outer_offset = first_ / alloc_states_per_word;
		const auto inner_offset = first_ % alloc_states_per_word;

		/* the bit mask to add */
		atomic_word res = (atomic_word(1U) << inner_offset);

		auto &aw = element_alloc_map()[outer_offset];
		aw |= res;
		PERSIST(aw);
	}
}

auto ccpm::area_ctl::el_deallocate_n(
	const index_t ix
	, const index_t n
) -> atomic_word &
{
	verifier v(this);
	assert(0 < n);
	const auto outer_offset = ix / alloc_states_per_word;
	const auto inner_offset = ix % alloc_states_per_word;
	const auto mask = ((atomic_word(1U) << (n)) - 1U) << inner_offset;
	auto &aw = element_alloc_map()[outer_offset];
	aw &= ~mask;
	/* ERROR: aw must persist before the element states, if deallocate chooses
	 * to change element stats.
	 */
	return aw;
}

/* pad count, in elements */
auto ccpm::area_ctl::front_pad_count(const unsigned header_ct_) const -> index_t
{
	verifier v(this);
	return front_pad_count(header_ct_, _sub_size);
}

auto ccpm::area_ctl::front_pad_count(const unsigned header_ct_, std::size_t sub_size_) -> index_t
{
	return index_t(div_round_up(sizeof(area_ctl) * header_ct_, sub_size_));
}

std::size_t ccpm::area_ctl::bytes_free() const
{
	verifier v(this);
	std::size_t r = 0;
	for ( index_t ix = 0; ix != _element_count; ++ix )
	{
		r +=
			el_is_free(ix) ? _sub_size
			: el_is_subdivision(ix) ? area_child(ix)->bytes_free()
			: std::size_t(0);
	}
	return r;
}

auto ccpm::area_ctl::elements_free_local() const -> index_t
{
	verifier v(this);
	index_t r = 0;
	for ( index_t ix = 0; ix != _element_count; ++ix )
	{
		r += el_is_free(ix);
	}
	return r;
}

std::size_t ccpm::area_ctl::bytes_free_local() const
{
	verifier v(this);
	return elements_free_local() * _sub_size;
}

std::size_t ccpm::area_ctl::bytes_free_sub() const
{
	verifier v(this);
	std::size_t r = 0;
	for ( index_t ix = 0; ix != _element_count; ++ix )
	{
		r +=
			el_is_subdivision(ix) ? area_child(ix)->bytes_free()
			: std::size_t(0);
	}
	return r;
}

/* Returns
 *  0: pointer to the area_ctl
 *  1: the index of the element
 *  2: the number of levels below the top at which the element was found
 *  3: true iff the pointer was aligned with the start of an element allocation
 */
auto ccpm::area_ctl::locate_element(
	void *const ptr_
) -> element_location
{
	verifier v(this);
	assert( element_void(0) <= ptr_ );
	if ( ptr_ < element_void(0) )
	{
		std::ostringstream o;
		o << "locate_element: ptr " << ptr_ << " below allocation range ["
			<< element_void(0) << ".." << element_void(_element_count) << ")";
		throw std::runtime_error(o.str());
	}

	assert( ptr_ < element_void(_element_count) );
	if ( element_void(_element_count) <= ptr_ )
	{
		std::ostringstream o;
		o << "locate_element: ptr " << ptr_ << " above allocation range ["
			<< element_void(0) << ".." << element_void(_element_count) << ")";
		throw std::runtime_error(o.str());
	}

	const auto offset =
		static_cast<const char *>(ptr_)
		- element_byte(0)
		;
	auto ix = index_t(offset / _sub_size);
	const auto ix_offset = offset % _sub_size;
	if ( el_is_client_start(ix) )
	{
		const bool aligned = el_is_client_start_aligned(ix);
		/* The start of a client, not a sub-allocation */
		if ( el_is_client_start_aligned(ix) )
		{
			if ( ix_offset != 0 )
			{
				throw
					std::runtime_error("locate_element: implausible ptr (not at start of aligned allocation)");
			}
		}
		else
		{
			if ( ix_offset == 0 )
			{
				throw
					std::runtime_error("locate_element: implausible ptr (not at start of misaligned allocation)");
			}
		}
		return element_location{this, ix, aligned};
	}
	else
	{
		/* not an exact hit, look for the sub-allocation */
		assert( _sub_size != min_alloc_size );
		if ( _sub_size == min_alloc_size )
		{
			throw
				std::runtime_error("locate_element: implausible ptr (not minimally aligned)");
		}

		/* Should be somewhere in a subdivision. If in a "continued" element, scan
		 * backwards to the start of the subdivision.
		 * A maximum of 4 tests, but could be read if each continued element of a
		 * subdivision had an offset encoded in its state.
		 */
		for ( ; el_is_continued(ix); --ix )
		{}

		assert( el_is_subdivision(ix) );
		if ( ! el_is_subdivision(ix) )
		{
			throw
				std::runtime_error(
					"locate_element: implausible ptr points inside an area not subdivision"
				);
		}
		return area_child(ix)->locate_element(ptr_);
	}
}

void ccpm::area_ctl::deallocate_local(
	area_top *const top_
	, doubt &dt_
	, void * & ptr_
	, const index_t element_ix_
	, const std::size_t bytes_
)
{
	verifier v(this);
	assert( el_is_client_start(element_ix_) );
	if ( ! el_is_client_start(element_ix_) )
	{
		throw
			std::runtime_error(
				"deallocate: implausible ptr (not start of an allocation)"
			);
	}

	/* remember the tier (within _level) at which this element should be found. */
	const auto old_run = el_max_free_run();
	const auto run_size = el_client_run_at(element_ix_);
	/* It is possible that the deallocation covers more elements than would be
	 * guessed by bytes_ due to aligment round-up.
	 * But the discovered run size always be at least enough to contain
	 * bytes_.
	 */
	assert(div_round_up(bytes_, _sub_size) <= run_size);
	/* two-step release:
	 * (1) reclaim space,
	 * (2) tell client that we have reclaimed the space
	 */
	dt_.set(__func__, ptr_, bytes_);
	auto &aw = el_deallocate_n(element_ix_, run_size);
	PERSIST(aw);
	/* Should not be necessary, as elements with alloc_state free and elements
	 * "in doubt" should have their state examined
	 */
#if 0
	fill_state(element_ix_, run_size, sub_state::free);
	PERSIST_N(&element_state(element_ix_), run_size);
#endif
	ptr_ = nullptr;
	PERSIST(ptr_);
	dt_.clear(__func__);
	/* Need to move or add this area in the chains, but do not know whether the
	 * element is currently in a chain. Remove if it is a chain, then add.
	 */
	if ( top_->is_in_chain(this, _level, old_run) )
	{
		this->remove();
	}
	top_->restore_to_chain(this, _level, el_max_free_run());
}

void ccpm::area_ctl::deallocate(area_top *const top_, void * & ptr_, const std::size_t bytes_)
{
	verifier v(this);
	const auto loc = locate_element(ptr_);
	loc.ctl->deallocate_local(
		top_
		, _dt
		, ptr_
		, loc.element_ix
		, bytes_
	);
}

void ccpm::area_ctl::set_allocated(void *const p_, const std::size_t bytes_)
{
	verifier v(this);
	const auto loc = locate_element(p_);
	loc.ctl->set_allocated_local(loc.element_ix, bytes_, loc.is_aligned);
}

void ccpm::area_ctl::set_allocated_local(const index_t ix_, const std::size_t bytes_, const bool aligned_)
{
	verifier v(this);
	el_allocate_n(
		ix_
		, index_t(div_round_up(bytes_, _sub_size))
		, aligned_ ? sub_state::client_aligned : sub_state::client_unaligned
	);
}

void ccpm::area_ctl::set_deallocated(void *const p_, const std::size_t bytes_)
{
	verifier v(this);
	const auto loc = locate_element(p_);
	loc.ctl->set_deallocated_local(loc.element_ix, bytes_);
}

void ccpm::area_ctl::set_deallocated_local(const index_t ix_, const std::size_t bytes_)
{
	verifier v(this);
	auto &aw = el_deallocate_n(ix_, index_t(div_round_up(bytes_, _sub_size)));
	PERSIST(aw);
}

void ccpm::area_ctl::allocate(
	doubt &dt_
	, void * & ptr_
	, const std::size_t bytes_
	, const std::size_t alignment_
	, const index_t run_length_
)
{
	verifier v(this);
	/* allocate at this level if possible */
	assert( run_length_ <= alloc_states_per_word );

	const auto ix = el_find_n_free(run_length_);

	/* The caller already check that this area_ctl had a sufficient free run */
	assert( ix != _element_count );

	/* Somewhere in the elements [ix:run_length_] should be an alignment_-aligned
	 * area.
	 */

	/* byte ptr to first element in subdivision */
	const auto p0 = this->element_byte(0);
	/* byte ptr to start of first element in free run */
	const auto pe = this->element_byte(ix);
	/* amount needed to retreat to achieve alignment */
	const auto offset_bytes = reinterpret_cast<uintptr_t>(pe) % alignment_;
	/* byte ptr to range (may or may not align with the start of an element) */
	const auto pr = pe +
		( offset_bytes
			? alignment_ - offset_bytes
			: 0
		)
		;

	/* Allocation must fall within the area; logic bug if it did not. */
	assert(element_void(0) <= pr);
	assert(pr + bytes_ < element_void(_element_count));

	/* index of first element to allocate */
	const auto ix_begin = index_t((pr - p0) / _sub_size);
	/* Note whether the allocation aligns with the start of an element.
	 * Remember that for deallocation sanity check.
	 */
	const auto is_element_aligned = (pr - p0) % _sub_size == 0;
	/* end of allocation range */
	const auto pr_end = pr + bytes_;
	/* index past last element to allocate */
	const auto ix_end = index_t(div_round_up(pr_end - p0, _sub_size));
	/* under no circumstance should the end of the result go beyond the end of the
	 * run found by el_find_n_free
	 */
	assert(ix_end <= ix_begin + run_length_);
	const auto run_length_as_aligned = ix_end - ix_begin;

	/* two-step release: (1) tell client about the space, (2) release the space */
	dt_.set(__func__, pr, run_length_as_aligned * _sub_size);
	ptr_ = pr;
	PERSIST(ptr_);
	auto &aw =
		el_allocate_n(
			ix_begin
			, run_length_as_aligned
			, is_element_aligned
				? sub_state::client_aligned
				: sub_state::client_unaligned
			);
	PERSIST(aw);
}

/* restore_at and restore_to did not work too well, too many calls.
 * Instead, restore just once.
 *
 * (Could refine this by restoring depth-first until we found a suitable run for
 * the current allocation, and resuming the restore when we next needed a run.
 * To do this, restore_all would accept and return a vector of subdivision indices,
 * which would be used to sweep across the tree left to right until all area_ctls
 * present (at least at the start of the current run) had been restored.
 */

/* Relink all elements of this area with at least min_run elements to the
 * area_top chain */
auto ccpm::area_ctl::restore_all(
	area_top *const top_
	, const unsigned current_level_ix_
) -> std::size_t
{
	verifier v(this);
	std::size_t count = 0;
	/*
	 * Restore only this level (no children), and if the area_ctl has a run of
	 * min_run or greater
	 */
	auto longest_run = el_max_free_run();
	if ( longest_run > 0 )
	{
		top_->restore_to_chain(this, current_level_ix_, longest_run);
		++count;
	}

	/* (First test is for performance only, elements at level 0 will never be
	 * subdivided.
	 */
	if ( current_level_ix_ != 0 )
	{
		for ( index_t e_ix = 0; e_ix != _element_count; ++e_ix )
		{
			if ( el_is_subdivision(e_ix) )
			{
				count +=
					area_child(e_ix)->restore_all(
						top_
						, current_level_ix_ - 1U
					);
			}
		}
	}
	return count;
}

void ccpm::area_ctl::print(std::ostream &o_, unsigned indent_) const
{
	verifier v(this);
	const auto si = std::string(indent_*2, ' ');
	o_ << si << "area_ctl " << this << " (" << element_void(0) << ".."
		<< element_void(_element_count)
		<< "] :\n";

	++indent_;
	const auto sj = std::string(indent_*2, ' ');

	o_ << sj << "level " << _level
		<< ", " << _element_count << "(" << elements_free_local() << " free)"
		<< " x " << _sub_size << " bytes" << "\n";
	for ( index_t ix = 0; ix != _element_count; ++ix )
	{
		if ( el_is_free(ix) )
		{
		}
		else
		{
			switch ( element_state(ix) )
			{
			case sub_state::subdivision:
				o_ << sj << element_void(ix) << ": element " << ix << ":"
					<< el_subdivision_run_at(ix)
					<< " " << "subdivision\n";
				area_child(ix)->print(o_, indent_ + 1);
				o_ << sj << "end subdivision\n";
				break;
			case sub_state::reserved:
				o_ << sj << element_void(ix) << ": element " << ix << ":"
					<< el_reserved_run_at(ix)
					<< " " << "reserved\n";
				break;
			case sub_state::client_aligned:
				o_ << sj << element_void(ix) << ": element " << ix << ":"
					<< el_client_run_at(ix)
					<< " " << "client.aligned\n";
				break;
			case sub_state::client_unaligned:
				o_ << sj << element_void(ix) << ": element " << ix << ":"
					<< el_client_run_at(ix)
					<< " " << "client.unaligned\n";
				break;
			case sub_state::continued:
				break;
			default:
				o_ << sj << element_void(ix) << ": element " << ix << ":"
					<< " (?unknown)\n";
				break;
			}
		}
	}
	o_ << si << "area_ctl end\n";
}

void ccpm::area_ctl::restore(ownership_callback_t resolver_)
{
	PLOG("restore %p", static_cast<void *>(this));
	verifier v(this);
	/* Address of the region transitioning to/from client allocated
	 * during a crash, if any
	 */
	if ( const auto p = _dt.get() )
	{
		/* Guessing that true means client-owned. Without a named function,
		 * could be either way.
		 */
		const bool client_owned = resolver_(p);
		if ( client_owned )
		{
			/* The client owns p, but the allocator may still indicate that
			 * p is free. Find the memory range for p and mark the range
			 * "client allocated."
			 */
			set_allocated(p, _dt.bytes());
			_dt.clear(__func__);
		}
		else
		{
			/* The client does not own p, but the allocator may still
			 * indicate that the p is allocated. Find the memory range
			 * for p and mark the range "free"
			 */
			set_deallocated(p, _dt.bytes());
			_dt.clear(__func__);
		}
	}
}

unsigned ccpm::area_ctl::height() const
{
	verifier v(this);
	auto s = _sub_size;
	unsigned h = 1;
	while ( s > min_alloc_size )
	{
		assert(s % alloc_states_per_word == 0);
		++h;
		s /= alloc_states_per_word;
	}
	assert(s == min_alloc_size);
	return h;
}
