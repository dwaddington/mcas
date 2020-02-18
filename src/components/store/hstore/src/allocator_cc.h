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


#ifndef MCAS_HSTORE_ALLOCATOR_CC_H
#define MCAS_HSTORE_ALLOCATOR_CC_H

#include "deallocator_cc.h"

#include "bad_alloc_cc.h"
#include "heap_cc.h"
#include "persister_cc.h"
#include "persistent.h"

#include <cstddef> /* size_t, ptrdiff_t */

template <typename T, typename Persister>
	class allocator_cc;

template <>
	class allocator_cc<void, persister>
		: public deallocator_cc<void, persister>
	{
	public:
		using deallocator_type = deallocator_cc<void, persister>;
		using typename deallocator_type::value_type;
	};

template <typename Persister>
	class allocator_cc<void, Persister>
		: public deallocator_cc<void, Persister>
	{
	public:
		using deallocator_type = deallocator_cc<void, Persister>;
		using typename deallocator_type::value_type;
};

template <typename T, typename Persister = persister>
	class allocator_cc
		: public deallocator_cc<T, Persister>
	{
	public:
		using deallocator_type = deallocator_cc<T, Persister>;
		using typename deallocator_type::size_type;
		using typename deallocator_type::value_type;
		using typename deallocator_type::pointer_type;

		allocator_cc(const heap_cc &pool_, Persister p_ = Persister()) noexcept
			: deallocator_cc<T, Persister>(pool_, (p_))
		{}

		allocator_cc(const allocator_cc &a_) noexcept = default;

		template <typename U, typename P>
			allocator_cc(const allocator_cc<U, P> &a_) noexcept
				: allocator_cc(a_.pool())
			{}

		allocator_cc &operator=(const allocator_cc &a_) = delete;

		void arm_extend()
		{
			this->pool()->extend_arm();
		}
		void disarm_extend()
		{
			this->pool()->extend_disarm();
		}

		void allocate(
			pointer_type & p_
			, size_type s_
			, size_type alignment_
		)
		{
			this->pool()->alloc(reinterpret_cast<persistent_t<void *> *>(&p_), s_ * sizeof(T), alignment_);
			/* Error: this check is too late;
			 * most of the intersting information is gone.
			 */
			if ( p_ == 0 )
			{
				throw bad_alloc_cc(s_, sizeof(T), alignment_);
			}
		}
	};

#endif
