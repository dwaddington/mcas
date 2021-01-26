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

#ifndef MCAS_COMMON_PERF_DURATION_STAT_H
#define MCAS_COMMON_PERF_DURATION_STAT_H

#include <atomic>
#include <cstdint> /* uint64_t */
#include <chrono>
#include <ostream>
#include <stdexcept> /* domain_error */

#include <cassert>
#include <iostream>

namespace common
{
	namespace perf
	{
		struct duration_stat
		{
		private:
			using clock_t = std::chrono::steady_clock;
		public:
			using duration_t = clock_t::duration;
			using time_point_t = clock_t::time_point;
			using time_point = time_point_t; /* old */
			using count_t = std::uint64_t;
		private:
			using rep_t = std::uint64_t;
			std::atomic<duration_t::rep> _duration;
			std::atomic<rep_t>           _dur_sq;
			std::atomic<count_t>         _count;
		private:
			double mean() const;
			double stddev() const;

			/* Absolute (or "active") versions */
			static time_point_t a_now()
			{
				return clock_t::now();
			}

			void a_record(const duration_t &d)
			{
				_duration += d.count();
				_dur_sq += static_cast<count_t>(d.count() * d.count());
				++_count;
			}

			time_point_t a_record(const time_point_t &s)
			{
				auto n = this->now();
				auto d = n-s;
				if ( n < s )
				{
std::cerr << "duration_stat: negative duration: then " << s.time_since_epoch().count() << " now " << n.time_since_epoch().count() << "\n";
assert(0);
					throw std::domain_error{"duration_stat: negative duration"};
				}
				record(d);
				return n;
			}

			/* Inactive functions, for use when disabled */
			static time_point_t i_now()
			{
				return time_point_t{};
			}

			void i_record(const duration_t &)
			{
			}

			time_point_t i_record(const time_point_t &)
			{
				return time_point_t{};
			}
		public:
			duration_stat();

			static bool _clock_enabled;

			static time_point_t now()
			{
				return _clock_enabled ? a_now() : i_now();
			}

			void record(const duration_t &d)
			{
				return _clock_enabled ?  a_record(d) : i_record(d);
			}

			time_point_t record(const time_point_t &s)
			{
				return _clock_enabled ? a_record(s) : i_record(s);
			}

			count_t count() const;

			double cv() const;

			double cv_or_zero() const;

			std::chrono::nanoseconds::rep sum_durations_ns() const;
			double sum_durations_sec() const;

			unsigned long long sum_durations_ns_squared() const;
			operator bool() const { return _count != 0; }
		};

		std::ostream &operator<<(std::ostream &o, const duration_stat &d);
	}
}

#endif
