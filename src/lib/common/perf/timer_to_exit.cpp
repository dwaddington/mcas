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

#include <common/perf/timer_to_exit.h>

#include <common/perf/duration_stat.h>
#include <common/perf/timer_split.h>

using namespace common;
using namespace perf;

timer_to_exit::timer_to_exit(timer_split &tm_, duration_stat &st_)
	: _tm(&tm_)
	, _st(&st_)
{}

timer_to_exit::timer_to_exit(timer_to_exit &&t_)
	: _tm(t_._tm)
	, _st(t_._st)
{
	t_._st = nullptr;
}

timer_to_exit &timer_to_exit::operator=(timer_to_exit &&t_)
{
	if ( _st )
	{
		this->split(*_st);
	}
	_tm = t_._tm;
	_st = t_._st;
	t_._st = nullptr;
	return *this;
}

/*
 * As above, but also attribute the timer split_time at construction to sp_
 */
timer_to_exit::timer_to_exit(timer_split &tm_, duration_stat &sp_, duration_stat &st_)
	: timer_to_exit(tm_, st_)
{
	this->split(sp_);
}

/*
 * iattribute the timer split_time up to now to sp_
 */
void timer_to_exit::split(duration_stat &sp_) const
{
	sp_.record(_tm->split_duration());
}

timer_to_exit::~timer_to_exit()
{
	if ( _st )
	{
		this->split(*_st);
	}
}
