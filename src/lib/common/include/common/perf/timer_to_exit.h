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

#ifndef MCAS_COMMON_PERF_TIMER_TO_EXIT_H
#define MCAS_COMMON_PERF_TIMER_TO_EXIT_H

namespace common
{
	namespace perf
	{
		struct timer_split;
		struct duration_stat;

		struct timer_to_exit
		{
		private:
			timer_split   *_tm;
			duration_stat *_st;
		public:
			/*
			 * Remember a timer_split and a duration_stat.
			 * When destructed, attribute the timer current "split time" to the duration_stat.
			 */
			timer_to_exit(timer_split &tm_, duration_stat &st_);
			timer_to_exit(const timer_to_exit &) = delete;
			timer_to_exit(timer_to_exit &&t_);
			timer_to_exit &operator=(const timer_to_exit &) = delete;
			timer_to_exit &operator=(timer_to_exit &&);

			/*
			 * As above, but also attribute the timer split_time at construction to sp_
			 */
			timer_to_exit(timer_split &tm_, duration_stat &sp_, duration_stat &st_);

			/*
			 * iattribute the timer split_time up to now to sp_
			 */
			void split(duration_stat &sp_) const;
			virtual ~timer_to_exit();
		};
	}
}

#endif
