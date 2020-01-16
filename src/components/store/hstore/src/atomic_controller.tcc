/*
 * (C) Copyright IBM Corporation 2018, 2019. All rights reserved.
 * US Government Users Restricted Rights - Use, duplication or disclosure restricted by GSA ADP Schedule Contract with IBM Corp.
 */

#include <algorithm> /* copy, move */
#include <stdexcept> /* out_of_range */
#include <string>
#include <vector>

/* NOTE: assumes a valid map, so must be constructed *after* the map
 */
template <typename Table>
	impl::atomic_controller<Table>::atomic_controller(
			persist_atomic<typename Table::value_type> &persist_
			, table_t &map_
			, construction_mode mode_
		)
			: allocator_type(map_.get_allocator())
			, _persist(&persist_)
			, _map(&map_)
			, _tick_expired(false)
		{
			if ( mode_ == construction_mode::reconstitute )
			{
#if USE_HEAP_CC == 3
				/* reconstitute allocated memory */
				_persist->mod_key.reconstitute(allocator_type(*this));
				_persist->mod_mapped.reconstitute(allocator_type(*this));
				if ( 0 < _persist->mod_size )
				{
					allocator_type(*this).reconstitute(_persist->mod_size, _persist->mod_ctl);
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
	}

template <typename Table>
	auto impl::atomic_controller<Table>::redo_replace() -> void
	{
		/*
		 * Note: relies on the Table::mapped_type::operator=(Table::mapped_type &)
		 * being restartable after a crash.
		 */
		auto &v = _map->at(_persist->mod_key);
		v = _persist->mod_mapped;
		this->persist(&v, sizeof v);
		redo_finish();
	}

template <typename Table>
	impl::atomic_controller<Table>::update_finisher::update_finisher(impl::atomic_controller<Table> &ctlr_)
		: _ctlr(ctlr_)
	{}

template <typename Table>
	impl::atomic_controller<Table>::update_finisher::~update_finisher()
	{
		try
		{
			_ctlr.update_finish();
		}
		catch (const perishable_expiry &)
		{
			_ctlr.tick_expired();
		}
	}

template <typename Table>
	auto impl::atomic_controller<Table>::redo_update() -> void
	{
		{
			update_finisher uf(*this);
			char *src = _persist->mod_mapped.data();
			char *dst = _map->at(_persist->mod_key).data();
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
		}
		if ( is_tick_expired() )
		{
			throw perishable_expiry();
		}
	}

template <typename Table>
	auto impl::atomic_controller<Table>::update_finish() -> void
	{
		std::size_t ct = _persist->mod_size;
		redo_finish();
		allocator_type(*this).deallocate(_persist->mod_ctl, ct);
	}

template <typename Table>
	void impl::atomic_controller<Table>::persist_range(
		const void *first_
		, const void *last_
		, const char *what_
	)
	{
		this->persist(first_, static_cast<const char *>(last_) - static_cast<const char *>(first_), what_);
	}

template <typename Table>
	void impl::atomic_controller<Table>::enter_replace(
		typename Table::allocator_type al_
		, const typename Table::key_type &key
		, const char *data_
		, std::size_t data_len_
		, std::size_t zeros_extend_
		, std::size_t alignment_
	)
	{
		_persist->mod_key = key;
		_persist->mod_mapped = typename Table::mapped_type(data_, data_ + data_len_, zeros_extend_, alignment_, al_);
		/* 8-byte atomic write */
		_persist->mod_size = -1;
		this->persist(&_persist->mod_size, sizeof _persist->mod_size);
		redo();
	}

template <typename Table>
	void impl::atomic_controller<Table>::enter_update(
		typename Table::allocator_type al_
		, const typename Table::key_type &key
		, std::vector<Component::IKVStore::Operation *>::const_iterator first
		, std::vector<Component::IKVStore::Operation *>::const_iterator last
	)
	{
		std::vector<char> src;
		std::vector<mod_control> mods;
		for ( ; first != last ; ++first )
		{
			switch ( (*first)->type() )
			{
			case Component::IKVStore::Op_type::WRITE:
				{
					const Component::IKVStore::Operation_write &wr =
						*static_cast<Component::IKVStore::Operation_write *>(
							*first
						);
					auto src_offset = src.size();
					auto dst_offset = wr.offset();
					auto size = wr.size();
					auto op_src = static_cast<const char *>(wr.data());
					/* No source for data yet, use Xs */
					std::copy(op_src, op_src + size, std::back_inserter(src));
					mods.emplace_back(src_offset, dst_offset, size);
				}
				break;
			default:
				throw std::invalid_argument("Unknown update code " + std::to_string(int((*first)->type())));
			};
		}
		_persist->mod_key = key;
		_persist->mod_mapped =
			typename Table::mapped_type(
				src.begin()
				, src.end()
				, al_
			);

		{
			/* ERROR: local pointer can leak */
			persistent_t<typename std::allocator_traits<allocator_type>::pointer> ptr = nullptr;
			allocator_type(*this).allocate(
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
		_persist->mod_size = mods.size();
		this->persist(&_persist->mod_size, sizeof _persist->mod_size);
		redo();
	}
