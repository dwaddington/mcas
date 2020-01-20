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


#ifndef _COMANCHE_HSTORE_OWNER_H
#define _COMANCHE_HSTORE_OWNER_H


#include "hstore_config.h"
#include "persistent.h"
#include "trace_flags.h"
#if TRACED_OWNER
#include "hop_hash_debug.h"
#endif

#include <cassert>
#include <cstddef> /* size_t */
#include <cstdint> /* uint64_t */
#include <limits> /* numeric_limits */
#include <string>

/*
 * The "owner" part of a hash bucket
 */

namespace impl
{
	class allocation_state_emplace;

	template <typename Bucket, typename Referent, typename Lock>
		struct bucket_shared_lock;
	template <typename Bucket, typename Referent, typename Lock>
		using bucket_shared_ref = bucket_shared_lock<Bucket, Referent, Lock> &;

	template <typename Bucket, typename Referent, typename Lock>
		struct bucket_unique_lock;
	template <typename Bucket, typename Referent, typename Lock>
		using bucket_unique_ref = bucket_unique_lock<Bucket, Referent, Lock> &;

	class owner
	{
	public:
		static constexpr unsigned size = 64U;
		using value_type = std::uint64_t; /* sufficient for size not over 64U */
		static constexpr auto pos_undefined = std::numeric_limits<std::size_t>::max();
	private:
		persistent_atomic_t<value_type> _value; /* at least owner::size bits */
#if TRACK_POS
		std::size_t _pos;
#endif
		static value_type mask_from_pos(unsigned pos) { return value_type(1U) << pos; }
		void insert(
			const std::size_t
#if TRACK_POS
				pos_
#endif
			, const unsigned p_
		)
		{
#if TRACK_POS
			assert(_pos == pos_undefined || _pos == pos_);
			assert(p_ < size);
			_pos = pos_;
#endif
			_value |= mask_from_pos(p_);
		}
	public:
		explicit owner()
			: _value(0)
#if TRACK_POS
			, _pos(pos_undefined)
#endif
		{}

		static value_type mask_all_ones() { return mask_from_pos(size) - 1U; }
		static unsigned rightmost_one_pos(value_type c) { return __builtin_ctzll(c); };

		/* ERROR: it is being a bit lazy to make PersistController as a template parameter.
		 * Chances are that what we need of the persist_controller_t could be provided
		 * in a more limited manner.
		 */
		template<typename Bucket, typename Referent, typename SharedMutex, typename PersistController>
			void insert(
				const std::size_t pos_
				, const unsigned p_
				, bucket_unique_ref<Bucket, Referent, SharedMutex>
				, PersistController *
#if USE_CC_HEAP == 4
				pc_
#endif
			)
			{
#if USE_CC_HEAP == 3
#elif USE_CC_HEAP == 4
				pc_->record_owner_addr_and_bitmask(&_value, mask_from_pos(p_));
#else
#error unknown USE_CC_HEAP
#endif
				insert(pos_, p_);
			}
		template<typename Bucket, typename Referent, typename SharedMutex>
			void erase(unsigned p, bucket_unique_ref<Bucket, Referent, SharedMutex>)
			{
				_value &= ~mask_from_pos(p);
			}
		template<typename Bucket, typename Referent, typename SharedMutex>
			void move(
				unsigned dst_
				, unsigned src_
				, bucket_unique_ref<Bucket, Referent, SharedMutex>
			)
			{
				assert(dst_ < size);
				assert(src_ < size);
				_value = (_value | mask_from_pos(dst_)) & ~mask_from_pos(src_);
			}
		template <typename Lock>
			auto value(Lock &) const -> value_type { return _value; }
		template <typename Lock>
			auto owned(std::size_t hop_hash_size, Lock &) const -> std::string;
		/* clear the senior owner of all the bits set in its new junior owner. */
		template <typename Bucket, typename Referent, typename SharedMutex>
			void clear_from(
				const owner &junior
				, bucket_unique_ref<Bucket, Referent, SharedMutex>
				, bucket_shared_ref<Bucket, Referent, SharedMutex>
			)
			{
				_value &= ~junior._value;
			}

#if TRACED_OWNER
		template <
			typename Lock
		>
			friend auto operator<<(
				std::ostream &o
				, const impl::owner_print<Lock> &
			) -> std::ostream &;
		template <typename Table>
			friend auto operator<<(
				std::ostream &o
				, const impl::owner_print<impl::bypass_lock<const typename Table::bucket_t, const impl::owner>> &
			) -> std::ostream &;
#endif
	};
}

#endif
