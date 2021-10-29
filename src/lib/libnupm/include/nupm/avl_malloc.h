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

#ifndef __NUPM_AVL_MALLOC_H__
#define __NUPM_AVL_MALLOC_H__

#include "mr_traits.h"

/* Note: creation of Fixed_stack with default size would call malloc which
 * would syscall mmap.  Similarly, deletion would call free which would
 * syscall munmap.
 */
#define STACK_SIZE_FIND_REGION 10000

#define SLAB_PLOG PLOG
#define SLAB_PERR PERR
#include "avl_malloc_base.h"

template <>
struct mr_traits<core::AVL_range_allocator>
{
  static auto allocate(core::AVL_range_allocator *pmr, unsigned, std::size_t bytes, std::size_t alignment)
  {
    return pmr->alloc(bytes, alignment)->addr();
  }
  static auto deallocate(core::AVL_range_allocator *pmr, unsigned, void *p, std::size_t, std::size_t)
  {
    return pmr->free(reinterpret_cast<addr_t>(p));
  }
};

#endif  //__NUPM_AVL_MALLOC_H__
