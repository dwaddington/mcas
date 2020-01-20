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


#ifndef COMANCHE_HSTORE_DEALLOCATOR_RC_H
#define COMANCHE_HSTORE_DEALLOCATOR_RC_H

#include "heap_rc.h"
#include "persister_cc.h"

#include <cstddef> /* size_t, ptrdiff_t */

template <typename T, typename Persister>
	class deallocator_rc;

template <>
	class deallocator_rc<void, persister>
	{
	public:
		using value_type = void;
	};

template <typename Persister>
	class deallocator_rc<void, Persister>
	{
	public:
		using value_type = void;
	};

template <typename T, typename Persister = persister>
	class deallocator_rc
		: public Persister
	{
		heap_rc _pool;
	public:
		using value_type = T;
		using size_type = std::size_t;

		explicit deallocator_rc(void *area_, Persister p_ = Persister())
			: Persister(p_)
			, _pool(area_)
		{}

		explicit deallocator_rc(const heap_rc &pool_, Persister p_ = Persister()) noexcept
			: Persister(p_)
			, _pool(pool_)
		{}

		explicit deallocator_rc(const deallocator_rc &) noexcept = default;

		template <typename U, typename P>
			explicit deallocator_rc(const deallocator_rc<U, P> &d_) noexcept
				: deallocator_rc(d_.pool())
			{}

		deallocator_rc &operator=(const deallocator_rc &e_) = delete;

		void deallocate(
			T* p
			, size_type sz_
			, size_type alignment_ = alignof(T)
		)
		{
			_pool->free(p, sizeof(T) * sz_, alignment_);
		}

		void persist(const void *ptr, size_type len, const char * = nullptr) const
		{
			Persister::persist(ptr, len);
		}

		auto pool() const
		{
			return _pool;
		}
	};

#endif
