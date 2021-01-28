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
#include "mcas_client.h"

#include <regex>

component::IMCAS * MCAS_client_factory::mcas_create(const unsigned debug_level,
                                                    const unsigned patience,
                                                    const std::string &,  // owner
                                                    const boost::optional<std::string> & src_device,
                                                    const boost::optional<std::string> & src_addr,
                                                    const std::string & dest_addr_port_str,
                                                    const std::string other)
{
  try {
    std::smatch m;

    try {
      /*
       * The mcas server uses JSON to pass complex structures. The mcas client
       * uses, well, not JSON
       *
       * e.g. 10.0.0.21:11911 (verbs)
       * 9.1.75.6:11911:sockets (sockets)
       */
      std::regex r("([[:digit:]]+[.][[:digit:]]+[.][[:digit:]]+[.][[:digit:]]+)[:]([[:"
                   "digit:]]+)(?:[:]([[:alnum:]]+))?");
      std::regex_search(dest_addr_port_str, m, r);
    }
    catch (...) {
      throw API_exception("invalid parameter");
    }

    const std::string                  dst_addr = m[1].str();
    char *                             end;
    const auto                         port = std::uint16_t(strtoul(m[2].str().c_str(), &end, 10));
    const boost::optional<std::string> provider = m[3].matched ?
      boost::optional<std::string>(m[3].str()) : boost::optional<std::string>();

    component::IMCAS *obj =
      static_cast<component::IMCAS *>(new MCAS_client(debug_level,
                                                      src_device,
                                                      src_addr,
                                                      provider,
                                                      dst_addr,
                                                      port,
                                                      patience,  // seconds to wait for single fabric completion
                                                      other
                                                      ));
    obj->add_ref();    
    return obj;
  }
  catch (const std::exception &e) {
    PLOG("libcomponent-mcasclient.so: failed to build IMCAS (mcas client): %s", e.what());
    /* callers expect nullptr to be the sole indication of failure */
    return nullptr;
  }
}

component::IKVStore *MCAS_client_factory::create(unsigned debug_level,
                                                 const std::string &,  // owner
                                                 const std::string &addr,
                                                 const std::string &device)
{
  std::smatch m;

  try {
    /*
     * The mcas server uses JSON to pass complex structures. The mcas client
     * uses, well, not JSON
     *
     * e.g. 10.0.0.21:11911 (verbs)
     * 9.1.75.6:11911:sockets (sockets)
     */
    std::regex r("([[:digit:]]+[.][[:digit:]]+[.][[:digit:]]+[.][[:digit:]]+)[:]([[:"
                 "digit:]]+)(?:[:]([[:alnum:]]+))?");
    std::regex_search(addr, m, r);
  }
  catch (...) {
    throw API_exception("invalid parameter");
  }

  const std::string    ip_addr = m[1].str();
  char *               end;
  const auto           port     = uint16_t(strtoul(m[2].str().c_str(), &end, 10));
  const std::string    provider = m[3].matched ? m[3].str() : "verbs"; /* default provider */
  component::IKVStore *obj      = static_cast<component::IKVStore *>(new MCAS_client(debug_level,
                                                                                     device,
                                                                                     boost::optional<std::string>(),
                                                                                     provider,
                                                                                     ip_addr,
                                                                                     port,
                                                                                     60));
  /* at least one caller (kvstore-perf) expects a valid pointer or an exception
   * (and not a null pointer). */
  obj->add_ref();
  return obj;
}

component::IKVStore *MCAS_client_factory::create(unsigned debug_level, const IKVStore_factory::map_create &p)
{
  using opt_str          = boost::optional<std::string>;
  auto src_addr_it       = p.find(k_src_addr);
  auto src_addr          = opt_str(src_addr_it == p.end() ? opt_str() : opt_str(src_addr_it->second));
  auto src_nic_device_it = p.find(k_interface);
  auto src_nic_device    = opt_str(src_nic_device_it == p.end() ? opt_str() : opt_str(src_nic_device_it->second));
  auto provider_it       = p.find(k_provider);
  auto provider          = opt_str(provider_it == p.end() ? opt_str() : opt_str(provider_it->second));

  auto dest_addr_it = p.find(k_dest_addr);
  if (dest_addr_it == p.end()) {
    throw std::domain_error("'MCAS_client_factory' create missing 'dest_addr' element");
  }
  auto dest_port_it = p.find(k_dest_port);
  if (dest_port_it == p.end()) {
    throw std::domain_error("'MCAS_client_factory' create missing 'dest_port' element");
  }

  const std::uint16_t port = std::uint16_t(std::stoul(dest_port_it->second));

  auto           patience_it = p.find(k_patience);
  const unsigned patience    = patience_it == p.end() ? 120 : unsigned(std::stoul(patience_it->second));

  component::IKVStore *obj = static_cast<component::IKVStore *>(new MCAS_client(debug_level,
                                                                                src_nic_device,
                                                                                src_addr,
                                                                                provider,
                                                                                dest_addr_it->second,
                                                                                port,
                                                                                patience));
  
  /* at least one caller (kvstore-perf) expects a valid pointer or an exception
   * (and not a null pointer). */
  obj->add_ref();
  return obj;
}
