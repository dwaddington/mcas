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

#ifndef MCAS_HSTORE_LOCK_RESULT_H
#define MCAS_HSTORE_LOCK_RESULT_H

#include <api/kvstore_itf.h>

#include <cstddef> /* size_t */

struct lock_result
{
	enum class e_state
	{
		extant, created, not_created, creation_failed
	} state;
	component::IKVStore::key_t key;
	void *value;
	std::size_t value_len;
	const char *key_ptr;
};

#endif
