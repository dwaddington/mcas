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
#include "cpp_rangeindex_client.h"
#include <chrono>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#define DEBUG_TEST
#define SEARCH_PRS 10 
#define SEARCH_MAX 10000000 

using namespace boost;

struct Options
{
  unsigned debug_level;
  unsigned patience;
  std::string server;
  std::string device;
  std::string data;
  unsigned port;
} g_options;

std::string search_array[SEARCH_MAX] ;
std::string range_array[SEARCH_MAX] ;
size_t search_cnt = 0;
size_t range_cnt = 0;



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
      ("data", po::value<std::string>(), "Words data file")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Device (e.g. mlnx5_0)")
      ("port", po::value<unsigned>()->default_value(11911), "Server port")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level")
      ("patience", po::value<unsigned>()->default_value(30), "Patience with server (seconds)")
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
  auto pool = i_mcas->create_pool("Dictionaries",
                                  MB(10000),
                                  0, /* flags */
                                  1000); /* obj count */
  
  cpp_rangeindex_personality::Symbol_table table(i_mcas, pool, "us-english");
  
  /* open data file */
  unsigned count = 0;
  std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
  try {
    std::ifstream ifs(g_options.data);

    std::string line;
    while(getline(ifs, line)) {
      if ((count%1000000==0) & (count>0)) {
	      std::cout << "insert count " << count  << std::endl;
    }    
       
      table.add_row(line);

      if (count%(SEARCH_PRS/2) == 0){
      // save the line for search 
	      if (count%SEARCH_PRS == 0) {
		     search_array[search_cnt] = line;
		     search_cnt++; 
	      }
	      else {
      // save the line for range 
		     range_array[range_cnt] = line; 
		     range_cnt++; 
	      }
      }
      


      count++;
      //      if(count == 100) break;
    }
  }
  catch(...) {
    PERR("Reading row file failed");
  }
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  uint64_t insert_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
  uint64_t trput = (uint64_t) (count * 1000.0 /insert_time_ms);
  std::cout << "Insert " << count << " took: "  << insert_time_ms << " [ms]" << " -> Inserts per sec " << trput << std::endl;
  PMAJOR("Loaded %u rows", count);
 
  table.build_index();
  

  //// SEARCH  
  begin = std::chrono::steady_clock::now();
  size_t cnt = 0;
  size_t res = 0;
  std::string is_range = "0";
  for (size_t i=0; i <  search_cnt; i++) {
	  cnt++;
	  std::vector<std::string> fields;
	  boost::split( fields, search_array[i], is_any_of(" ") );
	  std::string send_symbol = fields[2] +  " " + fields[0] + " " + is_range;
	  size_t num_res = table.get_symbol(send_symbol);
	  res += num_res;

  }
  end = std::chrono::steady_clock::now();
  std::cout << "finish search "<< std::endl;
  uint64_t search_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

  std::cout << "search " << cnt << "= exact " << res << std::endl;
  std::cout  << "Search " << cnt << " items took: "  << search_time_ms << " [ms]" << std::endl;
  if (search_time_ms > 0) {
	  std::cout  << " -> Searches per sec " << cnt*1000/search_time_ms << std::endl;
  }


/// SCAN
  cnt = 0;
  res = 0;
  is_range = "1";
  begin = std::chrono::steady_clock::now();
  for (size_t i=0; i <  range_cnt; i++) {
	  cnt++;
	  std::vector<std::string> fields;
	  boost::split( fields, range_array[i], is_any_of(" ") );
	  std::string send_symbol = fields[2] +  " " + fields[0] + " " + is_range;
	  size_t num_res = table.get_symbol(send_symbol);
	  res += num_res;

  }
  end = std::chrono::steady_clock::now();
  std::cout << "finish SCAN "<< std::endl;
  uint64_t scan_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

  std::cout << "sum " <<  res << std::endl;
  std::cout  << "Scan " << cnt << " items took: "  << scan_time_ms << " [ms]" << std::endl;
  if (scan_time_ms > 0) {
	  std::cout  << " -> Searches per sec " << cnt*1000/scan_time_ms << std::endl;
  }



  PLOG("Cleaning up.");
  i_mcas->delete_pool(pool);

  return 0;
}
