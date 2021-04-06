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

#include "fabric_connection_base.h"

namespace mcas
{
Fabric_connection_base::Fabric_connection_base(unsigned debug_level_,
                                               gsl::not_null<component::IFabric_server_factory *> factory,
                                               std::unique_ptr<component::IFabric_endpoint_unconnected_server> && fabric_preconnection)
  : common::log_source(debug_level_),
    _preconnection(std::move(fabric_preconnection)),
    _bm(debug_level_, _preconnection.get()),
    _recv_buffer_posted_count{},
    _send_buffer_posted_count{},
    _send_value_posted_count{},
    _completed_recv_buffers{},
    /* one receive buffer must be posted before connection is opened, to contain the first client message */
    _oc(factory, (post_recv_buffer(allocate(static_recv_callback)), factory->open_connection(_preconnection.get())), open_connection_construct_key{}),
    _max_message_size(transport()->max_message_size()),
    _deferred_unlock{}
{
}

/**
 * Dtor
 *
 */
Fabric_connection_base::~Fabric_connection_base()
{
  /* close connection */
  //  _factory->close_connection(_transport);
}

}  // namespace mcas
