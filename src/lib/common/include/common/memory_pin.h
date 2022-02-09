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

#ifndef _MCAS_MEMORY_PIN_H_
#define _MCAS_MEMORY_PIN_H_

#include <numa.h> /* bitmask */
#include <sys/mman.h> /* mlock, munlock */

template <typename T>
	struct memory_pin
		: public T
	{
		using T::iov_base;
		using T::iov_len;
		int lock_rc;
		template <typename ... Args>
			memory_pin(bool do_pin_, Args && ... args_)
				: T(std::forward<Args>(args_) ...)
				, lock_rc(do_pin_ ? ::mlock(T::iov_base, T::iov_len) : -1)
			{
			}
		memory_pin(const memory_pin &) = delete;
		memory_pin &operator=(const memory_pin &) = delete;
		~memory_pin()
		{
			if ( 0 == lock_rc )
			{
				CFLOGM(1, "unpinning region memory ({},{})", iov_base, iov_len);
				::munlock(iov_base, iov_len);
			}
		}
	};

#endif
