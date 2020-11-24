#include <unistd.h>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <boost/program_options.hpp>
#include <common/str_utils.h>
#include <common/utils.h> /* MiB */
#include "example_versioning_client.h"

struct Options
{
  unsigned debug_level;
  unsigned patience;
  std::string server;
  std::string device;
  unsigned port;
} g_options;


int main(int argc, char* argv[])
{
  namespace po = boost::program_options;

  try {
    po::options_description desc("Options");
    
    desc.add_options()("help", "Show help")
      ("server", po::value<std::string>()->default_value("10.0.0.101"), "Server hostname")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Device (e.g. mlnx5_0)")
      ("port", po::value<unsigned>()->default_value(11911), "Server port")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    
    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }
    
    g_options.server = vm["server"].as<std::string>();
    g_options.device = vm["device"].as<std::string>();
    g_options.port = vm["port"].as<unsigned>();
  }
  catch (po::error &) {
    printf("bad command line option\n");
    return -1;
  }


  std::stringstream url;
  url << g_options.server << ":" << g_options.port;

  /* main line */
  using namespace example_versioning;
  
  Client m(0,120,url.str(),g_options.device);

  auto pool = m.create_pool("myPets", MiB(128));

  m.put(pool, "dog", "Violet");
  m.put(pool, "dog", "MadameFluffFace");
  m.put(pool, "cat", "Jasmine");
  m.put(pool, "kitten", "Jenny");

  PINF("Naming chicken Nugget..");
  m.put(pool, "chicken", "Nugget");

  PINF("Re-naming chicken Bob..");
  m.put(pool, "chicken", "Bob");

  PINF("Re-naming chicken Rosemary..");
  m.put(pool, "chicken", "Rosemary");

  PINF("Re-naming chicken Ferdie..");
  m.put(pool, "chicken", "Ferdie");

  PINF("Re-naming chicken Zucchini..");
  m.put(pool, "chicken", "Zucchini");


  std::string chicken_name;
  m.get(pool, "chicken", 0, chicken_name);
  PLOG("Current chicken 0: %s", chicken_name.c_str());

  m.get(pool, "chicken", -1, chicken_name);
  PLOG("Previous chicken -1: %s", chicken_name.c_str());

  m.get(pool, "chicken", -2, chicken_name);
  PLOG("Previous-previous chicken -2: %s", chicken_name.c_str());

  m.close_pool(pool);
  m.delete_pool("myPets");
  
  return 0;
}
