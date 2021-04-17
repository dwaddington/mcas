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

static void __copy_real_functions(void);


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
free_function_t          free;
aligned_alloc_function_t aligned_alloc;
realloc_function_t       realloc;
}


static __attribute__((constructor)) void __init_components(void)
{
  PNOTICE(PREFIX "__init_components??");
  using namespace component;

  // void * module = dlopen("/home/danielwaddington/mcas/build/src/components/mm/dummy/libcomponent-mm-dummy.so", RTLD_NOW);

  // PLOG(PREFIX "__init_components: module(%p)", module);

  // foo_malloc = reinterpret_cast<malloc_function_t>(dlsym(module, "foo_malloc"));
  // assert(foo_malloc);

  // PLOG(PREFIX "__init_components: foo_malloc(%p)", reinterpret_cast<void*>(foo_malloc));

  // don't close until your done with it!
  //  dlclose(module);

  __copy_real_functions();

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
  size_t slab_size = MiB(128);
  globals::slab_memory = real::aligned_alloc(PAGE_SIZE, slab_size);
  globals::mm->add_managed_region(globals::slab_memory, slab_size);
  PLOG(PREFIX "allocated slab (%p)", globals::slab_memory);

  globals::intercept_active = true;
}

/* safe version of malloc */
extern "C" void * __wrap_malloc(size_t size)
{
  if(!real::malloc) __copy_real_functions();
  return real::malloc(size);
}

extern "C" void __wrap_free(void * ptr)
{
  if(!real::free) __copy_real_functions();
  return real::free(ptr);
}

extern "C" void * __wrap___cxa_allocate_exception(size_t thrown_size)
{
  return real::malloc(thrown_size);
}

extern "C" void __wrap___cxa_free_exception(void * thrown_exception)
{
  real::free(thrown_exception);
}



static void __copy_real_functions(void)
{
  real::malloc = reinterpret_cast<malloc_function_t>(dlsym(RTLD_NEXT, "malloc"));
  assert(real::malloc && (real::malloc != __wrap_malloc));
  
  real::free = reinterpret_cast<free_function_t>(dlsym(RTLD_NEXT, "free"));
  assert(real::free && (real::free != __wrap_free));
  
  real::aligned_alloc = reinterpret_cast<aligned_alloc_function_t>(dlsym(RTLD_NEXT, "aligned_alloc"));
  assert(real::aligned_alloc);

  real::realloc = reinterpret_cast<realloc_function_t>(dlsym(RTLD_NEXT, "realloc"));
  assert(real::realloc);
}


#define EXPORT_C extern "C" __attribute__((visibility("default")))

EXPORT_C void* mm_valloc(size_t size) noexcept;
EXPORT_C void* mm_pvalloc(size_t size) noexcept;
EXPORT_C void* mm_memalign(size_t alignment, size_t size) noexcept;
EXPORT_C void* mm_reallocarray(void* p, size_t count, size_t size) noexcept;
EXPORT_C int   mm_posix_memalign(void** p, size_t alignment, size_t size) noexcept;
EXPORT_C void* mm_aligned_alloc(size_t alignment, size_t size) noexcept;
EXPORT_C void  mm_free(void* p) noexcept;
EXPORT_C void* mm_realloc(void* p, size_t newsize) noexcept;
EXPORT_C void* mm_calloc(size_t count, size_t size) noexcept;
EXPORT_C void* mm_malloc(size_t size) noexcept;
EXPORT_C size_t mm_usable_size(const void* p) noexcept;
EXPORT_C void* mm_reallocf(void* p, size_t newsize) noexcept;


#include "alloc-override.c"


EXPORT_C void mm_free(void* p) noexcept
{
  if(!real::free) __copy_real_functions();

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
  if(!real::realloc) __copy_real_functions();

  return real::realloc(p, newsize);
}

EXPORT_C void* mm_calloc(size_t count, size_t size) noexcept
{
  if(!real::malloc) __copy_real_functions();
  PLOG("mm_calloc(%lu, %lu)", count, size);

  void * p = mm_malloc(count * size);
  memset(p, 0, count * size);
  return p;
}

EXPORT_C void* mm_malloc(size_t size) noexcept
{
  if(!real::malloc) __copy_real_functions();

  if(globals::intercept_active) {

    void * p = nullptr;
    if(globals::mm->allocate(size, &p) != S_OK) {
      return nullptr;
    }

    return p;
  }
  else {
    /* intercept is not yet active */
    return sbrk(size);
  }
}

EXPORT_C size_t mm_usable_size(const void* p) noexcept
{
  PLOG("mm_usable_size(%p)", p);

  return 100000000;
}

EXPORT_C void* mm_reallocf(void* p, size_t newsize) noexcept
{
  PLOG("mm_reallocf(%p, %lu)", p, newsize);
  asm("int3");
  return nullptr;
}

// static __attribute__((destructor))
// void __dtor(void)
// {
//   real::free(globals::slab_memory);
// }


  
// static __attribute__((constructor)) // can't seem to get this to happen early enough!
// void __init_mm(void)
// {
//   using namespace component;
//   PLOG(PREFIX "__init_mm");
  
//   /* load MM component ; could use environment variable */
//   IBase *comp = load_component("libcomponent-mm-rcalb.so",
//                                component::mm_rca_lb_factory);
//   assert(comp);
//   PLOG(PREFIX "loaded MM component OK (%p)", reinterpret_cast<void*>(comp));
  
//   auto fact =
//     make_itf_ref(
//       static_cast<IMemory_manager_factory *>(comp->query_interface(IMemory_manager_factory::iid()))
//     );

//   /* create instance of memory manager */
//   globals::mm = fact->create_mm_volatile_reconstituting(3);

//   /* this is example code, so let's allocate some slab memory from the real allocator */
//   size_t slab_size = MiB(32);
//   globals::slab_memory = real::aligned_alloc(PAGE_SIZE, slab_size);
//   globals::mm->add_managed_region(globals::slab_memory, slab_size);
//   PLOG(PREFIX "allocated slab (%p)", globals::slab_memory);

//   globals::intercept_active = true;
// }

// extern "C"
// void * malloc(size_t size) {
//   if(!real::malloc)
//     __copy_real_functions();

//   //  if(!globals::intercept_active)
//   return real::malloc(size);

//   globals::alloc_count++;
//   void * p = nullptr;
//   PLOG(PREFIX "about to intercept: malloc(%lu)", size);

//   if(globals::mm->allocate(size, &p) != S_OK)
//     throw General_exception("mm->allocated failed");

//   PLOG(PREFIX "intercept: malloc(%lu) ==> %p", size, p);
//   return p;
// }

// extern "C"
// void free(void * ptr) {
//   if(!real::free)
//     __copy_real_functions();

//   //  if(!globals::intercept_active || globals::alloc_count == 0)
//   return real::free(ptr);


//   PLOG(PREFIX "intercept: free(%p)", ptr);
//   globals::mm->deallocate_without_size(ptr);
// }

// // void* mi_new_nothrow(size_t size) noexcept {
// //   asm("int3"); retir
// //   void* p = mi_malloc(size);
// //   if (mi_unlikely(p == NULL)) return mi_try_new(size, true);
// //   return p;
// // }


#pragma GCC diagnostic pop
