#include <stdio.h>
#include <string>
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

class ADO_test : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
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

TEST_F(ADO_test, BasicInvokeAdo)
{
  using namespace Component;

  const std::string poolname = "pool0";
  const std::string testname = "BasicInvokeAdo";
  
  auto pool = mcas->create_pool(poolname,
                                MB(256),   /* size */
                                0,    /* flags */
                                500); /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  mcas->erase(pool, testname);

  std::string response;
  status_t rc;
  rc = mcas->invoke_ado(pool,
                        testname,
                        "RUN!TEST-BasicInvokeAdo",
                        IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                        response,
                        KB(4));

  ASSERT_TRUE(rc == S_OK);

  ASSERT_OK(mcas->close_pool(pool));
}

TEST_F(ADO_test, BasicInvokePutAdo)
{
  using namespace Component;

  const std::string poolname = "pool0";
  const std::string testname = "BasicInvokePutAdo";
  
  auto pool = mcas->create_pool(poolname,
                                64,   /* size */
                                0,    /* flags */
                                1000); /* obj count */
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  mcas->erase(pool, testname);

  std::string response;
  status_t rc;
  std::string cmd = "RUN!TEST-BasicInvokePutAdo";
  std::string value_to_put = "VALUE_TO_PUT";
  rc = mcas->invoke_put_ado(pool,
                            testname,
                            cmd.data(),
                            cmd.length(),
                            value_to_put.data(),
                            value_to_put.length(),
                            0,
                            IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                            response);
  ASSERT_TRUE(rc == S_OK);

  {
    void *ptr = nullptr;
    size_t ptr_len = 0;
    rc = mcas->get(pool, testname, ptr, ptr_len);
    std::string r((char *)ptr, ptr_len);
    ASSERT_TRUE(r == value_to_put);
  }

  ASSERT_OK(mcas->close_pool(pool));
}

TEST_F(ADO_test, InvokeAdoCreateOnDemand)
{
  using namespace Component;

  const std::string poolname = "pool0";
  const std::string testname = "InvokeAdoCreateOnDemand";
  
  auto pool = mcas->create_pool(poolname,
                                MB(256),   /* size */
                                0,    /* flags */
                                100); /* obj count */
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  status_t rc;
  std::string response;
  rc = mcas->invoke_ado(pool,
                        testname,
                        "RUN!TEST-InvokeAdoCreateOnDemand",
                        IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                        response,
                        KB(4));
  
  ASSERT_TRUE(rc == S_OK);
  ASSERT_OK(mcas->close_pool(pool));
}

TEST_F(ADO_test, FindKeyCallback)
{
  using namespace Component;

  const std::string poolname = "pool0";
  const std::string testname = "FindKeyCallback";
  
  auto pool = mcas->create_pool(poolname,
                                64,   /* size */
                                0,    /* flags */
                                100); /* obj count */
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  ASSERT_OK(mcas->configure_pool(pool, "AddIndex::VolatileTree"));

  ASSERT_OK(mcas->put(pool, "mySpecialKey", "Special-Value"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey2", "Special-Value2"));

  status_t rc;
  std::string response;
  rc = mcas->invoke_ado(pool,
                        testname,
                        "RUN!TEST-FindKeyCallback",
                        IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                        response,
                        KB(4));
  ASSERT_TRUE(rc == S_OK);
  
  ASSERT_TRUE(mcas->close_pool(pool) == S_OK);
}

TEST_F(ADO_test, BasicAllocatePoolMemory)
{
  using namespace Component;

  const std::string poolname = "pool0";
  const std::string testname = "BasicAllocatePoolMemory";
  
  auto pool = mcas->create_pool(poolname,
                                GB(10), /* size */
                                0,      /* flags */
                                100);   /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  ASSERT_OK(mcas->configure_pool(pool, "AddIndex::VolatileTree"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey", "Special-Value"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey2", "Special-Value2"));

  status_t rc;
  std::string response;
  for (int i = 0; i < 100; i++)
  {
    rc = mcas->invoke_ado(pool,
                          testname,
                          "RUN!TEST-BasicAllocatePoolMemory",
                          IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                          response,
                          KB(4));
    ASSERT_TRUE(rc == S_OK);
  }

  ASSERT_TRUE(rc == S_OK);
  ASSERT_OK(mcas->close_pool(pool));
}

TEST_F(ADO_test, BasicDetachedMemory)
{
  using namespace Component;

  const std::string poolname = "pool0";
  const std::string testname = "BasicDetachedMemory";
  
  auto pool = mcas->create_pool(poolname,
                                GB(1), /* size */
                                0,      /* flags */
                                100);   /* obj count */
  
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  status_t rc;
  std::string response;
  std::string key = "BasicDetachedMemory";
  std::string value1 = "THIS_IS_VALUE_1";
  std::string value2 = "AND_THIS_IS_VALUE_2";
  std::string value3 = "AND_THIS_IS_A_SLIGHTLY_BIGGER_VALUE_3";

  mcas->erase(pool, key);

  rc = mcas->invoke_put_ado(pool,
                            key,
                            "RUN!TEST-BasicDetachedMemory",
                            value1,
                            32,
                            IMCAS::ADO_FLAG_DETACHED,
                            response);
  ASSERT_TRUE(rc == S_OK);
  
  rc = mcas->invoke_put_ado(pool,
                            key,
                            "RUN!TEST-BasicDetachedMemory",
                            value2,
                            32,
                            IMCAS::ADO_FLAG_DETACHED,
                            response);
  ASSERT_TRUE(rc == S_OK);

  rc = mcas->invoke_put_ado(pool,
                            key,
                            "RUN!TEST-BasicDetachedMemory",
                            value3,
                            32,
                            IMCAS::ADO_FLAG_DETACHED,
                            response);
  ASSERT_TRUE(rc == S_OK);  
  
  ASSERT_OK(mcas->close_pool(pool));
}


TEST_F(ADO_test, GetReferenceVector)
{
  using namespace Component;

  const std::string testname = "GetReferenceVector";
  const std::string poolname = testname;

  auto pool = mcas->create_pool(poolname,
                                GB(1), /* size */
                                0,      /* flags */
                                100);   /* obj count */
  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  status_t rc;
  std::string response;

  ASSERT_OK(mcas->put(pool, "mySpecialKey", "Special-Value"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey2", "Special-Value2"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey3", "Special-Value3"));

  rc = mcas->invoke_ado(pool,
                        testname,
                        "RUN!TEST-GetReferenceVector",
                        0,
                        response,
                        KB(4));

  ASSERT_TRUE(rc == S_OK);
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}


#ifdef REMOVE_CONDITIONAL_AFTER_DAWN_300

TEST_F(ADO_test, GetReferenceVectorByTime)
{
  using namespace Component;

  const std::string testname = "GetReferenceVectorByTime";
  const std::string poolname = testname;

  auto pool = mcas->create_pool(poolname,
                                GB(1), /* size */
                                0,      /* flags */
                                100);   /* obj count */
  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  std::string response;

  for(unsigned i=0;i<10;i++) {
    ASSERT_OK(mcas->put(pool, Common::random_string(8), Common::random_string(16)));
  }
  sleep(3);
  for(unsigned i=0;i<10;i++) {
    ASSERT_OK(mcas->put(pool, Common::random_string(8), Common::random_string(16)));
  }
  
  ASSERT_OK(mcas->invoke_ado(pool,
                             testname,
                             "RUN!TEST-GetReferenceVectorByTime",
                             0,
                             response,
                             KB(4)));
  
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

#endif

TEST_F(ADO_test, Iterator)
{
  using namespace Component;

  const std::string testname = "Iterator";
  const std::string poolname = testname;

  auto pool = mcas->create_pool(poolname,
                                GB(1), /* size */
                                IMCAS::ADO_FLAG_CREATE_ONLY, /* flags */
                                100);   /* obj count */
  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  std::string response;

  for(unsigned i=0;i<10;i++) {
    ASSERT_OK(mcas->put(pool, Common::random_string(8), Common::random_string(16)));
  }
  sleep(3);
  for(unsigned i=0;i<10;i++) {
    ASSERT_OK(mcas->put(pool, Common::random_string(8), Common::random_string(16)));
  }
  
  ASSERT_OK(mcas->invoke_ado(pool,
                             testname, /* dummy key to trigger test */
                             "RUN!TEST-Iterate",
                             0,
                             response,
                             KB(1)));
  
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
    
    mcas->release_ref();
  }
  catch (po::error e)
  {
    printf("bad command line option\n");
    return -1;
  }

  return 0;
}
