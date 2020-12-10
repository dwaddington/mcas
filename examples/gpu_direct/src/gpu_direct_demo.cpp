#include <boost/program_options.hpp> 
#include <iostream>
#include <string>
#include <vector>
#include <stdio.h>
#include <linux/cuda.h>
#include <common/exceptions.h>
#include <common/logging.h>
#include <common/dump_utils.h>
#include <api/components.h>
#include <api/mcas_itf.h>

using namespace std;

struct {
  std::string addr;
  std::string device;
  unsigned    debug_level;
  unsigned    patience;
} Options;

extern "C" void run_cuda(Component::IMCAS * mcas);

using namespace component;

Component::IMCAS * create_mcas_instance()
{
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
  return mcas;
}

int main(int argc, char * argv[])
{
  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()("help", "Show help")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level 0-3")
      ("patience", po::value<unsigned>()->default_value(30), "Patience with server (seconds)")
      ("server", po::value<std::string>()->default_value("10.0.0.101:11911:verbs"), "Server address IP:PORT[:PROVIDER]")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Network device (e.g., mlx5_0)")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }

    Options.addr        = vm["server"].as<std::string>();
    Options.debug_level = vm["debug"].as<unsigned>();
    Options.patience    = vm["patience"].as<unsigned>();
    Options.device      = vm["device"].as<std::string>();
  }
  catch (...) {
    std::cerr << "bad command line option configuration\n";
    return -1;
  }

  auto mcas = create_mcas_instance();
  run_cuda(mcas);
  
  return 0;
}

