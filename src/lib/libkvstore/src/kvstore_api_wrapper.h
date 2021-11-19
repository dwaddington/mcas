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

/** 
 * C-only wrapper to store component with IKVStore interface. Mainly used for
 * foreign-function interfacing.
 * 
 */
#ifndef __KVSTORE_API_WRAPPER_H__
#define __KVSTORE_API_WRAPPER_H__

#include <stdint.h>
#include <unistd.h>
#include <bits/types/struct_iovec.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* type definitions */
  typedef int       status_t;
  typedef void *    kvstore_t;
  typedef void *    lock_token_t;
  typedef uint32_t  kvstore_flags_t;
  typedef uint64_t  pool_t;
  typedef uint64_t  addr_t;

  /* error definitions */
  static const status_t S_OK = 0;
  static const status_t S_OK_CREATED = 1;
  static const status_t S_OK_MORE = 2;
  static const status_t E_FAIL = -1;
  static const status_t E_INVAL = -2;
  static const status_t E_KEY_EXISTS  = -51;
  static const status_t E_KEY_NOT_FOUND = -52;
  static const status_t E_BAD_POOL_NAME = -53;
  static const status_t E_BAD_ALIGNMENT = -54;
  static const status_t E_TOO_LARGE   = -55;
  static const status_t E_ALREADY_OPEN  = -56;


  /* see kvstore_itf.h */
  static const kvstore_flags_t FLAGS_NONE        = 0x0;
  static const kvstore_flags_t FLAGS_READ_ONLY   = 0x1; /* lock read-only */
  static const kvstore_flags_t FLAGS_SET_SIZE    = 0x2;
  static const kvstore_flags_t FLAGS_CREATE_ONLY = 0x4;  /* only succeed if no existing k-v pair exist */
  static const kvstore_flags_t FLAGS_DONT_STOMP  = 0x8;  /* do not overwrite existing k-v pair */
  static const kvstore_flags_t FLAGS_NO_RESIZE   = 0x10; /* if size < existing size, do not resize */
  static const kvstore_flags_t FLAGS_MAX_VALUE   = 0x10;


  /** 
   * kvstore_create: open session to new store instance
   * 
   * @param debug_level Debug level 0=off
   * @param json_config JSON configuration
   * @param out_store_handle [out] Handle to instance
   * 
   * @return S_OK or error code
   */
  status_t kvstore_open(const unsigned debug_level,
                        const char * json_config,
                        kvstore_t * out_store_handle);

  /**
   * kvstore_close: close session
   *  
   * @param handle Opaque handle returned from kvstore_create
   *
   * @return S_OK or error code
   */
  status_t kvstore_close(kvstore_t store_handle);

  /** 
   * kvstore_create_pool: create a pool
   * 
   * @param store_handle Store instance handle
   * @param pool_name Name of pool
   * @param size Size of pool in bytes
   * @param flags Flags
   * 
   * @return S_OK or error code
   */
  status_t kvstore_create_pool(const kvstore_t store_handle,
                               const char * pool_name,
                               const size_t size,
                               const kvstore_flags_t flags,
                               pool_t * out_pool_handle);

  /** 
   * kvstore_create_pool_ex: create a pool (extended API)
   * 
   * @param store_handle Store instance handle
   * @param pool_name Name of pool
   * @param size Size of pool in bytes
   * @param flags Flags (e.g. FLAGS_READ_ONLY | FLAGS_CREATE_ONLY)
   * @param expected_object_count Expected object count
   * @param base_addr Base address for loading
   * @param out_pool_handle [out] Pool handle
   * 
   * @return S_OK or error code
   */
  status_t kvstore_create_pool_ex(const kvstore_t store_handle,
                                  const char * pool_name,
                                  const size_t size,
                                  const kvstore_flags_t flags,
                                  const size_t expected_object_count,
                                  const addr_t base_addr,
                                  pool_t * out_pool_handle);

  /** 
   * kvstore_open_pool: open an existing pool
   * 
   * @param store_handle Store instance handle
   * @param pool_name Name of pool
   * @param flags Flags (e.g. FLAGS_READ_ONLY)
   * @param out_pool_handle [out] Pool handle
   * 
   * @return S_OK or error code
   */
  status_t kvstore_open_pool(const kvstore_t store_handle,
                             const char * pool_name,
                             const kvstore_flags_t flags,
                             pool_t * out_pool_handle);

  /** 
   * kvstore_open_pool_ex: open an existing pool (enhanced API)
   * 
   * @param store_handle Store instance handle
   * @param pool_name Name of pool
   * @param flags Flags (e.g. FLAGS_READ_ONLY)
   * @param base_addr Load address for pool memory
   * @param out_pool_handle [out] Pool handle
   * 
   * @return S_OK or error code
   */
  status_t kvstore_open_pool_ex(const kvstore_t store_handle,
                                const char * pool_name,
                                const kvstore_flags_t flags,
                                const addr_t base_addr,
                                pool_t * out_pool_handle);

  /** 
   * kvstore_close_pool: close pool
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * 
   * @return S_OK or error code
   */
  status_t kvstore_close_pool(const kvstore_t store_handle,
                              const pool_t pool_handle);

  /** 
   * kvstore_delete_pool: delete pool
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * 
   * @return S_OK or error code
   */
  status_t kvstore_delete_pool(const kvstore_t store_handle,
                               const char * pool_name);

  /** 
   * kvstore_get_pool_names: get list of pool names
   * 
   * @param store_handle Store instance handle
   * @param out_names [out] Array of pointers to strings (release with POSIX free)
   * @param out_names_count [out] Name count (used to iterate out_names)
   * 
   * @return S_OK or error code
   */
  status_t kvstore_get_pool_names(const kvstore_t store_handle,
                                  char *** out_names,
                                  size_t * out_names_count);

  /** 
   * kvstore_grow_pool: grow the size of existing pool
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * @param increment_size Size to grow by (in bytes)
   * @param reconfigured_size [out] Post-grow final pool size
   * 
   * @return S_OK or error code
   */
  status_t kvstore_grow_pool(const kvstore_t store_handle,
                             const pool_t pool_handle,
                             const size_t increment_size,
                             size_t * reconfigured_size);

  /** 
   * kvstore_put: copy-based put (copying key and value to store)
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * @param key Key
   * @param value Pointer to value data
   * @param value_len Size of data to put
   * @param flags Flags (e.g., FLAGS_DONT_STOMP)
   * 
   * @return S_OK or error code
   */
  status_t kvstore_put(const kvstore_t store_handle,
                       const pool_t pool_handle,
                       const char * key,
                       const void * value,
                       const size_t value_len,
                       const kvstore_flags_t flags);

  /** 
   * kvstore_get: copy-based get (copying key and value from store)
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * @param key Key
   * @param value [out] Pointer to value data (release with POSIX free)
   * @param value_len [out] Size of data to get
   * 
   * @return S_OK or error code
   */
  status_t kvstore_get(const kvstore_t store_handle,
                       const pool_t pool_handle,
                       const char * key,
                       void ** value,
                       size_t * value_len);

  /** 
   * kvstore_swap_keys: swap keys around for two key-value pairs
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * @param key0 First second
   * @param key1 Second key
   * 
   * @return S_OK or error code
   */
  status_t kvstore_swap_keys(const kvstore_t store_handle,
                             const pool_t pool_handle,
                             const char * key0,
                             const char * key1);

  enum {
        KVSTORE_LOCK_NONE = 0,
        KVSTORE_LOCK_READ = 1,
        KVSTORE_LOCK_WRITE = 2,
  };


  /** 
   * kvstore_lock_existing: lock memory for existing key-value pair
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * @param key Key
   * @param lock_type Lock type (KVSTORE_LOCK_NONE, KVSTORE_LOCK_READ, KVSTORE_LOCK_WRITE)
   * @param out_value_ptr [out] Pointer to value memory
   * @param out_value_len [out] Length of value memory
   * @param out_lock_token [out] Token to use for unlocking
   * 
   * @return S_OK or error code
   */
  status_t kvstore_lock_existing(const kvstore_t store_handle,
                                 const pool_t pool_handle,
                                 const char * key,
                                 const int lock_type,
                                 void ** out_value_ptr,
                                 size_t * out_value_len,
                                 lock_token_t * out_lock_token);

  /** 
   * kvstore_lock_existing: lock memory for existing key-value pair, implicitly create if needed
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * @param key Key
   * @param lock_type Lock type (KVSTORE_LOCK_NONE, KVSTORE_LOCK_READ, KVSTORE_LOCK_WRITE)
   * @param alignment Alignment required for value (0 indicate none)
   * @param value_len Length of value to allocate when implicitly creating new one
   * @param out_value_ptr [out] Value pointer
   * @param out_value_len [out] Value length in bytes
   * @param out_lock_token [out] Token to use for subsequent unlock
   * 
   * @return S_OK or error code
   */
  status_t kvstore_lock_existing_or_create(const kvstore_t store_handle,
                                           const pool_t pool_handle,
                                           const char * key,
                                           const int lock_type,
                                           const size_t alignment,
                                           const size_t value_len,
                                           void ** out_value_ptr,
                                           size_t * out_value_len,
                                           lock_token_t * out_lock_token);


  /** 
   * kvstore_unlock: release locked key-value pair
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * @param lock_token Lock token (from lock call)
   * 
   * @return S_OK or error code
   */
  status_t kvstore_unlock(const kvstore_t store_handle,
                          const pool_t pool_handle,
                          const lock_token_t lock_token);

  
  /** 
   * kvstore_resize_value: change the value size (copy may be involved)
   * 
   * @param store_handle Store instance handle
   * @param pool_handle Pool handle
   * @param key Key
   * @param new_size New size of value in bytes
   * @param alignment Alignment in bytes
   * 
   * @return S_OK or error code
   */
  status_t kvstore_resize_value(const kvstore_t store_handle,
                                const pool_t pool_handle,
                                const char * key,
                                const size_t new_size,
                                const size_t alignment);


  /** 
   * kvstore_erase: erase key-value pair from pool
   * 
   * @param store_handle Store instance handle
   * @param pool Pool handle
   * @param key Key
   * 
   * @return S_OK or error code
   */
  status_t kvstore_erase(const kvstore_t store_handle,
                         const pool_t pool,
                         const char * key);

  /** 
   * kvstore_count: return number of pairs in pool
   * 
   * @param store_handle Store instance handle
   * @param pool Pool handle
   * 
   * @return S_OK or error code
   */  
  size_t kvstore_count(const kvstore_t store_handle,
                       const pool_t pool);


                    
#ifdef __cplusplus
}
#endif

#endif // __KVSTORE_API_WRAPPER_H__
