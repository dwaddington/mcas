/*
   Copyright [2017-2019] [IBM Corporation]
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


#ifndef _COMANCHE_HSTORE_PERSIST_MAP_H
#define _COMANCHE_HSTORE_PERSIST_MAP_H

#include "as_emplace.h"
#include "bucket_aligned.h"
#include "hash_bucket.h"
#include "persist_fixed_string.h"
#include "persistent.h"
#include "persist_atomic.h"
#include "segment_layout.h"
#include "size_control.h"

#include <cstddef> /* size_t */

/* Persistent data for hstore.
 */

namespace impl
{
	using segment_count_actual_t = value_unstable<segment_layout::six_t, 1>;

	template <typename Allocator>
		class persist_controller;

	template <typename Allocator>
		class persist_map
		{
			using value_type = typename Allocator::value_type;
			static constexpr std::size_t segment_align = 64U;
	public:
			using bucket_aligned_t = bucket_aligned<hash_bucket<value_type>>;
	private:
			using allocator_traits_type = std::allocator_traits<Allocator>;
			using bucket_allocator_t =
				typename allocator_traits_type::template rebind_alloc<bucket_aligned_t>;
			using bucket_ptr = typename std::allocator_traits<bucket_allocator_t>::pointer;

			/* bucket indexes */
			using bix_t = segment_layout::bix_t;
			/* segment indexes */
			using six_t = segment_layout::six_t;

			struct segment_count
			{
				/* current segment count */
				segment_count_actual_t _actual;
				/* desired segment count */
				persistent_atomic_t<six_t> _specified;
				segment_count(six_t specified_)
					: _actual(0)
					, _specified(specified_)
				{}
			};

			struct segment_control
			{
				persistent_t<bucket_ptr> bp;
				segment_control()
					: bp()
				{
				}
			};

			static constexpr six_t _segment_capacity = 32U;
			static constexpr unsigned log2_base_segment_size =
				segment_layout::log2_base_segment_size;
			static constexpr bix_t base_segment_size =
				segment_layout::base_segment_size;

			size_control _size_control;

			segment_count _segment_count;

			segment_control _sc[_segment_capacity];

			allocation_state_emplace _ase;	
		public:
			persist_map(std::size_t n, Allocator av);
			persist_map(persist_map &&) = default;
			void do_initial_allocation(Allocator av);
#if USE_CC_HEAP == 3
			void reconstitute(Allocator av);
#endif
			allocation_state_emplace &ase() { return _ase; }
			friend class persist_controller<Allocator>;
		};
}

#include "persist_map.tcc"

#endif
