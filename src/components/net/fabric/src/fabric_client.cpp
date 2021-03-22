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


#include "fabric_client.h"

#include "fabric_endpoint.h"

Fabric_client::Fabric_client(
    component::IFabric_endpoint_unconnected *aep_
	, event_producer &ep_
	, fabric_types::addr_ep_t peer_addr_
)
	: Fabric_connection_client(aep_, ep_, peer_addr_)
{}

Fabric_client::~Fabric_client()
{}

std::size_t Fabric_client::poll_completions(const component::IFabric_op_completer::complete_old &completion_callback)
{
	return aep()->poll_completions(completion_callback);
}

std::size_t Fabric_client::poll_completions(const component::IFabric_op_completer::complete_definite &completion_callback)
{
	return aep()->poll_completions(completion_callback);
}

std::size_t Fabric_client::poll_completions_tentative(const component::IFabric_op_completer::complete_tentative &completion_callback)
{
	return aep()->poll_completions_tentative(completion_callback);
}

std::size_t Fabric_client::poll_completions(const component::IFabric_op_completer::complete_param_definite &completion_callback, void *callback_param)
{
	return aep()->poll_completions(completion_callback, callback_param);
}

std::size_t Fabric_client::poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative &completion_callback, void *callback_param)
{
	return aep()->poll_completions_tentative(completion_callback, callback_param);
}

std::size_t Fabric_client::poll_completions(const component::IFabric_op_completer::complete_param_definite_ptr_noexcept completion_callback, void *callback_param)
{
	return aep()->poll_completions(completion_callback, callback_param);
}

std::size_t Fabric_client::poll_completions_tentative(const component::IFabric_op_completer::complete_param_tentative_ptr_noexcept completion_callback, void *callback_param)
{
	return aep()->poll_completions_tentative(completion_callback, callback_param);
}

std::size_t Fabric_client::stalled_completion_count()
{
	return aep()->stalled_completion_count();
}

void Fabric_client::wait_for_next_completion(unsigned polls_limit)
{
	return aep()->wait_for_next_completion(polls_limit);
}

void Fabric_client::wait_for_next_completion(std::chrono::milliseconds timeout)
{
	return aep()->wait_for_next_completion(timeout);
}

void Fabric_client::unblock_completions()
{
	return aep()->unblock_completions();
}

auto Fabric_client::register_memory(
	const_byte_span contig
	, std::uint64_t key
	, std::uint64_t flags
) -> memory_region_t
{
	return aep()->register_memory(contig, key, flags);
}

void Fabric_client::deregister_memory(
	const memory_region_t memory_region
)
{
	return aep()->deregister_memory(memory_region);
}

std::uint64_t Fabric_client::get_memory_remote_key(
	const memory_region_t memory_region
) const noexcept
{
	return aep()->get_memory_remote_key(memory_region);
}

void *Fabric_client::get_memory_descriptor(
	const memory_region_t memory_region
) const noexcept
{
	return aep()->get_memory_descriptor(memory_region);
}

void Fabric_client::post_send(
	gsl::span<const ::iovec> buffers
	, void **desc
	, void *context
)
{
	return aep()->post_send(buffers, desc, context);
}

void Fabric_client::post_send(
	gsl::span<const ::iovec> buffers
	, void *context
)
{
	return aep()->post_send(buffers, context);
}

void Fabric_client::post_recv(
	gsl::span<const ::iovec> buffers
	, void **desc
	, void *context
)
{
	return aep()->post_recv(buffers, desc, context);
}

void Fabric_client::post_recv(
	gsl::span<const ::iovec> buffers
	, void *context
)
{
	return aep()->post_recv(buffers, context);
}

void Fabric_client::post_read(
	gsl::span<const ::iovec> buffers
	, void **desc
	, std::uint64_t remote_addr
	, std::uint64_t key
	, void *context
)
{
	return aep()->post_read(buffers, desc, remote_addr, key, context);
}

void Fabric_client::post_read(
	gsl::span<const ::iovec> buffers,
	std::uint64_t remote_addr,
	std::uint64_t key,
	void *context
)
{
	return aep()->post_read(buffers, remote_addr, key, context);
}

void Fabric_client::post_write(
	gsl::span<const ::iovec> buffers
	, void **desc
	, std::uint64_t remote_addr
	, std::uint64_t key
	, void *context
)
{
	return aep()->post_write(buffers, desc, remote_addr, key, context);
}

void Fabric_client::post_write(
	gsl::span<const ::iovec> buffers,
	std::uint64_t remote_addr,
	std::uint64_t key,
	void *context
)
{
	return aep()->post_write(buffers, remote_addr, key, context);
}

void Fabric_client::inject_send(
	const void *buf
	, const std::size_t len
)
{
	return aep()->inject_send(buf, len);
}
