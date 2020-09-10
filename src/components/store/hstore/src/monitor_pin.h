/*
   Copyright [2019-2020] [IBM Corporation]
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


#ifndef MCAS_MONITOR_PIN_H
#define MCAS_MONITOR_PIN_H

#include "hstore_config.h" /* USE_CC_HEAP */
#include "logging.h"

template <typename Pool>
	struct monitor_pin_data
	{
	private:
		Pool _p;
#if USE_CC_HEAP == 4
#else
		char *_old_cptr;
		monitor_pin_data(const monitor_pin_data &) = delete;
		monitor_pin_data &operator=(const monitor_pin_data &) = delete;
#endif
	public:
		template <typename Value>
			monitor_pin_data(Value &v_, const Pool &p_)
				: _p(p_)
#if USE_CC_HEAP == 4
#else
				, _old_cptr(v_.get_cptr().P)
#endif
			{
#if USE_CC_HEAP == 4
				_p->pin_data_arm(v_.get_cptr());
#endif
			}

		char *get_cptr() const
		{
#if USE_CC_HEAP == 4
			return _p->pin_data_get_cptr();
#else
			return _old_cptr;
#endif
		}

		~monitor_pin_data() noexcept(! TEST_HSTORE_PERISHABLE)
		{
			if ( ! perishable_expiry::is_current() )
			{
#if USE_CC_HEAP == 4
				_p->pin_data_disarm();
#endif
			}
		}
	};

template <typename Pool>
	struct monitor_pin_key
	{
		Pool _p;
#if USE_CC_HEAP == 4
#else
		char *_old_cptr;
		monitor_pin_key(const monitor_pin_key &) = delete;
		monitor_pin_key &operator=(const monitor_pin_key &) = delete;
#endif
	public:
		template <typename Value>
			monitor_pin_key(Value &v_, const Pool &p_)
				: _p(p_)
#if USE_CC_HEAP == 4
#else
				, _old_cptr(v_.get_cptr().P)
#endif
			{
#if USE_CC_HEAP == 4
				_p->pin_key_arm(v_.get_cptr());
#endif
			}

		char *get_cptr() const
		{
#if USE_CC_HEAP == 4
			return _p->pin_key_get_cptr();
#else
			return _old_cptr;
#endif
		}

		~monitor_pin_key() noexcept(! TEST_HSTORE_PERISHABLE)
		{
			if ( ! perishable_expiry::is_current() )
			{
#if USE_CC_HEAP == 4
				_p->pin_key_disarm();
#endif
			}
		}
	};

#endif
