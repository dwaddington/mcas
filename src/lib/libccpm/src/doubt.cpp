/*
   Copyright [2019] [IBM Corporation]
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

#include "doubt.h"

#define DOUBT_FINE_TRACE 0
#if DOUBT_FINE_TRACE
#include "logging.h"
#endif
#include <libpmem.h>
#include <cstddef>

template <typename P>
	void persist(
		const P & p
	)
	{
		::pmem_persist(&p, sizeof p);
	}

#define PERSIST(x) do { persist(x); } while (0)
#define PERSIST_N(p, ct) do { ::pmem_persist(p, (sizeof *p) * ct); } while (0)

void ccpm::doubt::set(
	const char *
#if DOUBT_FINE_TRACE
		fn
#endif
	, void *p_
	, std::size_t bytes_
)
{
#if DOUBT_FINE_TRACE
	PLOG(PREFIX "set %s: doubt area %p.%zu", LOCATION, fn, p_, bytes_);
#endif
	_bytes = bytes_;
	_in_doubt = p_;
	PERSIST(*this);
}

void *ccpm::doubt::get() const
{
#if DOUBT_FINE_TRACE
	PLOG(PREFIX "get: doubt area %p.%zu", LOCATION, _in_doubt, _bytes);
#endif
	return _in_doubt;
}
