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


#ifndef _FABRIC_ENDPOINT_UNCONNECTED_SERVER_H_
#define _FABRIC_ENDPOINT_UNCONNECTED_SERVER_H_

#include <api/fabric_itf.h>

#include "fabric_endpoint.h"

struct fi_info;

struct event_producer;
struct fabric;

struct fabric_endpoint_server
  : public fabric_endpoint
{
public:
	explicit fabric_endpoint_server(
   		Fabric &fabric
	    , event_producer &ev
	    , ::fi_info &info
	);
	fabric_endpoint_server(const fabric_endpoint_server &) = delete;
	fabric_endpoint_server &operator=(const fabric_endpoint_server &) = delete;
	
	virtual ~fabric_endpoint_server();
	/**
	 * @throw std::range_error - address already registered
	 * @throw std::logic_error - inconsistent memory address tables
	 */
	memory_region_t register_memory(
		const_byte_span contig
		, std::uint64_t key
		, std::uint64_t flags
	) override
	{ return fabric_endpoint::register_memory(contig, key, flags); }

	/**
	 * @throw std::range_error - address not registered
	 * @throw std::logic_error - inconsistent memory address tables
	 */
	void deregister_memory(
		const memory_region_t memory_region
	) override
	{ return fabric_endpoint::deregister_memory(memory_region); }

	std::uint64_t get_memory_remote_key(
		const memory_region_t memory_region
	) const noexcept override
	{ return fabric_endpoint::get_memory_remote_key(memory_region); }

	void *get_memory_descriptor(
		const memory_region_t memory_region
	) const noexcept override
	{ return fabric_endpoint::get_memory_descriptor(memory_region); }

	/*
	 * @throw fabric_runtime_error : std::runtime_error : ::fi_recvv fail
	 */
	void post_recv(
		gsl::span<const ::iovec> buffers
		, void **desc
		, void *context
	) override
	{ return fabric_endpoint::post_recv(buffers, desc, context); }

	void post_recv(
		gsl::span<const ::iovec> buffers
		, void *context
	) override
	{ return fabric_endpoint::post_recv(buffers, context); }
};

#endif
