/*
   Copyright [2020] [IBM Corporation]
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

#ifndef _MCAS_COMMON_IOVEC_
#define _MCAS_COMMON_IOVEC_

#include <common/pointer_cast.h>
#include <cstddef>
#include <gsl/gsl_byte>
#include <gsl/span>
#include <sys/uio.h>

/* For minimal difference with older code; implement span as as iovec */
#define MCAS_SPAN_USES_IOVEC 1

namespace common
{
	/* span of a const area. No equivalent in ::iovec, so always use span */
	using const_byte_span = gsl::span<const gsl::byte>;
	inline const_byte_span make_const_byte_span(const void *base, std::size_t len)
    {
      return const_byte_span(static_cast<const_byte_span::pointer>(base), len);
    }
}

namespace
{
	/* Accessors: Non-member in order to match ::iovec accessors */
	/* Start of area as a void *, for use with %p format and for conversion to an arbitrary type */
	inline const void *base(const common::const_byte_span &r) { return r.data(); }
    /* Length of area in bytes, for use in byte-based address calculations and comparisons */
	inline std::size_t size(const common::const_byte_span &r) { return r.size(); }
	/* Start of area as byte *, for use in byte-based address calculations and comparisons */
	inline const gsl::byte *data(const common::const_byte_span &r) { return r.data(); }
	/* End of area as byte *, for use in byte-based address calculations and comparisons */
	inline const gsl::byte *data_end(const common::const_byte_span &r) { return r.data() + r.size(); }
	/* End of are as a void *, for use with %p format */
	inline const void *end(const common::const_byte_span &r) { return ::data_end(r); }
}

namespace
{
	/* Same accessors as above, for ::iovec */
	inline void *base(const ::iovec &r) { return r.iov_base; }
	inline std::size_t size(const ::iovec &r) { return r.iov_len; }
	inline gsl::byte *data(const ::iovec &r) { return static_cast<gsl::byte *>(::base(r)); }
	inline gsl::byte *data_end(const ::iovec &r) { return ::data(r) + ::size(r); }
	inline void *end(const ::iovec &r) { return ::data_end(r); }
}

namespace common
{
	inline constexpr ::iovec make_iovec(void *base, std::size_t len) { return ::iovec{base, len}; }
}

#if MCAS_SPAN_USES_IOVEC

namespace common
{
	using byte_span = ::iovec;
	/* Construct an iovec in syntax compatible with span (no braces) */
	inline byte_span make_byte_span(void *base, std::size_t len) { return byte_span{base, len}; }
}

#else

#include <common/pointer_cast.h>
namespace common
{
	using byte_span = gsl::span<gsl::byte>;
}

namespace
{
	/* Same accessors as above, for non-const byte span */
	inline void *base(const common::byte_span &r) { return r.data(); }
	inline std::size_t size(const common::byte_span &r) { return r.size(); }
	inline gsl::byte *data(const common::byte_span &r) { return r.data(); }
	inline gsl::byte *data_end(const common::byte_span &r) { return ::data(r) + ::size(r); }
	inline void *end(const common::byte_span &r) { return ::data_end(r); }
}

namespace common
{
	/* Construct a byte_span in syntax compatible with iovec (function, not constructor) */
	inline byte_span make_byte_span(void *base, std::size_t len) { return byte_span(common::pointer_cast<byte_span::value_type>(base), len); }
}
#endif

namespace common
{
	/* Converion from span of non-const to span of const */
	inline const_byte_span make_const_byte_span(const byte_span s) { return const_byte_span(::data(s), ::size(s)); }
}

#endif
