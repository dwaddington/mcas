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

#ifndef __COMMON_MEMORY_MAPPED_H__
#define __COMMON_MEMORY_MAPPED_H__

#include <common/moveable_struct.h>
#include <cstddef>
#include <sys/mman.h> /* MAP_FAILED */
#include <sys/uio.h> /* iovec */

namespace common
{
  struct iovec_moveable_traits
  {
    static constexpr ::iovec none{MAP_FAILED, 0};
  };
  /*
   * Memory which is mapped, and which is to be unmapped
   */
  struct memory_mapped : public moveable_struct<::iovec, iovec_moveable_traits>
  {
  public:
    /* minimalist: arguments are result of ::mmap, and size */
    memory_mapped(void *vaddr, std::size_t size) noexcept;
    /* non-minimalist: arguments are input to ::mmap */
    memory_mapped(void *vaddr, std::size_t size, int prot, int flags, int fd) noexcept;
	memory_mapped(memory_mapped &&) = default;
	memory_mapped &operator=(memory_mapped &&) = default;
    ~memory_mapped();
    using ::iovec::iov_base;
    using ::iovec::iov_len;
    ::iovec iov() const { return *this; }
  };
}

#endif
