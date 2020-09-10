/* note: we do not include component source, only the API definition */
#include <common/cycles.h>
#include <common/rand.h>
#include <common/utils.h>
#include <gtest/gtest.h>
#include <iostream>
#include <string>
#include <thread>

#include <threadipc/queue.h>
//#include <common/queue.h>
//#define GPERF_TOOLS

#ifdef GPERF_TOOLS
#include <gperftools/profiler.h>
#endif

struct {
  uint64_t uuid;
} Options;

// The fixture for testing class Foo.
class Libnupm_test : public ::testing::Test {
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

#define COUNT 100

void receiver()
{
  threadipc::Message *x;
  for (unsigned i = 0; i < COUNT; i++) {
    x = NULL;
    threadipc::Thread_ipc::instance()->get_next_ado(x);
    PINF("Receiver got: %s", x->cores.c_str());
    std::cout << "receive: " << x->cores << std::endl;
  }
  PLOG("Receiver thread exiting.");
}

TEST_F(Libnupm_test, mpmc_queue_test)
{
  std::thread forked_thread([&]() { receiver(); });

  sleep(1);

  for (unsigned i = 0; i < COUNT; i++) {
    //   message msg{i, Operation::kill, "", 1.0, "", i};
    std::string abc("abc");
    abc.append(std::to_string(i));
    threadipc::Thread_ipc::instance()->schedule_to_ado(i, abc, 1.0, 0);
    PINF("Sent %s", abc.c_str());
    std::cout << "sent: " << abc << std::endl;
  }

  forked_thread.join();
}

int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  if (argc > 1) {
    Options.uuid = std::stoull(argv[1]);
  }
  auto r = RUN_ALL_TESTS();

  return r;
}
