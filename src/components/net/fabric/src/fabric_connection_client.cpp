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


#include "fabric_connection_client.h"

#include "bad_dest_addr_alloc.h"
#include "event_producer.h"
#include "fabric.h" /* choose_port */
#include "fabric_check.h" /* CHECK_FI_ERR */
#include "fabric_endpoint.h"
#include "fabric_runtime_error.h"
#include "fabric_str.h" /* tostr */
#include "fabric_types.h"
#include "fd_control.h"

#include "rdma-fi_cm.h" /* fi_connect, fi_shutdown */

#include <algorithm> /* copy */
#include <cstdint> /* size_t */
#include <exception>
#include <iostream> /* cerr */
#include <memory> /* unique_ptr */

namespace
{
  /*
   * @throw fabric_runtime_error : std::runtime_error : ::fi_connect fail
   */
  void fi_void_connect(::fid_ep &ep_, const ::fi_info &ep_info_, const void *addr_, const void *param_, size_t paramlen_)
  try
  {
    CHECK_FI_ERR(::fi_connect(&ep_, addr_, param_, paramlen_));
  }
  catch ( const fabric_runtime_error &e )
  {
    throw e.add(tostr(ep_info_));
  }
}

Fabric_connection_client::Fabric_connection_client(
  component::IFabric_endpoint_unconnected *aep_
  , event_producer &ev_
  , fabric_types::addr_ep_t peer_addr_
)
try
  : fabric_connection(aep_, peer_addr_)
  , _ev(ev_)
{
  if ( aep()->ep_info().ep_attr->type == FI_EP_MSG )
  {
    std::size_t paramlen = 0;
    auto param = nullptr;
    fi_void_connect(aep()->ep(), aep()->ep_info(), aep()->ep_info().dest_addr, param, paramlen);
    /* ERROR: event will be an FI_NOTIFY if the server is present but has a different provider
     * that we expect (e.g. sockets vs. verbs). This it not handled here or anywhere else.
     */
    expect_event_sync(FI_CONNECTED);
  }
}
catch ( fabric_runtime_error &e )
{
  throw e.add("in Fabric_connection_client constuctor");
}

Fabric_connection_client::~Fabric_connection_client()
{
  try
  {
    /* "the flags parameter is reserved and must be 0" */
    ::fi_shutdown(&aep()->ep(), 0);
    /* The server may in turn give us a shutdown event. We do not need to see it. */
  }
  catch ( const std::exception &e )
  {
    std::cerr << "CLIENT connection shutdown error " << e.what() << "\n";
  }
}

/* _ev.read_eq() in client, no-op in server */
void Fabric_connection_client::solicit_event() const
{
  _ev.read_eq();
}

void Fabric_connection_client::wait_event() const
{
  _ev.wait_eq();
}

void Fabric_connection_client::expect_event_sync(std::uint32_t event_exp) const
{
  aep()->ensure_event(this);
  aep()->expect_event(event_exp);
}
