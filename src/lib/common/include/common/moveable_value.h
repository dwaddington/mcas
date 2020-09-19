/*
   Copyright [2020] [IBM Corporation]
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

#include <algorithm> /* swap */

#ifndef _MCAS_COMMON_MOVEABLE_VALUE_
#define _MCAS_COMMON_MOVEABLE_VALUE_

namespace common
{
  template <typename T>
    struct moveable_traits
    {
      static constexpr T none = T();
    };
  /* A value which zero-initializes the source when moved.
   * helpful for classes which use a pointer or bool to denote
   * "moved from" state.
   */
  /* Note: the choice of zero-initializer should be controlled
   * by a trait type, not by a template parameter.
   * Use of a template parameter restricts the type if T
   */
  template <typename T, typename Traits = moveable_traits<T>>
    struct moveable_value
    {
      T v;
      moveable_value(const T &v_)
        : v(v_)
      {}
      moveable_value(moveable_value &&o_) noexcept
        : v(Traits::none)
      {
        using std::swap;
        swap(v, o_.v);
      }

      moveable_value &operator=(moveable_value &&o_) noexcept
      {
        using std::swap;
        swap(v, o_.v);
        return *this;
      }

      operator T() const { return v; }
    };

  /* Like moveable_value, but T is a class or struct and
   * therefore can be a base class
   */
  template <typename T, typename Traits = moveable_traits<T>>
    struct moveable_struct
      : protected T
    {
      /* construct a moveable_struct from T constructor args */
      template <typename ... Args>
        explicit moveable_struct(Args&& ... args)
          : T(std::forward<Args>(args)...)
        {}

      moveable_struct(moveable_struct &&o_) noexcept
        : T(Traits::none)
      {
        using std::swap;
        swap(*static_cast<T *>(this), static_cast<T &>(o_));
      }

      moveable_struct &operator=(moveable_struct &&o_) noexcept
      {
        using std::swap;
        swap(*static_cast<T *>(this), static_cast<T &>(o_));
        return *this;
      }
    };

}
#endif
