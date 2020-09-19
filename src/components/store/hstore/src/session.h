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

#ifndef MCAS_HSTORE_SESSION_H
#define MCAS_HSTORE_SESSION_H

#include "hstore_config.h"
#include "atomic_controller.h"
#include "as_emplace.h"
#include "construction_mode.h"
#include "key_not_found.h"
#include "logging.h"
#include "hstore_alloc_type.h"
#include "hstore_nupm_types.h"
#include "is_locked.h"
#include "monitor_emplace.h"
#include "monitor_pin.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#include <tbb/scalable_allocator.h>
#pragma GCC diagnostic pop
#include <common/logging.h>
#include <common/time.h>
#include <limits>
#include <map>
#include <memory>
#include <utility> /* move */
#include <vector>

template <typename Handle, typename Allocator, typename Table, typename LockType>
	struct session;

class Devdax_manager;

struct lock_result
{
	enum class e_state
	{
		extant, created, not_created, creation_failed
	} state;
	component::IKVStore::key_t key;
	void *value;
	std::size_t value_len;
	const char *key_ptr;
};

/* open_pool_handle, alloc_t, table_t */
template <typename Handle, typename Allocator, typename Table, typename LockType>
	struct session
		: public Handle
		, private common::log_source
	{
	private:
		struct pool_iterator;
		using table_t = Table;
		using lock_type_t = LockType;
		using key_t = typename table_t::key_type;
		using mapped_t = typename table_t::mapped_type;
		using data_t = typename std::tuple_element<0, mapped_t>::type;
		using allocator_type = Allocator;
		allocator_type _heap;
		bool _pin_seq; /* used only for force undo_redo call */
		table_t _map;
		impl::atomic_controller<table_t> _atomic_state;
		std::uint64_t _writes;
		std::map<pool_iterator *, std::shared_ptr<pool_iterator>> _iterators;

		struct pool_iterator
			: public component::IKVStore::Opaque_pool_iterator
		{
		private:
			using table_t = Table;
			std::uint64_t _mark;
			typename table_t::const_iterator _end;
		public:
			typename table_t::const_iterator _iter;
		public:
			explicit pool_iterator(
				const session<Handle, Allocator, Table, LockType> * session_
			)
				: _mark(session_->writes())
				, _end(session_->map().end())
				, _iter(session_->map().begin())
			{}

			bool is_end() const { return _iter == _end; }
			bool check_mark(std::uint64_t writes) const { return _mark == writes; }
		};

		struct lock_impl
			: public component::IKVStore::Opaque_key
		{
		private:
			std::string _s;
		public:
			lock_impl(const std::string &s_)
				: component::IKVStore::Opaque_key{}
				, _s(s_)
			{
#if 0
				PINF(PREFIX "%s:%d lock: %s", LOCATION, _s.c_str());
#endif
			}
			const std::string &key() const { return _s; }
			~lock_impl()
			{
#if 0
				PINF(PREFIX "%s:%d unlock: %s", LOCATION, _s.c_str());
#endif
			}
		};

		static bool try_lock(typename std::tuple_element<0, mapped_t>::type &d, lock_type_t type)
		{
			return
				type == component::IKVStore::STORE_LOCK_READ
				? d.try_lock_shared()
				: d.try_lock_exclusive()
				;
		}

		struct definite_lock
		{
		private:
			typename table_t::iterator _it;
		public:
			template <typename K>
				definite_lock(
					AK_ACTUAL
					table_t &map_, const K &key_, allocator_type al_)
					: _it(map_.find(key_))
				{
					if ( _it == map_.end() )
					{
						throw impl::key_not_found{};
					}

					auto &d = data();

					if ( ! d.lockable() )
					{
						/* Allocating space for a lockable value is tricky.
						 *
						 * allocator_cc (crash-consistent allocator):
						 *   see notes in as_pin.h
						 *
						 * allocatpr_rc (reconstituting allocator):
						 *   TBD
						 */

						/* convert value to lockable */
#if 0
						using monitor = monitor_pin<session::allocator_type>;
						using monitor = monitor_pin<hstore_alloc_type<Persister>::heap_alloc_t>;
#endif
						monitor_pin_data<hstore_alloc_type<Persister>::heap_alloc_t> mp(d, al_.pool());
						/* convert d to immovable data */
						d.pin(AK_REF mp.get_cptr(), al_);
					}

					if ( ! d.try_lock_exclusive() )
					{
						throw impl::is_locked{};
					}
				}

			auto &mapped() const
			{
				return _it->second;
			}

			auto &data() const
			{
				auto &m = mapped();
				return std::get<0>(m);
			}

			~definite_lock()
			try
			{
				if ( ! perishable_expiry::is_current() )
				{
					/* release lock */
					const auto &d = data();
					d.unlock();
				}
			}
			catch ( const Exception & )
			{
				return;
			}
			catch ( const std::exception & )
			{
				return;
			}
		};

		auto allocator() const { return _heap; }
		table_t &map() noexcept { return _map; }
		const table_t &map() const noexcept { return _map; }

	public:
		using handle_t = Handle;
		/* PMEMoid, persist_data_t */
		template <typename OID, typename Persist>
			explicit session(
				OID
#if USE_CC_HEAP == 2
					heap_oid_
#endif
				, Handle &&pop_
				, Persist *persist_data_
				, unsigned debug_level_ = 0
			)
			: Handle(std::move(pop_))
			, common::log_source(debug_level_)
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
			, _pin_seq(undo_redo_pin_data(_heap) || undo_redo_pin_key(_heap))
			, _map(persist_data_, _heap)
			, _atomic_state(*persist_data_, _map)
			, _writes(0)
			, _iterators()
		{}

		auto writes() const { return _writes; }

		explicit session(
			AK_ACTUAL
			Handle &&pop_
			, construction_mode mode_
			, unsigned debug_level_ = 0
		)
			: Handle(std::move(pop_))
			, common::log_source(debug_level_)
			, _heap(
				Allocator(
					this->pool()->locate_heap()
				)
			)
			, _pin_seq(undo_redo_pin_data(AK_REF _heap) || undo_redo_pin_key(AK_REF _heap))
			, _map(AK_REF &this->pool()->persist_data()._persist_map, mode_, _heap)
			, _atomic_state(this->pool()->persist_data()._persist_atomic, _map, mode_)
			, _writes(0)
			, _iterators()
		{}

		~session()
		{
#if USE_CC_HEAP == 3 || USE_CC_HEAP == 4
			this->pool()->quiesce();
#endif
		}

		bool undo_redo_pin_data(
			AK_ACTUAL
			allocator_type heap_
		)
		{
#if USE_CC_HEAP == 3
			AK_REF_VOID;
			(void) (heap_);
			return true;
#elif USE_CC_HEAP == 4
			auto &aspd = heap_.pool()->aspd();
			auto armed = aspd.is_armed();
			if ( armed )
			{
				/* _arm_ptr points to a new cptr, within a "large", within a persist_fixed_string */
				auto *pfs = data_t::pfs_from_cptr_ref(*aspd.arm_ptr());

				if ( aspd.was_callback_tested() )
				{
					/* S_uncommitted or S_committed: allocator had an allocation in "in_doubt" state,
					 * meaning that cptr, if not null contains an allocation address and not inline data
					 */
					if ( pfs->get_cptr().P )
					{
						/* S_committed: roll forward */
						pfs->pin(AK_REF aspd.get_cptr(), this->allocator());
					}
					else
					{
						/* S_uncommitted: roll back */
						pfs->set_cptr(aspd.get_cptr(), this->allocator());
					}
				}
				else
				{
					/* S_calling: allocator did not reach "in doubt" state, meaning that
					 * cptr contains null or old inline data.
					 */
					/* roll back */
					pfs->set_cptr(aspd.get_cptr(), this->allocator());
				}
				aspd.disarm(this->allocator());
			}
			else
			{
				/* S_unarmed: do nothing */
			}
			return armed;
#endif
		}

		bool undo_redo_pin_key(
			AK_ACTUAL
			allocator_type heap_
		)
		{
#if USE_CC_HEAP == 3
			AK_REF_VOID;
			(void) (heap_);
			return true;
#elif USE_CC_HEAP == 4
			auto &aspk = heap_.pool()->aspk();
			auto armed = aspk.is_armed();
			if ( armed )
			{
				/* _arm_ptr points to a new cptr, within a "large", within a persist_fixed_string */
				auto *pfs = key_t::pfs_from_cptr_ref(*aspk.arm_ptr());

				if ( aspk.was_callback_tested() )
				{
					/* S_uncommitted or S_committed: allocator had an allocation in "in_doubt" state,
					 * meaning that cptr, if not null contains an allocation address and not inline data
					 */
					if ( pfs->get_cptr().P )
					{
						/* S_committed: roll forward */
						pfs->pin(AK_REF aspk.get_cptr(), this->allocator());
					}
					else
					{
						/* S_uncommitted: roll back */
						pfs->set_cptr(aspk.get_cptr(), this->allocator());
					}
				}
				else
				{
					/* S_calling: allocator did not reach "in doubt" state, meaning that
					 * cptr contains null or old inline data.
					 */
					/* roll back */
					pfs->set_cptr(aspk.get_cptr(), this->allocator());
				}
				aspk.disarm(this->allocator());
			}
			else
			{
				/* S_unarmed: do nothing */
			}
			return armed;
#endif
		}

		session(const session &) = delete;
		session& operator=(const session &) = delete;
		/* session constructor and get_pool_regions only */
		const Handle &handle() const { return *this; }
		auto *pool() const { return handle().get(); }

		auto insert(
			AK_ACTUAL
			const std::string &key,
			const void * value,
			const std::size_t value_len
		)
		{
			auto cvalue = static_cast<const char *>(value);

#if USE_CC_HEAP == 4
			/* Start of an emplace. Storage allocated by this->allocator()
			 * is to be disclaimed upon a restart unless
			 *  (1) verified in-use by the map (i.e., owner bit bit set to 1), or later
			 *  (2) forgotten by the tentative_allocation_state_emplace going out of scope, in which case the map bit has long since been set to 1.
			 */
			monitor_emplace<Allocator> m(this->allocator());
#endif
			++_writes;
			return
				map().emplace(
					AK_REF
					std::piecewise_construct
					, std::forward_as_tuple(AK_REF key.begin(), key.end(), this->allocator())
					, std::forward_as_tuple(
/* we wish that std::tuple had piecewise_construct, but it does not. */
#if 0
						std::piecewise_construct,
#endif
						std::forward_as_tuple(AK_REF cvalue, cvalue + value_len, this->allocator())
#if ENABLE_TIMESTAMPS
						, impl::tsc_now()
#endif
					)
				);
		}

		void update_by_issue_41(
			AK_ACTUAL
			const std::string &key,
			const void * value,
			const std::size_t value_len,
			void * /* old_value */,
			const std::size_t old_value_len
		)
		{
			definite_lock dl(AK_REF this->map(), key, _heap);

			/* hstore issue 41: "a put should replace any existing k,v pairs that match.
			 * If the new put is a different size, then the object should be reallocated.
			 * If the new put is the same size, then it should be updated in place."
			 */
			if ( value_len != old_value_len )
			{
				_atomic_state.enter_replace(
					AK_REF
					this->allocator()
					, key
					, static_cast<const char *>(value)
					, value_len
					, 0
					, std::tuple_element<0, mapped_t>::type::default_alignment /* requested default mapped_type alignment */
				);
			}
			else
			{
				std::vector<std::unique_ptr<component::IKVStore::Operation>> v;
				v.emplace_back(std::make_unique<component::IKVStore::Operation_write>(0, value_len, value));
				std::vector<component::IKVStore::Operation *> v2;
				std::transform(v.begin(), v.end(), std::back_inserter(v2), [] (const auto &i) { return i.get(); });
				this->atomic_update(AK_REF key, v2);
			}
		}

		auto get(
			const std::string &key,
			void* buffer,
			std::size_t buffer_size
		) const -> std::size_t
		{
			auto &v = map().at(key);
			auto value_len = std::get<0>(v).size();

			if ( value_len <= buffer_size )
			{
				std::memcpy(buffer, std::get<0>(v).data(), value_len);
			}
			return value_len;
		}

		auto get_alloc(
			const std::string &key
		) const -> std::tuple<void *, std::size_t>
		{
			auto &v = map().at(key);
			auto value_len = std::get<0>(v).size();

			auto value = ::scalable_malloc(value_len);
			if ( ! value )
			{
				throw std::bad_alloc();
			}

			std::memcpy(value, std::get<0>(v).data(), value_len);
			return std::pair<void *, std::size_t>(value, value_len);
		}

		auto get_value_len(
			const std::string & key
		) const -> std::size_t
		{
			auto &v = this->map().at(key);
			return std::get<0>(v).size();
		}

#if ENABLE_TIMESTAMPS
		auto get_write_epoch_time(
			const std::string & key
		) const -> std::size_t
		{
			auto &v = this->map().at(key);
			// TO FIX
			//                      return impl::tsc_to_epoch(std::get<1>(v));
			return boost::numeric_cast<std::size_t>(impl::tsc_to_epoch(std::get<1>(v)).seconds());
		}
#endif

		auto pool_grow(
			const std::unique_ptr<Devdax_manager> &dax_mgr_
			, const std::size_t increment_
		) const -> std::size_t
		{
			return this->pool()->grow(dax_mgr_, increment_);
		}

		void resize_mapped(
			AK_ACTUAL
			const std::string &key
			, std::size_t new_mapped_len
			, std::size_t alignment
		)
		{
			definite_lock dl(AK_REF this->map(), key, _heap);

			auto &v = this->map().at(key);
			auto &d = std::get<0>(v);
			/* Replace the data if the size changes or if the data should be realigned */
			if ( d.size() != new_mapped_len || reinterpret_cast<std::size_t>(d.data()) % alignment != 0 )
			{
				this->_atomic_state.enter_replace(
					AK_REF
					this->allocator()
					, key
					, d.data()
					, std::min(d.size(), new_mapped_len)
					, d.size() < new_mapped_len ? new_mapped_len - d.size() : std::size_t(0)
					, alignment
				);
			}
		}

		auto lock(
			AK_ACTUAL
			const std::string &key
			, lock_type_t type
			, void *const value
			, const std::size_t value_len
		) -> lock_result
		{
#if USE_CC_HEAP == 4
			monitor_emplace<Allocator> me(this->allocator());
#endif
			auto it = this->map().find(key);
			if ( it == this->map().end() )
			{
				/* if the key is not found
				 * we create it and allocate value space equal in size to
				 * value_len (but, as a special case, the creation is suppressed
				 * if value_len is 0).
				 */
				if ( value_len != 0 )
				{
					CPLOG(1, PREFIX "allocating object %zu bytes", LOCATION, value_len);

					++_writes;
					auto r =
						this->map().emplace(
							AK_REF
							std::piecewise_construct
							, std::forward_as_tuple(AK_REF fixed_data_location, key.begin(), key.end(), this->allocator())
							, std::forward_as_tuple(
/* we wish that std::tuple had piecewise_construct, but it does not. */
#if 0
								std::piecewise_construct,
#endif
								std::forward_as_tuple(AK_REF fixed_data_location, value_len, this->allocator())
#if ENABLE_TIMESTAMPS
								, impl::tsc_now()
#endif
							)
						);

					if ( ! r.second )
					{
						/* Should not happen. If we could not find it, should be able to create it */
						return { lock_result::e_state::creation_failed, component::IKVStore::KEY_NONE, value, value_len, nullptr };
					}

					auto &v = *r.first;
					auto &k = v.first;
					auto &m = v.second;
					auto &d = std::get<0>(m);
#if 0
					PLOG(PREFIX "data exposed (newly created): %p", LOCATION, d.data_fixed());
					PLOG(PREFIX "key exposed (newly created): %p", LOCATION, k.data_fixed());
#endif
					return {
						lock_result::e_state::created
						, try_lock(d, type)
							? new lock_impl(key)
							: component::IKVStore::KEY_NONE
						, d.data_fixed()
						, d.size()
						, k.data_fixed()
					};
				}
				else
				{
					return { lock_result::e_state::not_created, component::IKVStore::KEY_NONE, value, value_len, nullptr };
				}
			}
			else
			{
				auto &v = *it;
				const key_t &k = v.first;
				if ( ! k.is_fixed() )
				{
					auto &km = const_cast<typename std::remove_const<key_t>::type &>(k);
					monitor_pin_key<hstore_alloc_type<Persister>::heap_alloc_t> mp(km, _heap.pool());
					/* convert k to a immovable data */
					km.pin(AK_REF mp.get_cptr(), this->allocator());
				}
				mapped_t &m = v.second;
				auto &d = std::get<0>(m);
				/*
				 * "The complexity of software is an essential property, not an accidental one.
				 * Hence, descriptions of a software entity that abstract away its complexity
				 * often abstract away its essence." -- Fred Brooks, No Silver Bullet (1986)
				 */
				if( ! d.is_fixed() )
				{
					monitor_pin_data<hstore_alloc_type<Persister>::heap_alloc_t> mp(d, _heap.pool());
					/* convert d to a immovable data */
					d.pin(AK_REF mp.get_cptr(), this->allocator());
				}
#if 0
				PLOG(PREFIX "data exposed (extant): %p", LOCATION, d.data_fixed());
				PLOG(PREFIX "key exposed (extant): %p", LOCATION, k.data_fixed());
#endif
				/* Note: now returning E_LOCKED on lock failure as per a private request */
				lock_result r {
					lock_result::e_state::extant
					, try_lock(d, type)
						? new lock_impl(key)
						: component::IKVStore::KEY_NONE
					, d.data_fixed()
					, d.size()
					, k.data_fixed()
				};

#if ENABLE_TIMESTAMPS
				if ( type == component::IKVStore::STORE_LOCK_WRITE && r.key != component::IKVStore::KEY_NONE )
				{
					std::get<1>(m) = impl::tsc_now();
				}
#endif
				return r;
			}
		}

		auto unlock(component::IKVStore::key_t key_, component::IKVStore::unlock_flags_t flags_) -> status_t
		{
			if ( key_ )
			{
#if 0
				PINF(PREFIX "attempt unlock ...", LOCATION);
#endif
				if ( auto lk = dynamic_cast<lock_impl *>(key_) )
				{
#if 0
					PINF(PREFIX "attempt unlock %s", LOCATION, lk->key().c_str());
#endif
					try {
						auto &m = *this->map().find(lk->key());
						auto &v = std::get<1>(m);
						auto &d = std::get<0>(v);
						if ( flags_ & component::IKVStore::UNLOCK_FLAGS_FLUSH )
						{
							d.flush_if_locked_exclusive(this->allocator());
						}
						d.unlock();
					}
					catch ( const std::out_of_range &e )
					{
#if 0
						PINF(PREFIX "attempt unlock : key not found", LOCATION);
#endif
						return component::IKVStore::E_KEY_NOT_FOUND;
					}
					catch( ... ) {
						PLOG(PREFIX "attempt unlock : failed unexpected", LOCATION);
						throw General_exception(PREFIX "failed unexpectedly", __func__);
					}
					delete lk;
				}
				else
				{
					return E_INVAL; /* was not one of our locks */
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
		) -> status_t
		{
			auto it = this->map().find(key);
			if ( it != this->map().end() )
			{
				auto &v = *it;
				auto &m = v.second;
				auto &d = std::get<0>(m);
				if ( ! d.is_locked() )
				{
#if USE_CC_HEAP == 4
					monitor_emplace<Allocator> me(this->allocator());
#endif
					++_writes;
					map().erase(it);
					return S_OK;
				}
				else
				{
					return E_LOCKED;
				}
			}
			else
			{
				return component::IKVStore::E_KEY_NOT_FOUND;
			}
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
			> function_
		) -> void
		{
			for ( auto &mt : this->map() )
			{
				const auto &pstring = mt.first;
				const auto &m = mt.second;
				const auto &d = std::get<0>(m);
				function_(
					reinterpret_cast<const void*>(pstring.data())
					, pstring.size()
					, d.data()
					, d.size()
				);
			}
		}

		auto map(
			std::function
			<
				int(const void * key
				, std::size_t key_len
				, const void * val
				, std::size_t val_len
				, common::tsc_time_t timestamp
				)
			> function_
			, common::epoch_time_t t_begin
			, common::epoch_time_t t_end
		) -> status_t
		{
#if ENABLE_TIMESTAMPS
			using raw_t = decltype(impl::epoch_to_tsc(t_begin).raw());
			auto begin_tsc = t_begin.is_defined() ? std::numeric_limits<raw_t>::min() : impl::epoch_to_tsc(t_begin).raw();
			auto end_tsc = t_end.is_defined() ? std::numeric_limits<raw_t>::max() : impl::epoch_to_tsc(t_end).raw();

			for ( auto &mt : this->map() )
			{
				const auto &pstring = mt.first;
				const auto &m = mt.second;
				const auto t = std::get<1>(m).raw();
#if 0
{
std::ostringstream s;
auto e = impl::tsc_to_epoch(std::get<1>(m));
s << "(hstore::session::map) (t_begin " << t_begin.seconds() << " ref.timestamp " << e.seconds() << " t_end " << t_end.seconds() << ") (begin_tsc " << begin_tsc << " t " << t << " end_tsc " << end_tsc << ")";
PLOG("%s", s.str().c_str());
}
#endif
				if ( begin_tsc <= t && t <= end_tsc )
				{
					function_(
						reinterpret_cast<const void*>(pstring.data())
						, pstring.size()
						, std::get<0>(m).data()
						, std::get<0>(m).size()
						, impl::tsc_to_epoch(std::get<1>(m))
					);
				}
			}
			return S_OK;
#else
			(void) function_;
			(void) t_begin;
			(void) t_end;
			return E_FAIL;
#endif
		}

		void atomic_update_inner(
			AK_ACTUAL
			const std::string &key
			, const std::vector<component::IKVStore::Operation *> &op_vector
		)
		{
			_atomic_state.enter_update(AK_REF this->allocator(), key, op_vector.begin(), op_vector.end());
		}

		void atomic_update(
			AK_ACTUAL
			const std::string& key
			, const std::vector<component::IKVStore::Operation *> &op_vector
		)
		{
			this->atomic_update_inner(AK_REF key, op_vector);
		}

		void lock_and_atomic_update(
			AK_ACTUAL
			const std::string& key
			, const std::vector<component::IKVStore::Operation *> &op_vector
		)
		{
			definite_lock m(AK_REF this->map(), key, _heap.pool());
			this->atomic_update_inner(AK_REF key, op_vector);
		}

		void *allocate_memory(
			AK_ACTUAL
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

			persistent_t<char *> p = nullptr;
#if USE_CC_HEAP == 4
			/* ERROR: leaks memory on a crash */
#endif
			allocator().allocate(AK_REF p, size, alignment);
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

		void flush_memory(
			const void* addr
			, size_t size
		)
		{
			persistent_t<char *> p = static_cast<char *>(const_cast<void *>(addr));
            CPLOG(2, "%s: %p %zx", __func__, addr, size);
			allocator().persist(p, size);
		}

		unsigned percent_used() const
		{
			return this->pool()->percent_used();
		}

		auto swap_keys(
			AK_ACTUAL
			const std::string &key0
			, const std::string &key1
		) -> status_t
		try
		{
			definite_lock d0(AK_REF this->map(), key0, _heap.pool());
			definite_lock d1(AK_REF this->map(), key1, _heap.pool());

			_atomic_state.enter_swap(
				d0.mapped()
				, d1.mapped()
			);

			return S_OK;
		}
		catch ( const std::domain_error & )
		{
			return component::IKVStore::E_KEY_NOT_FOUND;
		}
		catch ( const std::range_error & )
		{
			return E_LOCKED;
		}


		auto open_iterator() -> component::IKVStore::pool_iterator_t
		{
			auto i = std::make_shared<pool_iterator>(this);
			_iterators.insert({i.get(), i});
			return i.get();
		}

		status_t deref_iterator(
			component::IKVStore::pool_iterator_t iter
			, const common::epoch_time_t t_begin
			, const common::epoch_time_t t_end
			, component::IKVStore::pool_reference_t & ref
			, bool& time_match
			, bool increment
		)
		{
			auto i = static_cast<pool_iterator *>(iter);
			if ( _iterators.count(i) != 1 )
			{
				return E_INVAL;
			}

			if ( i->is_end() )
			{
				return E_OUT_OF_BOUNDS;
			}

			if ( ! i->check_mark(_writes) )
			{
				return E_ITERATOR_DISTURBED;
			}

#if ENABLE_TIMESTAMPS
			using raw_t = decltype(impl::epoch_to_tsc(t_begin).raw());
			auto begin_tsc = t_begin.is_defined() ? std::numeric_limits<raw_t>::min() : impl::epoch_to_tsc(t_begin);
			auto end_tsc = t_end.is_defined() ? std::numeric_limits<raw_t>::max() : impl::epoch_to_tsc(t_end);
#else
			(void)t_begin;
			(void)t_end;
#endif

			auto &r = i->_iter;
			{
				const auto &k = r->first;
				ref.key = k.data();
				ref.key_len = k.size();
			}
			{
				const auto &m = r->second;
				const auto &d = std::get<0>(m);
				ref.value = d.data();
				ref.value_len = d.size();
#if ENABLE_TIMESTAMPS
				const auto t = std::get<1>(m);
				ref.timestamp = impl::tsc_to_epoch(t);
#if 0
{
std::ostringstream s;
s << "(hstore::session::dref_iterator) (t_begin " << t_begin.seconds() << " ref.timestamp " << ref.timestamp.seconds() << " t_end " << t_end.seconds() << ") (begin_tsc " << begin_tsc << " t " << t << " end_tsc " << end_tsc << ")";
PLOG("%s", s.str().c_str());
}
#endif
				time_match = ( begin_tsc <= t && t <= end_tsc );
#endif
			}

			if ( increment )
			{
				++r;
			}

			return S_OK;
		}

		status_t close_iterator(component::IKVStore::pool_iterator_t iter)
		{
			if ( iter == nullptr )
			{
				return E_INVAL;
			}
			auto i = static_cast<pool_iterator *>(iter);
			if ( _iterators.erase(i) != 1 )
			{
				return E_INVAL;
			}
			return S_OK;
		}
	};

#endif
