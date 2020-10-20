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
#include "tls_session.h"
#include "connection_state.h"
#include "connection_handler.h"

namespace mcas
{

Connection_state Connection_TLS_session::process_tls_session()
{
  return Connection_state::WAIT_NEW_MSG_RECV;// move to next state
}

}
