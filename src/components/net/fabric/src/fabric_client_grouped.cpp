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


/*
 * Authors:
 *
 */

#include "fabric_client_grouped.h"

/**
 * Fabric/RDMA-based network component
 *
 */

/* Note: the info is owned by the caller, and must be copied if it is to be saved. */
Fabric_client_grouped::Fabric_client_grouped(
    component::IFabric_endpoint_unconnected_client *aep_
    , event_producer &ep_
	, fabric_types::addr_ep_t peer_addr_
)
  : Fabric_connection_client(aep_, ep_, peer_addr_)
  , _g(this, aep()->rxcq(), aep()->txcq())
{
}

Fabric_client_grouped::~Fabric_client_grouped()
{
}
