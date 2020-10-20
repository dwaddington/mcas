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


/*
 * Authors:
 *
 */

#ifndef _NUPM_REGION_DESCRIPTOR_
#define _NUPM_REGION_DESCRIPTOR_

#include <experimental/string_view>
#include <string>
#include <vector>
#include <sys/uio.h>

namespace nupm
{
	struct region_descriptor
	{
		using address_map_t = std::vector<::iovec>;
		using string_view = std::experimental::string_view;
		std::string id;
		std::string data_file;
		address_map_t address_map;

		/* fsdax data */
		region_descriptor(
			const string_view &id_
			, const string_view &data_file_
			, const address_map_t address_map_
		)
			: id(id_)
			, data_file(data_file_)
			, address_map(address_map_)
		{}

		/* devdax data */
		region_descriptor(
			const address_map_t address_map_
		)
			: region_descriptor(string_view(), string_view(), address_map_)
		{}

		/* no data */
		region_descriptor()
			: region_descriptor(address_map_t())
		{}
	};
}

#endif
