/*
   Copyright [2021] [IBM Corporation]
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


#ifndef _EVENT_EXPECTER_H_
#define _EVENT_EXPECTER_H_

#include <cstddef> /* uint32_t */

/* interface for expecting a connect event */
struct event_expecter
{
public:
	virtual ~event_expecter() {}
	virtual void expect_event(std::uint32_t event_exp) = 0;
};

#endif
