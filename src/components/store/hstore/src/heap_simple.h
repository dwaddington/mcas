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


#ifndef MCAS_HSTORE_ALLOCATOR_SIMPLE_H
#define MCAS_HSTORE_ALLOCATOR_SIMPLE_H

#include "bad_alloc_cc.h"
#include "persister_cc.h"

#include <algorithm> /* min */
#include <array>
#include <cstring> /* memset */
#include <cstddef> /* size_t, ptrdiff_t */
#include <sstream> /* ostringstream */
#include <string>

struct sbrk_alloc
{
private:
	struct bound
	{
		char *_end;
		void set(char *e) noexcept { _end = e; }
		char *end() const noexcept { return _end; }
	};
	struct state /* persists */
	{
		void *_location; /* persists. contains its own expected address */
		unsigned _sw; /* persists. Initially 0. Toggles between 0 and 1 */
		char *_limit; /* persists */
		std::array<bound, 2U> _bounds; /* persists, depends on _sw */
		bound &current() { return _bounds[_sw]; }
		bound &other() { return _bounds[1U-_sw]; }
		char *begin() { return static_cast<char *>(static_cast<void *>(this+1)); }
		void swap() { _sw = 1U - _sw; }
		void *limit() const { return _limit; }
	};
	bound &current() { return _state->current(); }
	bound &other() { return _state->other(); }
	void swap() { _state->swap(); }
	state *_state;
	template <typename T>
		void persist(const T &) {}
	void restore() const
	{
		if ( _state->_location != &_state->_location )
		{
			std::ostringstream s;
			s << "cc_heap region mapped at " << &_state->_location << " but required to be at " << _state->_location;
			throw std::runtime_error{s.str()};
		}
		assert(_state->_sw < _state->_bounds.size());
	}
public:
	explicit sbrk_alloc(void *area, std::size_t sz)
		: _state(static_cast<state *>(area))
	{
		/* one-time initialization; assumes that initial bytes in area are zeros/nullptr */
		_state->_limit = static_cast<char *>(area) + sz;
		_state->_bounds[0].set(_state->begin());
		_state->_sw = 0;
		_state->_location = &_state->_location;
		persist(_state);
	}
	explicit sbrk_alloc(void *area)
		: _state(static_cast<state *>(area))
	{
		restore();
	}
	void *malloc(std::size_t sz)
	{
		/* round to double word */
		sz = (sz + 63UL) & ~63UL;
		if ( static_cast<std::size_t>(_state->_limit - current().end()) < sz )
		{
			return nullptr;
		}
		auto p = current().end();
		auto q = p + sz;
		other().set(q);
		persist(other());
		swap();
		persist(_state->_sw);
		return p;
	}
	void free(const void *, std::size_t) {}
	void *area() const { return _state; }
};

struct heap_simple
	: public sbrk_alloc
{
	explicit heap_simple(void *area, std::size_t sz)
		: sbrk_alloc(area, sz)
	{}
	explicit heap_simple(void *area)
		: sbrk_alloc(area)
	{}
};

#endif
