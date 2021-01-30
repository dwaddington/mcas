/*
   Copyright [2017-2021] [IBM Corporation]
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


#ifndef MCAS_HSTORE_PERSIST_FIXED_STRING_H
#define MCAS_HSTORE_PERSIST_FIXED_STRING_H

#include "alloc_key.h"
#include "hstore_config.h"
#include "cptr.h"
#include "fixed_string.h"
#include "persistent.h"
#include "perishable_expiry.h"
#include <common/pointer_cast.h>

#include <algorithm> /* fill_n, copy */
#include <array>
#include <cassert>
#include <cstddef> /* size_t */
#include <cstring> /* memcpy */
#include <memory> /* allocator_traits */

struct fixed_data_location_t {};
constexpr fixed_data_location_t fixed_data_location = fixed_data_location_t();

template <typename T, std::size_t SmallLimit, typename Allocator>
	union persist_fixed_string
	{
	public:
		using allocator_type = Allocator;
		static constexpr std::size_t default_alignment = 8;
		using cptr_t = ::cptr;
	private:
		using element_type = fixed_string<T>;
		using allocator_type_element =
			typename std::allocator_traits<Allocator>::
				template rebind_alloc<element_type>;

		using allocator_traits_type = std::allocator_traits<allocator_type_element>;
		using allocator_char_type =
			typename allocator_traits_type::
				template rebind_alloc<char>;

		using ptr_t = persistent_t<typename allocator_traits_type::pointer>;

		struct small_t
		{
			std::array<char, SmallLimit-1> value;
		private:
			/* _size == SmallLimit => data is stored out-of-line */
			unsigned char _size; /* discriminant */
		public:

			small_t(std::size_t size_)
				: value{}
				/* note: as of C++17, can use std::clamp */
				, _size((assert(size_ < SmallLimit || value[7] == 0)  , static_cast<unsigned char>(size_ < SmallLimit ? size_ : SmallLimit)) )
			{
			}

			small_t(fixed_data_location_t)
				: value{}
				, _size((assert(value[7] == 0), static_cast<unsigned char>(SmallLimit)))

			{
			}

			bool is_inline() const { return _size < SmallLimit; }

			unsigned int size() const { return _size; }

			void clear()
			{
				_size = 0;
			}

			void set_fixed()
			{
				assert(value[7] == 0);
				_size = SmallLimit;
			}

			template <typename IT>
				void assign(
					IT first_
					, IT last_
					, std::size_t fill_len_
				)
				{
					std::fill_n(
						std::copy(
							first_
							, last_
							, common::pointer_cast<T>(&value[0])
						)
						, fill_len_
						, T()
					);
					auto full_size = static_cast<unsigned char>(std::size_t(last_ - first_) * sizeof(*first_));
					assert(full_size < SmallLimit);
					_size = full_size;
				}
		};

		struct large_t
			: public allocator_char_type
			/* Ideallly, this would be private */
			, public cptr_t
		{
		public:
			allocator_char_type &al()
			{
				return static_cast<allocator_char_type &>(*this);
			}

			const allocator_char_type &al() const
			{
				return static_cast<const allocator_char_type &>(*this);
			}

			auto *ptr() const
			{
				return
					static_cast<typename persistent_traits<ptr_t>::value_type>(
						static_cast<void *>(persistent_load(this->P))
					);
			}

			template <typename IT, typename AL>
				void assign(
					AK_ACTUAL
					IT first_
					, IT last_
					, std::size_t fill_len_
					, std::size_t alignment_
					, AL al_
				)
				{
					auto data_size =
						std::size_t(last_ - first_) + fill_len_ * sizeof(T);
					using local_allocator_char_type =
						typename std::allocator_traits<AL>::template rebind_alloc<char>;
					this->P = nullptr;
					local_allocator_char_type(al_).allocate(
						AK_REF
						this->P
						, element_type::front_skip_element_count(alignment_)
							+ data_size
						, alignment_
					);
					new (ptr())
						element_type(first_, last_, fill_len_, alignment_);
					new (&al()) allocator_char_type(al_);
					ptr()->persist_this(al_);
				}

			void clear()
			{
				if (
					ptr()
					&&
					ptr()->ref_count() != 0
					&&
					ptr()->dec_ref(__LINE__, "clear") == 0
				)
				{
					auto sz = ptr()->alloc_element_count();
					ptr()->~element_type();
					this->deallocate(this->P, sz);
				}
			}

		};

		/* First of two members of the union: _small, for strings which are (a) moveable and (b) not more than SmallLimit bytes long */
		small_t _small;
		/* Second of two members of the union: _large, for strings which (a) may not be moved, or (b) are more than SmallLimit bytes long */
		large_t _large;
	public:
		template <typename U>
			using rebind = persist_fixed_string<U, SmallLimit, Allocator>;
		static_assert(
			sizeof _large <= sizeof _small.value
			, "large_t overlays _small.size"
		);

		/* ERROR: caller needs to to persist */
		persist_fixed_string()
			: _small(0)
		{
			_large.P = nullptr;
		}

		template <typename IT, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				const fixed_data_location_t &f_
				, IT first_
				, IT last_
				, std::size_t fill_len_
				, std::size_t alignment_
				, AL al_
			)
				: _small( f_ )
			{
				_large.assign(AK_REF first_, last_, fill_len_, alignment_, al_);
			}

		template <typename IT, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				IT first_
				, IT last_
				, AL al_
			)
				: persist_fixed_string(AK_REF first_, last_, 0U, default_alignment, al_)
			{
			}

		template <typename IT, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				const fixed_data_location_t &f_
				, IT first_
				, IT last_
				, AL al_
			)
				: persist_fixed_string(AK_REF f_, first_, last_, 0U, default_alignment, al_)
			{
			}

		template <typename IT, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				IT first_
				, IT last_
				, std::size_t fill_len_
				, std::size_t alignment_
				, AL al_
			)
				: _small(
					static_cast<std::size_t>(std::size_t(last_ - first_) + fill_len_) * sizeof(T)
				)
			{
				if ( is_inline() )
				{
					std::fill_n(
						std::copy(
							first_
							, last_
							, common::pointer_cast<T>(&_small.value[0])
						)
						, fill_len_
						, T()
					);
				}
				else
				{
					auto data_size =
						static_cast<std::size_t>(std::size_t(last_ - first_) + fill_len_) * sizeof(T);
					using local_allocator_char_type =
						typename std::allocator_traits<AL>::template rebind_alloc<char>;
					new (&_large.al()) allocator_char_type(al_);
					new (&_large.P) cptr_t{nullptr};
					local_allocator_char_type(al_).allocate(
						AK_REF
						_large.P
						, element_type::front_skip_element_count(alignment_)
							+ data_size
						, alignment_
					);
					new (_large.ptr())
						element_type(first_, last_, fill_len_, alignment_);
					_large.ptr()->persist_this(al_);
				}
			}

		template <typename AL>
			persist_fixed_string(
				AK_ACTUAL
				const fixed_data_location_t &f_
				, std::size_t data_len_
				, std::size_t alignment_
				, AL al_
			)
				: _small(f_)
			{
				auto data_size = data_len_ * sizeof(T);
				new (&_large.al()) allocator_char_type(al_);
				new (&_large.P) cptr_t{nullptr};
				al_.allocate(
					AK_REF
					_large.P
					, element_type::front_skip_element_count(alignment_)
					+ data_size
					, alignment_
				);
				new (_large.ptr()) element_type(data_size, alignment_);
			}

		/* Needed because the persist_fixed_string arguments are sometimes conveyed via
		 * forward_as_tuple, and the string is an element of a tuple, and std::tuple
		 * (unlike pair) does not support piecewise_construct.
		 */
		template <typename IT, typename AL>
			persist_fixed_string(
				std::tuple<AK_FORMAL IT&, IT&&, AL>&& p_
			)
				: persist_fixed_string(
					std::get<0>(p_)
					, std::get<1>(p_)
					, std::get<2>(p_)
#if AK_USED
					, std::get<3>(p_)
#endif
				)
			{}

		/* Needed because the persist_fixed_string arguments are sometimes conveyed via
		 * forward_as_tuple, and the string is an element of a tuple, and std::tuple
		 * (unlike pair) does not support piecewise_construct.
		 */
		template <typename AL>
			persist_fixed_string(
				std::tuple<AK_FORMAL const std::size_t &, AL>&& p_
			)
				: persist_fixed_string(
					std::get<0>(p_)
					, std::get<1>(p_)
#if AK_USED
					, std::get<2>(p_)
#endif
				)
			{}

		/* Needed because the persist_fixed_string arguments are sometimes conveyed via
		 * forward_as_tuple, and the string is an element of a tuple, and std::tuple
		 * (unlike pair) does not support picecewise_construct.
		 */
		template <typename AL>
			persist_fixed_string(
				std::tuple<AK_FORMAL const fixed_data_location_t &, const std::size_t &, AL>&& p_
			)
				: persist_fixed_string(
					std::get<0>(p_)
					, std::get<1>(p_)
					, std::get<2>(p_)
#if AK_USED
					, std::get<3>(p_)
#endif
				)
			{
			}

		template <typename AL>
			persist_fixed_string(
				AK_ACTUAL
				const fixed_data_location_t &f_
				, std::size_t data_len_
				, AL al_
			)
				: persist_fixed_string(AK_REF f_, data_len_, default_alignment, al_)
			{
			}

		template <typename IT, typename AL>
			persist_fixed_string &assign(
				AK_ACTUAL
				IT first_
				, IT last_
				, std::size_t fill_len_
				, std::size_t alignment_
				, AL al_
			)
			{
				this->clear();

				if ( (std::size_t(last_ - first_) + fill_len_) * (sizeof *first_) < SmallLimit )
				{
					_small.assign(first_, last_, fill_len_);
				}
				else
				{
					_large.assign(AK_REF first_, last_, fill_len_, alignment_, al_);
					_small.set_fixed();
				}
				return *this;
			}

		template <typename IT, typename AL>
			persist_fixed_string & assign(
				AK_ACTUAL
				IT first_
				, IT last_
				, AL al_
			)
			{
				return assign(AK_REF first_, last_, 0, default_alignment, al_);
			}

		void clear()
		{
			if ( is_inline() )
			{
				_small.clear();
			}
			else
			{
				_large.clear();
				_small.clear();
			}
		}

		persist_fixed_string(const persist_fixed_string &other)
			: _small(other._small)
		{
			if ( this != &other )
			{
				if ( ! is_inline() )
				{
					new (&_large.al()) allocator_char_type(other._large.al());
					new (&_large.P) cptr_t{other._large.P};
					if ( _large.ptr() )
					{
						_large.ptr()->inc_ref(__LINE__, "ctor &");
					}
				}
			}
		}

		persist_fixed_string(persist_fixed_string &&other) noexcept
			: _small(other._small)
		{
			if ( this != &other )
			{
				if ( ! is_inline() )
				{
					new (&_large.al()) allocator_type_element(other._large.al());
					new (&_large.P) ptr_t(other._large.ptr());
					other._large.P = nullptr;
				}
			}
		}

		/* Note: To handle "issue 41" updates, this operation must be restartable
		 * - must not alter "other" until this is persisted.
		 */
		persist_fixed_string &operator=(const persist_fixed_string &other)
		{
			if ( is_inline() )
			{
				if ( other.is_inline() )
				{
					/* _small <- _small */
					_small = other._small;
				}
				else
				{
					/* _small <- _large */
					_small = small_t(fixed_data_location);
					new (&_large.al()) allocator_type_element(other._large.al());
					new (&_large.P) ptr_t(other._large.ptr());
					_large.ptr()->inc_ref(__LINE__, "=&");
				}
			}
			else
			{
				/* _large <- ? */
				if (
					_large.ptr()
					&&
					_large.ptr()->ref_count() != 0
					&&
					_large.ptr()->dec_ref(__LINE__, "=&") == 0
				)
				{
					auto sz = _large.ptr()->alloc_element_count();
					_large.ptr()->~element_type();
					_large.al().deallocate(_large.P, sz);
				}
				_large.al().~allocator_char_type();

				if ( other.is_inline() )
				{
					/* _large <- _small */
					_small = other._small;
				}
				else
				{
					/* _large <- _large */
					_large.P = other._large.P;
					_large.ptr()->inc_ref(__LINE__, "=&");
					new (&_large.al()) allocator_type_element(other._large.al());
				}
			}
			return *this;
		}

		persist_fixed_string &operator=(persist_fixed_string &&other) noexcept
		{
			if ( is_inline() )
			{
				if ( other.is_inline() )
				{
					/* _small <- _small */
					_small = other._small;
				}
				else
				{
					/* _small <- _large */
					_small = small_t(fixed_data_location);
					new (&_large.al()) allocator_type_element(other._large.al());
					new (&_large.cptr) cptr_t(other._large.cptr);
					other._large.cptr = nullptr;
				}
			}
			else
			{
				/* _large <- ? */
				if (
					_large.ptr()
					&&
					_large.ptr()->ref_count() != 0
					&&
					_large.ptr()->dec_ref(__LINE__, "=&&") == 0
				)
				{
					auto sz = _large.ptr()->alloc_element_count();
					_large.ptr()->~element_type();
					_large.al().deallocate(_large.cptr, sz);
				}
				_large.al().~allocator_char_type();

				if ( other.is_inline() )
				{
					/* _large <- _small */
					_small = other._small;
				}
				else
				{
					/* _large <- _large */
					_large.cptr = other._large.cptr;
					new (&_large.al()) allocator_type_element(other._large.al());
					other._large.cptr = nullptr;
				}
			}
			return *this;
		}

		~persist_fixed_string() noexcept(! TEST_HSTORE_PERISHABLE)
		{
			if ( ! perishable_expiry::is_current() )
			{
				if ( is_inline() )
				{
				}
				else
				{
					if ( _large.ptr() && _large.ptr()->dec_ref(__LINE__, "~") == 0 )
					{
						auto sz = _large.ptr()->alloc_element_count();
						_large.ptr()->~element_type();
						_large.al().deallocate(_large.P, sz);
					}
					_large.al().~allocator_char_type();
				}
			}
		}

		void deconstitute() const
		{
#if USE_CC_HEAP == 3
			if ( ! is_inline() )
			{
				/* used only by the table_base destructor, at which time
				 * the reference count should be 1. There is not much point
				 * in decreasing the reference count except to mirror
				 * reconstitute.
				 */
				if ( _large.ptr()->dec_ref(__LINE__, "deconstitute") == 0 )
				{
					_large.al().~allocator_char_type();
				}
			}
#endif
		}

		template <typename AL>
			void reconstitute(AL al_)
			{
				if ( ! is_inline() )
				{
					/* restore the allocator
					 * ERROR: If the allocator should contain only a pointer to persisted memory,
					 * it would not need restoration. Arrange that.
					 */
					new (&const_cast<persist_fixed_string *>(this)->_large.al()) allocator_char_type(al_);
#if USE_CC_HEAP == 3
					using reallocator_char_type =
						typename std::allocator_traits<AL>::template rebind_alloc<char>;
					auto alr = reallocator_char_type(al_);
					if ( alr.is_reconstituted(_large.P) )
					{
						/* The data has already been reconstituted. Increase the reference
						 * count. */
						_large.ptr()->inc_ref(__LINE__, "reconstitute");
					}
					else
					{
						/* The data is not yet reconstituted. Reconstitute it.
						 * Although the original may have had a refcount
						 * greater than one, we have not yet seen the
						 * second reference, so the refcount must be set to one.
						 */
						alr.reconstitute(
							_large.ptr()->alloc_element_count() * sizeof(T), _large.P
						);
						new (_large.P)
							element_type( size(), _large.ptr()->alignment() );
					}
					reset_lock();
#else
					reset_lock_with_pending_retries();
#endif
				}
			}

		bool is_inline() const
		{
			return _small.is_inline();
		}

		std::size_t size() const
		{
			return is_inline() ? _small.size() : _large.ptr()->size();
		}

		/* There used to be one way to look at data: the current location.
		 * There are now two ways:
		 *   data (when you don't care whether a following operation will move the data) and
		 *   data_fixed (when the location must not change for the lifetime of the object,
		 *     even if the object (key or value) moves)
		 */
		const T *data_fixed() const
		{
			assert( ! is_inline() );
			return _large.ptr()->data();
		}

		T *data_fixed()
		{
			assert( !is_inline() );
			return _large.ptr()->data();
		}

		const T *data() const
		{
			return
				is_inline()
				? static_cast<const T *>(&_small.value[0])
				: data_fixed()
				;
		}

		T *data()
		{
			return
				is_inline()
				? static_cast<T *>(&_small.value[0])
				: data_fixed()
				;
		}

		/* inline items do not have a lock, but behave as if they do, to permit operations
		 * like put to work with the knowledge that the lock() calls cannot lock-like operations  */

		/* lockable and ! inline are the same thing, at the moment */
		bool is_fixed() const { return ! is_inline(); }
		bool lockable() const { return ! is_inline(); }
		bool try_lock_shared() const { return lockable() && _large.ptr()->try_lock_shared(); }
		bool try_lock_exclusive() const { return lockable() && _large.ptr()->try_lock_exclusive(); }
		bool is_locked() const { return lockable() && _large.ptr()->is_locked(); }
		template <typename AL>
			void flush_if_locked_exclusive(AL al_) const
			{
				if ( lockable() && _large.ptr()->is_locked_exclusive() )
				{
#if 0
					PLOG("FLUSH %p: %zu", _large.ptr()->data(), _large.ptr()->size());
#endif
					_large.ptr()->persist_this(al_);
				}
				else
				{
#if 0
					PLOG("FLUSH %p: no (shared)", _large.ptr()->data());
#endif
				}
			}
		void unlock() const
		{
			if ( lockable() )
			{
				_large.ptr()->unlock();
			}
		}
		void reset_lock() const { if ( lockable() ) { _large.ptr()->reset_lock(); } }
		/* The "crash consistent" version resets the lock before using allocation_states to
		 * ensure that the string is in a consistent state. Reset the lock carefully: the
		 * string lockable() may be inconsistent with _large.ptr().
		 */
		void reset_lock_with_pending_retries() const
		{
			if ( lockable() && _large.ptr() ) { _large.ptr()->reset_lock(); }
		}

		cptr_t &get_cptr()
		{
			return _large;
		}
		template <typename AL>
			void set_cptr(const cptr_t::c_element_type &ptr, AL al_)
			{
				auto &cp = get_cptr();
				cp = cptr{ptr};
				al_.persist(&cp, sizeof cp);
			}

		template <typename AL>
			void pin(
				AK_ACTUAL
				char *old_cptr, AL al_
			)
			{
				persist_fixed_string temp{};
				/* reconstruct the original small value, which is "this" but with data bits from the original cptr */
#pragma GCC diagnostic push
#if 9 <= __GNUC__
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
				std::memcpy(&temp, this, sizeof temp);
#pragma GCC diagnostic pop
				temp._large.P = persistent_t<char *>{old_cptr};
				hop_hash_log<false>::write(LOG_LOCATION, "size to copy ", old_cptr, " old cptr was ", old_cptr, " value ", std::string(temp.data(), temp.size()));
				auto begin = temp.data();
				auto end = begin + temp.size();
				_large.assign(AK_REF begin, end, 0, default_alignment, al_);
				_small.set_fixed();
				hop_hash_log<false>::write(LOG_LOCATION, "result size ", this->size(), " value ", std::string(this->data_fixed(), this->size()));
			}

		static persist_fixed_string *pfs_from_cptr_ref(cptr_t &cptr_)
		{
			return common::pointer_cast<persist_fixed_string>(static_cast<large_t *>(&cptr_));
		}
	};

template <typename T, std::size_t SmallSize, typename Allocator>
	bool operator==(
		const persist_fixed_string<T, SmallSize, Allocator> &a
		, const persist_fixed_string<T, SmallSize, Allocator> &b
	)
	{
		return
			a.size() == b.size()
			&&
			std::equal(a.data(), a.data() + a.size(), b.data())
		;
	}

#endif
