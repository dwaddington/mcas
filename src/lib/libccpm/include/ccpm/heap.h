/*
  Copyright [2020] [IBM Corporation]
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
#ifndef __CCPM_HEAP_H__
#define __CCPM_HEAP_H__

#include <string>
#include <api/mcas_itf.h>
#include <ccpm/heap.h>

namespace ccpm
{

/** 
 * Helper to allocate memory at specific address
 * 
 * @param size Size of region to allocate
 * @param target_addr Target virtual address
 * 
 * @return Pointer to allocated memory
 */
void * allocate_at(size_t size, const addr_t target_addr);


/** 
 * Free memory allocated with allocate_at
 * 
 * @param p Pointer to region to free
 * @param length Size of region to free
 * 
 * @return 0 on success
 */
int free_at(void * p, size_t length);


template <typename T>
class heap
{
public:
  heap(component::IMCAS * mcas,
       const component::IMCAS::pool_t pool,
       std::string name,
       size_t size)
    : _name(name), _mcas(mcas), _pool(pool), _size(size)
  {
    assert(size > sizeof(T));
    if(mcas == nullptr) throw std::invalid_argument("bad parameter");

    /* reserve space for data structure on MCAS server */
    std::vector<component::IMCAS::ADO_response> response;

    status_t rc = mcas->invoke_ado(pool,
                                   name,
                                   nullptr,
                                   0,
                                   component::IMCAS::ADO_FLAG_CREATE_ONLY,
                                   response,
                                   size);
    if(rc != S_OK)
      throw General_exception("invoke_ado failed %d",rc);

    assert(response.size() == 1);

    /* get virtual address from response */
    uint64_t value_vaddr = *(reinterpret_cast<const uint64_t*>(response[0].data()));
    PLOG("returned address: %lx", value_vaddr);

    /* allocate matching memory locally and populate if needed */
    _root = reinterpret_cast<T*>(allocate_at(size, value_vaddr));

    _regions.push_back({&_root[1], size - sizeof(T)});

    /* register for RDMA */
    _memory_handle = mcas->register_direct_memory(_root, size);
    PLOG("registered direct memory (%p, %lu)", _root, size);
    assert(_memory_handle != component::IMCAS::MEMORY_HANDLE_NONE);
  }
  
  virtual ~heap()
  {
  }

  ccpm::region_vector_t& regions() { return _regions; }

  inline component::IMCAS::pool_t pool() const { return _pool; }
  inline component::IMCAS * mcas() const { return _mcas; }
  inline const std::string& name() const { return _name; }
  inline T * root() const { return _root; }
  inline component::IKVStore::memory_handle_t memory_handle() const { return _memory_handle; }
  
protected:
  const std::string                    _name;
  component::IMCAS *                   _mcas;
  const component::IMCAS::pool_t       _pool;
  size_t                               _size;
  ccpm::region_vector_t                _regions;
  T *                                  _root;
  component::IKVStore::memory_handle_t _memory_handle;
};

}

#endif // __CCPM_HEAP_H__
