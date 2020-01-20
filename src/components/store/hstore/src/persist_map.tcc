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

#include <type_traits> /* is_base_of */

#include "perishable.h"
#include "segment_layout.h"

/*
 * ===== persist_map =====
 */

template <typename Allocator>
	impl::persist_map<Allocator>::persist_map(std::size_t n, Allocator av_)
		: _size_control()
		, _segment_count(
			/* The map tends to split when it is about 40% full.
			 * Triple the excpected object count when creating a segment count.
			 */
			((n*3U)/base_segment_size == 0 ? 1U : segment_layout::log2((3U * n)/base_segment_size))
		)
		, _sc{}
		, _ase{}
	{
		do_initial_allocation(av_);
	}

template <typename Allocator>
	void impl::persist_map<Allocator>::do_initial_allocation(Allocator av_)
	{
		if ( _segment_count._actual.is_stable() )
		{
			if ( _segment_count._actual.value() == 0 )
			{
#if USE_CC_HEAP == 4
				/* ERROR: local memory owner can leak */
#endif
				persistent_t<typename std::allocator_traits<bucket_allocator_t>::pointer> ptr = nullptr;
				bucket_allocator_t(av_).allocate(
					ptr
					, base_segment_size
					, segment_align
				);
				new ( &*ptr ) bucket_aligned_t[base_segment_size];
				_sc[0].bp = ptr;
				_segment_count._actual.incr();
				av_.persist(&_segment_count, sizeof _segment_count);
			}

			/* while not enough allocated segments to hold n elements */
			for ( auto ix = _segment_count._actual.value(); ix != _segment_count._specified; ++ix )
			{
				auto segment_size = base_segment_size<<(ix-1U);

#if USE_CC_HEAP == 4
				/* ERROR: local memory owner can leak */
#endif
				persistent_t<typename std::allocator_traits<bucket_allocator_t>::pointer> ptr = nullptr;
				bucket_allocator_t(av_).allocate(
					ptr
					, segment_size
					, segment_align
				);
				new (&*ptr) bucket_aligned_t[base_segment_size << (ix-1U)];
				_sc[ix].bp = ptr;
				_segment_count._actual.incr();
				av_.persist(&_segment_count, sizeof _segment_count);
			}

			av_.persist(&_size_control, sizeof _size_control);
		}
	}

#if USE_CC_HEAP == 3
template <typename Allocator>
	void impl::persist_map<Allocator>::reconstitute(Allocator av_)
	{
		auto av = bucket_allocator_t(av_);
		if ( ! _segment_count._actual.is_stable() || _segment_count._actual.value() != 0 )
		{
			segment_layout::six_t ix = 0U;
			av.reconstitute(base_segment_size, _sc[ix].bp);
			++ix;

			/* restore segments beyond the first */
			for ( ; ix != _segment_count._actual.value_not_stable(); ++ix )
			{
				auto segment_size = base_segment_size<<(ix-1U);
				av.reconstitute(segment_size, _sc[ix].bp);
			}
			if ( ! _segment_count._actual.is_stable() )
			{
				/* restore the last, "junior" segment */
				auto segment_size = base_segment_size<<(ix-1U);
				av.reconstitute(segment_size, _sc[ix].bp);
			}

		}
	}
#endif
