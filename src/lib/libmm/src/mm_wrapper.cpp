#include <dlfcn.h>
#include <stdio.h>
#include <common/logging.h>
#include <common/utils.h>
#include <common/exceptions.h>
#include <common/stack_trace.h>
#include <api/components.h>
#include <api/mm_itf.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"

using malloc_function_t = void* (*)(size_t);
using free_function_t = void (*)(void *ptr);
using aligned_alloc_function_t =  void* (*)(size_t alignment, size_t size);

#define PREFIX "MM-Wrapper: "
#define DEBUG_LEVEL 3
//#undef USE_POSIX_ALLOCATOR

/* everything POSIX needs is in the base */
namespace globals
{
static component::IMemory_manager_base * mm = nullptr; /* I assume this happens first? */
static void * slab_memory = nullptr; /*< memory which will be used as the slab for the allocator */
static unsigned alloc_count = 0;
static bool intercept_active = false;
}

namespace real
{
malloc_function_t        malloc;
free_function_t          free;
aligned_alloc_function_t aligned_alloc;
}


static __attribute__((destructor))
void __dtor(void)
{
  real::free(globals::slab_memory);
}

static void __init_real_functions(void)
{
  real::malloc = reinterpret_cast<malloc_function_t>(dlsym(RTLD_NEXT, "malloc"));
  real::free = reinterpret_cast<free_function_t>(dlsym(RTLD_NEXT, "free"));
  real::aligned_alloc = reinterpret_cast<aligned_alloc_function_t>(dlsym(RTLD_NEXT, "aligned_alloc"));
  if (!real::malloc || !real::free | !real::aligned_alloc)
    fprintf(stderr, "Error in `dlsym`: %s\n", dlerror());
}

static __attribute__((constructor)) // can't seem to get this to happen early enough!
void __init_mm(void)
{
  using namespace component;
  
  /* load MM component ; could use environment variable */
  IBase *comp = load_component("libcomponent-mm-rcalb.so",
                               component::mm_rca_lb_factory);
  assert(comp);
  PLOG(PREFIX "loaded MM component OK (%p)", reinterpret_cast<void*>(comp));
  
  auto fact =
    make_itf_ref(
      static_cast<IMemory_manager_factory *>(comp->query_interface(IMemory_manager_factory::iid()))
    );

  /* create instance of memory manager */
  globals::mm = fact->create_mm_volatile_reconstituting(3);

  /* this is example code, so let's allocate some slab memory from the real allocator */
  size_t slab_size = MiB(32);
  globals::slab_memory = real::aligned_alloc(PAGE_SIZE, slab_size);
  globals::mm->add_managed_region(globals::slab_memory, slab_size);
  PLOG(PREFIX "allocated slab (%p)", globals::slab_memory);

  globals::intercept_active = true;
}

extern "C"
void * malloc(size_t size) {
  if(!real::malloc)
    __init_real_functions();

  if(!globals::intercept_active)
    return real::malloc(size);

  globals::alloc_count++;
  void * p = nullptr;
  PLOG(PREFIX "about to intercept: malloc(%lu)", size);
  asm("int3");
  if(globals::mm->allocate(size, &p) != S_OK)
    throw General_exception("mm->allocated failed");

  PLOG(PREFIX "intercept: malloc(%lu) ==> %p", size, p);
  return p;
}

extern "C"
void free(void * ptr) {
  if(!real::free)
    __init_real_functions();

  //  if(!globals::intercept_active || globals::alloc_count == 0)
  return real::free(ptr);


  PLOG(PREFIX "intercept: free(%p)", ptr);
  globals::mm->deallocate_without_size(ptr);
}


#pragma GCC diagnostic pop
