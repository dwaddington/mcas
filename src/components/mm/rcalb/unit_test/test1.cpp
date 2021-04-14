/* note: we do not include component source, only the API definition */

#include <api/components.h>
#include <api/mm_itf.h>
#include <common/str_utils.h>
#include <common/utils.h>
// #include <nupm/allocator_ra.h>
// #include <nupm/allocator_ra.h>
// #include <nupm/rc_alloc_lb.h>
// #include <nupm/region_descriptor.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#include <gtest/gtest.h>

#include <list>
#include <ctime>

#define COUNT 1000000
#define LENGTH 16

using namespace component;
using namespace common;
using namespace std;

namespace
{
// The fixture for testing class Foo.
class MM_test : public ::testing::Test {
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
  static IMemory_manager_volatile_reconstituting * _mm;
};

component::IMemory_manager_volatile_reconstituting *MM_test::_mm = nullptr;


TEST_F(MM_test, Instantiate)
{
  /* create object instance through factory */
  component::IBase *comp = component::load_component("libcomponent-mm-rcalb.so",
                                                     component::mm_rca_lb_factory);

  ASSERT_TRUE(comp);
  auto fact =
    make_itf_ref(
      static_cast<IMemory_manager_factory *>(comp->query_interface(IMemory_manager_factory::iid()))
    );

  ASSERT_TRUE(fact != nullptr);

  _mm  = fact->create_mm_volatile_reconstituting(3);
}

TEST_F(MM_test, UseAsStdAllocator)
{
  using aac_t =  mm::allocator_adaptor<char>;
  aac_t aac(_mm);

  auto aac_size = MiB(4);
  aac.add_managed_region(::aligned_alloc(PAGE_SIZE, aac_size), aac_size);

  using string_t = std::basic_string<char, std::char_traits<char>, aac_t>;

  string_t s("hello", aac);
  s.append(" there, can you help me?");

  auto ptr = aac.aligned_allocate(MiB(1), 256);
  ASSERT_FALSE(reinterpret_cast<uint64_t>(ptr) & 0xFFUL);
  ASSERT_FALSE(ptr == nullptr);

  aac.deallocate(ptr, MiB(1));
}


}  // namespace

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  auto r = RUN_ALL_TESTS();

  return r;
}

#pragma GCC diagnostic pop
