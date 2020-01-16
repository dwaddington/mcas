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

#ifndef _MCAS_HSTORE_ALLOCATION_STATE_EMPLACE_H
#define _MCAS_HSTORE_ALLOCATION_STATE_EMPLACE_H

#include "hstore_config.h"
#include "owner.h"
#include <array>
#include <atomic>

namespace impl
{
	/* Client-side persistent allocation state when using the crash-consistent
	 * allocator in an emplace operation */
	class allocation_state_emplace
	{
		/* Allocation pointers stored here are tentative, and will be
		 * disclaimed upon restart *unless* (*pmask & mask) != 0, which
		 * indicates that the map ownershi mask at pmask acknowledges
		 * ownership of the pointers.
		 * An emplace operation allocate up to two values: key string
		 * and value string.
		 *
		 * The intent behind atomic is that stores are seen in the order
		 * in which they where coded.
		 */

		/* expect at most key, value allocations */
		static constexpr unsigned max_expected_allocations = 2;
		std::atomic<void *> _ptr0;
		std::atomic<void *> _ptr1;
		std::atomic<persistent_atomic_t<owner::value_type> *> _pmask;
		std::atomic<owner::value_type> _mask;
	public:
		allocation_state_emplace();
		allocation_state_emplace(allocation_state_emplace &&);
			
		bool is_in_use(const void *ptr);

		template <typename Allocator>
			void clear(Allocator av);

		void record_allocation(unsigned index_, void *p_)
		{
			assert(index_ < max_expected_allocations);
			( index_ == 0 ? _ptr0 : _ptr1 ) = p_;
		}
		template <typename Persister>
			void record_owner_addr_and_bitmask(
				persistent_atomic_t<owner::value_type> *pmask_
				, owner::value_type mask_
				, Persister p_
		)
		{
			_pmask = pmask_;
			_mask = mask_;
			p_.persist(this, sizeof *this);
		}
	};		
}

#include "as_emplace.tcc"

#endif
