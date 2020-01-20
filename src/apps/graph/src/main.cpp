#include <stdio.h>
#include <string>
#include <sstream>
#include <chrono>
#include <iostream>
#include <fstream>
#include <common/str_utils.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <boost/program_options.hpp>
#include <api/components.h>
#include <api/mcas_itf.h>
#include <graph_proto_generated.h>

using namespace Graph_ADO_protocol;
using namespace flatbuffers;
using namespace Component;


struct Options
{
  unsigned debug_level;
  std::string server;
  std::string device;
  std::string data_dir;
  unsigned port;
} g_options;

Component::IMCAS* init(const std::string& server_hostname,  int port);
void do_work(Component::IMCAS* mcas);
void do_load(Component::IMCAS* mcas);


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
      ("datadir", po::value<std::string>(), "Location of graph data")
      ("action", po::value<std::string>()->default_value("load"))
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

    if (vm.count("datadir") == 0) {
      std::cout << "--datadir option is required\n";
      return -1;
    }


    g_options.server = vm["server"].as<std::string>();
    g_options.device = vm["device"].as<std::string>();
    g_options.port = vm["port"].as<unsigned>();
    g_options.debug_level = vm["debug"].as<unsigned>();
    g_options.data_dir = vm["datadir"].as<std::string>();

    auto mcasptr = init(vm["server"].as<std::string>(), vm["port"].as<unsigned>());

    if(vm["action"].as<std::string>() == "load") {
      do_load(mcasptr);
    }
    //    do_work(mcasptr);
    mcasptr->release_ref();
  }
  catch (po::error e) {
    printf("bad command line option\n");
    return -1;
  }
 
  return 0;
}

void load_transactions(Component::IMCAS* mcas, Component::IKVStore::pool_t pool)
{
  std::ifstream ifs(g_options.data_dir.c_str() + std::string("nodes.transactions.client-sourcing.csv"));
  if(!ifs.is_open())
    throw General_exception("unable to open nodes.clients.csv");

  FlatBufferBuilder fbb;
  std::string line;
  unsigned count = 0;
  
  while(getline(ifs, line)) {
    fbb.Clear();
    std::stringstream ss(line);
    std::string id, source, target, date, time, amount, currency;
    
    getline(ss, id, '|');
    getline(ss, source, '|');
    getline(ss, target, '|');
    getline(ss, date, '|');
    getline(ss, time, '|');
    getline(ss, amount, '|');
    getline(ss, currency, '|');


    auto record = CreateTransactionDirect(fbb,
                                          source.c_str(),
                                          target.c_str(),
                                          date.c_str(),
                                          time.c_str(),
                                          std::stof(amount),
                                          currency.c_str());

    auto msg = CreateMessage(fbb, Element_Transaction, record.Union());
    fbb.Finish(msg);

    //    hexdump(fbb.GetBufferPointer(), fbb.GetSize());
    
    std::string response;
    status_t rc = mcas->invoke_ado(pool,
                                   "transaction",
                                   fbb.GetBufferPointer(),
                                   fbb.GetSize(),
                                   IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                                   response,
                                   512);

    if(rc != S_OK)
      throw General_exception("failed to put transaction (%d)", rc);

    count++;
  }
  PINF("** Loaded transactions OK! (pool count=%lu)", mcas->count(pool));;
  
}

void load_clients(Component::IMCAS* mcas, Component::IKVStore::pool_t pool)
{
  std::ifstream ifs(g_options.data_dir.c_str() + std::string("nodes.clients.csv"));
  if(!ifs.is_open())
    throw General_exception("unable to open nodes.clients.csv");

  // skip header
  std::string line;
  getline(ifs, line);
  
  FlatBufferBuilder fbb;
  while(getline(ifs, line)) {
    fbb.Clear();
    std::stringstream ss(line);
    std::string field[14];
    for(unsigned i=0;i<14;i++)
      getline(ss, field[i], '|');

    auto record = CreateClientRecordDirect(fbb,
                                           field[1].c_str(),
                                           field[2].c_str(),
                                           std::stoi(field[3]),
                                           field[4].c_str(),
                                           field[5].c_str(),
                                           field[6].c_str(),
                                           field[7].c_str(),
                                           field[8].c_str(),
                                           field[9].c_str(),
                                           field[10].c_str(),
                                           field[11].c_str(),
                                           field[12].c_str(),
                                           field[13].c_str());

    auto msg = CreateMessage(fbb, Element_ClientRecord, record.Union());
    fbb.Finish(msg);

    //    std::string id = "records.clients." + field[0];
    std::string id = field[0];
    status_t rc = mcas->put(pool,
                            id,
                            fbb.GetBufferPointer(),
                            fbb.GetSize());
    if(rc != S_OK)
      throw General_exception("failed to put");      
  }
  PINF("** Loaded Clients OK! (count=%lu)", mcas->count(pool));;
}


void load_atms(Component::IMCAS* mcas, Component::IKVStore::pool_t pool)
{
  std::ifstream ifs(g_options.data_dir.c_str() + std::string("nodes.atms.csv"));
  if(!ifs.is_open())
    throw General_exception("unable to open nodes.atms.csv");
  // skip header
  std::string line;
  getline(ifs, line);
  
  FlatBufferBuilder fbb;
  while(getline(ifs, line)) {
    fbb.Clear();
    std::stringstream ss(line);
    std::string id, longitude, latitude;
    getline(ss, id, '|');
    getline(ss, longitude, '|');
    getline(ss, latitude, '|');

    auto record = CreateAtmRecord(fbb,
                                  std::stof(longitude),
                                  std::stof(latitude));
    
    auto msg = CreateMessage(fbb, Element_AtmRecord, record.Union());
    fbb.Finish(msg);

    //    id = "records.atms." + id;
    status_t rc = mcas->put(pool,
                            id,
                            fbb.GetBufferPointer(),
                            fbb.GetSize());
    if(rc != S_OK)
      throw General_exception("failed to put");      
  }
  PINF("** Loaded ATMs OK! (count=%lu)", mcas->count(pool));;
}

void load_companies(Component::IMCAS* mcas, Component::IKVStore::pool_t pool)
{
  // load nodes.companies.csv
  std::ifstream ifs(g_options.data_dir.c_str() + std::string("nodes.companies.csv"));
  if(!ifs.is_open())
    throw General_exception("unable to open nodes.companies.csv");

  // skip line
  std::string line;
  std::getline(ifs, line);
  // {

  //   FlatBufferBuilder fbb;
        
  //   std::string line,field;
  //   std::getline(ifs, line);

  //   std::vector<flatbuffers::Offset<Field>> fvector;
  //   std::stringstream ss(line);
  //   while(getline(ss, field, '|'))
  //     fvector.push_back(CreateFieldDirect(fbb, field.c_str(), TypeId_STRING));

  //   auto ffvector = fbb.CreateVector(fvector);
  //   auto name = fbb.CreateString("nodes.companies");

  //   PropertyMapSchemaBuilder schema(fbb);
  //   schema.add_name(name);
  //   schema.add_fields(ffvector);

  //   auto msg = CreateMessage(fbb, Element_PropertyMapSchema, schema.Union());
  //   fbb.Finish(msg);

  //   //    hexdump(fbb.GetBufferPointer(), fbb.GetSize());

  //   std::string response;
  //   status_t rc = mcas->put(pool,
  //                           "nodes.companies.schema",
  //                           fbb.GetBufferPointer(),
  //                           fbb.GetSize());
  //   if(rc != S_OK)
  //     throw General_exception("failed to put");
  // }
  {
    std::string line;
    FlatBufferBuilder fbb;
    while(getline(ifs, line)) {
      fbb.Clear();
      std::stringstream ss(line);
      std::string id,type,name,country;
      getline(ss, id, '|');
      getline(ss, type, '|');
      getline(ss, name, '|');
      getline(ss, country, '|');     
      auto record = CreateCompanyRecordDirect(fbb,
                                              type.c_str(),
                                              name.c_str(),
                                              country.c_str());

      auto msg = CreateMessage(fbb, Element_CompanyRecord, record.Union());
      fbb.Finish(msg);

      //      id = "records.company." + id;
      status_t rc = mcas->put(pool,
                              id,
                              fbb.GetBufferPointer(),
                              fbb.GetSize());
      if(rc != S_OK)
        throw General_exception("failed to put");      
    }
  }
  PINF("** Loaded companies OK! (count=%lu)", mcas->count(pool));;
}

void do_load(Component::IMCAS* mcas)
{
  PLOG("Loading data from: (%s)", g_options.data_dir.c_str());

  const std::string poolname = "pool0";  
  auto pool = mcas->create_pool(poolname,
                                GB(1),
                                0, /* flags */
                                1000000); /* obj count */
  if(pool == Component::IKVStore::POOL_ERROR)
    throw General_exception("create_pool (%s) failed", poolname.c_str());

  mcas->configure_pool(pool, "AddIndex::VolatileTree");

  load_companies(mcas, pool);
  load_atms(mcas, pool);
  load_clients(mcas, pool);
  load_transactions(mcas, pool);
  
#ifdef CHECK
  offset_t matched;
  std::string matched_key;
  status_t rc = mcas->find(pool, "regex:records.*", 0,
                           matched, matched_key);
  PLOG("found key:%s", matched_key.c_str());
#endif
  
  mcas->close_pool(pool);
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

void send_schema(Component::IMCAS* mcas)
{
}

void do_work(Component::IMCAS* mcas)
{
  using namespace Component;

  const std::string poolname = "pool0";
  
  auto pool = mcas->create_pool(poolname,
                                GB(1),
                                0, /* flags */
                                1000); /* obj count */
  if(pool == IKVStore::POOL_ERROR)
    throw General_exception("create_pool (%s) failed", poolname.c_str());


  std::vector<std::string> str_samples;
  const unsigned num_strings = 1000000;
  for(unsigned i=0;i<num_strings;i++) {
    auto s = Common::random_string((rdtsc() % 32) + 8);
    str_samples.push_back(s);
  }

  mcas->erase(pool, "symbol0");

  std::string request, response;

  unsigned iterations = 10000;

  mcas->invoke_ado(pool,
                   "symbol0",
                   "st-init",
                   IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                   response,
                   MB(4));

  if(true)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
  
    for(unsigned i=0;i<iterations;i++) {
      mcas->invoke_ado(pool,
                       "symbol0", // key
                       str_samples[i],
                       IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                       response,
                       MB(4));
    }
    __sync_synchronize();
    auto   end_time = std::chrono::high_resolution_clock::now();

    auto  secs      = std::chrono::duration<double>(end_time - start_time).count();

    double per_sec = double(iterations) / secs;
    PINF("Synchronous ADO RTT");
    PINF("Time: %.2f sec", secs);
    PINF("Rate: %.0f /sec", per_sec);
  }


  /* -- asynchronous version -- */
  if(false)
  {
    auto start_time = std::chrono::high_resolution_clock::now();
  
    for(unsigned i=0;i<iterations;i++) {
      mcas->invoke_ado(pool,
                       "symbol0",
                       str_samples[i], //key
                       IMCAS::ADO_FLAG_CREATE_ON_DEMAND | IMCAS::ADO_FLAG_ASYNC,
                       response,
                       MB(4));
    }
    __sync_synchronize();
    auto end_time = std::chrono::high_resolution_clock::now();

    auto secs     = std::chrono::duration<double>(end_time - start_time).count();

    auto per_sec = double(iterations) / secs;
    PINF("Asynchronous ADO RTT");
    PINF("Time: %.2f sec", secs);
    PINF("Rate: %.0f /sec", per_sec);
  }
  
 // virtual status_t invoke_ado(const IKVStore::pool_t pool,
 //                              const std::string& key,
 //                              const std::vector<uint8_t>& request,
 //                              const uint32_t flags,                              
 //                              std::vector<uint8_t>& out_response,
 //                              const size_t value_size = 0) = 0;

  mcas->close_pool(pool);
}
