#ifndef __NUPM_CCPM_SLAB_H__
#define __NUPM_CCPM_SLAB_H__

#include <common/utils.h>
#include <exception>

namespace ccpm
{

template<typename NodeT>
class Slab
{
  static constexpr uint32_t MAGIC = 0x51ab0000;
  
private:

  struct Slab_struct {
    uint32_t      magic;
    uint32_t      resvd;
    uint64_t      size;
    Slab<NodeT> * next_slab;
    void *        slab_head;
    void *        heap_tail;
  } * _root __attribute__((packed));
  
public:

  /** 
   * Initializing constructor 
   * 
   * @param buffer Pointer to area of memory to use
   * @param size Size of memory area in bytes
   */
  Slab(void * buffer, size_t size) : _root(static_cast<Slab_struct*>(buffer))
  {
    if(!check_aligned(buffer, 64))
      throw std::invalid_argument("object instance is not 64bit aligned");

    if(size <= sizeof(Slab_struct))
      throw std::bad_alloc();

    _root->magic = MAGIC;
    _root->heap_tail = reinterpret_cast<byte *>(_root) + sizeof(Slab_struct);
    _root->slab_head = reinterpret_cast<byte *>(_root) + size;
  }

  /** 
   * Reconstituting constructor
   * 
   * @param buffer Existing (persistent) memory region
   */
  Slab(void * buffer) : _root(static_cast<Slab_struct*>(buffer)) {
    if(_root->magic != MAGIC)
      throw std::bad_alloc();

    if(_root->heap_tail >= _root->slab_tail)
      throw std::logic_error("heap tail > slab tail");
  }

  NodeT * allocate() {
    return nullptr;
  }

  void * heap_base() const {
    return &_root[1];
  }

  /** 
   * Return available space in terms of node type
   * 
   * 
   * @return Space in nodes
   */
  size_t free_node_space() const {    
    return (static_cast<unsigned long>(_root->slab_head) - static_cast<unsigned long>(_root->heap_tail)) / sizeof(NodeT);
  }
  
  void check() {
  };
};

}

#endif // __NUPM_CCPM_SLAB_H__
