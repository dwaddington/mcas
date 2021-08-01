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
#define DEBUG_TEST

struct Options
{
  unsigned debug_level;
  unsigned patience;
  std::string server;
  std::string device;
  std::string data;
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
      if (count%1000000== 0) {
	      std::cout << "insert counti " << count  << std::endl;
      }     
      table.add_word(line);
      count++;
      //      if(count == 100) break;
    }
  }
  catch(...) {
    PERR("Reading word file failed");
  }
  std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
  uint64_t insert_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

  std::cout << "Insert " << count << " took: "  << insert_time_ms << " [ms]" << " -> Inserts per sec " << count/insert_time_ms*1000 << std::endl;
  PMAJOR("Loaded %u words", count);
 
  table.build_index();

  uint64_t start_time = 5100* 1000; // 
  uint64_t jump_time = 1000;// jump every X millisec
  uint64_t end_time = 11740*1000; // 
  begin = std::chrono::steady_clock::now();

  uint64_t cnt = 0;
  uint64_t curr_start_time = start_time;
    
  srand(1);


  while (cnt<1000000) {
	  curr_start_time = start_time;
	  int rand_num = rand()%432000;
	  curr_start_time = rand_num*100 + 5100*1000;
	  if (rand_num >  66400) {
		  curr_start_time += 10180 * 1000;
	  }
//	  std::cout << " search curr_start_time " << curr_start_time <<  " curr_start_time + jump_time " << curr_start_time + jump_time  << "  cnt " << cnt << std::endl;
	  std::string send_symbol = "REST.HEAD.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
	  uint64_t num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  send_symbol = "REST.PUT.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
	  num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  send_symbol = "REST.GET.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
	  num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  curr_start_time = curr_start_time + jump_time/10;
          cnt++; 

  }
/*
  start_time = 21920* 1000; // 
  jump_time = 1000;// jump every X millisec
  end_time = 34000*1000; // 
  begin = std::chrono::steady_clock::now();

  curr_start_time = start_time;
  while (curr_start_time < end_time) { 
//	  std::cout << " search curr_start_time " << curr_start_time << " start_time " << start_time <<  " end_time "  << (curr_start_time+jump_time) <<  " jump_time " << jump_time <<  "  cnt " << cnt << std::endl;
	  std::string send_symbol = "REST.HEAD.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
//	  std::cout << send_symbol << std::endl;
	  uint64_t num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  send_symbol = "REST.PUT.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
//	  std::cout << send_symbol << std::endl;
	  num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  send_symbol = "REST.GET.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
//	  std::cout << send_symbol << std::endl;
	  num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  curr_start_time = curr_start_time + jump_time/10;
          cnt++; 

  }


  start_time = 34000* 1000; // 
  jump_time = 1000;// jump every X millisec
  end_time = 58480*1000; // 
  begin = std::chrono::steady_clock::now();

  curr_start_time = start_time;
  while (curr_start_time < end_time) { 
//	  std::cout << " search curr_start_time " << curr_start_time << " start_time " << start_time <<  " end_time "  << end_time <<  " jump_time " << jump_time <<  "  cnt " << cnt << std::endl;
	  std::string send_symbol = "REST.HEAD.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
//	  std::cout << send_symbol << std::endl;
	  uint64_t num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  send_symbol = "REST.PUT.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
//	  std::cout << send_symbol << std::endl;
	  num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  send_symbol = "REST.GET.OBJECT " + boost::lexical_cast<std::string>(curr_start_time) + " " + boost::lexical_cast<std::string> (curr_start_time+jump_time);
//	  std::cout << send_symbol << std::endl;
	  num_res = table.get_symbol(send_symbol);
//	  std::cout << "The number of elements in the range are: " << num_res << std::endl;

	  curr_start_time = curr_start_time + jump_time/10;
          cnt++; 

  }
*/
  end = std::chrono::steady_clock::now();
  std::cout << "finish search "<< std::endl;
  uint64_t search_time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();

  std::cout << "finish calculate time "<< std::endl;

  std::cout << "search " << cnt << "*3=" << cnt*3 << std::endl;
  std::cout  << " took: "  << search_time_ms << " [ms]" << std::endl;
  if (search_time_ms > 0) {
	  std::cout  << " -> Searches per sec " << cnt*3*1000/search_time_ms << std::endl;
  }
  PLOG("Cleaning up.");
  i_mcas->delete_pool(pool);

  return 0;
}
