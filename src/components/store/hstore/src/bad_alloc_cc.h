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


#ifndef COMANCHE_HSTORE_BAD_ALLOC_H
#define COMANCHE_HSTORE_BAD_ALLOC_H

#include <new> /* bad_alloc */

/* Every function which takes an alloc_key may throw bad_alloc.
 * Every constructor of an alloc_key must catch BAD_ALLOC and return an error, e.g. E_TOO_LARGE.
 */
#include "alloc_key.h"

#include <cstddef> /* size_t */
#include <string>

struct bad_alloc_cc
	: public std::bad_alloc
{
private:
	std::string _what;
public:
	bad_alloc_cc(AK_FORMAL std::size_t pad, std::size_t count, std::size_t size)
		: _what(std::string(__func__) + ": " + std::to_string(pad) + "+" + std::to_string(count) + "*" + std::to_string(size))
	{}
	const char *what() const noexcept override
	{
		return _what.c_str();
	}
};

#endif
