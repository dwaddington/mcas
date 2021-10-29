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

#include <common/exceptions.h>
#include <common/utils.h>
#include <numa.h>
#include <stdexcept>

#define ENABLE_LOGGING 1
#include "safe_print.h"
#include <nupm/rc_alloc_lb_base.h>
#define SLAB_PLOG SAFE_PRINT
#define SLAB_PERR SAFE_PRINT
#include <nupm/avl_malloc_base.h>
#include <nupm/slab.h>
#define REGION_LOG SAFE_PRINT
#define REGION_CLOG(x, ...) SAFE_PRINT(__VA_ARGS__)
#include <nupm/region.h>

static const unsigned trace = common::env_value<unsigned>("CCA_FINE_TRACE", 0);

using Rca_LB = nupm::Rca_LB;

Rca_LB::Rca_LB(unsigned debug_level_) : _rmap(new Region_map(debug_level_)) {}

Rca_LB::~Rca_LB() {}

void Rca_LB::add_managed_region(void * region_base,
                                size_t region_length,
                                int    numa_node)
{
  if (!region_base || region_length == 0 || numa_node < 0)
    throw std::invalid_argument("add_managed_region");

  _rmap->add_arena(region_base, region_length, numa_node);
}

void Rca_LB::inject_allocation(void *ptr, size_t size, int numa_node)
{
  _rmap->inject_allocation(ptr, size, numa_node);
}

void *Rca_LB::alloc(size_t size, int numa_node, size_t alignment)
{
  if (size == 0 || numa_node < 0 || numa_node > 4)
    throw std::invalid_argument("Rca_LB::alloc invalid size or numa node");

  void * result = _rmap->allocate(size, numa_node, alignment);
  if(result == nullptr) {
    SAFE_PRINT("Region allocator unable to allocate (size=%lu, alignment=%lu)", size, alignment);
    if ( 1 <= trace )
    {
      debug_dump();
    }
    throw std::bad_alloc();
  }
  //  PNOTICE("Rca_LB::alloc (%p,%lu)", result, size);
  return result;
}

void Rca_LB::free(void *ptr, int numa_node, size_t size)
{
  if (numa_node < 0) throw std::invalid_argument("invalid numa_node");
  if (!ptr) throw std::invalid_argument("ptr argument is null");
  _rmap->free(ptr, numa_node, size);
}

void Rca_LB::debug_dump(std::ostream &out_log)
{
  _rmap->debug_dump(out_log);
}

void Rca_LB::debug_dump(std::string *out_log)
{
  if ( out_log )
  {
    std::ostringstream ss;
    debug_dump(ss);
    out_log->append(ss.str());
  }
  else
  {
    debug_dump(std::cerr);
  }
}
