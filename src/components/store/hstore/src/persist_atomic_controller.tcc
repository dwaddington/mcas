/*
   Copyright [2018-2021] [IBM Corporation]
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

#include "hstore_config.h"
#include "alloc_key.h" /* AK_ACTUAL */
#include "hstore_kv_types.h"
#include "monitor_emplace.h"
#include <common/perf/tm.h>
#if MCAS_HSTORE_USE_PMEM_PERSIST
#include <libpmem.h>
#endif
#include <boost/align/aligned_allocator.hpp>
#include <gsl/pointers>
#include <algorithm> /* copy, move */
#include <array>
#include <stdexcept> /* out_of_range */
#include <string>
#include <vector>

struct perishable_expiry;

/* NOTE: assumes a valid map, so must be constructed *after* the map
 */
template <typename Table>
	impl::persist_atomic_controller<Table>::persist_atomic_controller(
			persist_type &persist_
			, allocator_type al_
			, construction_mode mode_
		)
			: allocator_type(al_)
			, _persisted(&persist_)
#if 0
			, _tick_expired(false)
#endif
		{
			if ( mode_ == construction_mode::reconstitute && al_.pool()->can_reconstitute() )
			{
				/* reconstitute allocated memory */
				_persisted->mod_key.reconstitute(allocator_type(*this));
				_persisted->mod_mapped.reconstitute(allocator_type(*this));
				if ( 0 < _persisted->mod_size )
				{
					allocator_type(*this).reconstitute(std::size_t(_persisted->mod_size), _persisted->mod_ctl);
				}
				else
				{
				}
			}
			try
			{
				TM_ROOT()
				do_op(TM_REF0);
			}
			catch ( const std::range_error & )
			{
			}
		}

template <typename Table>
	auto impl::persist_atomic_controller<Table>::do_op(TM_ACTUAL0) -> void
	{
		if ( _persisted->mod_size != 0 )
		{
			if ( 0 < _persisted->mod_size )
			{
				do_update(TM_REF0);
			}
			else if ( -2 == _persisted->mod_size )
			{
				do_swap();
			}
			else /* Issue 41-style replacement */
			{
				do_replace();
			}
		}
	}

template <typename Table>
	auto impl::persist_atomic_controller<Table>::do_finish() -> void
	{
		_persisted->mod_size = 0;
		persist_range(&_persisted->mod_size, &_persisted->mod_size + 1, "atomic size");

		monitor_emplace<allocator_type> m(*this);

		/* Identify the element owner for the allocations to be freed */
		if ( _persisted->ase() )
		{
			auto pe = gsl::not_null<allocator_type *>(this);
			_persisted->ase()->em_record_owner_addr_and_bitmask(&_persisted->mod_owner, 1, *pe);
		}

		/* Atomic set of initiative to clear elements owned by persist->mod_owner */
		_persisted->mod_owner = 0;
		this->persist(&_persisted->mod_owner, sizeof _persisted->mod_owner);
		/* Clear the elements */
		_persisted->mod_key.clear();
		_persisted->mod_mapped.clear();
	}

template <typename Table>
	auto impl::persist_atomic_controller<Table>::do_replace() -> void
	{
		TM_ROOT()
		/*
		 * Note: relies on the Table::mapped_type::operator=(Table::mapped_type &)
		 * being restartable after a crash.
		 */
		auto &v = _persisted->map->at(TM_REF _persisted->mod_key);
		std::get<0>(v) = _persisted->mod_mapped;
		/* Unclear whether timestamps should be updated. The only guidance we have is the
		 * mapstore implementation, which does update timestamps on a replace.
		 */
#if ENABLE_TIMESTAMPS
		std::get<1>(v) = tsc_now();
#endif
		this->persist(&v, sizeof v);
		do_finish();
	}

template <typename Table>
	impl::persist_atomic_controller<Table>::update_finisher::update_finisher(impl::persist_atomic_controller<Table> &ctlr_)
		: _ctlr(ctlr_)
	{}

template <typename Table>
	impl::persist_atomic_controller<Table>::update_finisher::~update_finisher() noexcept(! TEST_HSTORE_PERISHABLE)
	{
		if ( ! perishable_expiry::is_current() )
		{
			_ctlr.update_finish();
		}
	}

template <typename Table>
	auto impl::persist_atomic_controller<Table>::do_update(TM_ACTUAL0) -> void
	{
		TM_SCOPE()
		{
			update_finisher uf(*this);
			auto *src = _persisted->mod_mapped.data();
			/* NOTE: depends on mapped type */
			auto &v = _persisted->map->at(TM_REF _persisted->mod_key);
			auto *dst = std::get<0>(v).data();
			auto mod_ctl = &*(_persisted->mod_ctl);
			for ( auto i = mod_ctl; i != &mod_ctl[_persisted->mod_size]; ++i )
			{
				const std::size_t o_d = i->offset_dst;
				const auto dst_first = &dst[o_d];
				switch(i->op)
				{
				case component::IKVStore::Op_type::WRITE:
					{
						const std::size_t o_s = i->offset_src;
						const auto src_first = &src[o_s];
						const std::size_t sz = i->size;
						const auto src_last = src_first + sz;
#if MCAS_HSTORE_USE_PMEM_PERSIST
						::pmem_memcpy_persist(&*dst_first, &*src_first, (src_last - src_first) * sizeof *dst_first);
#else
						/* NOTE: could be replaced with a pmem persistent memcpy */
						persist_range(
							dst_first
							, std::copy(src_first, src_last, dst_first)
							, "atomic_update write"
						);
#endif
					}
					break;
				case component::IKVStore::Op_type::ZERO:
					/* NOTE: could be replaced with a pmem persistent memcpy */
					persist_range(
						dst_first
						, std::fill_n(dst_first, i->size, common::byte{})
						, "atomic_update zero"
					);
					break;
				default:
					throw std::logic_error("Unsupported update code " + std::to_string(int(persistent_load(i->op))));
				}
			}
			/* Unclear whether timestamps should be updated. The only guidance we have is the
			 * mapstore implementation, which does update timestamps on a replace.
			 */
#if ENABLE_TIMESTAMPS
			std::get<1>(v) = tsc_now();
#endif
		}
#if 0
		if ( is_tick_expired() )
		{
			throw perishable_expiry(__LINE__);
		}
#endif
	}

template <typename Table>
	auto impl::persist_atomic_controller<Table>::update_finish() -> void
	{
		std::size_t ct = std::size_t(_persisted->mod_size);
		do_finish();
		allocator_type(*this).deallocate(_persisted->mod_ctl, ct);
	}

template <typename Table>
	template <typename T>
		void impl::persist_atomic_controller<Table>::copy_and_persist(T *dst_, const void *src_, const char *why_)
		{
#if MCAS_HSTORE_USE_PMEM_PERSIST
			::pmem_memcpy_persist(dst_, src_, sizeof *dst_);
			(void)why_;
#else
#	pragma GCC diagnostic push
#	if 8 <= __GNUC__
#	pragma GCC diagnostic ignored "-Wclass-memaccess"
#	endif
			std::memcpy(dst_, src_, sizeof *dst_);
#	pragma GCC diagnostic pop
			this->persist(dst_, sizeof *dst_, why_);
#endif
		}

template <typename Table>
	auto impl::persist_atomic_controller<Table>::do_swap() -> void
	{
		copy_and_persist(_persisted->_swap.pd0, _persisted->_swap.pd1, "do swap part 1");
		copy_and_persist(_persisted->_swap.pd1, &_persisted->_swap.temp[0], "do swap part 2");
		/* Unclear whether timestamps should be updated. The only guidance we have is the
		 * mapstore implementation, which does update timestamps on a swap.
		 */
#if ENABLE_TIMESTAMPS
		auto now = tsc_now();
		std::get<1>(*_persisted->_swap.pd0) = now;
		std::get<1>(*_persisted->_swap.pd1) = now;
#endif
#if 0
		if ( is_tick_expired() )
		{
			throw perishable_expiry(__LINE__);
		}
#endif
		do_finish();
	}

template <typename Table>
	void impl::persist_atomic_controller<Table>::persist_range(
		const void *first_
		, const void *last_
		, const char *what_
	)
	{
		this->persist(first_, std::size_t(static_cast<const char *>(last_) - static_cast<const char *>(first_)), what_);
	}

template <typename Table>
	void impl::persist_atomic_controller<Table>::enter_replace(
		AK_ACTUAL
		TM_ACTUAL
		typename table_type::allocator_type al_
		, table_type *map_
		, lock_state lock_
		, const string_view_key key
		, const string_view_value value_
		, std::size_t zeros_extend_
		, std::size_t alignment_
	)
	{
		TM_SCOPE()
		/* leaky */

		_persisted->mod_owner = 0;
		{
			monitor_emplace<allocator_type> m(*this);

			if ( al_.pool()->is_crash_consistent() )
			{
				auto pe = gsl::not_null<allocator_type *>(this);
				_persisted->ase()->em_record_owner_addr_and_bitmask(&_persisted->mod_owner, 1, *pe);
			}
			_persisted->mod_key.assign(AK_REF key, lock_state::free, al_);
			_persisted->mod_mapped.assign(AK_REF value_, zeros_extend_, alignment_, lock_, al_);
			_persisted->map = map_;
			this->persist(&_persisted->map, sizeof _persisted->map);
			_persisted->mod_owner = 1;
			this->persist(&_persisted->mod_owner, sizeof _persisted->mod_owner);
		}

		/* 8-byte atomic write */
		_persisted->mod_size = -1;
		this->persist(&_persisted->mod_size, sizeof _persisted->mod_size);
		do_op(TM_REF0);
	}

template <typename Table>
	template <typename IT>
		void impl::persist_atomic_controller<Table>::enter_update(
			TM_ACTUAL
			AK_ACTUAL
			typename table_type::allocator_type al_
			, table_type *map_
			, lock_state lock_
			, const string_view_key key
			, IT first
			, IT last
		)
		{
			TM_SCOPE()
			std::vector<string_view_value::value_type, boost::alignment::aligned_allocator<string_view_value::value_type, 64>> src;
			std::vector<mod_control> mods;
			for ( ; first != last ; ++first )
			{
				auto op = (*first)->type();
				switch ( op )
				{
				case component::IKVStore::Op_type::WRITE:
					{
						const auto &wr =
							*static_cast<component::IKVStore::Operation_write *>(
								*first
							);
						auto src_offset = src.size();
						auto dst_offset = wr.offset();
						auto size = wr.size();
						auto op_src = static_cast<string_view_value::const_pointer>(wr.data());
						std::copy(op_src, op_src + size, std::back_inserter(src));
						mods.emplace_back(op, src_offset, dst_offset, size);
					}
					break;
				case component::IKVStore::Op_type::ZERO:
					{
						const auto &zr =
							*static_cast<component::IKVStore::Operation_zero *>(
								*first
							);
						mods.emplace_back(op, 0, zr.offset(), zr.size());
					}
					break;
				default:
					throw std::invalid_argument("Unsupported update code " + std::to_string(int(op)));
				};
			}

			/* leaky */
			_persisted->mod_key.assign(AK_REF key, lock_state::free, al_);
			_persisted->mod_mapped.assign(AK_REF string_view_value(&*src.begin(), src.size()), lock_, al_);
			{
				/* leaky ERROR: local pointer can leak */
				persistent_t<typename std::allocator_traits<allocator_type>::pointer> ptr = nullptr;
				allocator_type(*this).allocate(
					AK_REF
					ptr
					, mods.size()
					, alignof(mod_control)
				);
				new (&*ptr) mod_control[mods.size()];
				_persisted->mod_ctl = ptr;
			}

#if MCAS_HSTORE_USE_PMEM_PERSIST
			::pmem_memcpy_persist(&*_persisted->mod_ctl, &*mods.begin(), (mods.end() - mods.begin()) * sizeof *_persisted->mod_ctl);
#else
			std::copy(mods.begin(), mods.end(), &*_persisted->mod_ctl);
			persist_range(
				&*_persisted->mod_ctl
			, &*_persisted->mod_ctl + mods.size()
				, "mod control"
			);
#endif
			_persisted->map = map_;
			this->persist(&_persisted->map, sizeof _persisted->map);
			/* 8-byte atomic write */
			_persisted->mod_size = std::ptrdiff_t(mods.size());
			this->persist(&_persisted->mod_size, sizeof _persisted->mod_size);
			do_op(TM_REF0);
		}

template <typename Table>
	void impl::persist_atomic_controller<Table>::enter_swap(
		mt &d0
		, mt &d1
	)
	{
		TM_ROOT()
		_persisted->_swap.pd0 = &d0;
		_persisted->_swap.pd1 = &d1;
#if MCAS_HSTORE_USE_PMEM_PERSIST
		::pmem_memcpy_persist(&_persisted->_swap.temp[0], &d0, _persisted->_swap.temp.size());
#else
#pragma GCC diagnostic push
#	if 8 <= __GNUC__
#	pragma GCC diagnostic ignored "-Wclass-memaccess"
#	endif
		std::memcpy(&_persisted->_swap.temp[0], &d0, _persisted->_swap.temp.size());
#pragma GCC diagnostic pop
		this->persist(&_persisted->_swap, sizeof _persisted->_swap);
#endif

		/* 8-byte atomic write */
		_persisted->mod_size = -2;
		this->persist(&_persisted->mod_size, sizeof _persisted->mod_size);
		do_op(TM_REF0);
	}
