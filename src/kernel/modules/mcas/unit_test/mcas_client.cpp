#include <api/ado_itf.h>
#include <api/components.h>
#include <chrono>
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
    PLOG("size: %lu", size);
  }
  catch (const po::error &) {
    printf("bad command line option\n");
    return -1;
  }

  size_t mapped_size = 0;
  char *addr = static_cast<char *>(nupm::mmap_exposed_memory(token, mapped_size, reinterpret_cast<void *>(0x900000000)));
  PLOG("mapped size: %lu", mapped_size);
  assert(addr);

  PLOG("mapped exposed memory OK (addr=%p)", addr);
  
  PLOG("verifying memory: %p, size: %zu", addr, size);

  for(unsigned i=0;i<size;i++) {
    if(addr[i] != 0xb) {
      PLOG("addr[%u]: 0x%x", i, addr[i]);         
      throw General_exception("invalid memory");
    }
  }

  PLOG("memory verified OK.");

  for(unsigned i=0;i<size;i++) {
    addr[i] ++;
  }

  PMAJOR("Done!");
}
