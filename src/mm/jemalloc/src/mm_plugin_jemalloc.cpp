#include <stdio.h>
#include <assert.h>
#include <common/errors.h>
#include <common/utils.h>
#include <jemalloc/jemalloc.h>
#include <stdarg.h>

#include <vector>

#include "logging.h"
#include "../../mm_plugin_itf.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wunused-parameter"

// forward decls
static void hook_extents(unsigned arena_id);
  
static unsigned next_arena_id = 1;
static constexpr unsigned MAX_HEAPS = 255;

class Heap;
Heap * g_heap_map[MAX_HEAPS] = {0};

int mallocx_flags = 0;

typedef void * aligned_2M_region_t;
  
class Heap
{
public:
  Heap(unsigned arena_id) : _arena_id(arena_id) {}

  void add_region(void * base, size_t len) {

    if(!check_aligned(base, MiB(2))) {
      PPERR("adding non 2MB-aligned region");
      return;
    }

    bool needs_hooking = (_managed_size == 0);
    byte * ptr = reinterpret_cast<byte*>(base);
    unsigned count = 0;
    while(len > MiB(2)) {
      _2M_aligned_free_regions.push_back(ptr);
      ptr += MiB(2);
      len -= MiB(2);
      count++;
    }
    _managed_size += MiB(2);

    if(needs_hooking)
      hook_extents(_arena_id);
      
    PPLOG("added %lu blocks @2M", count);
  }    

  void * allocate_2M() {
    if(_2M_aligned_free_regions.empty()) return nullptr;
    auto p = _2M_aligned_free_regions.back();
    _2M_aligned_free_regions.pop_back();
    _2M_aligned_used_regions.push_back(p);
    PPNOTICE("allocating %p", p);
    return p;
  }

  void set_arena(unsigned id) { _arena_id = id; }
  
private:
  size_t   _managed_size = 0;
  unsigned _mallocx_flags = 0;
  unsigned _arena_id;
  std::vector<aligned_2M_region_t> _2M_aligned_free_regions;
  std::vector<aligned_2M_region_t> _2M_aligned_used_regions;
};


void * custom_extent_alloc(extent_hooks_t *extent_hooks,
                           void *new_addr,
                           size_t size,
                           size_t alignment,
                           bool *zero,
                           bool *commit,
                           unsigned arena_id)
{  
  PPNOTICE("%s: new_addr=%p size=%lu alignment=%lu arena=%u",
           __func__, new_addr, size, alignment, arena_id);

  if(g_heap_map[arena_id] == nullptr) return nullptr;
  
  if(new_addr != nullptr) {
    PPERR("%s does not know how to handle predefined newaddr",__func__);
  }
  assert(alignment == MiB(2));
  assert(size == MiB(2));

  assert(arena_id < MAX_HEAPS);
  assert(g_heap_map[arena_id]);

  void * p = g_heap_map[arena_id]->allocate_2M();
  PPLOG("2M block at %p allocated", p);
  return p;
}

bool custom_extent_dalloc(extent_hooks_t *extent_hooks, void *addr, size_t size, bool committed, unsigned arena_ind)
{
  PPLOG("%s",__func__);  
  return true;
}

void custom_extent_destroy(extent_hooks_t *extent_hooks,
                           void *addr,
                           size_t size,
                           bool committed,
                           unsigned arena_ind)
{
  PPLOG("%s",__func__);
}

bool custom_extent_commit(extent_hooks_t *extent_hooks,
                          void *addr,
                          size_t size,
                          size_t offset,
                          size_t length,
                          unsigned arena_ind)
{
  PPLOG("%s",__func__);
  return false;
}

bool custom_extent_decommit(extent_hooks_t *extent_hooks,
                            void *addr,
                            size_t size,
                            size_t offset,
                            size_t length,
                            unsigned arena_ind)
{
  PPLOG("%s",__func__);
  return false;
}

bool custom_extent_purge(extent_hooks_t *extent_hooks,
                         void *addr,
                         size_t size,
                         size_t offset,
                         size_t length,
                         unsigned arena_ind)
{
  PPLOG("%s",__func__);
  return false;
}

bool custom_extent_split(extent_hooks_t *extent_hooks,
                         void *addr,
                         size_t size,
                         size_t size_a,
                         size_t size_b,
                         bool committed,
                         unsigned arena_ind)
{
  return true; /* leave as whole */
}

bool custom_extent_merge(extent_hooks_t *extent_hooks,
                         void *addr_a,
                         size_t size_a,
                         void *addr_b,
                         size_t size_b,
                         bool committed,
                         unsigned arena_ind)
{
  return true;  /* leave split */
}

extent_hooks_t custom_extent_hooks =
  {
   custom_extent_alloc,
   custom_extent_dalloc,
   custom_extent_destroy,
   custom_extent_commit,
   custom_extent_decommit,
   NULL, /*custom_extent_purge_lazy*/
   NULL, /*custom_extent_purge_forced */
   custom_extent_split,
   custom_extent_merge,
  };                            

status_t mm_plugin_init()
{
  PPLOG("init");

  unsigned nbins, i, narenas;
  size_t mib[4];
  size_t len, miblen;

  len = sizeof(nbins);
  jel_mallctl("arenas.nbins", &nbins, &len, NULL, 0);
  PPLOG("n-bins: %u", nbins);
  
  len = sizeof(narenas);
  jel_mallctl("opt.narenas", &narenas, &len, NULL, 0);
  PPLOG("n-arenas: %u", narenas);

  miblen = 4;
  jel_mallctlnametomib("arenas.bin.0.size", mib, &miblen);
  for (i = 0; i < nbins; i++) {
    size_t bin_size;

    mib[2] = i;
    len = sizeof(bin_size);
    jel_mallctlbymib(mib, miblen, &bin_size, &len, NULL, 0);
    PPLOG("bin size=%lu", bin_size);
  }

  /* disable tcache */
  {
    bool off = 0;
    size_t off_size = sizeof(off);
    if(jel_mallctl("thread.tcache.enabled",(void*)&off,&off_size,NULL,0)) {
      PPERR("error: disabling tcache");
      return E_FAIL;      
    }
    PPLOG("disabled tcache.");
  }
  
  return S_OK;
}

static void hook_extents(unsigned arena_id)
{
  size_t hooks_mib[3];
  size_t hooks_miblen;
  extent_hooks_t *new_hooks, *old_hooks;
  size_t old_size, new_size;
 
  /* unsigned arena_id = 0; */
  /* size_t arena_id_size = sizeof(arena_id); */

  hooks_miblen = sizeof(hooks_mib)/sizeof(size_t);

  /* get hold of mib entry */
  if(jel_mallctlnametomib("arena.0.extent_hooks", hooks_mib, &hooks_miblen)) {
    PPERR("getting MIB entry for arena.0.extent_hooks");
    return;
  }

  hooks_mib[1] = arena_id;
  old_size = sizeof(extent_hooks_t *);
  new_hooks = &custom_extent_hooks;
  new_size = sizeof(extent_hooks_t *);
    
  if(jel_mallctlbymib(hooks_mib, hooks_miblen, (void *)&old_hooks, &old_size, (void *)&new_hooks, new_size)) {
    PERR("new hook attach failed");
    return;
  }    
}


status_t mm_plugin_create(const char * params, mm_plugin_heap_t * out_heap)
{
  PPLOG("mm_plugin_create (%s)", params);

  auto arena_id = next_arena_id;
  auto new_heap = new Heap(next_arena_id);
  g_heap_map[arena_id] = new_heap;
  *out_heap = reinterpret_cast<mm_plugin_heap_t>(new_heap);

  next_arena_id++;
  return S_OK;
}

status_t mm_plugin_add_managed_region(mm_plugin_heap_t heap,
                                      void * region_base,
                                      size_t region_size)
{
  PPNOTICE("%s base=%p size=%lu",__func__, region_base, region_size);
  auto h = reinterpret_cast<Heap*>(heap);
  h->add_region(region_base, region_size);
  return S_OK;
}

status_t mm_plugin_query_managed_region(mm_plugin_heap_t heap,
                                        unsigned region_id,
                                        void** out_region_base,
                                        size_t* out_region_size)
{
  PPLOG("%s",__func__);
  return S_OK;
}

status_t mm_plugin_register_callback_request_memory(mm_plugin_heap_t heap,
                                                    request_memory_callback_t callback,
                                                    void * param)
{
  PPLOG("%s",__func__);
  return S_OK;
}

status_t mm_plugin_allocate(mm_plugin_heap_t heap, size_t n, void ** out_ptr)
{
  PPLOG("%s",__func__);
  void * ptr = jel_mallocx(n, mallocx_flags);
  assert(ptr);
  *out_ptr = ptr;
  return S_OK;
}

status_t mm_plugin_aligned_allocate(mm_plugin_heap_t heap, size_t n, size_t alignment, void ** out_ptr)
{
  PPLOG("%s",__func__);
  return S_OK;
}

status_t mm_plugin_aligned_allocate_offset(mm_plugin_heap_t heap, size_t n, size_t alignment, size_t offset, void ** out_ptr)
{
  PPLOG("%s",__func__);
  return S_OK;
}

status_t mm_plugin_deallocate(mm_plugin_heap_t heap, void * ptr, size_t size)
{
  PPLOG("%s",__func__);
  return S_OK;
}

status_t mm_plugin_deallocate_without_size(mm_plugin_heap_t heap, void * ptr)
{
  PPLOG("%s",__func__);
  return S_OK;
}

status_t mm_plugin_callocate(mm_plugin_heap_t heap, size_t n, void ** out_ptr)
{
  PPLOG("%s",__func__);
  return S_OK;
}

status_t mm_plugin_reallocate(mm_plugin_heap_t heap, void * ptr, size_t size, void ** out_ptr)
{
  PPLOG("%s",__func__);
  return S_OK;
}

status_t mm_plugin_usable_size(mm_plugin_heap_t heap, void * ptr, size_t * out_size)
{
  PPLOG("%s",__func__);
  return S_OK;
}

void mm_plugin_debug(mm_plugin_heap_t heap)
{
  PPLOG("%s",__func__);
}


#pragma GCC diagnostic pop
