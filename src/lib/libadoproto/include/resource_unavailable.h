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

#ifndef __mcas_RESOURCE_UNAVAILABLE_H__
#define __mcas_RESOURCE_UNAVAILABLE_H__

#include <stdexcept>

class resource_unavailable
  : public std::runtime_error
{
public:
  resource_unavailable(const std::string &why)
    : std::runtime_error(why)
  {}
  resource_unavailable()
    : resource_unavailable("resource (probably a buffer) unavailable")
  {}
};

#endif
