/*
  Copyright [2017-2020] [IBM Corporation]
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

#ifndef MCAS_RANGE_T_H
#define MCAS_RANGE_T_H

#include <common/utils.h> /* rounding functions */

#include <utility>

namespace mcas
{
/* Lots of things call themselves "range", but none are standard yet.
 * This one is typically a pair of addresses which bound a region used
 * for registered memory.
 */
template <typename T>
struct range : public std::pair<T, T> {
  range(T a, T b) : std::pair<T, T>(a, b) {}
  range() : range(T(), T()) {}
  range round_inclusive(std::size_t page_size) const
  {
    return range(static_cast<T>(round_down(this->first, page_size)), static_cast<T>(round_up(this->second, page_size)));
  }
  std::size_t length() const { return std::size_t(this->second - this->first); }
};

template <typename T>
bool operator<(const range<T> &a, const range<T> &b)
{
  return a.first < b.first || ((!(b.first < a.first)) && a.second < b.second);
}
}  // namespace mcas
#endif
