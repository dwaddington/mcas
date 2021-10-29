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


/*
 * Authors:
 *
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */

#ifndef __NUPM_RC_ALLOC_LB__
#define __NUPM_RC_ALLOC_LB__

#include "mr_traits.h"
#include "rc_alloc_lb_base.h"

template <>
	struct mr_traits<nupm::Rca_LB>
	{
		static auto allocate(nupm::Rca_LB *pmr, unsigned numa_node, std::size_t bytes, std::size_t alignment)
		{
			return pmr->alloc(bytes, int(numa_node), alignment);
		}
		static auto deallocate(nupm::Rca_LB *pmr, unsigned numa_node, void *p, std::size_t bytes, std::size_t)
		{
			return pmr->free(p, int(numa_node), bytes);
		}
	};
#endif
