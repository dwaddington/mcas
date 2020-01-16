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


#ifndef COMANCHE_HSTORE_SESSION_H
#define COMANCHE_HSTORE_SESSION_H

#include "atomic_controller.h"
#include "as_emplace.h"
#include "construction_mode.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <tbb/scalable_allocator.h>
#pragma GCC diagnostic pop
#include <memory>
#include <utility> /* move */
#include <vector>

class Devdax_manager;

struct lock_result
{
	enum class e_state
	{
		extant, created, not_created
	} state;
	Component::IKVStore::key_t key;
	void *value;
	std::size_t value_len;
};

#if USE_CC_HEAP == 4
template <typename A>
	class tentative_allocation_state;

/* allocator making use of tentative_allocation_state */
template <typename A>
	struct tentative_allocator
		: public A
	{
		using traits_type = std::allocator_traits<A>;
		using value_type = typename traits_type::value_type;
		using size_type = typename traits_type::size_type;

		tentative_allocation_state<A> *tas;
		tentative_allocator(const A &a_, tentative_allocation_state<A> *tas_)
			: A(a_)
			, tas(tas_)
		{}

		tentative_allocator(const tentative_allocator &) = default;
		tentative_allocator& operator=(const tentative_allocator &) = default;

		void allocate(
			persistent<value_type *> & p_
			, size_type s_
			, size_type alignment_
		);

		void allocate(
			value_type * & p_
			, size_type s_
			, size_type alignment_
		);

		template <typename U> struct rebind {
			using other = tentative_allocator<typename traits_type::template rebind_alloc<U>>;
		};
	};

/* tentative (and transient) allocation state */
template <typename A>
	class tentative_allocation_state
	{
		impl::allocation_state_emplace *_ase;
		unsigned _allocation_index;
		tentative_allocator<A> _a;
	public:
		tentative_allocation_state(const A &a_, impl::allocation_state_emplace *ase_)
			: _ase(ase_)
			, _allocation_index(0)
			, _a(a_, this)
		{}
		tentative_allocation_state(const tentative_allocation_state &) = delete;
		tentative_allocation_state& operator=(const tentative_allocation_state &) = delete;
		auto allocator() const { return _a; }

		/* record either of the two allocations which might occur during an emplace */
		void record_allocation(void *p_)
		{
			if ( p_ )
			{
				assert(_allocation_index < 2);
				_ase->record_allocation(_allocation_index, p_);
				++_allocation_index;
			}
		}

		~tentative_allocation_state()
		{
			_ase->clear(_a);
		}
	};

template <typename A>
	void tentative_allocator<A>::allocate(
		persistent<value_type *> & p_
		, size_type s_
		, size_type alignment_
	)
	{
		A::allocate(p_, s_, alignment_);
		if ( p_ )
		{
			tas->record_allocation(p_);
		}
	}

template <typename A>
	void tentative_allocator<A>::allocate(
		value_type * & p_
		, size_type s_
		, size_type alignment_
	)
	{
		A::allocate(p_, s_, alignment_);
		if ( p_ )
		{
			tas->record_allocation(p_);
		}
	}
#endif

/* open_pool_handle, alloc_t, table_t */
template <typename Handle, typename Allocator, typename Table, typename LockType>
	class session
		: public Handle
	{
		using table_t = Table;
		using lock_type_t = LockType;
		using key_t = typename table_t::key_type;
		using mapped_t = typename table_t::mapped_type;
		using allocator_type = Allocator;
		Allocator _heap;
		table_t _map;
		impl::atomic_controller<table_t> _atomic_state;
#if USE_CC_HEAP == 4
		impl::allocation_state_emplace *_ase;
#endif
		bool _debug;

		class lock_impl
			: public Component::IKVStore::Opaque_key
		{
			std::string _s;
		public:
			lock_impl(const std::string &s_)
				: Component::IKVStore::Opaque_key{}
				, _s(s_)
			{}
			const std::string &key() const { return _s; }
		};

		static bool try_lock(table_t &map, lock_type_t type, const key_t &p_key)
		{
			return
				type == Component::IKVStore::STORE_LOCK_READ
				? map.lock_shared(p_key)
				: map.lock_unique(p_key)
				;
		}

		template <typename K>
			class definite_lock
			{
				table_t &_map;
				const K &_key;
			public:
				definite_lock(table_t &map_, const K &pkey_)
					: _map(map_)
					, _key(pkey_)
				{
					if ( ! _map.lock_unique(_key) )
					{
						throw std::range_error("unable to get unique lock");
					}
				}
				~definite_lock()
				{
					_map.unlock(_key); /* release lock */
				}
			};

		auto allocator() const { return _heap; }
		table_t &map() noexcept { return _map; }
		const table_t &map() const noexcept { return _map; }

		/* Atomic replace of a mapped_type located by the key */
		void enter_update(
			typename table_t::key_type const &key
			, std::vector<Component::IKVStore::Operation *>::const_iterator first
			, std::vector<Component::IKVStore::Operation *>::const_iterator last
		)
		{
			_atomic_state.enter_update(allocator(), key, first, last);
		}

		auto enter_replace(
			typename table_t::key_type const &key
			, const void *data
			, std::size_t data_len
			, std::size_t zeros_extend
			, std::size_t alignment
		)
		{
			_atomic_state.enter_replace(allocator(), key, static_cast<const char *>(data), data_len, zeros_extend, alignment);
		}
	public:
		using handle_t = Handle;
		/* PMEMoid, persist_data_t */
		template <typename OID, typename Persist>
			explicit session(
				OID
#if USE_CC_HEAP == 2
					heap_oid_
#endif
				, const pool_path &path_
				, Handle &&pop_
				, Persist *persist_data_
				, bool debug_ = false
			)
			: Handle(std::move(pop_))
			, _heap(
				Allocator(
#if USE_CC_HEAP == 2
					*new
						(pmemobj_direct(heap_oid_))
						heap_co(heap_oid_)
#elif USE_CC_HEAP == 3 || USE_CC_HEAP == 4
					this->pool() /* not used */
#endif /* USE_CC_HEAP */
				)
			)
			, _map(persist_data_, _heap)
			, _atomic_state(*persist_data_, _map)
#if USE_CC_HEAP == 4
			, _ase(&persist_data_->eas())
#endif
			, _debug(debug_)
		{}

		explicit session(
			const pool_path &
			, Handle &&pop_
			, construction_mode mode_
			, bool debug_ = false
		)
			: Handle(std::move(pop_))
			, _heap(
				Allocator(
					this->pool()->locate_heap()
				)
			)
			, _map(&this->pool()->persist_data(), mode_, _heap)
			, _atomic_state(this->pool()->persist_data(), _map, mode_)
#if USE_CC_HEAP == 4
			, _ase(&this->pool()->persist_data().ase())
#endif
			, _debug(debug_)
		{}

		~session()
		{
#if USE_CC_HEAP == 3 || USE_CC_HEAP == 4
			this->pool()->quiesce();
#endif
		}

		session(const session &) = delete;
		session& operator=(const session &) = delete;
		/* session constructor and get_pool_regions only */
		const Handle &handle() const { return *this; }
		auto *pool() const { return handle().get(); }

		auto insert(
			const std::string &key,
			const void * value,
			const std::size_t value_len
		)
		{
			auto cvalue = static_cast<const char *>(value);
#if USE_CC_HEAP == 3

			return
				map().emplace(
					std::piecewise_construct
					, std::forward_as_tuple(key.begin(), key.end(), this->allocator())
					, std::forward_as_tuple(cvalue, cvalue + value_len, this->allocator())
				);
#elif USE_CC_HEAP == 4
			/* Start of an emplace. Storage allocated by this->allocator()
			 * is to be disclaimed upon a restart unless
			 *  (1) verified in-use by the map, or
			 *  (2) forgotten by the tentative allocator going out of scope.
			 */
			tentative_allocation_state<allocator_type> tas(this->allocator(), _ase);

			return
				map().emplace(
					std::piecewise_construct
					, std::forward_as_tuple(key.begin(), key.end(), tas.allocator())
					, std::forward_as_tuple(cvalue, cvalue + value_len, tas.allocator())
				);
#else
#error unsupported USE_CC_HEAP
#endif
		}

		void update_by_issue_41(
			const std::string &key,
			const void * value,
			const std::size_t value_len,
			void * /* old_value */,
			const std::size_t old_value_len
		)
		{
			definite_lock<std::string> dl(this->map(), key);

			/* hstore issue 41: "a put should replace any existing k,v pairs that match.
			 * If the new put is a different size, then the object should be reallocated.
			 * If the new put is the same size, then it should be updated in place."
			 */
			if ( value_len != old_value_len )
			{
				auto p_key = key_t(key.begin(), key.end(), this->allocator());
				enter_replace(p_key, value, value_len, 0, 8 /* requested default mapped_type alignment */);
			}
			else
			{
				std::vector<std::unique_ptr<Component::IKVStore::Operation>> v;
				v.emplace_back(std::make_unique<Component::IKVStore::Operation_write>(0, value_len, value));
				std::vector<Component::IKVStore::Operation *> v2;
				std::transform(v.begin(), v.end(), std::back_inserter(v2), [] (const auto &i) { return i.get(); });
				this->atomic_update(key, v2);
			}
		}

		auto get(
			const std::string &key,
			void* buffer,
			std::size_t buffer_size
		) const -> std::size_t
		{
			auto &v = map().at(key);
			auto value_len = v.size();

			if ( value_len <= buffer_size )
			{
				std::memcpy(buffer, v.data(), value_len);
			}
			return value_len;
		}

		auto get_alloc(
			const std::string &key
		) const -> std::tuple<void *, std::size_t>
		{
			auto &v = map().at(key);
			auto value_len = v.size();

			auto value = ::scalable_malloc(value_len);
			if ( ! value )
			{
				throw std::bad_alloc();
			}

			std::memcpy(value, v.data(), value_len);
			return std::pair<void *, std::size_t>(value, value_len);
		}

		auto get_value_len(
			const std::string & key
		) const -> std::size_t
		{
			auto &v = this->map().at(key);
			return v.size();
		}

		auto pool_grow(
			const std::unique_ptr<Devdax_manager> &dax_mgr_
			, const std::size_t increment_
		) const -> std::size_t
		{
			return this->pool()->grow(dax_mgr_, increment_);
		}

		void resize_mapped(
			const std::string &key
			, std::size_t new_mapped_len
			, std::size_t alignment
		)
		{
			definite_lock<std::string> dl(this->map(), key);

			auto p_key = key_t(key.begin(), key.end(), this->allocator());

			mapped_t &m = this->map().at(p_key);
			/* Replace the data if the size changes or if the data must be realigned */
			if ( m.size() != new_mapped_len || reinterpret_cast<std::size_t>(m.data()) % alignment != 0 )
			{
				this->enter_replace(
					p_key
					, m.data()
					, std::min(m.size(), new_mapped_len)
					, m.size() < new_mapped_len ? new_mapped_len - m.size() : std::size_t(0)
					, alignment
				);
			}
		}

		auto lock(
			const std::string &key
			, lock_type_t type
			, void *const value
			, const std::size_t value_len
		) -> lock_result
		{
#if USE_CC_HEAP == 3
			const auto p_key = key_t(key.begin(), key.end(), this->allocator());
			try
			{
				mapped_t &val = this->map().at(p_key);
				return {
					lock_result::e_state::extant
					, try_lock(this->map(), type, p_key)
						? new lock_impl(key)
						: Component::IKVStore::KEY_NONE
					, val.data()
					, val.size()
				};
			}
#elif USE_CC_HEAP == 4
			tentative_allocation_state<allocator_type> tas(this->allocator(), _ase);
			const auto p_key = key_t(key.begin(), key.end(), tas.allocator());
			try
			{
				mapped_t &val = this->map().at(p_key);
				return {
					lock_result::e_state::extant
					, try_lock(this->map(), type, p_key)
						? new lock_impl(key)
						: Component::IKVStore::KEY_NONE
					, val.data()
					, val.size()
				};
			}
#else
#error unsupported USE_CC_HEAP
#endif
			catch ( const std::out_of_range & )
			{
				/* if the key is not found
				 * we create it and allocate value space equal in size to
				 * value_len
				 */
				if (
					true
/* Change: "key is not found AND the input length is not zero" */
#if 1
					&&
					value_len != 0
#endif
				)
				{
					if ( _debug )
					{
						PLOG(PREFIX "allocating object %zu bytes", __func__, value_len);
					}

#if USE_CC_HEAP == 3
					auto r =
						this->map().emplace(
							std::piecewise_construct
							, std::forward_as_tuple(p_key)
							, std::forward_as_tuple(value_len, this->allocator())
						);

#elif USE_CC_HEAP == 4
					auto r =
						this->map().emplace(
							std::piecewise_construct
							, std::forward_as_tuple(p_key)
							, std::forward_as_tuple(value_len, tas.allocator())
						);

#else
#error unsupported USE_CC_HEAP
#endif
					if ( ! r.second )
					{
						return { lock_result::e_state::extant, Component::IKVStore::KEY_NONE, value, value_len };
					}

					return {
						lock_result::e_state::created
						, try_lock(this->map(), type, p_key)
							? new lock_impl(key)
							: Component::IKVStore::KEY_NONE
						, r.first->second.data()
						, r.first->second.size()
					};
				}
				else
				{
					return { lock_result::e_state::not_created, Component::IKVStore::KEY_NONE, value, value_len };
				}
			}
		}

		auto unlock(Component::IKVStore::key_t key_) -> status_t
		{
			if ( key_ )
			{
				if ( auto lk = dynamic_cast<lock_impl *>(key_) )
				{
					try {
						auto p_key = key_t(lk->key().begin(), lk->key().end(), this->allocator());
						this->map().unlock(p_key);
					}
					catch ( const std::out_of_range &e )
					{
						return Component::IKVStore::E_KEY_NOT_FOUND;
					}
					catch( ... ) {
						throw General_exception(PREFIX "failed unexpectedly", __func__);
					}
					delete lk;
				}
				else
				{
					return S_OK; /* not really OK - was not one of our locks */
				}
			}
			return S_OK;
		}

		bool get_auto_resize() const
		{
			return this->map().get_auto_resize();
		}

		void set_auto_resize(bool auto_resize)
		{
			this->map().set_auto_resize(auto_resize);
		}

		auto erase(
			const std::string &key
		) -> std::size_t
		{
			auto p_key = key_t(key.begin(), key.end(), this->allocator());
			return map().erase(p_key);
		}

		auto count() const -> std::size_t
		{
			return map().size();
		}

		auto bucket_count() const -> std::size_t
		{
			typename table_t::size_type count = 0;
			/* bucket counter */
			for (
				auto n = this->map().bucket_count()
				; n != 0
				; --n
			)
			{
				auto last = this->map().end(n-1);
				for ( auto first = this->map().begin(n-1); first != last; ++first )
				{
					++count;
				}
			}
			return count;
		}

		auto map(
			std::function
			<
				int(const void * key, std::size_t key_len,
				const void * val, std::size_t val_len)
			> function
		) -> void
		{
			for ( auto &mt : this->map() )
			{
				const auto &pstring = mt.first;
				const auto &mapped = mt.second;
				function(reinterpret_cast<const void*>(pstring.data()),
				pstring.size(), mapped.data(), mapped.size());
			}

		}

		void atomic_update_inner(
			key_t &key
			, const std::vector<Component::IKVStore::Operation *> &op_vector
		)
		{
			this->enter_update(key, op_vector.begin(), op_vector.end());
		}

		void atomic_update(
			const std::string& key
			, const std::vector<Component::IKVStore::Operation *> &op_vector
		)
		{
			auto p_key = key_t(key.begin(), key.end(), this->allocator());
			this->atomic_update_inner(p_key, op_vector);
		}

		void lock_and_atomic_update(
			const std::string& key
			, const std::vector<Component::IKVStore::Operation *> &op_vector
		)
		{
			auto p_key = key_t(key.begin(), key.end(), this->allocator());
			definite_lock<key_t> m(this->map(), p_key);
			this->atomic_update_inner(p_key, op_vector);
		}

		void *allocate_memory(
			std::size_t size
			, std::size_t alignment
		)
		{
			if ( alignment != 0 && alignment < sizeof(void*) )
			{
				throw std::invalid_argument("alignment < sizeof(void*)");
			}
			if ( (alignment & (alignment - 1)) != 0 )
			{
				throw std::invalid_argument("alignment is not a power of 2");
			}

#if USE_CC_HEAP == 4
			/* ERROR: leaks memory on a crash */
#endif
			persistent_t<char *> p = nullptr;
			allocator().allocate(p, size, alignment);
			return p;
		}

		void free_memory(
			const void* addr
			, size_t size
		)
		{
			persistent_t<char *> p = static_cast<char *>(const_cast<void *>(addr));
#if USE_CC_HEAP == 4
			/* ERROR: leaks memory on a crash */
#endif
			allocator().deallocate(p, size);
		}

		unsigned percent_used() const
		{
			return this->pool()->percent_used();
		}

	};

#endif
