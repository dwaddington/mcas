#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <common/str_utils.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <common/utils.h>
#include <boost/program_options.hpp>
#include <api/components.h>
#include <api/mcas_itf.h>
#include <ccpm/immutable_list.h>
#include "cpp_list_client.h"

struct Options
{
  unsigned debug_level;
  unsigned patience;
  std::string server;
  std::string device;
  unsigned port;
} g_options;


component::IMCAS * init(const std::string& server_hostname,  int port)
{
  using namespace component;
  
  IBase *comp = component::load_component("libcomponent-mcasclient.so",
                                          mcas_client_factory);

  auto fact = (IMCAS_factory *) comp->query_interface(IMCAS_factory::iid());
  if(!fact)
    throw Logic_exception("unable to create MCAS factory");

  std::stringstream url;
  url << g_options.server << ":" << g_options.port;
  
  IMCAS * mcas = fact->mcas_create(g_options.debug_level, g_options.patience,
                                   "None",
                                   url.str(),
                                   g_options.device);

  if(!mcas)
    throw Logic_exception("unable to create MCAS client instance");

  fact->release_ref();
  return mcas;
}




int main(int argc, char * argv[])
{
  namespace po = boost::program_options;

  component::IMCAS* i_mcas = nullptr;
  try {
    po::options_description desc("Options");

    desc.add_options()("help", "Show help")
      ("server", po::value<std::string>()->default_value("10.0.0.21"), "Server hostname")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Device (e.g. mlnx5_0)")
      ("port", po::value<unsigned>()->default_value(11911), "Server port")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level")
      ("patience", po::value<unsigned>()->default_value(30), "Patience with server (seconds)")
      ("command", po::value<std::string>()->default_value("CREATE LIST<UINT64>"), "Command")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }

    if (vm.count("server") == 0) {
      std::cout << "--server option is required\n";
      return -1;
    }

    if (vm.count("command") == 0) {
      std::cout << "--command option is required\n";
      return -1;
    }


    g_options.server = vm["server"].as<std::string>();
    g_options.device = vm["device"].as<std::string>();
    g_options.port = vm["port"].as<unsigned>();
    g_options.debug_level = vm["debug"].as<unsigned>();
    g_options.patience = vm["patience"].as<unsigned>();

    /* create MCAS session */
    i_mcas = init(vm["server"].as<std::string>(), vm["port"].as<unsigned>());
  }
  catch (po::error &) {
    printf("bad command line option\n");
    return -1;
  }

  PLOG("Initialized OK.");
  
  /* main code */
  auto pool = i_mcas->create_pool("myPool",
                                MB(32),
                                0, /* flags */
                                10); /* obj count */

  {
    using namespace cpp_list_personality;
    
    Durable_list<int> list(i_mcas, pool, "myList", KB(32) /* memory size */);

    /* populate list */
    for(int i=0;i<5;i++)
      list.push_front((rand() % 100) + 10);

    /* print local version out */
    for(auto& i : list)
      PLOG("element: %d", i);

    /* copy to remote MCAS server */
    list.copy_to_remote();

    /* do some remote insertions */
    for(int i=0;i<5;i++)
      list.remote_push_front(i);

    /* request sorting of remote list */
    list.remote_sort();

    /* pull back the list to the local space */
    list.copy_from_remote();

    /* print local version out */
    for(auto& i : list)
      PLOG("element: %d", i);
  }

  
  /* implicitly close and delete pool */
  PLOG("Cleaning up.");
  i_mcas->delete_pool(pool);


  return 0;
}
