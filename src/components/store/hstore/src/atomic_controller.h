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


#ifndef _COMANCHE_HSTORE_ATOMIC_CTL_H_
#define _COMANCHE_HSTORE_ATOMIC_CTL_H_

#include "construction_mode.h"
#include "persist_atomic.h"
#include "persist_fixed_string.h"

#include <type_traits> /* is_base_of */
#include <vector>

namespace impl
{
	template <typename Table>
		class atomic_controller
			: private std::allocator_traits<typename Table::allocator_type>::template rebind_alloc<mod_control>
		{
			using table_t = Table;
			using allocator_type =
				typename std::allocator_traits<typename table_t::allocator_type>::template rebind_alloc<mod_control>;

			using persist_t = persist_atomic<typename Table::value_type>;
			using mod_key_t = typename persist_t::mod_key_t;
			persist_t *_persist; /* persist_atomic is a bad name. Should be a noun. */
			table_t *_map;
			bool _tick_expired;
			class update_finisher
			{
				impl::atomic_controller<Table> &_ctlr;
			public:
				update_finisher(impl::atomic_controller<Table> &ctlr_);
				~update_finisher();
			};
			void redo_update();
			void update_finish();
			void redo_replace();
			void redo_finish();
			/* Helpers for the perishable test, to avoid an exception in the finish_update destructor */
			void tick_expired() { _tick_expired = true; }
			bool is_tick_expired() { auto r = _tick_expired; _tick_expired = false; return r; }
		public:
			atomic_controller(
				persist_atomic<typename Table::value_type> &persist_
				, table_t &map_
				, construction_mode mode_
			);
			atomic_controller(const atomic_controller &) = delete;
			atomic_controller& operator=(const atomic_controller &) = delete;

			void redo();

			void persist_range(const void *first_, const void *last_, const char *what_);

			void enter_update(
				typename Table::allocator_type al_
				, const typename Table::key_type &key
				, std::vector<Component::IKVStore::Operation *>::const_iterator first
				, std::vector<Component::IKVStore::Operation *>::const_iterator last
			);
			void enter_replace(
				typename Table::allocator_type al
				, const typename Table::key_type &key
				, const char *data
				, std::size_t data_len
				, std::size_t zeros_extend
				, std::size_t alignment
			);
			friend class atomic_controller<Table>::update_finisher;
	};
}

#include "atomic_controller.tcc"

#endif
