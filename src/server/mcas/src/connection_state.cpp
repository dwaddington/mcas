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

#include "connection_state.h"
#include <ostream>
#include <map>

using Connection_state = mcas::Connection_state;

namespace
{
	const std::map<Connection_state, const char *> m{
		{Connection_state::INITIAL, "INITIAL"},
		{Connection_state::WAIT_HANDSHAKE, "WAIT_HANDSHAKE"},
		{Connection_state::WAIT_TLS_HANDSHAKE, "WAIT_TLS_HANDSHAKE"},
		{Connection_state::CLIENT_DISCONNECTED, "CLIENT_DISCONNECTED"},
		{Connection_state::CLOSE_CONNECTION, "CLOSE_CONNECTION"},
		{Connection_state::WAIT_NEW_MSG_RECV, "WAIT_NEW_MSG_RECV"}
	};
}

std::ostream &operator<<(std::ostream &o_, const Connection_state &s_)
{

	auto it = m.find(s_);
	return o_ << int(s_) << "/" << (it == m.end() ? "bad-state" : it->second);
}
