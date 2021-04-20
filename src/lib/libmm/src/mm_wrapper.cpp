#include <dlfcn.h>
#include <stdio.h>
#include <common/logging.h>
#include <common/utils.h>
#include <common/exceptions.h>
#include <common/stack_trace.h>
#include <api/components.h>
#include <api/mm_itf.h>
#include <sys/mman.h>

#include "mm_wrapper.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"

#define PREFIX "MM-Wrapper: "
#define DEBUG_LEVEL 3

static void __get_os_functions(void);


namespace globals
{
static component::IMemory_manager_base * mm = nullptr; /* I assume this happens first? */
static void * slab_memory = nullptr; /*< memory which will be used as the slab for the allocator */
static unsigned alloc_count = 0;
static bool intercept_active = false;
}

namespace real
{
malloc_function_t        malloc = nullptr;
free_function_t          free = nullptr;
aligned_alloc_function_t aligned_alloc = nullptr;
realloc_function_t       realloc = nullptr;
calloc_function_t        calloc = nullptr;
memalign_function_t      memalign = nullptr;
}


static void __init_components(void)
{
  using namespace component;

  //  __get_os_functions();

#ifdef USE_MM_RCALB
  
  /* load MM component ; could use environment variable */
  IBase *comp = load_component("libcomponent-mm-rcalb.so", component::mm_rca_lb_factory);
  assert(comp);
  PLOG(PREFIX "loaded MM component OK (%p)", reinterpret_cast<void*>(comp));
  
  auto fact =
    //    make_itf_ref(
    static_cast<IMemory_manager_factory *>(comp->query_interface(IMemory_manager_factory::iid()));
  //    );
  
  /* create instance of memory manager */
  globals::mm = fact->create_mm_volatile_reconstituting(3);

  /* this is example code, so let's allocate some slab memory from the real allocator */
  size_t slab_size = MiB(128);
  globals::slab_memory = real::aligned_alloc(PAGE_SIZE, slab_size);
  globals::mm->add_managed_region(globals::slab_memory, slab_size);
  PLOG(PREFIX "allocated slab (%p)", globals::slab_memory);
#else
  /* default is to use passthru */
  IBase *comp = load_component("libcomponent-mm-passthru.so", component::mm_passthru_factory);
  assert(comp);
  PLOG(PREFIX "loaded MM component OK (%p)", reinterpret_cast<void*>(comp));
  
  auto fact =
    //    make_itf_ref(
    static_cast<IMemory_manager_factory *>(comp->query_interface(IMemory_manager_factory::iid()));
    //    );

  /* create instance of memory manager */
  globals::mm = fact->create_mm_volatile(3);
#endif

  globals::intercept_active = true;
}

/* OS versions of the memory functions */
extern "C" void * __wrap_malloc(size_t size)
{
  if(!real::malloc) __get_os_functions();
  return real::malloc(size);
}

extern "C" void __wrap_free(void * ptr)
{
  if(!real::free) __get_os_functions();
  return real::free(ptr);
}

extern "C" void * __wrap_calloc(size_t nmemb, size_t size)
{
  if(!real::calloc) __get_os_functions();
  return real::calloc(nmemb, size);
}

extern "C" void * __wrap_realloc(void * ptr, size_t size)
{
  if(!real::realloc)  __get_os_functions();
  return real::realloc(ptr, size);
}

extern "C" void * __wrap_memalign(size_t alignment, size_t size)
{
  PNOTICE("Calling __wrap_memalign");
  if(!real::memalign)  __get_os_functions();
  return real::memalign(alignment, size);
}

// extern "C" void * __wrap___cxa_allocate_exception(size_t thrown_size)
// {
//   return real::malloc(thrown_size);
// }

// extern "C" void __wrap___cxa_free_exception(void * thrown_exception)
// {
//   real::free(thrown_exception);
// }

static void * dl_libc = nullptr;

/** 
 * Collect the "original" OS implementations, so they can be used by
 * the memory manager plugin itself.
 * 
 */
static void __get_os_functions(void)
{
  real::calloc = reinterpret_cast<calloc_function_t>(dlsym(RTLD_NEXT, "calloc"));
  assert(real::calloc);

  real::malloc = reinterpret_cast<malloc_function_t>(dlsym(RTLD_NEXT, "malloc"));
  assert(real::malloc && (real::malloc != __wrap_malloc));
  
  real::free = reinterpret_cast<free_function_t>(dlsym(RTLD_NEXT, "free"));
  assert(real::free && (real::free != __wrap_free));
  
  real::aligned_alloc = reinterpret_cast<aligned_alloc_function_t>(dlsym(RTLD_NEXT, "aligned_alloc"));
  assert(real::aligned_alloc);

  real::realloc = reinterpret_cast<realloc_function_t>(dlsym(RTLD_NEXT, "realloc"));
  assert(real::realloc);

  real::memalign = reinterpret_cast<memalign_function_t>(dlsym(RTLD_NEXT, "memalign"));
  assert(real::memalign);

  /* initialize backend */
  __init_components();
}


#define EXPORT_C extern "C" __attribute__((visibility("default")))

EXPORT_C void   mm_free(void* p) noexcept;
EXPORT_C void*  mm_realloc(void* p, size_t newsize) noexcept;
EXPORT_C void*  mm_calloc(size_t count, size_t size) noexcept;
EXPORT_C void*  mm_malloc(size_t size) noexcept;
EXPORT_C size_t mm_usable_size(void* p) noexcept;
EXPORT_C void*  mm_valloc(size_t size) noexcept;
EXPORT_C void*  mm_pvalloc(size_t size) noexcept;
EXPORT_C void*  mm_reallocarray(void* p, size_t count, size_t size) noexcept;
EXPORT_C void*  mm_memalign(size_t alignment, size_t size) noexcept;
EXPORT_C int    mm_posix_memalign(void** p, size_t alignment, size_t size) noexcept;
EXPORT_C void*  mm_aligned_alloc(size_t alignment, size_t size) noexcept;


#include "alloc-override.c"


EXPORT_C void mm_free(void* p) noexcept
{
  if(!real::free) __get_os_functions();

  if(globals::intercept_active) {
    globals::mm->deallocate(p, 0 /* size is not known */);
  }
  else {
    /* intercept is not yet active */
    real::free(p); 
  }
}

EXPORT_C void* mm_realloc(void* p, size_t newsize) noexcept
{
  if(!real::realloc) __get_os_functions();

  if(globals::intercept_active) {
    void * new_ptr = nullptr;
    globals::mm->reallocate(p, newsize, &new_ptr);
    return new_ptr;
  }
  else {
    return real::realloc(p, newsize);
  }
}

EXPORT_C void* mm_calloc(size_t count, size_t size) noexcept
{  
  if(!real::calloc) {
    /* calloc is used by dl loader, so until we have it 
       interpositioned, we use sbrk.  Apparently, we could
       return null too */
    void * p = sbrk(count * size);
    memset(p, 0, count * size);
    return p;
  }
  
  void * p = nullptr;
  if(globals::intercept_active) {
     globals::mm->callocate(count * size, &p);
  }
  else {
    p = real::calloc(count, size);
  }
  PLOG("mm_calloc(%lu, %lu) -> %p", count, size, p);
  return p;
}

EXPORT_C void* mm_malloc(size_t size) noexcept
{
  if(!real::malloc) __get_os_functions();

  if(globals::intercept_active) {

    void * p = nullptr;
    if(globals::mm->allocate(size, &p) != S_OK) {
      PWRN("globals::mm->allocate failed to allocate(%lu)", size);
      return nullptr;
    }

    return p;
  }
  else {
    /* intercept is not yet active */
    return sbrk(size);
  }
}

EXPORT_C size_t mm_usable_size(void* p) noexcept
{
  if(p == nullptr) return 0;
  size_t us = 0;
  if(globals::mm->usable_size(p, &us) != S_OK) {
    PWRN("globals::mm->usable_size() failed");
    return 0;
  }
  return us;
}

/* valloc is Linux deprecated */
EXPORT_C void* mm_valloc(size_t size) noexcept
{
  void * p = nullptr;
  if(size == 0) return nullptr;
  status_t s = globals::mm->aligned_allocate(size, sysconf(_SC_PAGESIZE), &p);
  if(s != S_OK) return nullptr; /* should really look at errno? */
  return p;
}

EXPORT_C void* mm_pvalloc(size_t size) noexcept
{
  void * p = nullptr;
  if(size == 0) return nullptr;
  status_t s = globals::mm->aligned_allocate(round_up(size, sysconf(_SC_PAGESIZE)), sysconf(_SC_PAGESIZE), &p);
  if(s != S_OK) return nullptr; /* should really look at errno? */
  return p;
}

EXPORT_C void* mm_reallocarray(void* p, size_t count, size_t size) noexcept
{
  return mm_realloc(p, count * size);
}

EXPORT_C void* mm_memalign(size_t alignment, size_t size) noexcept
{
  void * p = nullptr;
  if(size == 0) return nullptr;
  status_t s = globals::mm->aligned_allocate(size, alignment, &p);
  if(s != S_OK) return nullptr; /* should really look at errno? */
  return p;
}

EXPORT_C int mm_posix_memalign(void** p, size_t alignment, size_t size) noexcept
{
  if(p == nullptr) return EINVAL;
  status_t s = globals::mm->aligned_allocate(size, alignment, p);
  if(s != S_OK) {
    if(s == E_NO_MEM) return ENOMEM;
    return EINVAL;
  }
  return 0;
}

EXPORT_C void* mm_aligned_alloc(size_t alignment, size_t size) noexcept
{
  void * p = nullptr;
  status_t s = globals::mm->aligned_allocate(size, alignment, &p);
  if(s != S_OK) {
    if(s == E_NO_MEM) return ENOMEM;
    return EINVAL;
  }
  return 0;
}


#pragma GCC diagnostic pop
