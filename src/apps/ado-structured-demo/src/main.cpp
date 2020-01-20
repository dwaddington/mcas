#include <stdio.h>
#include <string>
#include <sstream>
#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <common/str_utils.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <boost/program_options.hpp>
#include <api/components.h>
#include <api/mcas_itf.h>
#include <flatbuffers/flatbuffers.h>
#include <structured_proto_generated.h>
#include <ccpm/immutable_list.h>
#include <nop/utility/stream_writer.h>

using namespace flatbuffers;
using namespace Component;


struct Options
{
  unsigned debug_level;
  std::string server;
  std::string device;
  unsigned port;
} g_options;

Component::IMCAS* init(const std::string& server_hostname,  int port);

using namespace Structured_ADO_protocol;

#define VALUE_SIZE KB(32)
#define POOL_SIZE MB(32) // 4 or 5 will fail

#include <common/utils.h>
#include <sys/mman.h>

static void * allocate_at(size_t size, const addr_t hint = 0)
{
  static addr_t base = 0x800000000;

  addr_t hint_addr;
  if(hint) {
    hint_addr = round_down_page(hint);
    size += PAGE_SIZE; //round_up_page(size);
  }
  else {
    hint_addr = base;
    base = round_up_page(size + base);   
  }
  
  void * p = mmap(reinterpret_cast<void *>(hint_addr), /* address hint */
                  size,
                  PROT_READ  | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS,
                  0,  /* file */
                  0); /* offset */
  
  if(p == reinterpret_cast<void*>(-1))
    throw General_exception("mmap failed in allocate_at");

  if(hint) {
    assert((hint - reinterpret_cast<addr_t>(p)) < PAGE_SIZE);
    return reinterpret_cast<void*>(hint); /* return precise hint */
  }
  else return p;
}

template <class T>
void execute_create_list(Component::IMCAS * i_mcas,
                         const Component::IMCAS::pool_t pool,
                         const std::string& name)
{
  using namespace Component;

  status_t rc;
  std::string response;
  
  /* delete anything prior */
  i_mcas->erase(pool, name);

  size_t size = MB(1);
  addr_t value_vaddr = 0;

  /* create element for data structure */
  rc = i_mcas->invoke_ado(pool,
                          name,
                          nullptr,
                          0,
                          IMCAS::ADO_FLAG_CREATE_ONLY,
                          response,
                          size);
  assert(rc == S_OK);
  /* get virtual address from response */
  value_vaddr = *(reinterpret_cast<const uint64_t*>(response.data()));
  PLOG("returned address: %lx", value_vaddr);

  /* allocate matching memory locally and populate if needed */
  void * ptr = allocate_at(size, value_vaddr);
  memset(ptr, 0, size);

  ccpm::region_vector_t regions{ptr, size};
  ccpm::Immutable_list<T> myList(regions, true);

  unsigned count = 1000;
  for(unsigned i=0;i<count;i++)
    myList.push_front((rdtsc() * i) % 10000); /* add something to list */
  
  /* push data structure into mcas */
  /* If small, use put */
  //  rc = i_mcas->put(pool, name, regions[0].iov_base, regions[0].iov_len);
  /* If large and we want to pay the cost of registering, we can use put_direct */
  auto handle = i_mcas->register_direct_memory(regions[0].iov_base, regions[0].iov_len);
  rc = i_mcas->put_direct(pool, name, regions[0].iov_base, regions[0].iov_len, handle);
  assert(rc == S_OK);
  
  PLOG("create invocation response: %d", rc);

}

template <typename T>
void execute_insert_list(Component::IMCAS * i_mcas,
                         const Component::IMCAS::pool_t pool,
                         const std::string& name,
                         const T element)
{
  using Writer = nop::StreamWriter<std::stringstream>;
  nop::Serializer<Writer> serializer;
  serializer.Write(element);

  FlatBufferBuilder fbb;
  auto params = fbb.CreateString(serializer.writer().take().str());
  auto method = fbb.CreateString("push_front");  
  auto cmd = CreateInvoke(fbb, method, params);
  auto msg = CreateMessage(fbb, Command_Invoke, cmd.Union());
  fbb.Finish(msg);

  /* invoke */
  status_t rc;
  std::string response;
  rc = i_mcas->invoke_ado(pool,
                          name,
                          fbb.GetBufferPointer(),
                          fbb.GetSize(),
                          0,
                          response);
  PLOG("execute_insert_list invoke response: %d (%s)", rc, response.c_str());
}

void execute_invoke_noargs(Component::IMCAS * i_mcas,
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
  std::string response;
  rc = i_mcas->invoke_ado(pool,
                          key_name,
                          fbb.GetBufferPointer(),
                          fbb.GetSize(),
                          0,
                          response);
  PLOG("execute_invoke_noargs: %d", rc);
  assert(rc == S_OK);
}


void execute(Component::IMCAS * i_mcas, const std::string& command)
{
    /* create pool if needed */
  auto pool = i_mcas->create_pool("myPool",
                                  POOL_SIZE,
                                  0, /* flags */
                                  1000); /* obj count */
  if(pool == IKVStore::POOL_ERROR)
    throw General_exception("create_pool failed");


  const std::string key = "myList"; 
  execute_create_list<uint64_t>(i_mcas, pool, key);
  
  for(unsigned i=0;i<5;i++) {
    execute_insert_list<uint64_t>(i_mcas, pool, key, 222 + i);
  }

  execute_invoke_noargs(i_mcas, pool, key, "sort");
  
  i_mcas->close_pool(pool);
}


int main(int argc, char * argv[])
{
  namespace po = boost::program_options;

  try {
    po::options_description desc("Options");

    desc.add_options()("help", "Show help")
      ("server", po::value<std::string>()->default_value("10.0.0.21"), "Server hostname")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Device (e.g. mlnx5_0)")
      ("port", po::value<unsigned>()->default_value(11911), "Server port")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level")
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

    auto mcasptr = init(vm["server"].as<std::string>(), vm["port"].as<unsigned>());

    // TODO
    execute(mcasptr, vm["command"].as<std::string>());
    
    mcasptr->release_ref();
  }
  catch (po::error e) {
    printf("bad command line option\n");
    return -1;
  }
 
  return 0;
}


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

// void do_work(Component::IMCAS* mcas)
// {
//   using namespace Component;

//   const std::string poolname = "pool0";
  
//   auto pool = mcas->create_pool(poolname,
//                                 GB(1),
//                                 0, /* flags */
//                                 1000); /* obj count */
//   if(pool == IKVStore::POOL_ERROR)
//     throw General_exception("create_pool (%s) failed", poolname.c_str());


//   std::vector<std::string> str_samples;
//   const unsigned num_strings = 1000000;
//   for(unsigned i=0;i<num_strings;i++) {
//     auto s = Common::random_string((rdtsc() % 32) + 8);
//     str_samples.push_back(s);
//   }

//   mcas->erase(pool, "symbol0");

//   std::string request, response;

//   unsigned iterations = 10000;

//   mcas->invoke_ado(pool,
//                    "symbol0",
//                    "st-init",
//                    IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
//                    response,
//                    MB(4));

//   if(true)
//   {
//     auto start_time = std::chrono::high_resolution_clock::now();
  
//     for(unsigned i=0;i<iterations;i++) {
//       mcas->invoke_ado(pool,
//                        "symbol0", // key
//                        str_samples[i],
//                        IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
//                        response,
//                        MB(4));
//     }
//     __sync_synchronize();
//     auto   end_time = std::chrono::high_resolution_clock::now();

//     auto  secs      = std::chrono::duration<double>(end_time - start_time).count();

//     double per_sec = double(iterations) / secs;
//     PINF("Synchronous ADO RTT");
//     PINF("Time: %.2f sec", secs);
//     PINF("Rate: %.0f /sec", per_sec);
//   }


//   /* -- asynchronous version -- */
//   if(false)
//   {
//     auto start_time = std::chrono::high_resolution_clock::now();
  
//     for(unsigned i=0;i<iterations;i++) {
//       mcas->invoke_ado(pool,
//                        "symbol0",
//                        str_samples[i], //key
//                        IMCAS::ADO_FLAG_CREATE_ON_DEMAND | IMCAS::ADO_FLAG_ASYNC,
//                        response,
//                        MB(4));
//     }
//     __sync_synchronize();
//     auto end_time = std::chrono::high_resolution_clock::now();

//     auto secs     = std::chrono::duration<double>(end_time - start_time).count();

//     auto per_sec = double(iterations) / secs;
//     PINF("Asynchronous ADO RTT");
//     PINF("Time: %.2f sec", secs);
//     PINF("Rate: %.0f /sec", per_sec);
//   }
  
//  // virtual status_t invoke_ado(const IKVStore::pool_t pool,
//  //                              const std::string& key,
//  //                              const std::vector<uint8_t>& request,
//  //                              const uint32_t flags,                              
//  //                              std::vector<uint8_t>& out_response,
//  //                              const size_t value_size = 0) = 0;

//   mcas->close_pool(pool);
// }
