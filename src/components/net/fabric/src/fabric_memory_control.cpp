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


/*
 * Authors:
 *
 */

#include "fabric_endpoint.h"

#include "fabric.h"
#include "fabric_check.h" /* CHECK_FI_ERR */
#include "fabric_ptr.h" /* FABRIC_TACE_FID */
#include "fabric_runtime_error.h"
#include "fabric_util.h" /* make_fi_infodup */

#include <common/byte_span.h>
#include <common/logging.h> /* PLOG */
#include <common/pointer_cast.h>
#include <sys/uio.h> /* iovec */
#include <sys/time.h> /* rusage */
#include <sys/resource.h> /* rusage */
#include <sys/mman.h> /* madvise */

#include <algorithm> /* find_if */
#include <iostream>
#include <iterator> /* back_inserter */
#include <memory> /* make_unique */
#include <stdexcept> /* domain_error, range_error */
#include <sstream> /* ostringstream */
#include <string>

#if 0 /* moved to fabric_endpoint.cpp */
using guard = std::unique_lock<std::mutex>;

namespace
{
#if 0
  void *iov_end(const ::iovec &v)
  {
    return static_cast<char *>(v.iov_base) + v.iov_len;
  }
#endif
  /* True if range of a is a superset of range of b */
  using byte_span = common::byte_span;
  bool covers(const byte_span a, const byte_span b)
  {
    return ::base(a) <= ::base(b) && ::end(b) <= ::end(a);
  }
  std::ostream &operator<<(std::ostream &o, const byte_span v)
  {
    return o << "[" << ::base(v) << ".." << ::end(v) << ")";
  }

  long ru_flt()
  {
    rusage usage;
    auto rc = ::getrusage(RUSAGE_SELF, &usage);
    return rc == 0 ? usage.ru_minflt + usage.ru_majflt : 0;
  }
  bool mr_trace = std::getenv("FABRIC_MR_TRACE");
}

ru_flt_counter::ru_flt_counter(bool report_)
  : _report(report_)
  , _ru_flt_start(ru_flt())
{}

ru_flt_counter::~ru_flt_counter()
{
  if ( _report )
  {
    PLOG("fault count %li", ru_flt() - _ru_flt_start);
  }
}
#if 0
/**
 * Fabric/RDMA-based network component
 *
 */

/* Note: the info is owned by the caller, and must be copied if it is to be saved. */
Fabric_memory_control::Fabric_memory_control(
  Fabric &fabric_
  , fi_info &info_
)
  : _fabric(fabric_)
  , _domain_info(make_fi_infodup(info_, "domain"))
  /* NOTE: "this" is returned for context when domain-level events appear in the event queue bound to the domain
   * and not bound to a more specific entity (an endpoint, mr, av, pr scalable_ep).
   */
  , _domain(_fabric.make_fid_domain(*_domain_info, this))
  , _m{}
  , _mr_addr_to_mra{}
  , _paging_test(bool(std::getenv("FABRIC_PAGING_TEST")))
  , _fault_counter(_paging_test || bool(std::getenv("FABRIC_PAGING_REPORT")))
{
}

Fabric_memory_control::~Fabric_memory_control()
{
}

struct mr_and_address
{
  using byte_span = common::byte_span;
  using const_byte_span = common::const_byte_span;
  mr_and_address(::fid_mr *mr_, const_byte_span contig_)
    : mr(fid_ptr(mr_))
    , v(common::make_byte_span(const_cast<void *>(::base(contig_)), ::size(contig_)))
  {}
  std::shared_ptr<::fid_mr> mr;
  byte_span v;
};
#endif
namespace
{
  void print_registry(const std::multimap<const void *, std::unique_ptr<mr_and_address>> &matm)
  {
    unsigned i = 0;
    const unsigned limit = 0;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
    for ( const auto &j : matm )
    {
      if ( i < limit )
      {
        std::cerr << "Token " << common::pointer_cast<component::IFabric_memory_region>(&*j.second) << " mr " << j.second->mr << " " << j.second->v << "\n";
      }
      else
      {
        std::cerr << "Token (suppressed " << matm.size() - limit << " more)\n";
        break;
      }
      ++i;
    }
#pragma GCC diagnostic pop
  }
}

auto Fabric_endpoint::register_memory(const_byte_span contig_, std::uint64_t key_, std::uint64_t flags_) -> memory_region_t
{
  auto mra =
    std::make_unique<mr_and_address>(
      make_fid_mr_reg_ptr(contig_,
                          std::uint64_t(FI_SEND|FI_RECV|FI_READ|FI_WRITE|FI_REMOTE_READ|FI_REMOTE_WRITE),
                          key_,
                          flags_)
      , contig_
    );

  assert(mra->mr);

  /* operations which access local memory will need the "mr." Record it here. */
  guard g{_m};

  auto it = _mr_addr_to_mra.emplace(::base(contig_), std::move(mra));

  /*
   * Operations which access remote memory will need the memory key.
   * If the domain has FI_MR_PROV_KEY set, we need to return the actual key.
   */
  if ( mr_trace )
  {
    std::cerr << "Registered token " << common::pointer_cast<component::IFabric_memory_region>(&*it->second) << " mr " << it->second->mr << " " << it->second->v << "\n";
    print_registry(_mr_addr_to_mra);
  }
  return common::pointer_cast<component::IFabric_memory_region>(&*it->second);
}

void Fabric_endpoint::deregister_memory(const memory_region_t mr_)
{
  /* recover the memory region as a unique ptr */
  auto mra = common::pointer_cast<mr_and_address>(mr_);

  guard g{_m};

  auto lb = _mr_addr_to_mra.lower_bound(::data(mra->v));
  auto ub = _mr_addr_to_mra.upper_bound(::data(mra->v));

  map_addr_to_mra::size_type scan_count = 0;
  auto it =
    std::find_if(
      lb
      , ub
      , [&mra, &scan_count] ( const map_addr_to_mra::value_type &m ) { ++scan_count; return &*m.second == mra; }
  );

  if ( it == ub )
  {
    std::ostringstream err;
    err << __func__ << " token " << mra << " mr " << mra->mr << " (with range " << mra->v << ")"
      << " not found in " << scan_count << " of " << _mr_addr_to_mra.size() << " registry entries";
    std::cerr << "Deregistered token " << mra << " mr " << mra->mr << " failed, not in \n";
    print_registry(_mr_addr_to_mra);
    throw std::logic_error(err.str());
  }
  if ( mr_trace )
  {
    std::cerr << "Deregistered token (before) " << mra << " mr/sought " << mra->mr << " mr/found " << it->second->mr << " " << it->second->v << "\n";
    print_registry(_mr_addr_to_mra);
  }
  _mr_addr_to_mra.erase(it);
  if ( mr_trace )
  {
    std::cerr << "Deregistered token (after)\n";
    print_registry(_mr_addr_to_mra);
  }
}

std::uint64_t Fabric_endpoint::get_memory_remote_key(const memory_region_t mr_) const noexcept
{
  /* recover the memory region */
  auto mr = &*common::pointer_cast<mr_and_address>(mr_)->mr;
  /* ask fabric for the key */
  return ::fi_mr_key(mr);
}

void *Fabric_endpoint::get_memory_descriptor(const memory_region_t mr_) const noexcept
{
  /* recover the memory region */
  auto mr = &*common::pointer_cast<mr_and_address>(mr_)->mr;
  /* ask fabric for the descriptor */
  return ::fi_mr_desc(mr);
}

/* If local keys are needed, one local key per buffer. */
std::vector<void *> Fabric_memory_control::populated_desc(const std::vector<::iovec> & buffers)
{
  return populated_desc(&*buffers.begin(), &*buffers.end());
}

/* find a registered memory region which covers the iovec range */
::fid_mr *Fabric_endpoint::covering_mr(const byte_span v)
{
  /* _mr_addr_to_mr is sorted by starting address.
   * Find the last acceptable starting address, and iterate
   * backwards through the map until we find a covering range
   * or we reach the start of the table.
   */

  guard g{_m};

  auto ub = _mr_addr_to_mra.upper_bound(::data(v));

  auto it =
    std::find_if(
      map_addr_to_mra::reverse_iterator(ub)
      , _mr_addr_to_mra.rend()
      , [&v] ( const map_addr_to_mra::value_type &m ) { return covers(m.second->v, v); }
    );

  if ( it == _mr_addr_to_mra.rend() )
  {
    std::ostringstream e;
    e << "No mapped region covers " << v;
    throw std::range_error(e.str());
  }

#if 0
  std::cerr << "covering_mr( " << v << ") found mr " << it->second->mr << " with range " << it->second->v << "\n";
#endif
  return &*it->second->mr;
}

/* If local keys are needed, one local key per buffer. */
std::vector<void *> Fabric_endpoint::populated_desc(gsl::span<const ::iovec> buffers)
{
  std::vector<void *> desc;

  std::transform(
    first
    , last
    , std::back_inserter(desc)
    , [this] (const ::iovec &v) { return ::fi_mr_desc(covering_mr(common::make_byte_span(v.iov_base, v.iov_len))); }
  );

  return desc;
}

/* (no context, synchronous only) */
/*
 * ERROR: the sixth parameter is named "requested key" in fi_mr_reg doc, but
 * if the user asks for a key which is unavailable the error returned is
 * "Required key not available." The parameter name and the error disagree:
 * "requested" is not the same as "required."
 */
fid_mr * Fabric_endpoint::make_fid_mr_reg_ptr(
  const_byte_span buf
  , uint64_t access
  , uint64_t key
  , uint64_t flags
) const
{
  ::fid_mr *f;
  auto constexpr offset = 0U; /* "reserved and must be zero" */
  /* used iff the registration completes asynchronously
   * (in which case the domain has been bound to an event queue with FI_REG_MR)
   */
  auto constexpr context = nullptr;
  try
  {
    /* Note: this was once observed to return "Cannot allocate memory" when called from JNI code. */
    /* Note: this was once observed to return an error when the DAX persistent Apache Pass memory
     * seemed properly aligned. The work-around was to issue a pre-emptive madvise(MADV_DONTFORK)
     * against the entire memory space of the DAX device.
     */
    /* Note: this was once observed to return "Bad address" when the (GPU) memory seemed properly aligned. */
    CHECK_FI_ERR(::fi_mr_reg(&*_domain, ::data(buf), ::size(buf), access, offset, key, flags, &f, context));
    if ( _paging_test )
    {
      auto rc = ::madvise(const_cast<common::byte *>(::data(buf)), ::size(buf), MADV_DONTNEED);
      PLOG("Paging test madvisee(%p, 0x%zx, MADV_DONTNEED) %s", ::base(buf), ::size(buf), rc ? " refused" : " accepted");
    }
  }
  catch ( const fabric_runtime_error &e )
  {
    std::ostringstream s;
    s << std::showbase << std::hex << " in " << __func__ << " calling ::fi_mr_reg(domain "
      << &*_domain << " buf " << ::base(buf) << ", len " << ::size(buf) << ", access " << access
      << ", offset " << offset << ", key " << key << ", flags " << flags << ", fid_mr " << &f
      << ", context " << common::p_fmt(context) << ")";
    throw e.add(s.str());
  }
  FABRIC_TRACE_FID(f);
  return f;
}
#endif
