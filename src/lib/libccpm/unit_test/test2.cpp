/* note: we do not include component source, only the API definition */

#include <gtest/gtest.h>

#include <ccpm/cca.h>
#include <common/errors.h>
#include <common/logging.h>
#include <common/utils.h>

//#define GPERF_TOOLS

#ifdef GPERF_TOOLS
#include <gperftools/profiler.h>
#endif
#include <cstdlib> // alligned_alloc
#include <string> // stoull

struct {
  uint64_t uuid;
} Options;

// The fixture for testing class Foo.
class Libccpm_test : public ::testing::Test {
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


TEST_F(Libccpm_test, ccpm_cca_scenario_A)
{
  std::size_t size = MB(128);
  auto pr = aligned_alloc(4096,size);
  ASSERT_NE(nullptr, pr);

  ccpm::region_vector_t rv{pr, size};
  {
    ccpm::cca ccheap(rv);

    void  * p = nullptr;

    EXPECT_EQ(S_OK, ccheap.allocate(p,1024,8));
    EXPECT_NE(nullptr, p);
    p = nullptr;

    EXPECT_EQ(S_OK, ccheap.allocate(p,1024,8));
    EXPECT_NE(nullptr, p);
    p = nullptr;

    EXPECT_EQ(S_OK, ccheap.allocate(p,328,8));
    EXPECT_NE(nullptr, p);
    p = nullptr;

    EXPECT_EQ(S_OK, ccheap.allocate(p,1024,8));
    EXPECT_NE(nullptr, p);
    p = nullptr;

    EXPECT_EQ(S_OK, ccheap.allocate(p,472,8));
    EXPECT_NE(nullptr, p);
    p = nullptr;

    
    PLOG("allocations OK");
  }
}

TEST_F(Libccpm_test, ccpm_cca)
{
  std::size_t size = 409600;
  auto pr = aligned_alloc(4096,size);
  ASSERT_NE(nullptr, pr);
  const void *ph = static_cast<const char *>(pr) + size;
  ccpm::region_vector_t rv{pr, size};
  {
  int r = S_OK;
  ccpm::cca bt(rv);
  auto remain_cb = [&] () {
      std::size_t remain;
      r = bt.remaining(remain);
      PLOG("Remaining : %zu of %zu", remain, size);
      EXPECT_EQ(S_OK, r);
      EXPECT_LT(0, remain);
      EXPECT_GT(size, remain);
      return remain;
    };
  std::size_t remain0 = remain_cb();
  bt.print(std::cerr);

  void *p8 = nullptr;
  r = bt.allocate(p8, 8, 8);
  EXPECT_EQ(S_OK, r);
  EXPECT_NE(nullptr, p8);
  EXPECT_LE(pr, p8);
  EXPECT_GT(ph, p8);
  std::size_t remain1 = remain_cb();
  EXPECT_GE(remain0, remain1);
  bt.print(std::cerr);

  void *p16 = nullptr;
  r = bt.allocate(p16, 16, 16);
  EXPECT_EQ(S_OK, r);
  EXPECT_NE(nullptr, p16);
  EXPECT_NE(p8, p16);
  EXPECT_NE(nullptr, p16);
  EXPECT_LE(pr, p16);
  std::size_t remain2 = remain_cb();
  EXPECT_GE(remain1, remain2);
  bt.print(std::cerr);

  r = bt.free(p8, 8);
  EXPECT_EQ(S_OK, r);
  EXPECT_EQ(nullptr, p8);
  std::size_t remain3 = remain_cb();
  EXPECT_LE(remain2, remain3);

  r = bt.free(p16, 16);
  EXPECT_EQ(S_OK, r);
  EXPECT_EQ(nullptr, p16);
  std::size_t remain4 = remain_cb();
  EXPECT_LE(remain3, remain4);
  }
  {
    ccpm::cca bt(rv, [] (const void *) -> bool { return true; } );
  }
  {
    ccpm::region_vector_t rv_bad{static_cast<char *>(pr)+8, size};
    try
    {
      ccpm::cca bt(rv_bad, [] (const void *) -> bool { return true; } );
      EXPECT_EQ(0, 1);
    }
    catch ( const std::domain_error &e )
    {
      std::cerr << "Expected falure: " << e.what() << "\n";
    }  
  }
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  if (argc > 1) {
    Options.uuid = std::stoull(argv[1]);
  }
  (void) Options.uuid; // unused
  auto r = RUN_ALL_TESTS();

  return r;
}
