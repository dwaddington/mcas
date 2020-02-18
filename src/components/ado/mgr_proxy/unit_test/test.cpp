#include <api/ado_itf.h>
#include <api/components.h>
#include <common/str_utils.h>
#include <common/utils.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

using namespace Component;
using namespace Common;
using namespace std;

namespace {
// The fixture for testing class Foo.
class IADO_manager_proxy_test : public ::testing::Test {
protected:
  // If the constructor and destructor are not enough for setting up
  // and cleaning up each test, you can define the following methods:

  virtual void SetUp() {
    // Code here will be called immediately after the constructor (right
    // before each test).
  }

  virtual void TearDown() {
    // Code here will be called immediately after each test (right
    // before the destructor).
  }

  // Objects declared here can be used by all tests in the test case
  static std::unique_ptr<void> kill_buffer;
  static Component::IADO_manager_proxy *_ado_manager;
  static Component::IADO_proxy *_ado;
};

Component::IADO_manager_proxy *IADO_manager_proxy_test::_ado_manager;
Component::IADO_proxy *IADO_manager_proxy_test::_ado;

TEST_F(IADO_manager_proxy_test, Instantiate) {
  /* create object instance through factory */
  Component::IBase *comp = Component::load_component(
      "libcomponent-adomgrproxy.so", Component::ado_manager_proxy_factory);

  ASSERT_TRUE(comp);
  auto fact =
    static_cast<IADO_manager_proxy_factory *>(comp->query_interface(IADO_manager_proxy_factory::iid()));

  _ado_manager = fact->create(1, 0, "", 1);

  fact->release_ref();
}

TEST_F(IADO_manager_proxy_test, createADOProx) {
  PINF("Run create ADO proxy");
  string filename = "/home/xuluna/workspace/mcas/build/src/server/ado/ado";
  vector<string> args;
  ASSERT_TRUE(_ado_manager);
  _ado = _ado_manager->create(123, // auth id
                              nullptr,
                              999,
                              "pool-name", //      const std::string &pool_name,
                              MB(1),//      const size_t pool_size,
                              0,//      const unsigned int pool_flags,
                              1000,//      const uint64_t expected_obj_count,
                              filename, args, 0, 0);
  ASSERT_TRUE(_ado);
}


TEST_F(IADO_manager_proxy_test, Shutdown) {
  _ado_manager->shutdown(_ado);
  _ado_manager->release_ref();
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  auto r = RUN_ALL_TESTS();

  return r;
}
