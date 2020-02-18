#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
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
#include "cpp_symtab_client.h"

struct Options
{
  unsigned debug_level;
  std::string server;
  std::string device;
  std::string data;
  unsigned port;
} g_options;


Component::IMCAS * init(const std::string& server_hostname,  int port)
{
  using namespace Component;
  
  IBase *comp = Component::load_component("libcomponent-mcasclient.so",
                                          mcas_client_factory);

  auto fact = (IMCAS_factory *) comp->query_interface(IMCAS_factory::iid());
  if(!fact)
    throw Logic_exception("unable to create MCAS factory");

  std::stringstream url;
  url << g_options.server << ":" << g_options.port;
  
  IMCAS * mcas = fact->mcas_create(g_options.debug_level,
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

  Component::IMCAS* i_mcas = nullptr;
  try {
    po::options_description desc("Options");

    desc.add_options()("help", "Show help")
      ("server", po::value<std::string>()->default_value("10.0.0.21"), "Server hostname")
      ("data", po::value<std::string>(), "Words data file")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Device (e.g. mlnx5_0)")
      ("port", po::value<unsigned>()->default_value(11911), "Server port")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level")
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

    if (vm.count("data") == 0) {
      std::cout << "--data option is required\n";
      return -1;
    }


    g_options.server = vm["server"].as<std::string>();
    g_options.device = vm["device"].as<std::string>();
    g_options.data = vm["data"].as<std::string>();
    g_options.port = vm["port"].as<unsigned>();
    g_options.debug_level = vm["debug"].as<unsigned>();   

    /* create MCAS session */
    i_mcas = init(vm["server"].as<std::string>(), vm["port"].as<unsigned>());
  }
  catch (po::error &) {
    printf("bad command line option\n");
    return -1;
  }

  PLOG("Initialized OK.");
  
  /* main code */
  auto pool = i_mcas->create_pool("Dictionaries",
                                  MB(32),
                                  0, /* flags */
                                  1000); /* obj count */
  
  cpp_symtab_personality::Symbol_table table(i_mcas, pool, "us-english");
  
  /* open data file */
  unsigned count = 0;
  try {
    std::ifstream ifs(g_options.data);

    std::string line;
    while(getline(ifs, line)) {
      table.add_word(line);
      count++;
      //      if(count == 100) break;
    }
  }
  catch(...) {
    PERR("Reading word file failed");
  }
  PMAJOR("Loaded %u words", count);

  table.build_index();

  auto sym = table.get_symbol("business");
  PLOG("Symbol for business:%lx", sym);

  auto reverse_lookup = table.get_string(sym);
  PLOG("Reverse lookup: %s", reverse_lookup.c_str());
  
  /* implicitly close and delete pool */
  PLOG("Cleaning up.");
  i_mcas->delete_pool(pool);


  return 0;
}

