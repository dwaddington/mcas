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

#include "alloc_key.h" /* AK_FORMAL, AK_ACTUAL */
#include "hstore_config.h"
#include "cptr.h"
#include "fixed_string.h"
#include "lock_state.h"
#include "persistent.h"
#include "perishable_expiry.h"
#include <common/pointer_cast.h>
#include <common/perf/tm.h>

#include <array>
#include <cassert>
#include <cstddef> /* size_t */
#include <cstring> /* memcpy */
#include <memory> /* allocator_traits */
#include <tuple> /* make_from_tuple */

#include <common/byte.h>
struct fixed_data_location_t {};
constexpr fixed_data_location_t fixed_data_location = fixed_data_location_t();

template <typename T, std::size_t SmallLimit>
	struct inline_t
	{
		using value_type = T;
		using pointer = value_type *;
		using const_pointer = const value_type *;
		std::array<value_type, SmallLimit-1> value;
	private:
		/* _size == SmallLimit => data is stored out-of-line */
		unsigned char _size; /* discriminant */
	public:
		inline_t(std::size_t size_)
			: value{}
			/* note: as of C++17, can use std::clamp */
			, _size((assert(size_ < SmallLimit || int(value[7]) == 0)  , static_cast<unsigned char>(size_ < SmallLimit ? size_ : SmallLimit)) )
		{
		}

		inline_t(fixed_data_location_t)
			: value{}
			, _size((assert(int(value[7]) == 0), static_cast<unsigned char>(SmallLimit)))

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
			assert(int(value[7]) == 0);
			_size = SmallLimit;
		}

		template <typename C>
			void assign(
				common::basic_string_view<C> source_
				, std::size_t fill_len_
			)
			{
				persisted_zero_n(
					persisted_copy(
						source_
						, common::pointer_cast<value_type>(&value[0])
					)
					, fill_len_
				);

				auto full_size = static_cast<unsigned char>(source_.length() * sizeof(C));
				assert(full_size < SmallLimit);
				_size = full_size;
			}

		const_pointer const_data() const { return static_cast<const_pointer>(&value[0]); }
		pointer data() { return static_cast<pointer>(&value[0]); }
	};

template <typename T, std::size_t SmallSize, typename Allocator>
	bool operator==(
		const inline_t<T, SmallSize> &a
		, const inline_t<T, SmallSize> &b
	)
	{
		assert(a.is_inline() || b.is_inline());
		return a.size() == b.size() && std::equal(a.const_data(), a.const_data() + a.size(), b.const_data());
	}

template <typename T, typename AllocatorChar, typename PtrT, typename Cptr, typename ElementType>
	struct outline_t
		: public AllocatorChar
		/* Ideally, this would be private */
		, public Cptr
	{
		using value_type = T;
		using ptr_t = PtrT;
		using cptr_type = Cptr;
		using element_type = ElementType;
		using allocator_char_type = AllocatorChar;
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

		template <typename C, typename AL>
			void assign(
				AK_ACTUAL
				common::basic_string_view<C> source_
				, std::size_t fill_len_
				, std::size_t alignment_
				, lock_state lock_
				, AL al_
			)
			{
				auto data_size =
					(source_.size() + fill_len_) * sizeof(value_type);
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
					element_type(source_, fill_len_, alignment_, lock_);
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

		std::size_t size() const { return ptr()->size(); }
	};

template <typename T, std::size_t SmallLimit, typename Allocator>
	union persist_fixed_string
	{
		using allocator_type = Allocator;
#if MCAS_HSTORE_USE_PMEM_PERSIST
		/* cache-line aligned, for better pmem_mem* performance.
		 * Aligning only longer elements might be an even better
		 * strategy.
		 */
		static constexpr std::size_t default_alignment = 64;
#else
		static constexpr std::size_t default_alignment = 8;
#endif
		using cptr_type = ::cptr;
		using value_type = T;
		using pointer = value_type *;
		using const_pointer = const value_type *;
	private:
		using element_type = fixed_string<value_type>;
		using allocator_type_element =
			typename std::allocator_traits<Allocator>::
				template rebind_alloc<element_type>;

		using allocator_traits_type = std::allocator_traits<allocator_type_element>;
		using allocator_char_type =
			typename allocator_traits_type::
				template rebind_alloc<char>;

		using ptr_t = persistent_t<typename allocator_traits_type::pointer>;

		/* First of two members of the union: _inline, for strings which are (a) moveable and (b) not more than SmallLimit bytes long */
		using inline_type = inline_t<value_type, SmallLimit>;
		inline_type _inline;
		/* Second of two members of the union: _outline, for strings which (a) may not be moved, or (b) are more than SmallLimit bytes long */
		using outline_type = outline_t<value_type, allocator_char_type, ptr_t, cptr_type, element_type>;
		outline_type _outline;
	public:
		template <typename U>
			using rebind = persist_fixed_string<U, SmallLimit, Allocator>;
		static_assert(
			sizeof _outline <= sizeof _inline.value
			, "outline_type overlays _inline.size"
		);

		/* ERROR: caller needs to persist */
		persist_fixed_string()
			: _inline(0)
		{
			_outline.P = nullptr;
		}

		template <typename C, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				const fixed_data_location_t &f_
				, common::basic_string_view<C> source_
				, std::size_t fill_len_
				, std::size_t alignment_
				, lock_state lock_
				, AL al_
			)
				: _inline( f_ )
			{
				_outline.assign(AK_REF source_, fill_len_, alignment_, lock_, al_);
			}

		template <typename C, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				common::basic_string_view<C> source_
				, lock_state lock_
				, AL al_
			)
				: persist_fixed_string(AK_REF source_, 0U, default_alignment, lock_, al_)
			{
			}

		template <typename C, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				const fixed_data_location_t &f_
				, common::basic_string_view<C> source_
				, lock_state lock_
				, AL al_
			)
				: persist_fixed_string(AK_REF f_, source_, 0U, default_alignment, lock_, al_)
			{
			}

		template <typename IT, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				IT first_, IT last_
				, lock_state lock_
				, AL al_
			)
				: persist_fixed_string(AK_REF common::basic_string_view<common::byte>(first_, std::size_t(last_-first_)), 0U, default_alignment, lock_, al_)
			{}

		template <typename C, typename AL>
			persist_fixed_string(
				AK_ACTUAL
				common::basic_string_view<C> source_
				, std::size_t fill_len_
				, std::size_t alignment_
				, lock_state lock_
				, AL al_
			)
				: _inline(
					static_cast<std::size_t>(source_.length() + fill_len_) * sizeof(value_type)
				)
			{
				if ( is_inline() )
				{
					persisted_zero_n(
						persisted_copy(
							source_
							, common::pointer_cast<value_type>(&_inline.value[0])
						)
						, fill_len_
					);
				}
				else
				{
					auto data_size =
						static_cast<std::size_t>(source_.length() + fill_len_) * sizeof(value_type);
					using local_allocator_char_type =
						typename std::allocator_traits<AL>::template rebind_alloc<char>;
					new (&_outline.al()) allocator_char_type(al_);
					new (&_outline.P) cptr_type{nullptr};
					local_allocator_char_type(al_).allocate(
						AK_REF
						_outline.P
						, element_type::front_skip_element_count(alignment_)
							+ data_size
						, alignment_
					);
					new (_outline.ptr())
						element_type(source_, fill_len_, alignment_, lock_);
					_outline.ptr()->persist_this(al_);
				}
			}

		template <typename AL>
			persist_fixed_string(
				AK_ACTUAL
				const fixed_data_location_t &f_
				, std::size_t data_len_
				, std::size_t alignment_
				, lock_state lock_
				, AL al_
			)
				: _inline(f_)
			{
				auto data_size = data_len_ * sizeof(value_type);
				new (&_outline.al()) allocator_char_type(al_);
				new (&_outline.P) cptr_type{nullptr};
				al_.allocate(
					AK_REF
					_outline.P
					, element_type::front_skip_element_count(alignment_)
					+ data_size
					, alignment_
				);
				new (_outline.ptr()) element_type(data_size, alignment_, lock_);
			}

		/* Needed because the persist_fixed_string arguments are sometimes conveyed via
		 * forward_as_tuple, and the string is an element of a tuple, and std::tuple
		 * (unlike pair) does not support piecewise_construct.
		 */
#if 0 && 201703L <= __cplusplus /* this may work, some day */
		template <typename Tuple>
			persist_fixed_string(
				Tuple &&t
			);
			: persist_fixed_string(std::make_from_tuple<persist_fixed_string<T, SmallLimit, Allocator>>(std::move(t)))
		{}
#else
		template <typename IT, typename AL>
			persist_fixed_string(
				std::tuple<AK_FORMAL IT&, IT&&, lock_state &&, AL>&& p_
			)
				: persist_fixed_string(
					std::get<0>(p_)
					, std::get<1>(p_)
					, std::get<2>(p_)
					, std::get<3>(p_)
#if AK_USED
					, std::get<4>(p_)
#endif
				)
			{}

		/* Needed because the persist_fixed_string arguments are sometimes conveyed via
		 * forward_as_tuple, and the string is an element of a tuple, and std::tuple
		 * (unlike pair) does not support piecewise_construct.
		 */
		template <typename View, typename AL>
			persist_fixed_string(
				std::tuple<AK_FORMAL View&&, lock_state &&, AL>&& p_
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
				std::tuple<AK_FORMAL const std::size_t &, lock_state &&, AL>&& p_
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
				std::tuple<AK_FORMAL const fixed_data_location_t &, const std::size_t &, lock_state &&, AL>&& p_
			)
				: persist_fixed_string(
					std::get<0>(p_)
					, std::get<1>(p_)
					, std::get<2>(p_)
					, std::get<3>(p_)
#if AK_USED
					, std::get<4>(p_)
#endif
				)
			{
			}

		template <typename AL>
			persist_fixed_string(
				std::tuple<AK_FORMAL const fixed_data_location_t &, const std::size_t &, const std::size_t &, lock_state &&, AL>&& p_
			)
				: persist_fixed_string(
					std::get<0>(p_)
					, std::get<1>(p_)
					, std::get<2>(p_)
					, std::get<3>(p_)
					, std::get<4>(p_)
#if AK_USED
					, std::get<5>(p_)
#endif
				)
			{
			}
#endif
		template <typename AL>
			persist_fixed_string(
				AK_ACTUAL
				const fixed_data_location_t &f_
				, std::size_t data_len_
				, lock_state lock_
				, AL al_
			)
				: persist_fixed_string(AK_REF f_, data_len_, default_alignment, lock_, al_)
			{
			}

		template <typename C, typename AL>
			persist_fixed_string &assign(
				AK_ACTUAL
				common::basic_string_view<C> source_
				, std::size_t fill_len_
				, std::size_t alignment_
				, lock_state lock_
				, AL al_
			)
			{
				this->clear();

				if ( (source_.length() + fill_len_) * sizeof(C) < SmallLimit )
				{
					_inline.assign(source_, fill_len_);
				}
				else
				{
					_outline.assign(AK_REF source_, fill_len_, alignment_, lock_, al_);
					_inline.set_fixed();
				}
				return *this;
			}

		template <typename C, typename AL>
			persist_fixed_string & assign(
				AK_ACTUAL
				common::basic_string_view<C> source_
				, lock_state lock_
				, AL al_
			)
			{
				return assign(AK_REF source_, 0, default_alignment, lock_, al_);
			}

		void clear()
		{
			if ( is_inline() )
			{
				_inline.clear();
			}
			else
			{
				_outline.clear();
				_inline.clear();
			}
		}

		persist_fixed_string(const persist_fixed_string &other)
			: _inline(other._inline)
		{
			if ( this != &other )
			{
				if ( ! is_inline() )
				{
					new (&_outline.al()) allocator_char_type(other._outline.al());
					new (&_outline.P) cptr_type{other._outline.P};
					if ( _outline.ptr() )
					{
						_outline.ptr()->inc_ref(__LINE__, "ctor &");
					}
				}
			}
		}

		persist_fixed_string(persist_fixed_string &&other) noexcept
			: _inline(other._inline)
		{
			if ( this != &other )
			{
				if ( ! is_inline() )
				{
					new (&_outline.al()) allocator_type_element(other._outline.al());
					new (&_outline.P) ptr_t(other._outline.ptr());
					other._outline.P = nullptr;
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
					/* _inline <- _inline */
					_inline = other._inline;
				}
				else
				{
					/* _inline <- _outline */
					_inline = inline_type(fixed_data_location);
					new (&_outline.al()) allocator_type_element(other._outline.al());
					new (&_outline.P) ptr_t(other._outline.ptr());
					_outline.ptr()->inc_ref(__LINE__, "=&");
				}
			}
			else
			{
				/* _outline <- ? */
				if (
					_outline.ptr()
					&&
					_outline.ptr()->ref_count() != 0
					&&
					_outline.ptr()->dec_ref(__LINE__, "=&") == 0
				)
				{
					auto sz = _outline.ptr()->alloc_element_count();
					_outline.ptr()->~element_type();
					_outline.al().deallocate(_outline.P, sz);
				}
				_outline.al().~allocator_char_type();

				if ( other.is_inline() )
				{
					/* _outline <- _inline */
					_inline = other._inline;
				}
				else
				{
					/* _outline <- _outline */
					_outline.P = other._outline.P;
					_outline.ptr()->inc_ref(__LINE__, "=&");
					new (&_outline.al()) allocator_type_element(other._outline.al());
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
					/* _inline <- _inline */
					_inline = other._inline;
				}
				else
				{
					/* _inline <- _outline */
					_inline = inline_type(fixed_data_location);
					new (&_outline.al()) allocator_type_element(other._outline.al());
					new (&_outline.cptr) cptr_type(other._outline.cptr);
					other._outline.cptr = nullptr;
				}
			}
			else
			{
				/* _outline <- ? */
				if (
					_outline.ptr()
					&&
					_outline.ptr()->ref_count() != 0
					&&
					_outline.ptr()->dec_ref(__LINE__, "=&&") == 0
				)
				{
					auto sz = _outline.ptr()->alloc_element_count();
					_outline.ptr()->~element_type();
					_outline.al().deallocate(_outline.cptr, sz);
				}
				_outline.al().~allocator_char_type();

				if ( other.is_inline() )
				{
					/* _outline <- _inline */
					_inline = other._inline;
				}
				else
				{
					/* _outline <- _outline */
					_outline.cptr = other._outline.cptr;
					new (&_outline.al()) allocator_type_element(other._outline.al());
					other._outline.cptr = nullptr;
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
					if ( _outline.ptr() && _outline.ptr()->dec_ref(__LINE__, "~") == 0 )
					{
						auto sz = _outline.ptr()->alloc_element_count();
						_outline.ptr()->~element_type();
						_outline.al().deallocate(_outline.P, sz);
					}
					_outline.al().~allocator_char_type();
				}
			}
		}

		void deconstitute() const
		{
			if ( ! is_inline() && _outline.al().pool()->can_reconstitute() )
			{
				/* used only by the table_base destructor, at which time
				 * the reference count should be 1. There is not much point
				 * in decreasing the reference count except to mirror
				 * reconstitute.
				 */
				if ( _outline.ptr()->dec_ref(__LINE__, "deconstitute") == 0 )
				{
					_outline.al().~allocator_char_type();
				}
			}
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
					new (&const_cast<persist_fixed_string *>(this)->_outline.al()) allocator_char_type(al_);
					if ( al_.pool()->can_reconstitute() )
					{
						using reallocator_char_type =
							typename std::allocator_traits<AL>::template rebind_alloc<char>;
						auto alr = reallocator_char_type(al_);
						if ( alr.is_reconstituted(_outline.P) )
						{
							/* The data has already been reconstituted. Increase the reference
							 * count. */
							_outline.ptr()->inc_ref(__LINE__, "reconstitute");
						}
						else
						{
							/* The data is not yet reconstituted. Reconstitute it.
							 * Although the original may have had a refcount
							 * greater than one, we have not yet seen the
							 * second reference, so the refcount must be set to one.
							 */
							alr.reconstitute(
								_outline.ptr()->alloc_element_count() * sizeof(T), _outline.P
							);
							new (_outline.P)
								element_type( size(), _outline.ptr()->alignment(), lock_state::free );
						}
						reset_lock();
					}
					else
					{
						reset_lock_with_pending_retries();
					}
				}
			}

		bool is_inline() const
		{
			return _inline.is_inline();
		}

		std::size_t size() const
		{
			return is_inline() ? _inline.size() : _outline.size();
		}

		/* performance help for size matching: precondition: both elements are inline */
		bool inline_match(const persist_fixed_string<value_type, SmallLimit, Allocator> &other_) const
		{
			assert(is_inline() && other_.is_inline());
			return _inline == other_._inline;
		}

		bool general_match(const persist_fixed_string<value_type, SmallLimit, Allocator> &other) const
		{
			return size() == other.size() && std::equal(data(), data() + size(), other.data());
		}

		auto outline_size() const
		{
			assert(!is_inline());
			return _outline.size();
		}

		/* There used to be one way to look at data: the current location.
		 * There are now two ways:
		 *   data (when you don't care whether a following operation will move the data) and
		 *   data_fixed (when the location must not change for the lifetime of the object,
		 *     even if the object (key or value) moves)
		 */
		const_pointer data_fixed() const
		{
			assert( ! is_inline() );
			return _outline.ptr()->data();
		}

		pointer data_fixed()
		{
			assert( !is_inline() );
			return _outline.ptr()->data();
		}

		const_pointer data() const
		{
			return
				is_inline()
				? static_cast<const_pointer>(&_inline.value[0])
				: data_fixed()
				;
		}

		pointer data()
		{
			return
				is_inline()
				? static_cast<pointer>(&_inline.value[0])
				: data_fixed()
				;
		}

		/* inline items do not have a lock, but behave as if they do, to permit operations
		 * like put to work with the knowledge that the lock() calls cannot lock-like operations  */

		/* lockable and ! inline are the same thing, at the moment */
		bool is_fixed() const { return ! is_inline(); }
		bool lockable() const { return ! is_inline(); }
		bool try_lock_shared() const { return lockable() && _outline.ptr()->try_lock_shared(); }
		bool try_lock_exclusive() const { return lockable() && _outline.ptr()->try_lock_exclusive(); }
		bool is_locked_exclusive() const { return lockable() && _outline.ptr()->is_locked_exclusive(); }
		bool is_locked() const { return lockable() && _outline.ptr()->is_locked(); }
		template <typename AL>
			void flush_if_locked_exclusive(TM_ACTUAL AL al_) const
			{
				if ( lockable() && _outline.ptr()->is_locked_exclusive() )
				{
#if 0
					PLOG("FLUSH %p: %zu", _outline.ptr()->data(), _outline.size());
#endif
					TM_SCOPE()
					_outline.ptr()->persist_this(al_);
				}
				else
				{
#if 0
					PLOG("FLUSH %p: no (shared)", _outline.ptr()->data());
#endif
				}
			}

		void unlock_indefinite() const
		{
			if ( lockable() )
			{
				_outline.ptr()->unlock_indefinite();
			}
		}

		void unlock_exclusive() const
		{
			if ( lockable() )
			{
				_outline.ptr()->unlock_exclusive();
			}
		}

		void unlock_shared() const
		{
			if ( lockable() )
			{
				_outline.ptr()->unlock_shared();
			}
		}

		void reset_lock() const { if ( lockable() ) { _outline.ptr()->reset_lock(); } }
		/* The "crash consistent" version resets the lock before using allocation_states to
		 * ensure that the string is in a consistent state. Reset the lock carefully: the
		 * string lockable() may be inconsistent with _outline.ptr().
		 */
		void reset_lock_with_pending_retries() const
		{
			if ( lockable() && _outline.ptr() ) { _outline.ptr()->reset_lock(); }
		}

		cptr_type &get_cptr()
		{
			return _outline;
		}
		template <typename AL>
			void set_cptr(const cptr_type::c_element_type &ptr, AL al_)
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
				/* reconstruct the original inline value, which is "this" but with data bits from the original cptr */
#pragma GCC diagnostic push
#if 9 <= __GNUC__
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif
				std::memcpy(&temp, this, sizeof temp);
#pragma GCC diagnostic pop
				temp._outline.P = persistent_t<char *>{old_cptr};
				hop_hash_log<false>::write(LOG_LOCATION, "size to copy ", old_cptr, " old cptr was ", old_cptr, " value ", std::string(common::pointer_cast<char>(temp.data()), temp.size()));
				_outline.assign(AK_REF common::basic_string_view<value_type>(temp.data(), temp.size()), 0, default_alignment, lock_state::free, al_);
				_inline.set_fixed();
				hop_hash_log<false>::write(LOG_LOCATION, "result size ", this->size(), " value ", std::string(common::pointer_cast<char>(this->data_fixed()), this->size()));
			}

		static persist_fixed_string *pfs_from_cptr_ref(cptr_type &cptr_)
		{
			return common::pointer_cast<persist_fixed_string>(static_cast<outline_type *>(&cptr_));
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
