/*
   Copyright [2017-2019] [IBM Corporation]
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


#ifndef __API_MCAS_ITF__
#define __API_MCAS_ITF__

#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <api/components.h>
#include <api/kvstore_itf.h>
#include <api/kvindex_itf.h>

namespace Component
{

/** 
 * mcas client interface (this will include both KV and AS capabilities)
 */
class IMCAS : public Component::IBase
{
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0x33af1b99,0xbc51,0x49ff,0xa27b,0xd4,0xe8,0x19,0x03,0xbb,0x02);
  // clang-format on

public:

  static constexpr Component::IKVStore::pool_t POOL_ERROR
  = Component::IKVStore::POOL_ERROR; // same as IKVStore

  /* per-shard statistics */
  struct alignas(8) Shard_stats {
    uint64_t op_request_count;
    uint64_t op_put_count;
    uint64_t op_get_count;
    uint64_t op_put_direct_count;
    uint64_t op_get_twostage_count;
    uint64_t op_ado_count;
    uint64_t op_erase_count;
    uint64_t op_failed_request_count;
    uint64_t last_op_count_snapshot;
    uint16_t client_count;
    
  public:
    Shard_stats() :
      op_request_count(0),
      op_put_count(0),
      op_get_count(0),
      op_put_direct_count(0),
      op_get_twostage_count(0),
      op_ado_count(0),
      op_erase_count(0),
      op_failed_request_count(0),
      last_op_count_snapshot(0),
      client_count(0)
      {}
  } __attribute__((packed));

  using ado_flags_t = uint32_t;
  /*< operation is asynchronous */
  static constexpr ado_flags_t ADO_FLAG_ASYNC            = 0x1;
  /*< create KV pair if needed */
  static constexpr ado_flags_t ADO_FLAG_CREATE_ON_DEMAND = 0x2;
  /*< create only - allocate key,value but don't call ADO */
  static constexpr ado_flags_t ADO_FLAG_CREATE_ONLY      = 0x4;
  /*< do not overwrite value if it already exists */
  static constexpr ado_flags_t ADO_FLAG_NO_OVERWRITE     = 0x8;
  /*< create value but do not attach to key, unless key does not exist */
  static constexpr ado_flags_t ADO_FLAG_DETACHED         = 0x10;
  
public:
  using pool_t = Component::IKVStore::pool_t;
  
  /** 
   * Determine thread safety of the component
   * 
   * 
   * @return THREAD_MODEL_XXXX
   */
  virtual int thread_safety() const = 0;

  /** 
   * Create an object pool
   * 
   * @param ppol_name Unique pool name
   * @param size Size of pool in bytes (for keys,values and metadata)
   * @param flags Creation flags
   * @param expected_obj_count Expected maximum object count (optimization)
   * 
   * @return Pool handle
   */
  virtual IKVStore::pool_t create_pool(const std::string& pool_name,
                                       size_t size,
                                       unsigned int flags = 0,
                                       uint64_t expected_obj_count = 0) = 0;
  
  /** 
   * Open an existing pool
   * 
   * @param pool_name Name of object pool
   * @param flags Open flags e.g., FLAGS_READ_ONLY
   * 
   * @return Pool handle
   */
  virtual IKVStore::pool_t open_pool(const std::string& pool_name,
                                     unsigned int flags = 0) = 0;

  /** 
   * Close pool handle
   * 
   * @param pool Pool handle
   */
  virtual status_t close_pool(IKVStore::pool_t pool) = 0;

  /** 
   * Delete an existing pool. Pool must not be open
   * 
   * @param pool Pool name
   *
   * @return S_OK or E_ALREADY_OPEN
   */
  virtual status_t delete_pool(const std::string& pool_name) = 0;

  /** 
   * Close and delete an existing pool from a pool handle. Only one 
   * reference count should exist. Any ADO plugin is notified before
   * the pool is deleted.
   * 
   * @param pool Pool handle
   *
   * @return S_OK or E_BUSY if reference count > 1
   */
  virtual status_t delete_pool(IKVStore::pool_t pool) = 0;

  /** 
   * Configure a pool
   * 
   * @param setting Configuration request (e.g., AddIndex::VolatileTree)

   * 
   * @return S_OK on success
   */
  virtual status_t configure_pool(IKVStore::pool_t pool,
                                  const std::string& setting) = 0;
  
  /** 
   * Write or overwrite an object value. If there already exists a
   * object with matching key, then it should be replaced
   * (i.e. reallocated) or overwritten. 
   * 
   * @param pool Pool handle
   * @param key Object key
   * @param value Value data
   * @param value_len Size of value in bytes
   * @param flags Additional flags
   * 
   * @return S_OK or error code
   */
  virtual status_t put(IKVStore::pool_t pool,
                       const std::string& key,
                       const void * value,
                       size_t value_len,
                       unsigned int flags = IKVStore::FLAGS_NONE) = 0;

  virtual status_t put(IKVStore::pool_t pool,
                       const std::string& key,
                       const std::string& value,
                       unsigned int flags = IKVStore::FLAGS_NONE) {
    /* this does not store any null terminator */
    return put(pool, key, value.data(), value.length(), flags);
  }

  /** 
   * Zero-copy put operation.  If there does not exist an object
   * with matching key, then an error E_KEY_EXISTS should be returned.
   * 
   * @param pool Pool handle
   * @param key Object key
   * @param key_len Key length in bytes
   * @param value Value
   * @param value_len Value length in bytes
   * @param handle Memory registration handle
   * @param flags Additional flags 
   *
   * @return S_OK or error code
   */
  virtual status_t put_direct(IKVStore::pool_t pool,
                              const std::string& key,
                              const void * value,
                              size_t value_len,
                              IKVStore::memory_handle_t handle = IKVStore::HANDLE_NONE,
                              unsigned int flags = IKVStore::FLAGS_NONE) = 0;

  /** 
   * Read an object value
   * 
   * @param pool Pool handle
   * @param key Object key
   * @param out_value Value data (if null, component will allocate memory)
   * @param out_value_len Size of value in bytes
   * 
   * @return S_OK or error code
   */
  virtual status_t get(const IKVStore::pool_t pool,
                       const std::string& key,
                       void*& out_value, /* release with free_memory() API */
                       size_t& out_value_len) = 0;

  virtual status_t get(const IKVStore::pool_t pool,
                       const std::string& key,
                       std::string& out_value) {
    void* val = nullptr;
    size_t val_size = 0;
    auto s = this->get(pool, key, val, val_size);

    /* copy result */
    if(s == S_OK) {
      out_value.assign(static_cast<char*>(val), val_size);
      this->free_memory(val);
    }
    return s;
  }


  /**
   * Read an object value directly into client-provided memory.
   *
   * @param pool Pool handle
   * @param key Object key
   * @param out_value Client provided buffer for value
   * @param out_value_len [in] size of value memory in bytes [out] size of value
   * @param handle Memory registration handle
   * 
   * @return S_OK, S_MORE if only a portion of value is read, E_BAD_ALIGNMENT on invalid alignment, or other error code
   */
  virtual status_t get_direct(const IKVStore::pool_t pool,
                              const std::string& key,
                              void* out_value,
                              size_t& out_value_len,
                              IKVStore::memory_handle_t handle = IKVStore::HANDLE_NONE) = 0;
  
  /** 
   * Perform key search based on regex or prefix
   * 
   * @param pool Pool handle
   * @param key_expression Regular expression or prefix (e.g. "prefix:carKey")
   * @param offset Offset from which to search
   * @param out_matched_offset Out offset of match
   * @param out_keys Out vector of matching keys
   * 
   * @return S_OK on success
   */
  virtual status_t find(const IKVStore::pool_t pool,
                        const std::string& key_expression,
                        const offset_t offset,
                        offset_t& out_matched_offset,
                        std::string& out_matched_key) = 0;

  /** 
   * Erase an object
   * 
   * @param pool Pool handle
   * @param key Object key
   * 
   * @return S_OK or error code
   */
  virtual status_t erase(const IKVStore::pool_t pool,
                         const std::string& key)= 0;

  /** 
   * Return number of objects in the pool
   * 
   * @param pool Pool handle
   * 
   * @return Number of objects
   */
  virtual size_t count(const IKVStore::pool_t pool) = 0;

  /** 
   * Get attribute for key or pool (see enum Attribute)
   * 
   * @param pool Pool handle
   * @param attr Attribute to retrieve
   * @param out_attr Result
   * @param key [optiona] Key
   * 
   * @return S_OK on success
   */
  virtual status_t get_attribute(IKVStore::pool_t pool,
                                 IKVStore::Attribute attr,
                                 std::vector<uint64_t>& out_attr,
                                 const std::string* key = nullptr) = 0;

  /** 
   * Retrieve shard statistics
   * 
   * @param out_stats 
   * 
   * @return S_OK on success
   */
  virtual status_t get_statistics(Shard_stats& out_stats) = 0;
  
  /** 
   * Register memory for zero copy DMA
   * 
   * @param vaddr Appropriately aligned memory buffer
   * @param len Length of memory buffer in bytes
   * 
   * @return Memory handle or NULL on not supported.
   */
  virtual IKVStore::memory_handle_t register_direct_memory(void * vaddr, size_t len) = 0;

  /** 
   * Durict memory regions should be unregistered before the memory is released on the client side.
   * 
   * @param vaddr Address of region to unregister.
   * 
   * @return S_OK on success
   */
  virtual status_t unregister_direct_memory(IKVStore::memory_handle_t handle) = 0;

  /**
   * Free API allocated memory
   *
   * @param p Pointer to memory allocated through a get call
   * 
   * @return S_OK on success
   */
  virtual status_t free_memory(void * p) = 0;

  /** 
   * Used to invoke an operation on an active data object
   * 
   * @param pool Pool handle
   * @param key Key
   * @param request Request data
   * @param request_len Length of request in bytes
   * @param flags Flags for invocation (see ADO_FLAG_XXX)
   * @param out_response Response from invocation
   * @param value_size Optional parameter to define value size to create for on-demand
   * 
   * @return S_OK on success
   */
  virtual status_t invoke_ado(const IKVStore::pool_t pool,
                              const std::string& key,
                              const void * request,
                              size_t request_len,
                              ado_flags_t flags,
                              std::string& out_response,
                              size_t value_size = 0) = 0;

  inline status_t invoke_ado(const IKVStore::pool_t pool,
                             const std::string& key,
                             const std::string& request,
                             ado_flags_t flags,
                             std::string& out_response,
                             size_t value_size = 0) {
    return invoke_ado(pool, key, request.data(), request.length(), flags, out_response, value_size);
  }

  /** 
   * Used to invoke a combined put + ADO operation on an active data object.
   * 
   * @param pool Pool handle
   * @param key Key
   * @param request Request data
   * @param request_len Length of request data in bytes
   * @param value Value data
   * @param value_len Length of value data in bytes
   * @param root_len Length to allocate for root value (with ADO_FLAGS_DETACHED)
   * @param flags Flags for invocation (see ADO_FLAG_XXX)
   * @param out_response Response from invocation
   * 
   * @return S_OK on success
   */
  virtual status_t invoke_put_ado(const IKVStore::pool_t pool,
                                  const std::string& key,
                                  const void * request,
                                  size_t request_len,
                                  const void * value,
                                  size_t value_len,
                                  size_t root_len,
                                  ado_flags_t flags,
                                  std::string& out_response) = 0;
  
  inline status_t invoke_put_ado(const IKVStore::pool_t pool,
                                 const std::string& key,
                                 const std::string& request,
                                 const std::string& value,
                                 size_t root_len,
                                 ado_flags_t flags,
                                 std::string& out_response) {
    return invoke_put_ado(pool, key, request.data(), request.length(),
                          value.data(), value.length(), root_len, flags, out_response);
  }


  /** 
   * Debug routine
   * 
   * @param pool Pool handle
   * @param cmd Debug command
   * @param arg Parameter for debug operation
   */
  virtual void debug(IKVStore::pool_t pool, unsigned cmd, uint64_t arg) = 0;
};


class IMCAS_factory : public IKVStore_factory
{
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xfacf1b99,0xbc51,0x49ff,0xa27b,0xd4,0xe8,0x19,0x03,0xbb,0x02);
  // clang-format on
  

  /** 
   * Create a "session" to a remote shard
   * 
   * @param debug_level Debug level (0-3)
   * @param owner Owner info (not used)
   * @param addr_with_port Address and port information, e.g. 10.0.0.22:11911 (must be RDMA)
   * @param nic_device RDMA network device (e.g., mlnx5_0)
   * 
   * @return Pointer to IMCAS instance. Use release_ref() to close.
   */
  virtual IMCAS * mcas_create(unsigned debug_level,
                              const std::string& owner,
                              const std::string& addr_with_port,
                              const std::string& nic_device) {
    throw API_exception("IMCAS_factory::mcas_create(debug_level,owner,param,param2) not implemented");
  }
 

};


}


#endif
