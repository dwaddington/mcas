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

#include <common/pointer_cast.h>
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
		unsigned _ref_count;
		unsigned _alignment;
		signed _lock;
		uint64_t _size;

		/* offset to data, for a particular alignment */
		std::size_t front_pad() const noexcept { return data_offset() - sizeof *this; }

		std::size_t front_skip_element_count() const
		{
			return front_skip_element_count(_alignment);
		}

		static std::size_t data_offset(std::size_t alignment_) noexcept
		{
			return front_skip_element_count(alignment_) * sizeof(T);
		}
		std::size_t data_offset() const noexcept { return data_offset(_alignment); }

	public:
		template <typename IT>
			fixed_string(
				IT first_, IT last_
				, std::size_t pad_
				, std::size_t alignment_
			)
				: fixed_string(
					std::size_t(last_-first_) + pad_
					, alignment_
				)
			{
				/* for small lengths we copy */
				/* fill for alignment, returning address of first aligned byte */
				const auto c0 =
					std::fill_n(
						common::pointer_cast<char>(this+1)
						, front_pad()
						, 0
				);
				/* first aligned element starts at first aligned byte */
				const auto e0 = common::pointer_cast<T>(c0);
				std::fill_n(
					std::copy(first_, last_, e0)
					, pad_
					, T()
				);
			}

		fixed_string(std::size_t data_len_, std::size_t alignment_)
			: _ref_count(1U)
			, _alignment(unsigned(alignment_))
			, _lock(0)
			, _size(data_len_)
		{
			if ( _alignment != alignment_ )
			{
				throw
					std::domain_error(
						"object alignment too large; probably exceeds 2^31"
					);
			}
		}

	public:
		template <typename Allocator>
			void persist_this(const Allocator &al_)
			{
				al_.persist(this, sizeof *this + alloc_element_count() * sizeof(T));
			}
		uint64_t size() const { return _size; }
		uint64_t alignment() const noexcept { return _alignment; }
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

		void unlock()
		{
			switch ( _lock )
			{
			case 0:
				break;
			case -1:
				_lock = 0;
				break;
			default:
				--_lock;
			}
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
			auto c0 = common::pointer_cast<char>(this) + data_offset();
			auto e0 = common::pointer_cast<T>(c0);
			return e0;
		}

		static std::size_t front_skip_element_count(std::size_t alignment_) noexcept
		{
			if ( alignment_ == 0 )
			{
				alignment_ = 1;
			}
			/* the offset in bytes must be a multiple of both alignment (to align the data)
			 * and sizeof(T) (since the returned unit is sizeof(T) bytes
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
