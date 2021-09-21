/*
   Copyright [2017-2019] [IBM Corporation]
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
#include "server_grouped_connection.h"

#include "eyecatcher.h"
#include <api/fabric_itf.h> /* IFabric_server_grouped_factory */
#include <common/logging.h>
#include <exception>

namespace
{
	gsl::not_null<component::IFabric_endpoint_unconnected_server *> get_ep(gsl::not_null<component::IFabric_server_grouped_factory *> f_)
	{
		component::IFabric_endpoint_unconnected_server *ep = nullptr;
		while ( ! ( ep = f_->get_new_endpoint_unconnected() ) ) {}
		return gsl::not_null<component::IFabric_endpoint_unconnected_server *>(ep);
	}
}

server_grouped_connection::server_grouped_connection(component::IFabric_server_grouped_factory &f_)
  : _f(&f_)
  , _ep(get_ep(_f))
  , _cnxn(_f->open_connection(_ep))
  , _comm(_cnxn->allocate_group())
{
}
server_grouped_connection::~server_grouped_connection()
{
  delete _comm;
  try
  {
    _f->close_connection(_cnxn);
  }
  catch ( std::exception &e )
  {
    FLOGM("exception {} {}", e.what(), eyecatcher);
  }
}

component::IFabric_endpoint_connected *server_grouped_connection::allocate_group() const
{
  return cnxn().allocate_group();
}
