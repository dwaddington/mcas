#include <ccpm/heap.h>

namespace ccpm
{

void * allocate_at(size_t size, const addr_t target_addr)
{
  addr_t addr;
  if(target_addr) {
    addr = round_down_page(target_addr);
    size = round_up_page(size);
  }
  else {
    return nullptr;
  }
  
  void * p = mmap(reinterpret_cast<void *>(addr), /* address hint */
                  size,
                  PROT_READ  | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED,
                  0,  /* file */
                  0); /* offset */
  
  if(p == reinterpret_cast<void*>(-1))
    throw General_exception("mmap failed in allocate_at");

  assert((addr - reinterpret_cast<addr_t>(p)) < PAGE_SIZE);

  return reinterpret_cast<void*>(addr); /* return precise addr */
}

int free_at(void * p, size_t length)
{
  return munmap(p, length);
}

}


