#include <api/components.h>
#include <api/mcas_itf.h>
#include <common/cycles.h>
#include <common/str_utils.h> /* random_string */
#include <common/utils.h> /* KiB, MiB, GiB */
#include <gtest/gtest.h>
#include <stdio.h>
#include <boost/program_options.hpp>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

/**
 * This test program works in collaboration with the 'testing' ADO plugin
 *
 */
#define ASSERT_OK(X) ASSERT_EQ(S_OK, (X))

struct Options {
  unsigned debug_level;
  unsigned patience;
  boost::optional<std::string> device;
  boost::optional<std::string> src_addr;
  std::string server;
  unsigned port;
  bool async;
} g_options;

class ADO_test : public ::testing::Test {
protected:
  virtual void SetUp() {}
  virtual void TearDown() {}
};

using namespace component;

Itf_ref<IMCAS> mcas;
Itf_ref<IMCAS> init(const std::string &server_hostname, int port);

Itf_ref<IMCAS> init(const std::string &server_hostname, int port)
{
  using namespace component;

  IBase *comp = load_component("libcomponent-mcasclient.so", mcas_client_factory);

  auto fact = make_itf_ref(static_cast<IMCAS_factory *>(comp->query_interface(IMCAS_factory::iid())));
  if (!fact) throw Logic_exception("unable to create MCAS factory");

  std::stringstream url;
  url << g_options.server << ":" << g_options.port;

  auto mcas = make_itf_ref(fact->mcas_create(g_options.debug_level,
                                             g_options.patience,
                                             "None",
                                             g_options.device,
                                             g_options.src_addr,
                                             url.str()));

  if (!mcas) throw Logic_exception("unable to create MCAS client instance");

  return mcas;
}

/**
 * SECTION: Tests
 *
 */

TEST_F(ADO_test, BasicInvokeAdo)
{
  const std::string testname = "BasicInvokeAdo";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(256), /* size */
                                0,                 /* flags */
                                500);              /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  mcas->erase(pool, testname);

  std::vector<IMCAS::ADO_response> response;
  status_t                                    rc;
  rc = mcas->invoke_ado(pool, testname, "RUN!TEST-BasicInvokeAdo", IMCAS::ADO_FLAG_CREATE_ON_DEMAND, response, KiB(4));

  ASSERT_TRUE(rc == S_OK);

  ASSERT_OK(mcas->close_pool(pool));

  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, BasicAdoResponse)
{
  const std::string testname = "BasicAdoResponse";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0,                 /* flags */
                                50);              /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  mcas->erase(pool, testname);

  std::vector<IMCAS::ADO_response> response;
  status_t rc;
  rc = mcas->invoke_ado(pool, testname, "RUN!TEST-BasicAdoResponse", IMCAS::ADO_FLAG_CREATE_ON_DEMAND, response, KiB(4));

  ASSERT_TRUE(rc == S_OK);
  ASSERT_LT(0, response.size());

  std::string r = response[0].str();
  PLOG("Response: (%s)", r.c_str());

  ASSERT_TRUE(r == testname);

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(ADO_test, BasicInvokePutAdo)
{
  const std::string testname = "BasicInvokePutAdo";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0,            /* flags */
                                1000);        /* obj count */
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  mcas->erase(pool, testname);

  std::vector<IMCAS::ADO_response> response;
  status_t                                    rc;
  std::string                                 cmd          = "RUN!TEST-BasicInvokePutAdo";
  std::string                                 value_to_put = "VALUE_TO_PUT";
  rc = mcas->invoke_put_ado(pool, testname, cmd.data(), cmd.length(), value_to_put.data(), value_to_put.length(), 0,
                            IMCAS::ADO_FLAG_CREATE_ON_DEMAND, response);
  ASSERT_TRUE(rc == S_OK);

  {
    void * ptr     = nullptr;
    size_t ptr_len = 0;
    rc             = mcas->get(pool, testname, ptr, ptr_len);
    std::string r((char *) ptr, ptr_len);
    ASSERT_TRUE(r == value_to_put);
  }

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, InvokeAdoCreateOnDemand)
{
  const std::string testname = "InvokeAdoCreateOnDemand";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0,                 /* flags */
                                100);              /* obj count */
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  status_t                                    rc;
  std::vector<IMCAS::ADO_response> response;
  rc = mcas->invoke_ado(pool, testname, "RUN!TEST-InvokeAdoCreateOnDemand", IMCAS::ADO_FLAG_CREATE_ON_DEMAND, response,
                        KiB(4));

  ASSERT_TRUE(rc == S_OK);
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, AdoKeyReference)
{
  const std::string testname = "AdoKeyReference";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0,                 /* flags */
                                500);              /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  mcas->erase(pool, testname);

  std::vector<IMCAS::ADO_response> response;
  status_t                                    rc;
  rc = mcas->invoke_ado(pool, testname, "RUN!TEST-AdoKeyReference", IMCAS::ADO_FLAG_CREATE_ON_DEMAND, response, KiB(4));

  ASSERT_TRUE(rc == S_OK);

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, FindKeyCallback)
{
  const std::string testname = "FindKeyCallback";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0,            /* flags */
                                100);         /* obj count */
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  ASSERT_OK(mcas->configure_pool(pool, "AddIndex::VolatileTree"));

  ASSERT_OK(mcas->put(pool, "mySpecialKey", "Special-Value"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey2", "Special-Value2"));

  status_t                                    rc;
  std::vector<IMCAS::ADO_response> response;
  rc = mcas->invoke_ado(pool, testname, "RUN!TEST-FindKeyCallback", IMCAS::ADO_FLAG_CREATE_ON_DEMAND, response, KiB(4));
  ASSERT_TRUE(rc == S_OK);

  ASSERT_TRUE(mcas->close_pool(pool) == S_OK);
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(ADO_test, BasicAllocatePoolMemory)
{
  const std::string testname = "BasicAllocatePoolMemory";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, GiB(1), /* size */
                                0,                /* flags */
                                100);             /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  ASSERT_OK(mcas->configure_pool(pool, "AddIndex::VolatileTree"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey", "Special-Value"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey2", "Special-Value2"));

  status_t                                    rc;
  std::vector<IMCAS::ADO_response> response;
  for (int i = 0; i < 100; i++) {
    rc = mcas->invoke_ado(pool, testname, "RUN!TEST-BasicAllocatePoolMemory", IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                          response, KiB(4));
    ASSERT_TRUE(rc == S_OK);
  }

  ASSERT_TRUE(rc == S_OK);
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(ADO_test, BasicDetachedMemory)
{
  const std::string testname = "BasicDetachedMemory";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, GiB(1), /* size */
                                0,               /* flags */
                                100);            /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  status_t                                    rc;
  std::vector<IMCAS::ADO_response> response;
  std::string                                 key    = "BasicDetachedMemory";
  std::string                                 value1 = "THIS_IS_VALUE_1";
  std::string                                 value2 = "AND_THIS_IS_VALUE_2";
  std::string                                 value3 = "AND_THIS_IS_A_SLIGHTLY_BIGGER_VALUE_3";

  mcas->erase(pool, key);

  rc = mcas->invoke_put_ado(pool, key, "RUN!TEST-BasicDetachedMemory", value1, 32, IMCAS::ADO_FLAG_DETACHED, response);
  ASSERT_TRUE(rc == S_OK);

  rc = mcas->invoke_put_ado(pool, key, "RUN!TEST-BasicDetachedMemory", value2, 32, IMCAS::ADO_FLAG_DETACHED, response);
  ASSERT_TRUE(rc == S_OK);

  rc = mcas->invoke_put_ado(pool, key, "RUN!TEST-BasicDetachedMemory", value3, 32, IMCAS::ADO_FLAG_DETACHED, response);
  ASSERT_TRUE(rc == S_OK);

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(ADO_test, PersistedDetachedMemory)
{
  const std::string testname = "PersistedDetachedMemory";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  /* phase 1: create the pool, write some values to "detached memory" */
  auto pool = mcas->create_pool(poolname, MiB(64), /* size */
                                0,               /* flags */
                                100);            /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  const char *key = "PersistedDetachedMemory";
  const char *key_saturate = "PersistedDetachedMemorySaturate";

  std::string value1 = std::string(sizeof(void *)*17, char('0'));
  mcas->erase(pool, key);
  std::vector<IMCAS::ADO_response> response;
  /* RUN!TEST-AddDetachedMemory shall allocate 17 areas of detached memory, and replace the 17*8 bytes written by value1 with 17 pointers to those areas */
  status_t rc =
    mcas->invoke_put_ado(
      pool
      , key
      , "RUN!TEST-AddDetachedMemory"
      , value1
      , 0 // root_len - only used with ADO_FLAG_DETACHED
      , IMCAS::ADO_FLAG_NONE
      , response
    );
  ASSERT_EQ(S_OK, rc);
  ASSERT_OK(mcas->close_pool(pool));

  /* phase 2: reopen pool, allocate all available space.
   * Verify that the values written by phase 1 have not been overwritten
   */

  pool = mcas->open_pool(poolname);

  unsigned j = 0;
  while ( rc == S_OK )
  {
    rc = mcas->put(pool, key_saturate + std::to_string(j), std::string(4096, 'x'));
  }
  /* expect to use up all pool space */
  ASSERT_EQ(IKVStore::E_TOO_LARGE, rc);

  /* RUN!TEST-CompareDetachedMemory shall verify that the 17 areas of detached memory written by RUN!TEST-AddDetachedMemory are intact */
  rc = mcas->invoke_ado(pool, key, "RUN!TEST-CompareDetachedMemory", IMCAS::ADO_FLAG_NONE, response);
  ASSERT_EQ(S_OK, rc);

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(ADO_test, GetReferenceVector)
{
  const std::string testname = "GetReferenceVector";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, GiB(1), /* size */
                                0,               /* flags */
                                100);            /* obj count */
  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  status_t                                    rc;
  std::vector<IMCAS::ADO_response> response;

  ASSERT_OK(mcas->put(pool, "mySpecialKey", "Special-Value"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey2", "Special-Value2"));
  ASSERT_OK(mcas->put(pool, "mySpecialKey3", "Special-Value3"));

  rc = mcas->invoke_ado(pool, testname, "RUN!TEST-GetReferenceVector", 0, response, KiB(4));

  ASSERT_EQ(S_OK, rc);
  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, GetReferenceVectorByTime)
{
  const std::string testname = "GetReferenceVectorByTime";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(32), /* size */
                                0,               /* flags */
                                100);            /* obj count */
  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  std::vector<IMCAS::ADO_response> response;

  for (unsigned i = 0; i < 10; i++) {
    ASSERT_OK(mcas->put(pool, common::random_string(8), common::random_string(16)));
  }

  sleep(2);

  for (unsigned i = 0; i < 10; i++) {
    ASSERT_OK(mcas->put(pool, common::random_string(8), common::random_string(16)));
  }

  sleep(2);

  ASSERT_OK(mcas->invoke_ado(pool, testname, "RUN!TEST-GetReferenceVectorByTime", 0, response, KiB(4)));

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, Iterator)
{
  const std::string testname = "Iterator";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(32),             /* size */
                                IMCAS::ADO_FLAG_CREATE_ONLY, /* flags */
                                100);                        /* obj count */
  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  std::vector<IMCAS::ADO_response> response;

  for (unsigned i = 0; i < 10; i++) {
    ASSERT_OK(mcas->put(pool, common::random_string(8), common::random_string(16)));
  }
  sleep(3);
  for (unsigned i = 0; i < 10; i++) {
    ASSERT_OK(mcas->put(pool, common::random_string(8), common::random_string(16)));
  }

  ASSERT_OK(mcas->invoke_ado(pool, testname, /* dummy key to trigger test */
                             "RUN!TEST-Iterate", 0, response, KiB(1)));

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, IteratorTS)
{
  const std::string testname = "IteratorTS";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(32),             /* size */
                                IMCAS::ADO_FLAG_CREATE_ONLY, /* flags */
                                100);                        /* obj count */

  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  std::vector<IMCAS::ADO_response> response;

  common::epoch_time_t ts1 = common::epoch_now();

  ASSERT_TRUE(ts1.seconds() > 0);
  PLOG("now epoch.seconds = %lu", ts1.seconds());

  wmb();
  sleep(3);

  for (unsigned i = 0; i < 10; i++) {
    ASSERT_OK(mcas->put(pool, common::random_string(8), common::random_string(16)));
  }

  wmb();
  sleep(2);

  PLOG("now epoch.seconds = %lu ", common::epoch_now().seconds());

  ASSERT_OK(mcas->invoke_ado(pool, testname, /* dummy key to trigger test - this will create another pair */
                             "RUN!TEST-IteratorTS " + std::to_string(ts1.seconds()), 0, response, KiB(1)));

  ASSERT_LT(0, response.size());
  uint64_t cnt = *(response[0].cast_data<uint64_t>());
  PLOG("Iterator TS: count (1st round) = %lu", cnt);
  ASSERT_TRUE(cnt == 11);

  sleep(3);
  wmb();
  common::epoch_time_t ts2 = common::epoch_now();
  wmb();
  for (unsigned i = 0; i < 10; i++) { /* put another 10 */
    ASSERT_OK(mcas->put(pool, common::random_string(8), common::random_string(16)));
  }


  ASSERT_OK(mcas->invoke_ado(pool, testname, /* dummy key to trigger test - this will create another pair */
                             "RUN!TEST-IteratorTS " + std::to_string(ts2.seconds()), 0, response, KiB(1)));

  cnt = *(response[0].cast_data<uint64_t>());
  PLOG("Iterator TS: count (2nd round) = %lu", cnt);

  ASSERT_TRUE(cnt == 10 || cnt == 11); /* IteratorTS key updated */

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, Erase)
{
  const std::string testname = "Erase";
  const std::string poolname = testname;
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, GiB(1),             /* size */
                                IMCAS::ADO_FLAG_CREATE_ONLY, /* flags */
                                100);                        /* obj count */
  ASSERT_FALSE(pool == IMCAS::POOL_ERROR);

  std::vector<IMCAS::ADO_response> response;

  ASSERT_OK(mcas->put(pool, testname, common::random_string(16)));

  mcas->invoke_ado(pool, testname, /* dummy key to trigger test - this will create another pair */
                   "RUN!TEST-Erase", 0, response, KiB(1));
  std::string outvalue;
  auto        r = mcas->get(pool, testname, outvalue);

  PLOG("get removed key: %s, value: %s", testname.c_str(), outvalue.c_str());
  ASSERT_FALSE(r == S_OK);

  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(ADO_test, BasicAsyncInvokeAdo)
{
  const std::string poolname = "pool0";

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0,                 /* flags */
                                50);              /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  IMCAS::async_handle_t handles[4];
  const char * keys[] = {"BasicAdoResponse-A","BasicAdoResponse-B","BasicAdoResponse-C","BasicAdoResponse-D"};

  std::vector<IMCAS::ADO_response> responses[4];

  for(unsigned h=0; h<4 ; h++) {
    status_t rc = mcas->async_invoke_ado(pool, keys[h], "RUN!TEST-BasicAdoResponse",
                                IMCAS::ADO_FLAG_CREATE_ON_DEMAND, responses[h], handles[h], KiB(4));
    ASSERT_OK(rc);
  }

  PLOG("Async issues made..");

  for(unsigned h=0; h<4 ; h++) {
    int attempts = 0;
    while(mcas->check_async_completion(handles[h]) == E_BUSY) {
      sleep(1);
      PLOG("Waiting for async (%u) completion....", h);
      attempts++;
      ASSERT_FALSE(attempts > 4);
    }

    ASSERT_TRUE(responses[h].size() == 1);
    ASSERT_TRUE(responses[h][0].str() == std::string(keys[h]));
    PLOG("response=%s (count=%lu)", responses[h][0].str().c_str(), responses[h].size());
  }


  ASSERT_OK(mcas->close_pool(pool));
  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(ADO_test, RepeatInvokeAdo)
{
  const std::string testname = "RepeatInvokeAdo";
  const std::string poolname = "THIS_IS_A_TEST_POOL";
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0,                 /* flags */
                                50);              /* obj count */

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  /* add index to pool */
  ASSERT_TRUE(mcas->configure_pool(pool, "AddIndex::VolatileTree") == S_OK);

  mcas->erase(pool, testname);

  std::vector<IMCAS::ADO_response> response;
  status_t                                    rc;

  for(unsigned i=0;i<10;i++) {
    rc = mcas->invoke_ado(pool,
                          testname,
                          "RUN!TEST-RepeatInvokeAdo",
                          IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                          response,
                          KiB(4));
    
    ASSERT_TRUE(rc == S_OK);
  }
 
  ASSERT_OK(mcas->close_pool(pool));

  ASSERT_OK(mcas->delete_pool(poolname));
}


TEST_F(ADO_test, BaseAddr)
{
  const std::string testname = "BaseAddr";
  const std::string poolname = "THIS_IS_A_TEST_POOL";
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0, /* flags */
                                50, /* obj count */
                                IMCAS::Addr{0xBB00000000});

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  mcas->erase(pool, testname);

  std::vector<IMCAS::ADO_response> response;
  status_t rc;

  rc = mcas->invoke_ado(pool,
                        testname,
                        "RUN!TEST-BaseAddr",
                        IMCAS::ADO_FLAG_CREATE_ON_DEMAND,
                        response,
                        KiB(4));
 
  ASSERT_OK(mcas->close_pool(pool));

  ASSERT_OK(mcas->delete_pool(poolname));
}

TEST_F(ADO_test, PutSignal)
{
  const std::string testname = "PutSignal";
  const std::string poolname = "THIS_IS_A_TEST_POOL";
  mcas->delete_pool(poolname);

  auto pool = mcas->create_pool(poolname, MiB(1), /* size */
                                0, /* flags */
                                50, /* obj count */
                                IMCAS::Addr{0xBB00000000});

  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  mcas->erase(pool, testname);

  std::vector<IMCAS::ADO_response> response;
  status_t rc;

  rc = mcas->put(pool,
                 "someKey",
                 "someValue");
  ASSERT_OK(rc);
  
  ASSERT_OK(mcas->close_pool(pool));

  ASSERT_OK(mcas->delete_pool(poolname));
}



int main(int argc, char *argv[])
{
  ::testing::InitGoogleTest(&argc, argv);

  namespace po = boost::program_options;

  try {
    po::options_description desc("Options");

    desc.add_options()
      ("help", "Show help")
      ("server", po::value<std::string>(), "Server hostname")
      ("src_addr", po::value<std::string>(), "Source IP address")
      ("device", po::value<std::string>(), "Device (e.g. mlnx5_0)")
      ("port", po::value<std::uint16_t>()->default_value(0), "Server port. Default 0 (mapped to 11911 for verbs, 11921 for sockets)")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level")
      ("patience", po::value<unsigned>()->default_value(30), "Patience with server (seconds)")
      ("async", "Use asynchronous invocation");

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

    g_options.server      = vm["server"].as<std::string>();
    if ( vm.count("src_addr") )
      {
        g_options.src_addr = vm["src_addr"].as<std::string>();
      }
    if ( vm.count("device") )
      {
        g_options.device = vm["device"].as<std::string>();
      }
    if ( ! g_options.src_addr && ! g_options.device )
      {
        g_options.device = "mlx5_0";
      }
    g_options.port        = vm["port"].as<std::uint16_t>();
    g_options.debug_level = vm["debug"].as<unsigned>();
    g_options.patience = vm["patience"].as<unsigned>();
    g_options.async       = vm.count("async");

    mcas = init(g_options.server, g_options.port);
    assert(mcas);
    auto r = RUN_ALL_TESTS();
  }
  catch (po::error e) {
    printf("bad command line option\n");
    return -1;
  }
  catch (const Exception &e) {
    /* init() throws */
    printf("failed: %s\n", e.cause());
    return -1;
  }

  return 0;
}
