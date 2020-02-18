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

#ifndef __CCPM_AREATOP_ALLOCATOR_H__
#define __CCPM_AREATOP_ALLOCATOR_H__

#include "atomic_word.h"
#include "list_item.h"
#include <ccpm/interfaces.h>
#include <array>
#include <cstddef>
#include <vector>

struct iovec;

namespace ccpm
{
	class level_hints
	{
		/* Each level has a list for every possible number of contiguous free elements.
		 * Since "contiguous" free elements do not span a word, that is one list
		 * for every possible contiguous size in a word.
		 * The list at element n locates an area_ctl with has maximal runs of exactly
		 * n+1 free elements,
		 */

		using free_ctls_t = std::array<list_item, alloc_states_per_word>;
		free_ctls_t _free_ctls;
		free_ctls_t::size_type find_free_ctl_ix(unsigned min_run_length) const;
		free_ctls_t::size_type tier_ix_from_run_length(unsigned run_length) const;

	public:
		static constexpr auto size() { return alloc_states_per_word; }

		/* return smallest tier index which has an area with
		 *   run length >= min_run_length
		 * If no such tier, return sub_states_per_word.
		 */
		auto find_free_ctl(unsigned min_run_length_) const -> const list_item *
		{
			return & _free_ctls[find_free_ctl_ix(min_run_length_)];
		}

		auto find_free_ctl(unsigned min_run_length_) -> list_item *
		{
			return & _free_ctls[find_free_ctl_ix(min_run_length_)];
		}

		const auto *tier_from_run_length(unsigned run_length_) const
		{
			return & _free_ctls[tier_ix_from_run_length(run_length_)];
		}

		auto *tier_from_run_length(unsigned run_length_)
		{
			return & _free_ctls[tier_ix_from_run_length(run_length_)];
		}

		const auto *tier_end() const
		{
			return &_free_ctls[alloc_states_per_word];
		}

		level_hints()
			: _free_ctls()
		{}
	};

	class area_ctl;
	/*
	 * Location of global, non-persisted items in the crash-consistent allocator.
	 * At the moment there are none. The _bytes_free member is a good candidate.
	 * Other candidates: links to chains of areas containing free elements of
	 * various sizes.
	 */
	class area_top
	{
		area_ctl *_ctl;
		std::size_t _bytes_free;
		bool _all_restored;
		std::vector<level_hints> _level;

		area_top(area_ctl *ctl);
		area_top(const area_top &) = delete;
		area_top &operator=(const area_top &) = delete;

		void allocate_strategy_1(
			void * & ptr_
			, std::size_t bytes
			, std::size_t alignment
			, unsigned level_ix
			, unsigned run_length
		);

		bool allocate_recovery_1();
		bool allocate_recovery_2(unsigned level_ix);

	public:
		static area_top *create(const ::iovec &iov);

		static area_top *restore(const ::iovec &iov, const ownership_callback_t &resolver);

		bool includes(const void *addr) const;

		/* Free byte count. Required by users */
		std::size_t bytes_free() const;

		void allocate(void * & ptr, std::size_t bytes, std::size_t alignment);

		void deallocate(void * & ptr, std::size_t bytes);

		void print(std::ostream &o, unsigned level) const;
		unsigned height() const { return unsigned(_level.size()); }

		/*
		 * called by area_ctl to add area_ctl a, at level level_ix, with a longest
		 * free run (consecutive free elements) of free_run, to _level, which is the
		 * non-persistent catalog of area_ctl items.
		 */
		void restore_to_chain(area_ctl *a, unsigned level_ix, unsigned run_length);

		bool contains(const void *p) const;

		bool is_in_chain(
			const area_ctl *a
			, unsigned level_ix
			, unsigned run_length
		) const;
	};
}

#endif
