#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <stdint.h>
#include <signal.h>
#include <inttypes.h>
#include "mcas.h"

// prints LSB to MSB
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <vector>

#include <nupm/nupm.h>

#define REDUCE_KB(X) (X >> 10)
#define REDUCE_MB(X) (X >> 20)
#define REDUCE_GB(X) (X >> 30)
#define REDUCE_TB(X) (X >> 40)

#define KB(X) (X << 10)
#define MB(X) (X << 20)
#define GB(X) (((unsigned long) X) << 30)
#define TB(X) (((unsigned long) X) << 40)

#ifndef MAP_HUGETLB
#define MAP_HUGETLB 0x40000 /* arch specific */
#endif

#ifndef MAP_HUGE_SHIFT
#define MAP_HUGE_SHIFT 26
#endif

#ifndef MAP_HUGE_MASK
#define MAP_HUGE_MASK 0x3f
#endif

#define MAP_HUGE_2MB (21 << MAP_HUGE_SHIFT)
#define MAP_HUGE_1GB (30 << MAP_HUGE_SHIFT)

#define USE_AEP

static constexpr uint64_t TOKEN = 0xFEEB;

int child_go = 0;
void sighup(int sig)
{
  child_go = 1;
}

int main()
{
  size_t size = 1000000;


  //  dax_manager ddm({{"/dev/dax0.3", 0x9000000000, 0},
  int fd = open("/dev/mcas", O_RDWR);
  assert(fd != -1);

  void * ptr = nullptr;
  void * pm_addr = ((void*) 0x900000000);

#ifdef USE_AEP
  nupm::_manager pm({{"/dev/dax1.0", ((uint64_t)pm_addr), 0}}, true);
  ptr = pm.create_region(1, 0, size);
#else
  int flags = MAP_PRIVATE | MAP_ANONYMOUS;
  flags |= MAP_HUGETLB | MAP_HUGE_2MB; // | MAP_FIXED;

  ptr = mmap(pm_addr,
             size,
             PROT_READ|PROT_WRITE,
             flags,
             -1,
             0);

#endif

  // touch memory
  memset(ptr, 0, size);

  PINF("touched ptr=%p", ptr);

  IOCTL_EXPOSE_msg ioparam;
  ioparam.token = TOKEN;
  ioparam.vaddr = ptr;
  ioparam.vaddr_size = size;

  /* expose memory */
  int rc = ioctl(fd, IOCTL_CMD_EXPOSE, &ioparam);  //ioctl call
  if(rc != 0) {
    PWRN("ioctl to expose failed: rc=%d", rc);
  }

  /* map exposed memory */
  {
    int fd = open("/dev/mcas", O_RDWR, 0666);
    assert(fd != -1);
    offset_t offset = ((offset_t)TOKEN) << 12; /* must be 4KB aligned */

    void * target_addr = ((void*)0x700000000);
    void * ptr = ::mmap(target_addr,
                        size,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_FIXED, // | MAP_HUGETLB, // | MAP_HUGE_2MB,
                        fd,
                        offset);

    if(ptr != ((void*) -1)) {
      PLOG("Success!! (ptr=%p)", ptr);

      PLOG("press return to contrine ...");
      getchar();

      /* touch memory */
      memset(ptr, 0xee, size);
      munmap(ptr, size);
    }
    else {
      PLOG("Failed! %s", strerror(errno));
    }

    close(fd);
  }

  PLOG("removing exposure...");

  /* signal child */
  //  kill(child_pid, SIGHUP);
  //  sleep(3);

  {
    int rc;
    IOCTL_REMOVE_msg ioparam;
    ioparam.token = TOKEN;
    rc = ioctl(fd, IOCTL_CMD_REMOVE, &ioparam);  //ioctl call
    if(rc != 0)
      PWRN("ioctl IOCTL_CMD_REMOVE failed");
    else
      PLOG("ioctl IOCTL_CMD_REMOVE OK!");
  }

  PLOG("parent closing");
  munmap(ptr, size);


  close(fd);
}
