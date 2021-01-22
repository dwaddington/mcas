#include "region_modifications.h"
#include "allocator_ra.h"
#include "rc_alloc_lb.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <memory>

//#define GPERF_TOOLS

#ifdef GPERF_TOOLS
#include <gperftools/profiler.h>
#endif

// The fixture for testing class Libnupm.
class Libnupm_test : public ::testing::Test
{
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

  // Objects declared here can be used by all tests in the test case
};

TEST_F(Libnupm_test, RegionModifications)
{
  /* Note: the nupm::region_tracker_add function uses a thread_local
   * Region_modifications object, not the one declared below.
   * However, calls to region_tracker_add are ignored unlessi
   * nupm::tracker_active is true. The constructor of r sets it true,
   * and the destructor of r returns it to false, providing a scope
   * in which region_tracker_add is effective.
   */
  nupm::Region_modifications r;
  struct
  {
    char c = 0;
    /* padding here */
    int i = 2;
    int j = 2;
  } z;
  nupm::region_tracker_add(&z.j, sizeof z.j);
  nupm::region_tracker_add(&z.c, sizeof z.c);
  nupm::region_tracker_add(&z.i, sizeof z.i);

  const void *v;
  std::size_t sz;
  sz = nupm::region_tracker_get_region(1, v);
  EXPECT_EQ(sizeof z.i + sizeof z.j, sz);
  EXPECT_EQ(&z.i, v);
  sz = nupm::region_tracker_get_region(0, v);
  EXPECT_EQ(sizeof z.c, sz);
  EXPECT_EQ(&z.c, v);
  /* past last element */
  sz = nupm::region_tracker_get_region(3, v);
  EXPECT_EQ(0, sz);
  /* farther past last element */
  sz = nupm::region_tracker_get_region(5, v);
  EXPECT_EQ(0, sz);
  /* before first element: also nothing */
  sz = nupm::region_tracker_get_region(offset_t(-1), v);
  EXPECT_EQ(0, sz);
  nupm::region_tracker_coalesce_across_TLS();
  sz = nupm::region_tracker_get_region(1, v);
  EXPECT_EQ(sizeof z.i + sizeof z.j, sz);
  EXPECT_EQ(&z.i, v);
  nupm::region_tracker_clear();
  sz = nupm::region_tracker_get_region(0, v);
  EXPECT_EQ(0, sz);
}

TEST_F(Libnupm_test, AVL_allocator)
{
  auto size = 1000000U;
  void *v = malloc(size);
  ASSERT_NE(nullptr, v);
  /* AVL_range_allocator requires an addr_t, defined in comanche common/types.h */
  core::AVL_range_allocator ra(reinterpret_cast<addr_t>(v), size);
  auto a = nupm::allocator_ra<char>(ra);
  const std::size_t ITERATIONS = 100;
  PLOG("AVL_allocator running... %zu allocations", ITERATIONS);

  std::vector<char *> alloc_v;
  alloc_v.reserve(ITERATIONS);

  auto start = std::chrono::high_resolution_clock::now();
  constexpr auto alloc_size = 200;
  for (unsigned i = 0; i < ITERATIONS; i++)
  {
    auto p = a.allocate(alloc_size);
    ASSERT_TRUE(p);
    EXPECT_LE(v, p);
    EXPECT_LT(p + alloc_size, static_cast<char *>(v) + size);
    alloc_v.push_back(p);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto secs = std::chrono::duration<double>(end - start).count();
  PINF("AVL_allocator: %lu allocs/sec",
       static_cast<unsigned long>(double(ITERATIONS) / secs));

  PLOG("AVL_allocator: Freeing %zu allocations...", alloc_v.size());

  for (auto &vv : alloc_v)
  {
      a.deallocate(vv, alloc_size);
  }
}

TEST_F(Libnupm_test, RCA_LB_allocator)
{
  std::size_t region_size = 10485760;
  int numa_node = 0;
  auto v0 = std::make_unique<char[]>(region_size);
  ASSERT_NE(nullptr, v0);
  auto v1 = std::make_unique<char[]>(region_size);
  ASSERT_NE(nullptr, v0);
  /* AVL_range_allocator requires an addr_t, defined in comanche common/types.h */
  nupm::Rca_LB lb;
  lb.add_managed_region(v0.get(), region_size, numa_node);
#if 0 /* a second area would make the EXPECT sanity checks on erturn pointers trickier */
  lb.add_managed_region(v1.get(), region_size, numa_node);
#endif
  nupm::allocator_adaptor<char, nupm::Rca_LB> a(lb);
  const std::size_t ITERATIONS = 100;
  PLOG("AVL_allocator running... %zu allocations", ITERATIONS);

  std::vector<char *> alloc_v;
  alloc_v.reserve(ITERATIONS);

  auto start = std::chrono::high_resolution_clock::now();
  constexpr auto alloc_size = 200;
  std::size_t alignment = 8;
  for (unsigned i = 0; i < ITERATIONS; i++)
  {
    auto p = a.allocate(alloc_size, alignment);
    ASSERT_TRUE(p);
    EXPECT_LE(v0.get(), p);
    EXPECT_LT(p + alloc_size, static_cast<char *>(v0.get()) + region_size);
    alloc_v.push_back(p);
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto secs = std::chrono::duration<double>(end - start).count();
  PINF("AVL_allocator: %lu allocs/sec",
       static_cast<unsigned long>(double(ITERATIONS) / secs));

  PLOG("AVL_allocator: Freeing %zu allocations...", alloc_v.size());
  for (auto &vv : alloc_v)
  {
    a.deallocate(vv, alloc_size, alignment);
  }
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  auto r = RUN_ALL_TESTS();

  return r;
}
