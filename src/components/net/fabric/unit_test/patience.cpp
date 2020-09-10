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
#include "patience.h"

#include <gtest/gtest.h>
#include <api/fabric_itf.h> /* IFabric, IFabric_client, IFabric_client_grouped */
#include <system_error>

component::IFabric_client *open_connection_patiently(component::IFabric &fabric_, const std::string &fabric_spec_, const std::string ip_address_, std::uint16_t port_)
{
  component::IFabric_client *cnxn = nullptr;
  int try_count = 0;
  while ( ! cnxn )
  {
    try
    {
      cnxn = fabric_.open_client(fabric_spec_, ip_address_, port_);
    }
    catch ( std::system_error &e )
    {
      if ( e.code().value() != ECONNREFUSED )
      {
        throw;
      }
    }
    ++try_count;
  }
  EXPECT_LT(0U, cnxn->max_message_size());
  return cnxn;
}

component::IFabric_client_grouped *open_connection_grouped_patiently(component::IFabric &fabric_, const std::string &fabric_spec_, const std::string ip_address_, std::uint16_t port_)
{
  component::IFabric_client_grouped *cnxn = nullptr;
  int try_count = 0;
  while ( ! cnxn )
  {
    try
    {
      cnxn = fabric_.open_client_grouped(fabric_spec_, ip_address_, port_);
    }
    catch ( std::system_error &e )
    {
      if ( e.code().value() != ECONNREFUSED )
      {
        throw;
      }
    }
    ++try_count;
  }
  EXPECT_LT(0U, cnxn->max_message_size());
  return cnxn;
}
