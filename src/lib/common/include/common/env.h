/*
   Copyright [2014-2021] [IBM Corporation]
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

#ifndef MCAS_COMMON_PERF_ENV_H
#define MCAS_COMMON_PERF_ENV_H

#include <limits>
#include <string>
#include <type_traits>

/* Function to extract a value from an environment variable */
namespace common
{

	template <typename T>
		T env_value(const char *key, T dflt);

	template <int>
		struct env_selector
		{
		};
 
	template<>
		struct env_selector<1>
		{
			template <typename T>
				static T f(
					const char *key
					, T dflt
					, T max = std::numeric_limits<T>::max()
				)
				{
					return T(g(key, dflt, max));
				}
				static unsigned long long g(const char *key, unsigned long long dflt, unsigned long long max);
		};
 
	template<>
		struct env_selector<2>
		{
			template <typename T>
				static T f(
					const char *key
					, T dflt
					, T min = std::numeric_limits<T>::min()
					, T max = std::numeric_limits<T>::max()
				)
				{
					return T(g(key, dflt, max));
				}
				static signed long long g(const char *key, signed long long dflt, signed long long min, signed long long max);
		};

	template <typename T>
		T env_value(const char *key, T dflt)
		{
			return env_selector<std::is_unsigned<T>::value*1 + std::is_signed<T>::value*2>::f(key, dflt);
		}

	template <>
		double env_value<double>(const char *const key, double dflt);

	template <>
		const char *env_value<const char *>(const char *const key, const char *dflt);

	template <>
		std::string env_value<std::string>(const char *const key, std::string dflt);
}

#endif
