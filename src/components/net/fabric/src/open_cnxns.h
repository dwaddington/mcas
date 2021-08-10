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


#ifndef _OPEN_CONNECTIONS_H_
#define _OPEN_CONNECTIONS_H_

#include <memory> /* shared_ptr */
#include <mutex>
#include <set>
#include <vector>

/* The generic factory code keeps the list of open connections.
 * The specific factory will cast the list elements to IFabric_server *
 * or IFabric_server_grouped * as it requires.
 */
class event_expecter;

class Open_cnxns
{
public:
	/* the external type */
  using cnxn_type = event_expecter;
private:
	/* the internal type */
	using owned_type = std::unique_ptr<cnxn_type>;

  std::mutex _m; /* protects _s */
  std::set<owned_type> _s;
public:
  Open_cnxns();
  void add(cnxn_type *c);
  void remove(cnxn_type *c);
  std::vector<cnxn_type *> enumerate();
};

#endif
