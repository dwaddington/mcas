/*
   Copyright [2019-2020] [IBM Corporation]
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
#ifndef EXP_DIRECT_MEMORY_REGISTERED_H
#define EXP_DIRECT_MEMORY_REGISTERED_H

#include <common/logging.h>

#include <utility> /* move, swap */

template <typename T> /* register_direct_memory/unregister_direct_memory is in both kvstore and mcas */
	class direct_memory_registered
	{
		unsigned _debug_level;
		T *_t;
		typename T::memory_handle_t _r;
	public:
		explicit direct_memory_registered(
			unsigned debug_level_
			, T *transport_
			, void* base_
			, const size_t len_
		)
			: _debug_level(debug_level_)
			, _t(transport_)
			, _r(_t->register_direct_memory(base_, len_))
		{
			if ( 2 < _debug_level )
			{
				PLOG("%s %p (%p:0x%zx)", __func__, static_cast<const void *>(_r), base_, len_);
			}
		}

		direct_memory_registered(const direct_memory_registered &) = delete;
		explicit direct_memory_registered(unsigned debug_level_)
			: _debug_level(debug_level_)
			, _t{nullptr}
			, _r{T::HANDLE_NONE}
		{}
		direct_memory_registered(direct_memory_registered &&other_)
			: _debug_level(other_._debug_level)
			, _t(nullptr)
			, _r(std::move(other_._r))
		{
			std::swap(_t, other_._t);
		}
		direct_memory_registered &operator=(const direct_memory_registered &) = delete;
		direct_memory_registered &operator=(direct_memory_registered &&other_)
		{
			swap(*this, other_);
			return *this;
		}
		friend void swap(direct_memory_registered &a, direct_memory_registered &b)
		{
			using std::swap;
			swap(a._t, b._t);
			swap(a._r, b._r);
		}
		~direct_memory_registered()
		{
			if ( _t )
			{
				if ( 2 < _debug_level )
				{
					PLOG("%s %p", __func__, static_cast<const void *>(_r));
				}
				_t->unregister_direct_memory(_r);
			}
		}
		auto mr() const { return _r; }
	};

#endif
