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


#ifndef MCAS_HSTORE_ALLOC_TYPE_H_
#define MCAS_HSTORE_ALLOC_TYPE_H_

#include "hstore_config.h"

#include "allocator_co.h"
#include "allocator_rc.h"
#include "allocator_cc.h"

template <typename Persister>
	struct hstore_alloc_type
	{
#if USE_CC_HEAP == 2
		using alloc_t = allocator_co<char, Persister>;
		using heap_alloc_t = heap_co;
#elif USE_CC_HEAP == 3
		using alloc_t = allocator_rc<char, Persister>;
		using heap_alloc_shared_t = heap_rc_shared;
		using heap_alloc_t = heap_rc;
#elif USE_CC_HEAP == 4
		using alloc_t = allocator_cc<char, Persister>;
		using heap_alloc_shared_t = heap_cc_shared;
		using heap_alloc_t = heap_cc;
#endif
	};

#endif
