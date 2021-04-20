/*
   Copyright [2021] [IBM Corporation]
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
       http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

/*
 * Authors:
 *
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */
#ifndef __API_MM_ITF__
#define __API_MM_ITF__

#include <api/itf_ref.h>
#include <api/components.h>
#include <common/types.h>

/** 
 * Please keep these interfaces based around C types so that they can
 * be easily wrapped in a pure C interface.  Error handling should be
 * status_t return codes.  Pointers only, no references.  This
 * approach makes it easier to implement allocators in other languages
 * such as Rust.
 */

namespace component
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Weffc++"


/** 
 * Base interface. Methods that all allocators should support.
 */
class IMemory_manager_base : public component::IBase {

public:
  /*< call back function to request more memory for slab */
  typedef void (*request_memory_callback_t)(void * param, size_t alignment, size_t size_hint, void * addr_hint);

  /*< call back function for allocator to release memory back to OS */
  typedef void (*release_memory_callback_t)(void * param, void * addr, size_t size);
  
public:
  
  /** 
   * Add (slab) region of memory to fuel the allocator.  The passthru
   * allocator does not need this as it takes directly from the OS.
   * 
   * @param region_base Pointer to beginning of region
   * @param region_size Region length in bytes
   * 
   * @return E_NOT_IMPL, S_OK, E_FAIL, E_INVAL
   */
  virtual status_t add_managed_region(void * region_base,
                                      size_t region_size) {
    return E_NOT_IMPL;
  }

  /** 
   * Query memory regions being used by the allocator
   * 
   * @param region_id Index counting from 0
   * @param [out] Base address of region
   * @param [out] Size of region in bytes
   * 
   * @return S_MORE (continue to next region id), S_OK (done), E_INVAL
   */  
  virtual status_t query_managed_region(unsigned region_id,
                                        void** out_region_base,
                                        size_t* out_region_size) {
    return E_NOT_IMPL;
  }

  /** 
   * Register callback the allocator can use to request more memory
   * 
   * @param callback Call back function pointer
   * @param param Optional parameter which will be pass to callback function
   * 
   * @return E_NOT_IMPL, S_OK
   */  
  virtual status_t register_callback_request_memory(request_memory_callback_t callback,
                                                    void * param = nullptr) {
    return E_NOT_IMPL;
  }

  /** 
   * Register callback the allocator can use to release memory back to OS
   * 
   * @param callback Call back function pointer
   * @param param Optional parameter which will be pass to callback function
   * 
   * @return E_NOT_IMPL, S_OK
   */  
  virtual status_t register_callback_release_memory(release_memory_callback_t callback,
                                                    void * param = nullptr) {
    return E_NOT_IMPL;
  }
  
  /** 
   * Allocate a region of memory without alignment or hint
   * 
   * @param Length in bytes
   * @param [out] Pointer to allocated region
   *
   * @return S_OK or E_FAIL
   */
  virtual status_t allocate(size_t n, void ** out_ptr) = 0;

  /** 
   * Allocation region of memory that is aligned
   * 
   * @param n Size of region in bytes
   * @param alignment Alignment in bytes
   * @param out_ptr [out] Pointer to allocated region
   * 
   * @return S_OK, E_FAIL, E_INVAL (depending on implementation)
   */
  virtual status_t aligned_allocate(size_t n, size_t alignment, void ** out_ptr) = 0;

  /** 
   * Special case for EASTL
   * 
   * @param n Size of region in bytes
   * @param alignment Alignment in bytes
   * @param offset Unknown
   * 
   * @return 
   */
  virtual status_t aligned_allocate(size_t n, size_t alignment, size_t offset, void ** out_ptr) {
    return E_NOT_IMPL;
  }

  /** 
   * Free a previously allocated region of memory with length known
   * 
   * @param ptr Pointer to previously allocated region
   * @param size Length of region in bytes
   *
   * @return S_OK or E_INVAL;
   */
  virtual status_t deallocate(void * ptr, size_t size) = 0;

  /** 
   * Free previously allocated region without known length
   * 
   * @param ptr Pointer to region
   * 
   * @return S_OK
   */
  virtual status_t deallocate_without_size(void * ptr) {
    return E_NOT_IMPL;
  }

  /** 
   * Allocate region and zero memory
   * 
   * @param size Size of region in bytes
   * @param ptr [out] Pointer to allocated region
   * 
   * @return S_OK
   */
  virtual status_t callocate(size_t n, void ** out_ptr)  {
    return E_NOT_IMPL;
  }

  /** 
   * Resize an existing allocation
   * 
   * @param ptr Pointer to existing allocated region
   * @param size New size in bytes
   * @param ptr [out] New reallocated region or null on unable to reallocate
   * 
   * @return S_OK
   */
  virtual status_t reallocate(void * ptr, size_t size, void ** out_ptr) {
    return E_NOT_IMPL;
  }

  /** 
   * Get the number of usable bytes in block pointed to by ptr.  The
   * allocator *may* not have this information and should then return
   * E_NOT_IMPL. Returned size may be larger than requested allocation
   * 
   * @param ptr Pointer to block base
   * 
   * @return Number of bytes in allocated block
   */
  virtual status_t usable_size(void * ptr, size_t * out_size) {
    return E_NOT_IMPL;
  }
  
  /** 
   * [optional] Get debugging information
   * 
   */
  virtual void debug_dump() {}

};

/**
 * IMemory_manager_volatile - memory manager for volatile memory
 */
class IMemory_manager_volatile : public IMemory_manager_base {
 public:
  DECLARE_INTERFACE_UUID(0x69bcb3bb, 0xf4d9, 0x4437, 0x9fc7, 0x9f, 0xc7, 0xf0, 0x7d, 0x39, 0xe9);

  // TODO
};


/**
 * IMemory_manager_volatile_reconstituting - memory manager that
 * resides in volatile memory but is reconstituted from persistent
 * memory. NUMA properties are managed as a different concern.
 */
class IMemory_manager_volatile_reconstituting : public IMemory_manager_base {
 public:
  DECLARE_INTERFACE_UUID(0xe9ec587f, 0x1328, 0x480e, 0xa92a, 0x7b, 0xa5, 0xdf, 0xa5, 0x3d, 0xd6);

  /** 
   * Re-inject state of a block (used on recovery from persistent state)
   * 
   * @param ptr Allocation base
   * @param size Size of allocation in bytes
   *
   * @return S_OK or E_INVAL
   */
  virtual status_t inject_allocation(void * ptr, size_t size) = 0;

};


/**
 * IMemory_manager_persistent - memory manager that
 * resides in persistent memory and is crash-consistent
 */
class IMemory_manager_persistent : public IMemory_manager_base {
 public:
  DECLARE_INTERFACE_UUID(0x740e5b5c, 0x6f2b, 0x478b, 0x90ac, 0x28, 0x7e, 0x83, 0x88, 0x91, 0x05);

  // TODO
};



/** 
 * Factory for memory managers
 * 
 */

class IMemory_manager_factory : public component::IBase {
 public:

  DECLARE_INTERFACE_UUID(0xfaccb3bb, 0xf4d9, 0x4437, 0x9fc7, 0x9f, 0xc7, 0xf0, 0x7d, 0x39, 0xe9);

  /* factories */
  virtual IMemory_manager_volatile *
  create_mm_volatile(const unsigned debug_level) = 0;

  virtual IMemory_manager_volatile_reconstituting *
  create_mm_volatile_reconstituting(const unsigned debug_level) = 0;
  
  virtual IMemory_manager_persistent *
  create_mm_persistent(const unsigned debug_level) = 0;
};

}  // namespace component


#include <common/logging.h>
namespace mm
{

/** 
 * Template to wrap around IMemory_xxx interfaces for Std C++ use case.  This template
 * takes the backend interface type as a template parameter.
 * 
 */
template<typename T>
struct pointer_traits {
  using reference = T &;
  using const_reference = const T &;
};

template<>
struct pointer_traits<void> {
};

template<typename T = void, typename MM_interface = component::IMemory_manager_volatile_reconstituting>
struct allocator_adaptor : public pointer_traits<T> {
public:
  using value_type = T;
  using size_type = std::size_t;
  using pointer = value_type *;
  using difference_type = typename std::pointer_traits<pointer>::difference_type;
  using const_pointer = typename std::pointer_traits<pointer>::template
    rebind<value_type const>;
  using void_pointer       = typename std::pointer_traits<pointer>::template
    rebind<void>;
  using const_void_pointer = typename std::pointer_traits<pointer>::template
    rebind<const void>;

  allocator_adaptor(MM_interface * mm) noexcept
    : _mm(mm) {
    assert(_mm);
    _mm->add_ref();
  }
  
  ~allocator_adaptor() noexcept {
    _mm->release_ref();
  }

  template<typename U>
  allocator_adaptor(const allocator_adaptor<U> &) noexcept = delete;

  allocator_adaptor &operator=(const allocator_adaptor &a_) = delete;

  value_type * allocate(std::size_t n, const_void_pointer hint)  {
    return allocate(n);
  }

  value_type * allocate(std::size_t n)  {
    void * ptr = nullptr;
    return (_mm->allocate(n, &ptr) == S_OK) ? static_cast<value_type*>(ptr) : nullptr;
  }

  value_type * aligned_allocate(std::size_t n, std::size_t alignment) {
    void * ptr = nullptr;
    return (_mm->aligned_allocate(n, alignment, &ptr) == S_OK) ?
      static_cast<value_type*>(ptr) : nullptr;
  }

  void deallocate(value_type * ptr, std::size_t size) {
    _mm->deallocate(ptr, size);
  }

  status_t add_managed_region(void * region_base,
                              size_t region_length) {
    return _mm->add_managed_region(region_base, region_length);
  }

  template<typename U>
  struct rebind {
    typedef allocator_adaptor<U> other;
  };

private:

  MM_interface * _mm;
};


}


#pragma GCC diagnostic pop

#endif
