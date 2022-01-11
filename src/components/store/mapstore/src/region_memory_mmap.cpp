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

#include "region_memory_mmap.h"

#include <common/logging.h>
#include <sys/mman.h> /* munmap */

region_memory_mmap::region_memory_mmap(unsigned debug_level_, void *p, std::size_t size)
  : region_memory(debug_level_, p, size)
{}

region_memory_mmap::~region_memory_mmap()
{
  CFLOGM(1, "freeing region memory ({},{})", iov_base, iov_len);
  if (::munmap(iov_base, iov_len))
  {
    FLOGM("munmap of region memory {}.{} failed", iov_base, iov_len);
  }
}
