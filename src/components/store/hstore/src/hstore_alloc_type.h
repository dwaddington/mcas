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


#ifndef MCAS_HSTORE_ALLOC_TYPE_H_
#define MCAS_HSTORE_ALLOC_TYPE_H_

#include "hstore_config.h"

#if HEAP_OID
#include "allocator_co.h"
#elif HEAP_RECONSTITUTE
#include "allocator_rc.h"
#elif HEAP_CONSISTENT
#include "allocator_cc.h"
#endif

template <typename Persister>
	struct hstore_alloc_type
	{
#if HEAP_OID
		using alloc_type = allocator_co<char, Persister>;
		using heap_alloc_type = heap_co;
#elif HEAP_RECONSTITUTE
		using alloc_type = allocator_rc<char, Persister>;
		using heap_alloc_shared_type = heap_rc;
#elif HEAP_CONSISTENT
		using alloc_type = allocator_cc<char, Persister>;
#if HEAP_MM
		using heap_alloc_shared_type = typename alloc_type::heap_type;
#else
		using heap_alloc_shared_type = typename alloc_type::heap_type;
#endif
#endif
		using heap_alloc_access_type = heap_access<heap_alloc_shared_type>;
	};

#endif
