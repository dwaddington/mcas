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

#include <common/perf/duration_stat.h>

#include <common/env.h>

#include <stdexcept> /* domain_error */

#include <cassert>
#include <iostream>
using namespace common;
using namespace perf;

bool duration_stat::_clock_enabled{env_value("MCAS_DURATION_CLOCK_ENABLED", false)};

duration_stat::duration_stat()
	: _duration{}
	, _delta{}
	, _dur_sq{}
	, _count{}
{}

double duration_stat::mean() const
{
	return static_cast<double>(_duration)/static_cast<double>(_count);
}

double duration_stat::stddev() const
{
	auto dc = _duration.load();
	return static_cast<double>(_dur_sq) - static_cast<double>(dc*dc)/static_cast<double>(_count);
}

auto duration_stat::count() const
	-> count_t
{
	return _count;
}

double duration_stat::cv() const
{
	if ( _count == 0 )
	{
std::cerr << "duration_state::cv called for statistic with no samples\n";
assert(0);
		throw std::domain_error{"duration_state::cv called for statistic with no samples"};
	}
	return stddev()/mean();
}

double duration_stat::cv_or_zero() const
{
	return _count == 0 ? 0.0 : cv();
}

std::chrono::nanoseconds::rep duration_stat::sum_durations_ns() const
{
	return std::chrono::duration_cast<std::chrono::nanoseconds>(clock_t::duration(_duration)).count();
}

double duration_stat::sum_durations_sec() const
{
	using period = clock_t::duration::period;
	return double(this->_duration)*period::num/period::den;
}

unsigned long long duration_stat::sum_durations_ns_squared() const
{
	/*
	 * convert the sum of durations squared to nanoseconds squared.
	 * We use sqrt followed by square, to convert from high_requlution_clock::rep to
	 * nanoseconds::rep, but we could use the square of the ratios of the periods to
	 * avoid the trip through sqrt and square.
	 */
	using dpd = duration_t::period;
	using npd = std::chrono::nanoseconds::period;
	auto ns2 = static_cast<double>(_dur_sq) * static_cast<double>(dpd::num * dpd::num * npd::den * npd::den) / static_cast<double>(dpd::den * dpd::den * npd::num * npd::num);
	return static_cast<unsigned long long>(ns2);
}

std::ostream &common::perf::operator<<(std::ostream &o, const duration_stat &d)
{
	return o << d.sum_durations_sec() << " " << d.count();
}
