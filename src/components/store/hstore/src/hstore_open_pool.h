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


#ifndef COMANCHE_HSTORE_OPEN_POOL_H
#define COMANCHE_HSTORE_OPEN_POOL_H

#include <utility> /* forward */

/* A struct which resembles unique_ptr or shared_ptr, but for non-owing uses */
template <typename T>
	struct non_owner
	{
	private:
		T *_p;
	public:
		explicit non_owner() : non_owner(nullptr) {}
		explicit non_owner(T *p_) : _p(p_) {}
		non_owner(const non_owner &other) = default;
		non_owner &operator=(const non_owner &other) = default;
		auto get() const { return _p; }
		operator bool() const { return bool(_p); }
		auto operator->() const { return _p; }
		auto operator*() const { return *_p; }
	};

template <typename Handle>
	struct open_pool
		: private Handle
	{
		using handle_type = Handle;

		template <typename ... Args>
			explicit open_pool(
				Args && ... args_
			)
				: Handle(std::forward<Args>(args_)...)
			{}

		open_pool(const open_pool &) = delete;
		virtual ~open_pool() {}
		open_pool& operator=(const open_pool &) = delete;
		using Handle::get;
		using Handle::operator bool;
		using Handle::operator->;
	};

#endif
