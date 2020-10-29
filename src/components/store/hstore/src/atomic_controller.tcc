/*
   Copyright [2018-2019] [IBM Corporation]
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
#include "hstore_kv_types.h"
#include "monitor_emplace.h"
#include <algorithm> /* copy, move */
#include <stdexcept> /* out_of_range */
#include <string>
#include <vector>

struct perishable_expiry;

/* NOTE: assumes a valid map, so must be constructed *after* the map
 */
template <typename Table>
	impl::atomic_controller<Table>::atomic_controller(
			persist_atomic<typename table_t::value_type> &persist_
			, table_t &map_
			, construction_mode mode_
		)
			: allocator_type(map_.get_allocator())
			, _persist(&persist_)
			, _map(&map_)
#if 0
			, _tick_expired(false)
#endif
		{
			if ( mode_ == construction_mode::reconstitute )
			{
#if USE_CC_HEAP == 3
				/* reconstitute allocated memory */
				_persist->mod_key.reconstitute(allocator_type(*this));
				_persist->mod_mapped.reconstitute(allocator_type(*this));
				if ( 0 < _persist->mod_size )
				{
					allocator_type(*this).reconstitute(std::size_t(_persist->mod_size), _persist->mod_ctl);
				}
				else
				{
				}
#endif
			}
			try
			{
				redo();
			}
			catch ( const std::range_error & )
			{
			}
		}

template <typename Table>
	auto impl::atomic_controller<Table>::redo() -> void
	{
		if ( _persist->mod_size != 0 )
		{
			if ( 0 < _persist->mod_size )
			{
				redo_update();
			}
			else if ( -2 == _persist->mod_size )
			{
				redo_swap();
			}
			else /* Issue 41-style replacement */
			{
				redo_replace();
			}
		}
	}

template <typename Table>
	auto impl::atomic_controller<Table>::redo_finish() -> void
	{
		_persist->mod_size = 0;
		persist_range(&_persist->mod_size, &_persist->mod_size + 1, "atomic size");

		monitor_emplace<allocator_type> m(*this);

		/* Identify the element owner for the allocations to be freed */
#if USE_CC_HEAP == 4
		{
			auto pe = static_cast<allocator_type *>(this);
			_persist->ase().em_record_owner_addr_and_bitmask(&_persist->mod_owner, 1, *pe);
		}
#endif
		/* Atomic set of initiative to clear elements owned by persist->mod_owner */
		_persist->mod_owner = 0;
		this->persist(&_persist->mod_owner, sizeof _persist->mod_owner);
		/* Clear the elements */
		_persist->mod_key.clear();
		_persist->mod_mapped.clear();
	}

template <typename Table>
	auto impl::atomic_controller<Table>::redo_replace() -> void
	{
		/*
		 * Note: relies on the Table::mapped_type::operator=(Table::mapped_type &)
		 * being restartable after a crash.
		 */
		auto &v = _map->at(_persist->mod_key);
		std::get<0>(v) = _persist->mod_mapped;
		/* Unclear whether timestamps should be updated. The only guidance we have is the
		 * mapstore implementation, which does update timestamps on a replace.
		 */
#if ENABLE_TIMESTAMPS
		std::get<1>(v) = tsc_now();
#endif
		this->persist(&v, sizeof v);
		redo_finish();
	}

template <typename Table>
	impl::atomic_controller<Table>::update_finisher::update_finisher(impl::atomic_controller<Table> &ctlr_)
		: _ctlr(ctlr_)
	{}

template <typename Table>
	impl::atomic_controller<Table>::update_finisher::~update_finisher() noexcept(! TEST_HSTORE_PERISHABLE)
	{
		if ( ! perishable_expiry::is_current() )
		{
			_ctlr.update_finish();
		}
	}

template <typename Table>
	auto impl::atomic_controller<Table>::redo_update() -> void
	{
		{
			update_finisher uf(*this);
			char *src = _persist->mod_mapped.data();
			/* NOTE: depends on mapped type */
			auto v = _map->at(_persist->mod_key);
			char *dst = std::get<0>(v).data();
			auto mod_ctl = &*(_persist->mod_ctl);
			for ( auto i = mod_ctl; i != &mod_ctl[_persist->mod_size]; ++i )
			{
				std::size_t o_s = i->offset_src;
				auto src_first = &src[o_s];
				std::size_t sz = i->size;
				auto src_last = src_first + sz;
				std::size_t o_d = i->offset_dst;
				auto dst_first = &dst[o_d];
				/* NOTE: could be replaced with a pmem persistent memcpy */
				persist_range(
					dst_first
					, std::copy(src_first, src_last, dst_first)
					, "atomic ctl"
				);
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
	auto impl::atomic_controller<Table>::update_finish() -> void
	{
		std::size_t ct = std::size_t(_persist->mod_size);
		redo_finish();
		allocator_type(*this).deallocate(_persist->mod_ctl, ct);
	}

template <typename Table>
	auto impl::atomic_controller<Table>::redo_swap() -> void
	{
#pragma GCC diagnostic push
#if 9 <= __GNUC__
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
		std::memcpy(_persist->_swap.pd0, _persist->_swap.pd1, sizeof *_persist->_swap.pd0);
#pragma GCC diagnostic pop
		this->persist(_persist->_swap.pd0, sizeof *_persist->_swap.pd0, "redo swap part 1");
#pragma GCC diagnostic push
#if 9 <= __GNUC__
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
		std::memcpy(_persist->_swap.pd1, &_persist->_swap.temp[0], sizeof *_persist->_swap.pd1);
#pragma GCC diagnostic pop
		this->persist(_persist->_swap.pd1, sizeof *_persist->_swap.pd1, "redo swap part 2");
		/* Unclear whether timestamps should be updated. The only guidance we have is the
		 * mapstore implementation, which does update timestamps on a swap.
		 */
#if ENABLE_TIMESTAMPS
		auto now = tsc_now();
		std::get<1>(*_persist->_swap.pd0) = now;
		std::get<1>(*_persist->_swap.pd1) = now;
#endif
#if 0
		if ( is_tick_expired() )
		{
			throw perishable_expiry(__LINE__);
		}
#endif
		redo_finish();
	}

template <typename Table>
	void impl::atomic_controller<Table>::persist_range(
		const void *first_
		, const void *last_
		, const char *what_
	)
	{
		this->persist(first_, std::size_t(static_cast<const char *>(last_) - static_cast<const char *>(first_)), what_);
	}

template <typename Table>
	void impl::atomic_controller<Table>::enter_replace(
		AK_ACTUAL
		typename table_t::allocator_type al_
		, const std::string &key
		, const char *data_
		, std::size_t data_len_
		, std::size_t zeros_extend_
		, std::size_t alignment_
	)
	{
		/* leaky */

		_persist->mod_owner = 0;
		{
			monitor_emplace<allocator_type> m(*this);
#if USE_CC_HEAP == 4
			{
				auto pe = static_cast<allocator_type *>(this);
				_persist->ase().em_record_owner_addr_and_bitmask(&_persist->mod_owner, 1, *pe);
			}
#endif
			_persist->mod_key.assign(AK_REF key.begin(), key.end(), al_);
			_persist->mod_mapped.assign(AK_REF data_, data_ + data_len_, zeros_extend_, alignment_, al_);
			_persist->mod_owner = 1;
			this->persist(&_persist->mod_owner, sizeof _persist->mod_owner);
		}

		/* 8-byte atomic write */
		_persist->mod_size = -1;
		this->persist(&_persist->mod_size, sizeof _persist->mod_size);
		redo();
	}

template <typename Table>
	void impl::atomic_controller<Table>::enter_update(
		AK_ACTUAL
		typename table_t::allocator_type al_
		, const std::string &key
		, std::vector<component::IKVStore::Operation *>::const_iterator first
		, std::vector<component::IKVStore::Operation *>::const_iterator last
	)
	{
		std::vector<char> src;
		std::vector<mod_control> mods;
		for ( ; first != last ; ++first )
		{
			switch ( (*first)->type() )
			{
			case component::IKVStore::Op_type::WRITE:
				{
					const component::IKVStore::Operation_write &wr =
						*static_cast<component::IKVStore::Operation_write *>(
							*first
						);
					auto src_offset = src.size();
					auto dst_offset = wr.offset();
					auto size = wr.size();
					auto op_src = static_cast<const char *>(wr.data());
					std::copy(op_src, op_src + size, std::back_inserter(src));
					mods.emplace_back(src_offset, dst_offset, size);
				}
				break;
			default:
				throw std::invalid_argument("Unknown update code " + std::to_string(int((*first)->type())));
			};
		}

		/* leaky */
		_persist->mod_key.assign(AK_REF key.begin(), key.end(), al_);
		_persist->mod_mapped.assign(AK_REF src.begin(), src.end(), al_);

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
			_persist->mod_ctl = ptr;
		}

		std::copy(mods.begin(), mods.end(), &*_persist->mod_ctl);
		persist_range(
			&*_persist->mod_ctl
			, &*_persist->mod_ctl + mods.size()
			, "mod control"
		);
		/* 8-byte atomic write */
		_persist->mod_size = std::ptrdiff_t(mods.size());
		this->persist(&_persist->mod_size, sizeof _persist->mod_size);
		redo();
	}

template <typename Table>
	void impl::atomic_controller<Table>::enter_swap(
		mt &d0
		, mt &d1
	)
	{
		_persist->_swap.pd0 = &d0;
		_persist->_swap.pd1 = &d1;
		std::memcpy(&_persist->_swap.temp[0], &d0, _persist->_swap.temp.size());
		this->persist(&_persist->_swap, sizeof _persist->_swap);

		/* 8-byte atomic write */
		_persist->mod_size = -2;
		this->persist(&_persist->mod_size, sizeof _persist->mod_size);
		redo();
	}
