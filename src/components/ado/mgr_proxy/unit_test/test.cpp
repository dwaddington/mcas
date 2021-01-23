#include <api/ado_itf.h>
#include <api/components.h>
#include <common/str_utils.h>
#include <common/utils.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <memory>
#include <vector>

using namespace component;
using namespace common;
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
#if 0 // unused
  static std::unique_ptr<void> kill_buffer;
#endif
  static component::Itf_ref<component::IADO_manager_proxy> _ado_manager;
  static component::IADO_proxy *_ado;
};

component::Itf_ref<component::IADO_manager_proxy> IADO_manager_proxy_test::_ado_manager;
component::IADO_proxy *IADO_manager_proxy_test::_ado;

TEST_F(IADO_manager_proxy_test, Instantiate) {
  /* create object instance through factory */
  component::IBase *comp = component::load_component(
      "libcomponent-adomgrproxy.so", component::ado_manager_proxy_factory);

  ASSERT_TRUE(comp);
  auto fact =
    make_itf_ref(
      static_cast<IADO_manager_proxy_factory *>(comp->query_interface(IADO_manager_proxy_factory::iid()))
    );;

  _ado_manager.reset(fact->create(1, 0, "", 1));
}

TEST_F(IADO_manager_proxy_test, createADOProx) {
  PINF("Run create ADO proxy");
  string filename = "~/mcas/build/src/server/ado/ado";
  vector<string> args;
  ASSERT_TRUE(_ado_manager.get());
  _ado = _ado_manager->create(123, // auth id
			      0, // debug level
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
  _ado_manager->shutdown_ado(_ado);
  _ado_manager.reset(nullptr);
}

} // namespace

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  auto r = RUN_ALL_TESTS();

  return r;
}
