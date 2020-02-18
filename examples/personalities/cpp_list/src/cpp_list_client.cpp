#include "cpp_list_client.h"

#include <stdio.h>
#include <sys/mman.h>
#include <string>
#include <sstream>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <common/str_utils.h>
#include <common/dump_utils.h>
#include <common/cycles.h>

namespace cpp_list_personality
{

void * allocate_at(size_t size, const addr_t target_addr)
{
  addr_t addr;
  if(target_addr) {
    addr = round_down_page(target_addr);
    size = round_up_page(size);
  }
  else {
    return nullptr;
  }
  
  void * p = mmap(reinterpret_cast<void *>(addr), /* address hint */
                  size,
                  PROT_READ  | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
                  0,  /* file */
                  0); /* offset */
  
  if(p == reinterpret_cast<void*>(-1))
    throw General_exception("mmap failed in allocate_at");

  assert((addr - reinterpret_cast<addr_t>(p)) < PAGE_SIZE);

  return reinterpret_cast<void*>(addr); /* return precise addr */
}

int free_at(void * p, size_t length)
{
  return munmap(p, length);
}
  
status_t execute_invoke_noargs(Component::IMCAS * i_mcas,
                               const Component::IMCAS::pool_t pool,
                               const std::string& key_name,
                               const std::string& method_name)
{
  FlatBufferBuilder fbb;
  auto method = fbb.CreateString(method_name);
  auto cmd = CreateInvoke(fbb, method);
  auto msg = CreateMessage(fbb, Command_Invoke, cmd.Union());
  fbb.Finish(msg);

  /* invoke */
  status_t rc;
  std::vector<Component::IMCAS::ADO_response> response;
  
  rc = i_mcas->invoke_ado(pool,
                          key_name,
                          fbb.GetBufferPointer(),
                          fbb.GetSize(),
                          0,
                          response);
  PLOG("execute_invoke_noargs: %d", rc);
  assert(rc == S_OK);
  return rc;
}



} // namespace cpp_list_personality






// #if 0
// static void execute(Component::IMCAS * i_mcas, const std::string& command)
// {
//     /* create pool if needed */
//   auto pool = i_mcas->create_pool("myPool",
//                                   POOL_SIZE,
//                                   0, /* flags */
//                                   1000); /* obj count */
//   if(pool == IKVStore::POOL_ERROR)
//     throw General_exception("create_pool failed");


//   const std::string key = "myList"; 
//   execute_create_list<uint64_t>(i_mcas, pool, key);
  
//   for(unsigned i=0;i<5;i++) {
//     execute_insert_list<uint64_t>(i_mcas, pool, key, 222 + i);
//   }

//   execute_invoke_noargs(i_mcas, pool, key, "sort");
  
//   i_mcas->close_pool(pool);
// }
// #endif


// // int main(int argc, char * argv[])
// // {
// //   namespace po = boost::program_options;

// //   try {
// //     po::options_description desc("Options");

// //     desc.add_options()("help", "Show help")
// //       ("server", po::value<std::string>()->default_value("10.0.0.21"), "Server hostname")
// //       ("device", po::value<std::string>()->default_value("mlx5_0"), "Device (e.g. mlnx5_0)")
// //       ("port", po::value<unsigned>()->default_value(11911), "Server port")
// //       ("debug", po::value<unsigned>()->default_value(0), "Debug level")
// //       ("command", po::value<std::string>()->default_value("CREATE LIST<UINT64>"), "Command")
// //       ;

// //     po::variables_map vm;
// //     po::store(po::parse_command_line(argc, argv, desc), vm);

// //     if (vm.count("help") > 0) {
// //       std::cout << desc;
// //       return -1;
// //     }

// //     if (vm.count("server") == 0) {
// //       std::cout << "--server option is required\n";
// //       return -1;
// //     }

// //     if (vm.count("command") == 0) {
// //       std::cout << "--command option is required\n";
// //       return -1;
// //     }


// //     g_options.server = vm["server"].as<std::string>();
// //     g_options.device = vm["device"].as<std::string>();
// //     g_options.port = vm["port"].as<unsigned>();
// //     g_options.debug_level = vm["debug"].as<unsigned>();

// //     auto mcasptr = init(vm["server"].as<std::string>(), vm["port"].as<unsigned>());

// //     // TODO
// //     execute(mcasptr, vm["command"].as<std::string>());
    
// //     mcasptr->release_ref();
// //   }
// //   catch (po::error e) {
// //     printf("bad command line option\n");
// //     return -1;
// //   }
 
// //   return 0;
// // }




// } // namespace cpp_list_personality

// // void do_work(Component::IMCAS* mcas)
// // {
// //   using namespace Component;

// //   const std::string poolname = "pool0";
  
// //   auto pool = mcas->create_pool(poolname,
// //                                 GB(1),
// //                                 0, /* flags */
// //                                 1000); /* obj count */
// //   if(pool == IKVStore::POOL_ERROR)
// //     throw General_exception("create_pool (%s) failed", poolname.c_str());


// //   std::vector<std::string> str_samples;
// //   const unsigned num_strings = 1000000;
// //   for(unsigned i=0;i<num_strings;i++) {
// //     auto s = Common::random_string((rdtsc() % 32) + 8);
// //     str_samples.push_back(s);
// //   }

// //   mcas->erase(pool, "symbol0");

// //   std::string request, response;

// //   unsigned iterations = 10000;

// //   mcas->invoke_ado(pool,
// //                    "symbol0",
// //                    "st-init",
// //                    IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
// //                    response,
// //                    MB(4));

// //   if(true)
// //   {
// //     auto start_time = std::chrono::high_resolution_clock::now();
  
// //     for(unsigned i=0;i<iterations;i++) {
// //       mcas->invoke_ado(pool,
// //                        "symbol0", // key
// //                        str_samples[i],
// //                        IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
// //                        response,
// //                        MB(4));
// //     }
// //     __sync_synchronize();
// //     auto   end_time = std::chrono::high_resolution_clock::now();

// //     auto  secs      = std::chrono::duration<double>(end_time - start_time).count();

// //     double per_sec = double(iterations) / secs;
// //     PINF("Synchronous ADO RTT");
// //     PINF("Time: %.2f sec", secs);
// //     PINF("Rate: %.0f /sec", per_sec);
// //   }


// //   /* -- asynchronous version -- */
// //   if(false)
// //   {
// //     auto start_time = std::chrono::high_resolution_clock::now();
  
// //     for(unsigned i=0;i<iterations;i++) {
// //       mcas->invoke_ado(pool,
// //                        "symbol0",
// //                        str_samples[i], //key
// //                        IMCAS::ADO_FLAG_CREATE_ON_DEMAND | IMCAS::ADO_FLAG_ASYNC,
// //                        response,
// //                        MB(4));
// //     }
// //     __sync_synchronize();
// //     auto end_time = std::chrono::high_resolution_clock::now();

// //     auto secs     = std::chrono::duration<double>(end_time - start_time).count();

// //     auto per_sec = double(iterations) / secs;
// //     PINF("Asynchronous ADO RTT");
// //     PINF("Time: %.2f sec", secs);
// //     PINF("Rate: %.0f /sec", per_sec);
// //   }
  
// //  // virtual status_t invoke_ado(const IKVStore::pool_t pool,
// //  //                              const std::string& key,
// //  //                              const std::vector<uint8_t>& request,
// //  //                              const uint32_t flags,                              
// //  //                              std::vector<uint8_t>& out_response,
// //  //                              const size_t value_size = 0) = 0;

// //   mcas->close_pool(pool);
// // }
