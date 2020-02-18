/*
   Copyright [2017-2020] [IBM Corporation]
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


#ifndef MCAS_HSTORE_ALLOCATOR_RC_H
#define MCAS_HSTORE_ALLOCATOR_RC_H

#include "deallocator_rc.h"

#include "bad_alloc_cc.h"
#include "persister_cc.h"
#include "persistent.h"

#include <cstddef> /* size_t, ptrdiff_t */

template <typename T, typename Persister>
	class allocator_rc;

template <>
	class allocator_rc<void, persister>
		: public deallocator_rc<void, persister>
	{
	public:
		using deallocator_type = deallocator_rc<void, persister>;
		using typename deallocator_type::value_type;
	};

template <typename Persister>
	class allocator_rc<void, Persister>
		: public deallocator_rc<void, Persister>
	{
	public:
		using deallocator_type = deallocator_rc<void, Persister>;
		using typename deallocator_type::value_type;
	};

template <typename T, typename Persister = persister>
	class allocator_rc
		: public deallocator_rc<T, Persister>
	{
	public:
		using deallocator_type = deallocator_rc<T, Persister>;
		using typename deallocator_type::value_type;
		using typename deallocator_type::size_type;

		explicit allocator_rc(void *area_, std::size_t size_, Persister p_ = Persister())
			: deallocator_rc<T, Persister>(area_, size_, p_)
		{}

		explicit allocator_rc(void *area_, Persister p_ = Persister())
			: deallocator_rc<T, Persister>(area_, p_)
		{}

		allocator_rc(const heap_rc &pool_, Persister p_ = Persister()) noexcept
			: deallocator_rc<T, Persister>(pool_, (p_))
		{}

		allocator_rc(const allocator_rc &a_) noexcept = default;

		template <typename U, typename P>
			allocator_rc(const allocator_rc<U, P> &a_) noexcept
				: allocator_rc(a_.pool())
			{}

		allocator_rc &operator=(const allocator_rc &a_) = delete;

		auto allocate(
			size_type s
			, size_type alignment = alignof(T)
		) -> value_type *
		{
			auto ptr = this->pool()->alloc(s * sizeof(T), alignment);
			if ( ptr == 0 )
			{
				throw bad_alloc_cc(0, s, sizeof(T));
			}
			return static_cast<value_type *>(ptr);
		}

		void allocatep(
			persistent<value_type *> &ptr
			, size_type sz
			, size_type alignment = alignof(T)
		)
		{
			allocate(ptr, sz, alignment);
		}

		void allocate(
			value_type * &ptr
			, size_type sz
			, size_type alignment = alignof(T)
		)
		{
			ptr = allocate(sz, alignment);
		}

		void reconstitute(
			size_type s
			, const void *location
			, const char * = nullptr
		)
		{
			this->pool()->inject_allocation(location, s * sizeof(T));
		}

		bool is_reconstituted(
			const void *location
		)
		{
			return this->pool()->is_reconstituted(location);
		}
	};

#endif
