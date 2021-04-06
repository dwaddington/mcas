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


#ifndef _FABRIC_CONNECTION_H_
#define _FABRIC_CONNECTION_H_

#include <api/fabric_itf.h> /* component::IFabric_connection */

#include "fabric_types.h"

#include <string>

struct fabric_endpoint;

struct fabric_connection
  : public component::IFabric_connection
{
private:
	fabric_endpoint *_aep;
	fabric_types::addr_ep_t _peer_addr;
protected:
	explicit fabric_connection(
		component::IFabric_endpoint_unconnected_client *aep
		, fabric_types::addr_ep_t
	);
	explicit fabric_connection(
		component::IFabric_endpoint_unconnected_server *aep
		, fabric_types::addr_ep_t
	);

	fabric_connection(const fabric_connection &) = delete;
	fabric_connection &operator=(const fabric_connection &) = delete;
	auto get_name() const -> fabric_types::addr_ep_t;
public:
	fabric_endpoint *aep() const;
	virtual void solicit_event() const = 0;
	virtual void wait_event() const = 0;
	std::string get_peer_addr() override;
	std::string get_local_addr() override;
	std::size_t max_message_size() const noexcept override;
	std::size_t max_inject_size() const noexcept override;
};

#endif
