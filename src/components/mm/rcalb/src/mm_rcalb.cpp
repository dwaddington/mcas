#include <stdlib.h>
#include <unistd.h>
#include "mm_rcalb.h"


/* safe version of malloc */
extern "C" void * __wrap_malloc(size_t size)
{
  PNOTICE("__wrap_malloc");
  //  return ::mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
  return sbrk(size);
}


extern "C" void __wrap_free(void *)
{
  PNOTICE("__wrap_free");
}



/**
 * Factory entry point
 *
 */
extern "C" void* factory_createInstance(component::uuid_t component_id)
{
  if (component_id == Rca_LB_memory_manager_factory::component_id()) {
    return static_cast<void*>(new Rca_LB_memory_manager_factory());
  }
  else
    return NULL;
}
