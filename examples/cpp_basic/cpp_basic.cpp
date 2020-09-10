/*
   Copyright [2019] [IBM Corporation]
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

#include <iostream>
#include <unistd.h>
#include <boost/program_options.hpp>

#include <common/exceptions.h>
#include <common/str_utils.h>
#include <api/components.h>
#include <api/mcas_itf.h>

struct {
  std::string addr;
  std::string device;
  unsigned    debug_level;
  unsigned    patience;
} Options;


int main(int argc, char* argv[])
{
  using namespace component;

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()("help", "Show help")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level 0-3")
      ("patiencd", po::value<unsigned>()->default_value(30), "Patience with server (seconds)")
      ("server-addr", po::value<std::string>()->default_value("10.0.0.101:11911:verbs"), "Server address IP:PORT[:PROVIDER]")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Network device (e.g., mlx5_0)")
      ;
    
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }

    Options.addr        = vm["server-addr"].as<std::string>();
    Options.debug_level = vm["debug"].as<unsigned>();
    Options.patience    = vm["patience"].as<unsigned>();
    Options.device      = vm["device"].as<std::string>();
  }
  catch (...) {
    std::cerr << "bad command line option configuration\n";
    return -1;
  }

  
  /* load component and create factory */
  IBase *comp = load_component("libcomponent-mcasclient.so", mcas_client_factory);
  auto factory = static_cast<IMCAS_factory *>(comp->query_interface(IMCAS_factory::iid()));
  assert(factory);
  
  /* create instance of MCAS client session */
  auto mcas = factory->mcas_create(1 /* debug level, 0=off */,
                                   Options.patience,
                                   getlogin(),
                                   Options.addr, /* MCAS server endpoint */
                                   Options.device); /* see mcas_client.h */

  factory->release_ref();
  assert(mcas);

  /* open existing pool or create one */
  const std::string poolname = "myBasicPool";
  auto pool = mcas->open_pool(poolname, 0);

  if (pool == IKVStore::POOL_ERROR) {
    /* ok, try to create pool instead */
    pool = mcas->create_pool(poolname, MB(32));
  }
  assert(pool != IKVStore::POOL_ERROR);

  auto key = common::random_string(8);
  std::string value = "This is my value " + common::random_string(8);
  
  /* add new item to pool */
  if(mcas->put(pool,
               key,
               value) != S_OK)
    throw General_exception("put failed unexpectedly.");
  std::cout << "Put " << key << " [" << value << "]\n";
  
  /* get the item back */
  std::string retrieved_value;
  if(mcas->get(pool, key, retrieved_value) != S_OK)
    throw General_exception("get failed unexpectedly.");

  std::cout << "Retrieved [" << retrieved_value << "]\n";
  
  /* close pool */
  mcas->close_pool(pool);

  /* cleanup */
  mcas->release_ref();
  return 0;
}

