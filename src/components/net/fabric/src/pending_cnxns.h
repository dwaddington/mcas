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


#ifndef _PENDING_CONNECTIONS_H_
#define _PENDING_CONNECTIONS_H_

#include "rdma-fabric.h"

#include <memory>
#include <mutex>
#include <queue>

struct fi_info_delete
{
  void operator()(::fi_info *f) { ::fi_freeinfo(f); }
};

struct pending_cnxns
{
public:
  using cnxn_t = std::unique_ptr<::fi_info, fi_info_delete>;
private:
  std::mutex _m; /* protects _q */
  using guard = std::lock_guard<std::mutex>;
  std::queue<cnxn_t> _q;
public:
  pending_cnxns();
  void push(cnxn_t && c);
  cnxn_t remove();
  void clear();
};

#endif
