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

#ifndef __CONNECTION_STATE_H__
#define __CONNECTION_STATE_H__

#include <iosfwd>

namespace mcas
{

enum class Connection_state
  {
   INITIAL,
   WAIT_HANDSHAKE,
   WAIT_NEW_MSG_RECV,
   WAIT_TLS_HANDSHAKE,
   CLOSE_CONNECTION,
   CLIENT_DISCONNECTED,
  };
}

std::ostream &operator<<(std::ostream &, const mcas::Connection_state &);

#endif
