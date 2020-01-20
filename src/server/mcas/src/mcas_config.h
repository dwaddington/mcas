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
#ifndef __MCAS_CONFIG_H__
#define __MCAS_CONFIG_H__

/* NUM_SHARD_BUFFERS: number of 2MiB buffers per connection */
static constexpr size_t NUM_SHARD_BUFFERS = 8;

/* WORK_REQUEST_ALLOCATOR_COUNT: number of work request slots for ADO communications */
static constexpr size_t WORK_REQUEST_ALLOCATOR_COUNT = 16;

/* Maximum number of comparison to make on a index scan.  We limit the max so that
   the shard thread does not get "jammed up" scanning the index. */
static constexpr unsigned MAX_INDEX_COMPARISONS = 10000;

#if defined(__powerpc64__)
#define LIKELY(X) (X) /* TODO: fix for Power */
#define UNLIKELY(X) (X)
#endif

namespace mcas
{
namespace Global
{
extern unsigned debug_level;
}
}  // namespace mcas

#endif
