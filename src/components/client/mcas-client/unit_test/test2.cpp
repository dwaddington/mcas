/*
   Copyright [2017-2020] [IBM Corporation]
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
       http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/
#define LARGE_VALUE_SIZE KB(128);  // MB(4);
#define LARGE_ITERATIONS 10000
#define EXPECTED_OBJECTS 10000

#include <api/components.h>
#include <api/mcas_itf.h>
#include <common/cpu.h>
#include <common/str_utils.h>
#include <common/task.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <sys/mman.h>

#include <boost/program_options.hpp>
#include <boost/optional.hpp>
#include <chrono> /* milliseconds */
#include <iostream>
#include <thread> /* this_thread::sleep_for */

//#define TEST_PERF_SMALL_PUT
//#define TEST_PERF_SMALL_GET_DIRECT
#define TEST_PERF_LARGE_PUT_DIRECT
#define TEST_PERF_LARGE_GET_DIRECT

//#define TEST_SCALE_IOPS

// #define TEST_PERF_SMALL_PUT_DIRECT

struct {
  std::string                  addr;
  std::string                  pool;
  std::string                  device;
  unsigned                     debug_level;
  unsigned                     base_core;
  size_t                       value_size;
} Options{};

namespace
{
template <typename T>
boost::optional<T> optional_option(const boost::program_options::variables_map &vm_, const std::string &key_)
{
  return 0 < vm_.count(key_) ? vm_[key_].as<T>() : boost::optional<T>();
}
}  // namespace

component::IKVStore_factory *fact;
component::Itf_ref<component::IKVStore> _mcas;

using namespace component;

namespace
{
// The fixture for testing class Foo.
struct mcas_client_test : public ::testing::Test {
 protected:
  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  virtual void SetUp()
  {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  virtual void TearDown()
  {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

};

component::Itf_ref<component::IKVStore> _mcas;


void instantiate()
{
  using namespace component;
  /* create object instance through factory */
  IBase *comp = load_component("libcomponent-mcasclient.so", mcas_client_factory);

  auto factory = static_cast<IMCAS_factory *>(comp->query_interface(IMCAS_factory::iid()));
  assert(factory);

  _mcas.reset(factory->create(Options.debug_level, "cpp_bench", Options.addr, Options.device));
  factory->release_ref();
}

TEST_F(mcas_client_test, GetDirectWithContent)
{
  PMAJOR("Running OpenCloseDelete...");
  using namespace component;
  IKVStore::pool_t pool;

  const std::string poolname = Options.pool + "/GetDirectWithContent";
  ASSERT_TRUE((pool = _mcas->create_pool(poolname, MB(1))) != IKVStore::POOL_ERROR);
  ASSERT_FALSE(pool == IKVStore::POOL_ERROR);

  size_t size = KB(256);
  char * buffer = static_cast<char*>(aligned_alloc(64, size));

  auto handle = _mcas->register_direct_memory(buffer, size);
  ASSERT_TRUE(handle != IMCAS::MEMORY_HANDLE_NONE);

  /* populate with 0xA */
  memset(buffer, 0xA, size);

  /* put into storage */
  std::string key = "KEY";
  ASSERT_TRUE(_mcas->put_direct(pool,
                                key,
                                buffer,
                                size,
                                handle) == S_OK);

  PMAJOR("Put direct OK (%lu)", size);

  /* reset buffer */
  memset(buffer, 0xF, size);

  size_t rd_size = size;
  ASSERT_TRUE(_mcas->get_direct(pool,
                                key,
                                buffer,
                                size,
                                handle) == S_OK);

  ASSERT_TRUE(rd_size == size);

  PMAJOR("Get direct OK (%lu)", rd_size);

  /* check content */
  for(size_t i=0;i<size;i++) {
    if(buffer[i] != 0xA) PWRN("bad data");
    ASSERT_TRUE(buffer[i] == 0xA);
  }
  PMAJOR("Data check OK!");
  
  _mcas->unregister_direct_memory(handle);

  ::free(buffer);

  ASSERT_TRUE(_mcas->close_pool(pool) == S_OK);  
}


TEST_F(mcas_client_test, Release)
{
  PLOG("Releasing instance...");

  /* release instance */
  _mcas.reset(nullptr);
}

}  // namespace

int main(int argc, char **argv)
{
  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Show help")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level 0-3")
      ("server-addr", po::value<std::string>()->default_value("10.0.0.101:11911:verbs"), "Server address IP:PORT[:PROVIDER]")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Network device (e.g., mlx5_0)")
      ("pool", po::value<std::string>()->default_value("myPool"), "Pool name")
      ("value_size", po::value<std::size_t>()->default_value(0), "Value size")
      ("base", po::value<unsigned>()->default_value(0), "Base core.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }

    Options.addr        = vm["server-addr"].as<std::string>();
    Options.debug_level = vm["debug"].as<unsigned>();
    Options.pool        = vm["pool"].as<std::string>();
    Options.device      = vm["device"].as<std::string>();
    Options.base_core   = vm["base"].as<unsigned>();
    Options.value_size  = vm["value_size"].as<std::size_t>();

    PLOG("Instantiating..");
    instantiate();
    PLOG("Instantiation OK.");
    
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
  }
  catch (...) {
    PLOG("bad command line option configuration");
    return -1;
  }

  return 0;
}
