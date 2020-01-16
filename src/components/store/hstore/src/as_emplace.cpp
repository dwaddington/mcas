/*
   Copyright [2019] [IBM Corporation]
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

#include "as_emplace.h"

#include "hstore_config.h"
#include "as_emplace.h"
#include <common/logging.h>
#include <cassert>

/*
 * ===== emplace_allocation_state =====
 */

impl::allocation_state_emplace::allocation_state_emplace()
	: _ptr0(nullptr)
	, _ptr1(nullptr)
	, _pmask(nullptr)
	, _mask(0)
{
}

bool impl::allocation_state_emplace::is_in_use(const void *const ptr_)
{
	auto in_use =
		ptr_ != nullptr
#if USE_CC_HEAP == 4
		&&
		_pmask != nullptr
		&&
		( *_pmask & _mask ) != 0
		&&
		( ptr_ == _ptr0 || ptr_ == _ptr1 )
#endif
		;
	PLOG("Ptr %p, in use %s", ptr_, in_use ? "true" : "false");
	return in_use;
}

impl::allocation_state_emplace::allocation_state_emplace(allocation_state_emplace &&m_)
	: _ptr0(m_._ptr0.load())
	, _ptr1(m_._ptr1.load())
	, _pmask(m_._pmask.load())
	, _mask(m_._mask.load())
{
}
