/*
   Copyright [2021] [IBM Corporation]
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

#ifndef MCAS_NUPM_RANGE_MANAGER_H__
#define MCAS_NUPM_RANGE_MANAGER_H__

#include <common/byte_span.h>
#include <boost/icl/split_interval_map.hpp>
#include <cstddef> /* size_t */

namespace nupm
{
	struct range_manager
	{
		/*
		 * Until we get past boost 1.65, this interface has to work across
		 * C++14 and C++17. common::byte, which is gsl::byte, has different
		 * types in the two g++ compilers.
		 */
		using byte = char;
		using byte_interval = boost::icl::discrete_interval<char *>;
		using byte_interval_set = boost::icl::interval_set<char *>;
	protected:
		~range_manager() {}
	public:
		virtual bool interferes(byte_interval coverage) const = 0;
		virtual void add_coverage(byte_interval_set coverage) = 0;
		virtual void remove_coverage(byte_interval coverage) = 0;
		virtual void *locate_free_address_range(std::size_t size) const = 0;
	};
}

#endif
