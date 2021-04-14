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
 * Please keep these interfaces based around C types so that they can be easily wrapped
 * in a pure C interface.
 */

namespace component
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Weffc++"

/**
 * IMemory_manager_volatile - memory manager for volatile memory
 */
class IMemory_manager_volatile : public component::IBase {
 public:
  DECLARE_INTERFACE_UUID(0x69bcb3bb, 0xf4d9, 0x4437, 0x9fc7, 0x9f, 0xc7, 0xf0, 0x7d, 0x39, 0xe9);

  virtual void print_info() = 0;
  // TODO
};


/**
 * IMemory_manager_volatile_reconstituting - memory manager that
 * resides in volatile memory but is reconstituted from persistent
 * memory
 */
class IMemory_manager_volatile_reconstituting : public component::IBase {
 public:
  DECLARE_INTERFACE_UUID(0xe9ec587f, 0x1328, 0x480e, 0xa92a, 0x7b, 0xa5, 0xdf, 0xa5, 0x3d, 0xd6);

  virtual void print_info() = 0;

  /** 
   * Add (slab) region of memory to fuel the allocator
   * 
   * @param region_base 
   * @param region_length 
   * @param numa_node 
   * 
   * @return S_OK or E_FAIL
   */
  virtual status_t add_managed_region(void * region_base,
                                      size_t region_length,
                                      int    numa_node) = 0;    
  
  /** 
   * Allocate a region of memory of 'n' contiguous bytes without alignment of hint
   * 
   * @param Length in bytes
   * 
   * @return Pointer to allocated region
   */
  virtual void * allocate(size_t n) = 0;

  /** 
   * Free a previously allocated region of memory with length known
   * 
   * @param ptr Pointer to previously allocated region
   * @param size Length of region in bytes
   */
  virtual void deallocate(void * ptr, size_t size) = 0;
};


/**
 * IMemory_manager_persistent - memory manager that
 * resides in persistent memory and is crash-consistent
 */
class IMemory_manager_persistent : public component::IBase {
 public:
  DECLARE_INTERFACE_UUID(0x740e5b5c, 0x6f2b, 0x478b, 0x90ac, 0x28, 0x7e, 0x83, 0x88, 0x91, 0x05);

  virtual void print_info() = 0;
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
  virtual IMemory_manager_volatile * create_mm_volatile(const unsigned debug_level) = 0;
  virtual IMemory_manager_volatile_reconstituting * create_mm_volatile_reconstituting(const unsigned debug_level) = 0;
  virtual IMemory_manager_persistent * create_mm_persistent(const unsigned debug_level) = 0;
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
    return static_cast<value_type*>(_mm->allocate(n));
  }

  void deallocate(value_type * ptr, size_t size) {
    _mm->deallocate(ptr, size);
  }

  status_t add_managed_region(void * region_base,
                              size_t region_length,
                              int    numa_node) {
    return _mm->add_managed_region(region_base, region_length, numa_node);
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
