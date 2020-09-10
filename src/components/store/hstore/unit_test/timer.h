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
#ifndef _mcas_HSTORE_TEST_TIMER_H_
#define _mcas_HSTORE_TEST_TIMER_H_

#include <chrono>
#include <functional> /* function */

struct timer
{
	using clock_t = std::chrono::steady_clock;
	using duration_t = typename clock_t::duration;
private:
	std::function<void(duration_t) noexcept> _f;
	clock_t::time_point _start;
public:
#pragma GCC diagnostic push
#if defined __GNUC__ && 6 < __GNUC__ && __cplusplus < 201703L
#pragma GCC diagnostic ignored "-Wnoexcept-type"
#endif
	timer(std::function<void(duration_t) noexcept> f_)
		: _f(f_)
		, _start(clock_t::now())
	{}
#pragma GCC diagnostic pop
	~timer()
	{
		_f(clock_t::now() - _start);
	}
};

#endif
