#include <stdio.h>
#include <string>
#include <queue>
#include <sstream>
#include <chrono>
#include <iostream>
#include <common/str_utils.h>
#include <common/cycles.h>
#include <boost/program_options.hpp>
#include <api/components.h>
#include <api/mcas_itf.h>
#include <gtest/gtest.h>

#define ASSERT_OK(X) ASSERT_TRUE(X == S_OK)

struct Options
{
  unsigned debug_level;
  std::string server;
  std::string device;
  unsigned port;
  bool async;
} g_options;

class KV_test : public ::testing::Test {
 protected:
  virtual void SetUp() {  }
  virtual void TearDown()  { }
};

Component::IMCAS * mcas;
Component::IMCAS *init(const std::string &server_hostname, int port);

Component::IMCAS *init(const std::string &server_hostname, int port)
{
  using namespace Component;

  IBase *comp = Component::load_component("libcomponent-mcasclient.so",
                                          mcas_client_factory);

  auto fact = (IMCAS_factory *)comp->query_interface(IMCAS_factory::iid());
  if (!fact)
    throw Logic_exception("unable to create MCAS factory");

  std::stringstream url;
  url << g_options.server << ":" << g_options.port;

  IMCAS *mcas = fact->mcas_create(g_options.debug_level,
                                  "None",
                                  url.str(),
                                  g_options.device);

  if (!mcas)
    throw Logic_exception("unable to create MCAS client instance");

  fact->release_ref();

  return mcas;
}

/**
 * SECTION: Tests
 * 
 */

TEST_F(KV_test, BasicPoolOperations)
{
  using namespace Component;

  const std::string poolname = "pool0";
  
  auto pool = mcas->create_pool(poolname,
                                MB(1),   /* size */
                                0,    /* flags */
                                100); /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);  

  ASSERT_OK(mcas->close_pool(pool));

  PLOG("Reopen pool (from name) after close");
  pool = mcas->open_pool(poolname);
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  ASSERT_OK(mcas->close_pool(pool));
  PLOG("Deleting pool (from name) after close");
  ASSERT_OK(mcas->delete_pool(poolname));

  PLOG("Re-creating pool");
  pool = mcas->create_pool(poolname,
                           MB(2),   /* size */
                           IMCAS::ADO_FLAG_CREATE_ONLY,    /* flags */
                           100); /* obj count */
  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  ASSERT_TRUE(mcas->create_pool(poolname,
                                MB(2),   /* size */
                                IMCAS::ADO_FLAG_CREATE_ONLY,    /* flags */
                                100) == IMCAS::POOL_ERROR);
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(KV_test, OpenCloseDeletePool)
{
  using namespace Component;

  const std::string poolname = "pool2";
  
  auto pool = mcas->create_pool(poolname,
                                MB(64),   /* size */
                                0,    /* flags */
                                100); /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);  

  ASSERT_OK(mcas->close_pool(pool));

  /* re-open pool */
  pool = mcas->open_pool(poolname);

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);
  
  ASSERT_OK(mcas->delete_pool(pool));
}

/* test for dawn-311 */
TEST_F(KV_test, DeletePoolOperations)
{
  using namespace Component;

  const std::string poolname = "pool1";
  
  auto pool = mcas->create_pool(poolname,
                                MB(64),   /* size */
                                0,    /* flags */
                                100); /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);  

  ASSERT_OK(mcas->delete_pool(pool));

  ASSERT_TRUE(mcas->open_pool(poolname) == IMCAS::POOL_ERROR);
}

TEST_F(KV_test, BasicPutGetOperations)
{
  using namespace Component;

  const std::string poolname = "BasicPutGetOperations";
  
  auto pool = mcas->create_pool(poolname,
                                MB(32),   /* size */
                                0, /* flags */
                                100); /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  /* delete pool fails on mapstore when there is something in it. Bug # DAWN-287 */
  std::string key0 = "key0";
  std::string key1 = "key1";
  std::string value0 = "this_is_value_0";
  std::string value1 = "this_is_value_1_and_its_longer";
  ASSERT_OK(mcas->put(pool, key0, value0, 0));

  std::string out_value;
  ASSERT_OK(mcas->get(pool, key0, out_value));
  ASSERT_TRUE(value0 == out_value);

  ASSERT_OK(mcas->put(pool, key0, value1, 0));
  ASSERT_OK(mcas->get(pool, key0, out_value));
  PLOG("value1(%s) out_value(%s)", value1.c_str(), out_value.c_str());
  ASSERT_TRUE(value1 == out_value);

  /* try overwrite with DONT STOMP flag */
  ASSERT_TRUE(mcas->put(pool, key0, value1, IKVStore::FLAGS_DONT_STOMP) == IKVStore::E_KEY_EXISTS);

  ASSERT_OK(mcas->erase(pool, key0));

  /* here inout_value_len is zero, therefore on-demand creation is disabled */
  ASSERT_TRUE(mcas->get(pool, key0, out_value) == IKVStore::E_KEY_NOT_FOUND);

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(KV_test, PutDirect)
{
  using namespace Component;

  const std::string poolname = "PutDirect";
  
  auto pool = mcas->create_pool(poolname,
                                GB(1),   /* size */
                                0, /* flags */
                                100); /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  //  ASSERT_OK(mcas->put(pool, key0, value0, 0));

  size_t user_buffer_len = MB(128);
  void * user_buffer = aligned_alloc(KB(4), user_buffer_len);
  IMCAS::memory_handle_t mem = mcas->register_direct_memory(user_buffer, user_buffer_len);

  ASSERT_OK(mcas->put_direct(pool, "someLargeObject", user_buffer, user_buffer_len, mem));
  ASSERT_OK(mcas->put_direct(pool, "anotherLargeObject", user_buffer, user_buffer_len, mem));

  std::vector<uint64_t> attrs;
  ASSERT_OK(mcas->get_attribute(pool, IMCAS::Attribute::COUNT, attrs));
  ASSERT_TRUE(attrs[0] == 2); /* there should be only one object */
  

  size_t user_buffer2_len = MB(128);
  void * user_buffer2 = aligned_alloc(KB(4), user_buffer_len);
  IMCAS::memory_handle_t mem2 = mcas->register_direct_memory(user_buffer2, user_buffer2_len);

  ASSERT_OK(mcas->get_direct(pool, "someLargeObject", user_buffer2, user_buffer2_len, mem2));

  ASSERT_TRUE(memcmp(user_buffer, user_buffer2, user_buffer_len) == 0); /* integrity check */

  ASSERT_OK(mcas->unregister_direct_memory(mem2));
  ASSERT_OK(mcas->unregister_direct_memory(mem));
            
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
  free(user_buffer);
}  

TEST_F(KV_test, AsyncPutErase)
{
  using namespace Component;

  const std::string poolname = "AsyncPutErase";
  
  auto pool = mcas->create_pool(poolname,
                                MB(32),   /* size */
                                0, /* flags */
                                100); /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  std::string value0 = "this_is_value_0";

  IMCAS::async_handle_t handle = IMCAS::ASYNC_HANDLE_INIT;
  ASSERT_OK(mcas->async_put(pool,
                            "testKey",
                            value0.data(),
                            value0.length(),
                            handle));
  ASSERT_TRUE(handle != nullptr);

  ASSERT_OK(mcas->erase(pool,"testKey"));

  int iterations = 0;
  while(mcas->check_async_completion(handle) == E_BUSY) {
    ASSERT_TRUE(iterations < 1000000);
    iterations++;
  }

  constexpr int batch_size = 32; // see client_fabric_transport.h
  std::vector<std::string> keys;
  std::vector<std::string> values;
  for(int i=0;i<batch_size;i++) {
    keys.push_back(Common::random_string(8));
    values.push_back(Common::random_string(48));
  }

  std::queue<IMCAS::async_handle_t> issued;

  /* do multiple runs */
  for(unsigned j=0;j<100;j++) {
    /* issue batch */
    for(int i=0;i<batch_size;i++) {
      IMCAS::async_handle_t handle = IMCAS::ASYNC_HANDLE_INIT;
      ASSERT_OK(mcas->async_put(pool,
                                keys[i],
                                values[i].data(),
                                values[i].length(),
                                handle));
      ASSERT_TRUE(handle != nullptr);
      issued.push(handle);
    }

    /* wait for completions */
    while(!issued.empty()) {
      status_t s = mcas->check_async_completion(issued.front());
      ASSERT_TRUE(s == S_OK || s == E_BUSY);
      if(s == S_OK)
        issued.pop();
    }

    /* now erase them */
    for(int i=0;i<batch_size;i++) {
      IMCAS::async_handle_t handle = IMCAS::ASYNC_HANDLE_INIT;
      ASSERT_OK(mcas->async_erase(pool,
                                  keys[i],
                                  handle));
      ASSERT_TRUE(handle != nullptr);
      issued.push(handle);
    }

    /* wait for completions */
    while(!issued.empty()) {
      status_t s = mcas->check_async_completion(issued.front());
      ASSERT_TRUE(s == S_OK || s == E_BUSY);
      if(s == S_OK)
        issued.pop();
    }    

    std::vector<uint64_t> attr;
    ASSERT_OK(mcas->get_attribute(pool,
                                  IMCAS::Attribute::COUNT,
                                  attr));
    ASSERT_TRUE(attr[0] == 0);

  }
  
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}



TEST_F(KV_test, MultiPool)
{
  using namespace Component;
  std::map<std::string, IMCAS::pool_t> pools;

  const unsigned COUNT = 32;

  for(unsigned i=0;i<COUNT;i++) {
    auto poolname = Common::random_string(16);
    
    auto pool = mcas->create_pool(poolname,
                                  KB(32),   /* size */
                                  0, /* flags */
                                  100); /* obj count */

    ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

    pools[poolname] = pool;
  }

  for(auto& p : pools) {
    ASSERT_OK(mcas->close_pool(p.second));
    ASSERT_TRUE(mcas->close_pool(p.second) == E_INVAL);
    ASSERT_OK(mcas->delete_pool(p.first));
  }
}

TEST_F(KV_test, PoolCapacity)
{
  auto poolname = Common::random_string(16);

  const unsigned OBJ_COUNT = 6000;
  auto pool = mcas->create_pool(poolname,
                                MB(32),   /* size */
                                0, /* flags */
                                OBJ_COUNT); /* obj count */

  for(unsigned i=0;i<OBJ_COUNT;i++) {
    ASSERT_TRUE(mcas->put(pool,
                          Common::random_string(16),
                          Common::random_string(KB(4))) == S_OK);
  }
               
}

TEST_F(KV_test, BadPutGetOperations)
{
  using namespace Component;

  const std::string poolname = "BadPutGetOperations";
  
  auto pool = mcas->create_pool(poolname,
                                MB(32),   /* size */
                                0, /* flags */
                                100); /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  /* delete pool fails on mapstore when there is something in it. Bug # DAWN-287 */
  std::string key0 = "key0";
  std::string key1 = "key1";
  std::string value0 = "this_is_value_0";
  std::string value1 = "this_is_value_1_and_its_longer";
  ASSERT_OK(mcas->put(pool, key0, value0, 0));

  std::string out_value;
  ASSERT_OK(mcas->get(pool, key0, out_value));
  ASSERT_TRUE(value0 == out_value);

  /* bad parameters */
  ASSERT_TRUE(mcas->put(pool, key1, nullptr, 0) == E_INVAL);
  ASSERT_TRUE(mcas->put(pool, key1, value0.c_str(), 0) == E_INVAL);
  ASSERT_TRUE(mcas->put(pool, key1, nullptr, 100) == E_INVAL);
  ASSERT_TRUE(mcas->put(0x0, key1, nullptr, 100) == E_INVAL);
  
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}




int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);
  
  namespace po = boost::program_options;

  try
  {
    po::options_description desc("Options");

    desc.add_options()("help", "Show help")
      ("server", po::value<std::string>(), "Server hostname")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Device (e.g. mlnx5_0)")
      ("port", po::value<unsigned>()->default_value(11911), "Server port")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level")
      ("async", "Use asynchronous invocation");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0)
    {
      std::cout << desc;
      return -1;
    }

    if (vm.count("server") == 0)
    {
      std::cout << "--server option is required\n";
      return -1;
    }

    g_options.server = vm["server"].as<std::string>();
    g_options.device = vm["device"].as<std::string>();
    g_options.port = vm["port"].as<unsigned>();
    g_options.debug_level = vm["debug"].as<unsigned>();
    g_options.async = vm.count("async");

    mcas = init(vm["server"].as<std::string>(), vm["port"].as<unsigned>());
    assert(mcas);
    
    auto r = RUN_ALL_TESTS();
    PLOG("kv-test has finished");
    
    mcas->release_ref();
  }
  catch (po::error e)
  {
    printf("bad command line option\n");
    return -1;
  }

  return 0;
}
