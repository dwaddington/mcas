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


#include "fabric_connection_server.h"

#include "fabric_check.h" /* CHECK_FI_ERR */
#include "fabric_endpoint.h"
#include "fd_control.h"

#include "rdma-fi_cm.h" /* fi_accept, fi_shutdown */

#include <cstdint> /* size_t */
#include <exception>
#include <iostream> /* cerr */
#include <memory> /* unique_ptr */

Fabric_connection_server::Fabric_connection_server(
  component::IFabric_endpoint_unconnected *aep_
)
  : fabric_connection(aep_, fabric_types::addr_ep_t{})
{
  if ( aep()->ep_info().ep_attr->type == FI_EP_MSG )
  {
    std::size_t paramlen = 0;
    auto param = nullptr;
    CHECK_FI_ERR(::fi_accept(&aep()->ep(), param, paramlen));
  }
}

Fabric_connection_server::~Fabric_connection_server()
{
  try
  {
    /* "the flags parameter is reserved and must be 0" */
    ::fi_shutdown(&aep()->ep(), 0);
  /* The client may in turn call fi_shutdown, giving us an event. We do not need to see it.
   */
  }
  catch ( const std::exception &e )
  {
    std::cerr << "SERVER connection shutdown error " << e.what() << "\n";
  }
}

/* The server does not need to do anything to solicit an event,
 * as the server_factory continuously reads the server's event queue
 */
void Fabric_connection_server::solicit_event() const
{
}

void Fabric_connection_server::wait_event() const
{
}

#if 0
std::size_t Fabric_connection_server::poll_completions(const component::IFabric_op_completer::complete_old &completion_callback)
{
	return _aep->poll_completions(completion_callback);
}

std::size_t Fabric_connection_server::poll_completions(const component::IFabric_op_completer::complete_param_definite &completion_callback, void *callback_param)
{
	return _aep->poll_completions(completion_callback, callback_param);
}

std::size_t Fabric_connection_server::poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative &completion_callback, void *callback_param)
{
	return _aep->poll_completions_tentative(completion_callback, callback_param);
}

std::size_t Fabric_connection_server::poll_completions(const component::IFabric_op_completer::complete_definite &completion_callback)
{
	return _aep->poll_completions(completion_callback);
}

std::size_t Fabric_connection_server::poll_completions_tentative(const component::IFabric_op_completer::complete_tentative &completion_callback)
{
	return _aep->poll_completions_tentative(completion_callback);
}

std::size_t Fabric_connection_server::poll_completions(const component::IFabric_op_completer::complete_param_definite_ptr_noexcept completion_callback, void *callback_param)
{
	return _aep->poll_completions(completion_callback, callback_param);
}

std::size_t Fabric_connection_server::poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative_ptr_noexcept completion_callback, void *callback_param)
{
	return _aep->poll_completions_tentative(completion_callback, callback_param);
}

std::size_t Fabric_connection_server::stalled_completion_count()
{
	return _aep->stalled_completion_count();
}

void Fabric_connection_server::wait_for_next_completion(unsigned polls_limit)
{
	return _aep->wait_for_next_completion(polls_limit);
}

void Fabric_connection_server::wait_for_next_completion(std::chrono::milliseconds timeout)
{
	return _aep->wait_for_next_completion(timeout);
}

void Fabric_connection_server::unblock_completions()
{
	return _aep->unblock_completions();
}
#endif
#if 0
memory_region_t Fabric_connection_server::register_memory(
	const_byte_span contig
	, std::uint64_t key
	, std::uint64_t flags
)
{
	return _aep->register_memory(contig, key, flags);
}

void Fabric_connection_server::deregister_memory(
	const memory_region_t memory_region
)
{
	return _aep->deregister_memory(memory_region);
}

std::uint64_t Fabric_connection_server::get_memory_remote_key(
	const memory_region_t memory_region
) const noexcept
{
	return _aep->get_memory_remote_key(memory_region);
}

 void *Fabric_connection_server::get_memory_descriptor(
	const memory_region_t memory_region
) const noexceptcw
{
	return _aep->get_memory_descriptor(memory_region);
}
#endif
#if 0
std::string Fabric_connection_server::get_peer_addr()
{
	return fabric_connection::get_peer_addr();
}
#endif
#if 0
std::string Fabric_connection_server::get_local_addr()
{
	return fabric_connection::get_local_addr();
}
#endif

std::size_t Fabric_connection_server::max_message_size() const noexcept
{
	return aep()->ep_info().ep_attr->max_msg_size;
}

std::size_t Fabric_connection_server::max_inject_size() const noexcept
{
	return aep()->ep_info().tx_attr->inject_size;
}
