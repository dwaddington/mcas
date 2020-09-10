/*
   Copyright [2019, 2020] [IBM Corporation]
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

#include "logging.h"
#include <libpmem.h>
#include <cstddef>
#include <cstdlib> // getenv

namespace
{
	auto doubt_fine_trace = std::getenv("CCA_DOUBT_FINE_TRACE");
}

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
	const char *fn
	, void *p_
	, std::size_t bytes_
)
{
	if ( doubt_fine_trace )
	{
		PLOG(PREFIX "set %s: doubt area %p.%zu", LOCATION, fn, p_, bytes_);
	}
	_bytes = bytes_;
	_in_doubt = p_;
	PERSIST(*this);
}

void *ccpm::doubt::get() const
{
	if ( doubt_fine_trace )
	{
		PLOG(PREFIX "get: doubt area %p.%zu", LOCATION, _in_doubt, _bytes);
	}

	return _in_doubt;
}
