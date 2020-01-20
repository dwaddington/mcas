#include <api/ado_itf.h>
#include <api/components.h>
#include <common/str_utils.h>
#include <common/utils.h>
#include <common/logging.h>
#include <gtest/gtest.h>
#include <nupm/dax_map.h>
#include <nupm/mcas_mod.h>
#include <boost/program_options.hpp>

using namespace Component;
using namespace Common;
using namespace std;

unsigned token = 1099;

int main(int argc, char **argv)
{
  
  namespace po = boost::program_options;

  po::variables_map vm;
  size_t size;
  
  try {
    po::options_description desc("Options");

    desc.add_options()("help", "Show help")
      ("size", po::value<unsigned>()->default_value(4096), "Size of memory in bytes")
      ;

    po::store(po::parse_command_line(argc, argv, desc), vm);

    size = vm["size"].as<unsigned>();
  }
  catch (const po::error &) {
    printf("bad command line option\n");
    return -1;
  }

  vector<nupm::Devdax_manager::config_t> conf;
  nupm::Devdax_manager::config_t c;
  c.path = "/dev/dax0.0";
  c.addr = 0x9000000000;
  c.region_id = 0;
  conf.push_back(c);
  nupm::Devdax_manager ddm(conf, true);
  
  nupm::revoke_memory(token);

  char *addr = reinterpret_cast<char *>(ddm.create_region(1234, 0, size));
  memset(addr, 0xB, size);
  PLOG("touched memory.");

  for(unsigned i=0;i<size;i++) {
    if(addr[i] != 0xB)
      throw General_exception("invalid memory");
  }
  
  status_t rc = nupm::expose_memory(token, addr, size);
  if(rc != S_OK) PLOG("Exposed memory rc=%d", rc);

  PLOG("Press return to continue...");
  getchar();

  PLOG("Starting check for 0xC...");
  for(unsigned i=0;i<size;i++) {
    if(addr[i] != 0xC)
      throw General_exception("invalid memory");
  }

  PMAJOR("Verified OK!");
}
