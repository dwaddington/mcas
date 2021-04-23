#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

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
#include "mm_plugin_itf.h"
#include "safe_print.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-function"

#define PREFIX "MM-Wrapper: "

static void __get_os_functions(void);

//static const char * PLUGIN_PATH = "/home/danielwaddington/mcas/build/src/mm/passthru/libmm-plugin-passthru.so";
static const char * PLUGIN_PATH = "/home/danielwaddington/mcas/build/dist/lib/libmm-plugin-jemalloc.so";

namespace globals
{
static void * slab_memory = nullptr; /*< memory which will be used as the slab for the allocator */
static unsigned alloc_count = 0;
static bool intercept_active = false;
}

static mm_plugin_function_table_t __mm_funcs;
static void * __mm_plugin_module;
static mm_plugin_heap_t __mm_heap;
#define LOAD_SYMBOL(X) __mm_funcs.X = reinterpret_cast<typeof(__mm_funcs.X)>(dlsym(__mm_plugin_module, # X)); assert(__mm_funcs.X)

/* real function implementations */
namespace real
{
malloc_function_t        malloc = nullptr;
free_function_t          free = nullptr;
aligned_alloc_function_t aligned_alloc = nullptr;
realloc_function_t       realloc = nullptr;
calloc_function_t        calloc = nullptr;
memalign_function_t      memalign = nullptr;
vfprintf_function_t      vfprintf = nullptr;
puts_function_t          puts = nullptr;

malloc_usable_size_function_t malloc_usable_size = nullptr;
}

static void __init_components(void)
{
  using namespace component;

  __mm_plugin_module = dlopen(PLUGIN_PATH, RTLD_NOW | RTLD_DEEPBIND);

  if(!__mm_plugin_module) printf("Error: %s\n", dlerror());
  assert(__mm_plugin_module);
  
  LOAD_SYMBOL(mm_plugin_init);
  LOAD_SYMBOL(mm_plugin_create);
  LOAD_SYMBOL(mm_plugin_add_managed_region);
  LOAD_SYMBOL(mm_plugin_query_managed_region);
  LOAD_SYMBOL(mm_plugin_register_callback_request_memory);
  LOAD_SYMBOL(mm_plugin_allocate);
  LOAD_SYMBOL(mm_plugin_aligned_allocate);
  LOAD_SYMBOL(mm_plugin_aligned_allocate_offset);
  LOAD_SYMBOL(mm_plugin_deallocate);
  LOAD_SYMBOL(mm_plugin_deallocate_without_size);
  LOAD_SYMBOL(mm_plugin_callocate);
  LOAD_SYMBOL(mm_plugin_reallocate);
  LOAD_SYMBOL(mm_plugin_usable_size);
  LOAD_SYMBOL(mm_plugin_debug);

  __mm_funcs.mm_plugin_init();  
  __mm_funcs.mm_plugin_create(nullptr, &__mm_heap);

  /* give some memory */
#if 1
  size_t slab_size = MiB(256);
  globals::slab_memory = real::aligned_alloc(MiB(2), slab_size);
  
  __mm_funcs.mm_plugin_add_managed_region(__mm_heap,
                                          globals::slab_memory,
                                          slab_size);
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
  
//   void * p = sbrk(nmemb * size);
  
  // if(!real::calloc) __get_os_functions();
  // return real::calloc(nmemb, size);
  if(!real::malloc) __get_os_functions();
  void * p = real::malloc(size * nmemb);
  __builtin_memset(p, 0, size * nmemb);
  return p;
}

extern "C" void * __wrap_realloc(void * ptr, size_t size)
{
  if(!real::realloc)  __get_os_functions();
  return real::realloc(ptr, size);
}

extern "C" void * __wrap_memalign(size_t alignment, size_t size)
{
  if(!real::memalign)  __get_os_functions();
  return real::memalign(alignment, size);
}

extern "C" int __wrap_puts(const char *s)
{
  if(!real::puts)  __get_os_functions();
  return real::puts(s);
}

extern "C" size_t __wrap_malloc_usable_size(void * ptr)
{
  if(!real::malloc_usable_size) __get_os_functions();
  return real::malloc_usable_size(ptr);
}

/** 
 * Collect the "original" OS implementations, so they can be used by
 * the memory manager plugin itself.
 * 
 */
static void __get_os_functions()
{
  real::malloc = reinterpret_cast<malloc_function_t>(dlsym(RTLD_NEXT, "malloc"));
  assert(real::malloc && (real::malloc != __wrap_malloc));

  real::calloc = reinterpret_cast<calloc_function_t>(dlsym(RTLD_NEXT, "calloc"));
  assert(real::calloc);
  
  real::free = reinterpret_cast<free_function_t>(dlsym(RTLD_NEXT, "free"));
  assert(real::free && (real::free != __wrap_free));
  
  real::aligned_alloc = reinterpret_cast<aligned_alloc_function_t>(dlsym(RTLD_NEXT, "aligned_alloc"));
  assert(real::aligned_alloc);

  real::realloc = reinterpret_cast<realloc_function_t>(dlsym(RTLD_NEXT, "realloc"));
  assert(real::realloc);

  real::memalign = reinterpret_cast<memalign_function_t>(dlsym(RTLD_NEXT, "memalign"));
  assert(real::memalign);

  real::vfprintf = reinterpret_cast<vfprintf_function_t>(dlsym(RTLD_NEXT, "vfprintf"));
  assert(real::vfprintf);

  real::puts = reinterpret_cast<puts_function_t>(dlsym(RTLD_NEXT, "puts"));
  assert(real::puts);

  real::malloc_usable_size = reinterpret_cast<malloc_usable_size_function_t>(dlsym(RTLD_NEXT, "malloc_usable_size"));
  assert(real::malloc_usable_size);
  

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
  if(!real::free) return; //__get_os_functions();

  if(globals::intercept_active) {
    __mm_funcs.mm_plugin_deallocate_without_size(__mm_heap, p);
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
    __mm_funcs.mm_plugin_reallocate(__mm_heap, p, newsize, &new_ptr);
    return new_ptr;
  }
  else {
    return real::realloc(p, newsize);
  }
}

EXPORT_C void* mm_calloc(size_t count, size_t size) noexcept
{
  /* dlopen / dlsym use calloc */
  if(!real::calloc) {
    SAFE_PRINT("Real calloc not ready\n");
    /* use poor man's calloc, because dlopen uses calloc */
    void * p = sbrk(count*size);
    __builtin_memset(p, 0, count * size);
    return p;
  }
  SAFE_PRINT("Real calloc\n");
  return real::calloc(count, size);
  // void * p;
  // if(globals::intercept_active) {
  //   p = nullptr;
  //   //    __mm_funcs.mm_plugin_callocate(__mm_heap, count * size, &p);
  //   __mm_funcs.mm_plugin_allocate(__mm_heap, count * size, &p);
  //   assert(p);
  // }
  // else {
  //   p = real::calloc(count, size);
  // }

  // return p;
}

EXPORT_C void* mm_malloc(size_t size) noexcept
{
  if(!real::malloc) __get_os_functions();

  if(globals::intercept_active) {

    void * p = nullptr;
    __mm_funcs.mm_plugin_allocate(__mm_heap, size, &p);
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
  // if(globals::mm->usable_size(p, &us) != S_OK) {
  //   PWRN("globals::mm->usable_size() failed");
  //   return 0;
  // }
  return us;
}

/* valloc is Linux deprecated */
EXPORT_C void* mm_valloc(size_t size) noexcept
{
  void * p = nullptr;
  if(size == 0) return nullptr;

  __mm_funcs.mm_plugin_aligned_allocate(__mm_heap, size, sysconf(_SC_PAGESIZE), &p);
  return p;
}

EXPORT_C void* mm_pvalloc(size_t size) noexcept
{
  void * p = nullptr;
  if(size == 0) return nullptr;
  __mm_funcs.mm_plugin_aligned_allocate(__mm_heap, round_up(size, sysconf(_SC_PAGESIZE)), sysconf(_SC_PAGESIZE), &p);
  return p;
}

EXPORT_C void* mm_reallocarray(void* p, size_t count, size_t size) noexcept
{
  return mm_realloc(p, count * size);
}

EXPORT_C void* mm_memalign(size_t alignment, size_t size) noexcept
{
  if(size == 0) return nullptr;
  void * p = nullptr;
  __mm_funcs.mm_plugin_aligned_allocate(__mm_heap, size, alignment, &p);
  return p;
}

EXPORT_C int mm_posix_memalign(void** p, size_t alignment, size_t size) noexcept
{
  if(p == nullptr) return EINVAL;
  __mm_funcs.mm_plugin_aligned_allocate(__mm_heap, size, alignment, p);
  return 0;
}

EXPORT_C void* mm_aligned_alloc(size_t alignment, size_t size) noexcept
{
  void * p = nullptr;
  __mm_funcs.mm_plugin_aligned_allocate(__mm_heap, size, alignment, &p);
  return p;
}


#pragma GCC diagnostic pop
