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


#ifndef MCAS_HSTORE_FIXED_STRING_H
#define MCAS_HSTORE_FIXED_STRING_H

#include "lock_state.h"
#include <common/pointer_cast.h>
#include <common/perf/tm.h>
#if MCAS_HSTORE_USE_PMEM_PERSIST
#include <libpmem.h>
#endif
#include <algorithm> /* fill_n, copy */
#include <cassert>
#include <cstddef> /* size_t */
#include <stdexcept> /* domain_error */

namespace
{
	template <typename I>
		static auto gcd(I a, I b) -> I
		{ /* Euclid's algorithm */
			while (true)
			{
				if (a == I()) return b;
				b %= a;
				if (b == I()) return a;
				a %= b;
			}
		}

	template <typename I>
		static auto lcm(I a, I b) -> I
		{
			auto g = gcd(a,b);
			return g ? (a/g * b) : I();
		}

	/* paramenters and return like std::fill_n, but can use pmem_memset_persist */
	template <typename IO_>
		IO_ persisted_zero_n(IO_ dst_, std::size_t n_)
		{
#if MCAS_HSTORE_USE_PMEM_PERSIST
			::pmem_memset_persist(&*dst_, 0, n_ * sizeof *dst_);
			return dst_ + n_;
#else
			return std::fill_n( dst_, n_, 0);
			/* persist call is owed */
#endif
		}

	/* paramenters and return like std::copy, but can use pmem_memcpy_persist */
	template <typename C, typename IO_>
		IO_ persisted_copy(const common::basic_string_view<C> source_, IO_ dst_)
		{
#if MCAS_HSTORE_USE_PMEM_PERSIST
			::pmem_memcpy_persist(&*dst_, &*source_.begin(), source_.size() * sizeof(C));
			return dst_ + source_.size();
#else
			return std::copy(source_.begin(), source.end(), dst_);
			/* persist call is owed */
#endif
		}
}

/*
 * - fixed_string
 * - ref_count: because the object may be referenced twice as the table expands
 * - size: length of data (data immediately follows the fixed_string object)
 */
template <typename T>
	struct fixed_string
	{
	private:
		uint64_t _size;
		uint8_t _ref_count;
		uint8_t _log_alignment;
		signed _lock;
		std::size_t _data_offset;

		/* offset to data, for a particular alignment */
		std::size_t front_pad() const noexcept { return _data_offset - sizeof *this; }

		std::size_t front_skip_element_count() const
		{
			return front_skip_element_count(alignment());
		}

		static std::size_t data_offset(std::size_t alignment_) noexcept
		{
			return front_skip_element_count(alignment_) * sizeof(T);
		}

		static uint8_t log2(std::size_t a)
		{
			auto n = a ? __builtin_ctzl(a) : 0UL;
			if ( (a >> n) > 1 )
			{
				throw
					std::domain_error(
						"object alignment " + std::to_string(a) + " not a power of 2"
					);
			}
			return uint8_t(n);
		}

	public:
		template <typename C>
			fixed_string(
				common::basic_string_view<C> source_
				, std::size_t pad_
				, std::size_t alignment_
				, lock_state lock_
			)
				: fixed_string(
					source_.size() + pad_
					, alignment_
					, lock_
				)
			{
				/* for small lengths we copy */
				/* fill for alignment, returning address of first aligned byte */
				const auto c0 =
					persisted_zero_n(
						common::pointer_cast<C>(this+1)
						, front_pad()
					);

				/* first aligned element starts at first aligned byte */
				const auto e0 = common::pointer_cast<T>(c0);
				persisted_zero_n(
					persisted_copy(source_, e0)
					, pad_
				);
			}

		fixed_string(std::size_t data_len_, std::size_t alignment_, lock_state lock_)
			: _size(data_len_)
			, _ref_count(1U)
			, _log_alignment(log2(alignment_))
			, _lock(signed(lock_))
			, _data_offset(data_offset(alignment()))
		{
		}

	public:
		template <typename Allocator>
			void persist_this(const Allocator &al_)
			{
				/* If using native pmem persist functions, persist should only be necessary
				 * for the header. Data should have been persistted by pmem_memcpy_persist
				 * or pmem_memset_persist
				 */
#if MCAS_HSTORE_USE_PMEM_PERSIST
				al_.persist(this, sizeof *this);
#else
				al_.persist(this, sizeof *this + alloc_element_count() * sizeof(T));
#endif
			}

		uint64_t size() const { return _size; }
		uint64_t alignment() const noexcept { return uint64_t(1) << _log_alignment; }
		unsigned inc_ref(int, const char *) noexcept { return _ref_count++; }
		unsigned dec_ref(int, const char *) noexcept
		{
			assert(_ref_count != 0);
			return --_ref_count;
		}
		unsigned ref_count() noexcept { return _ref_count; }

		bool try_lock_shared()
		{
			const bool ok = 0 <= _lock;
			if ( ok )
			{
				++_lock;
			}
			return ok;
		}

		bool try_lock_exclusive()
		{
			bool ok = 0 == _lock;
			if ( ok )
			{
				_lock = -1;
			}
			return ok;
		}

		/* unlock when you have forgotten whether the lock you had was
		 * shared or exclusive.
		 */
		void unlock_indefinite()
		{
			switch ( _lock )
			{
			case 0: /* no lock held */
				break;
			case -1: /* exclusive lock held */
				_lock = 0;
				break;
			default: /* shared lock held */
				--_lock;
			}
		}

		void unlock_exclusive()
		{
			assert(-1 == _lock);
			_lock = 0;
		}

		void unlock_shared()
		{
			assert(0 < _lock);
			--_lock;
		}

		bool is_locked() const
		{
			return _lock != 0;
		}

		bool is_locked_exclusive() const
		{
			return _lock == -1;
		}

		void reset_lock()
		{
			_lock = 0;
		}

		T *data()
		{
			auto c0 = common::pointer_cast<char>(this) + _data_offset;
			auto e0 = common::pointer_cast<T>(c0);
			return e0;
		}

		static std::size_t front_skip_element_count(std::size_t alignment_) noexcept
		{
			/* the offset in bytes must be a multiple of both alignment (to align the data)
			 * and sizeof(T) (since the returned unit is sizeof(T) bytes)
			 */
			auto s = lcm(alignment_, sizeof(T));
			auto b = sizeof(fixed_string<T>) + s - 1; /* maximum required size in bytes */
			b = b / s * s; /* round down to a multiple of s */
			return b / sizeof(T);
		}

		std::size_t alloc_element_count() const
		{
			return front_skip_element_count() + size();
		}
	};

#endif
