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

#ifndef __API_KVSTORE_ITF__
#define __API_KVSTORE_ITF__

#include <api/components.h>
#include <common/errors.h> /* ERROR_BASE */
#include <common/time.h>
#include <sys/uio.h> /* iovec */

#include <cinttypes> /* PRIx64 */
#include <cstdlib>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

namespace nupm
{
  struct region_descriptor;
}

/* print format for the pool type */
#define PRIxIKVSTORE_POOL_T PRIx64

namespace component
{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

#define DECLARE_OPAQUE_TYPE(NAME) \
  struct Opaque_##NAME {          \
    virtual ~Opaque_##NAME() {}   \
  }

/**
 * Key-value interface for pluggable backend (e.g. mapstore, hstore, hstore-cc)
 */
class IKVStore : public component::IBase {
 public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0x62f4829f,0x0405,0x4c19,0x9898,0xa3,0xae,0x21,0x5a,0x3e,0xe8);
  // clang-format on

 private:
  DECLARE_OPAQUE_TYPE(lock_handle);

 public:
  DECLARE_OPAQUE_TYPE(memory_region); /* Buffer_manager::buffer_t need this */
  DECLARE_OPAQUE_TYPE(key);
  DECLARE_OPAQUE_TYPE(pool_iterator);

 public:
  using pool_t          = uint64_t;
  using memory_handle_t = Opaque_memory_region*;
  using key_t           = Opaque_key*;
  using pool_lock_t     = Opaque_lock_handle*;
  using pool_iterator_t = Opaque_pool_iterator*;

  static constexpr memory_handle_t HANDLE_NONE = nullptr;
  static constexpr key_t           KEY_NONE    = nullptr;

  struct Addr {
    explicit Addr(addr_t addr_) : addr(addr_) {}
    Addr() = delete;
    addr_t addr;
  };
  

  enum {
    THREAD_MODEL_UNSAFE,
    THREAD_MODEL_SINGLE_PER_POOL,
    THREAD_MODEL_RWLOCK_PER_POOL,
    THREAD_MODEL_MULTI_PER_POOL,
  };

  using flags_t = std::uint32_t ;
  static constexpr flags_t FLAGS_NONE      = 0x0;
  static constexpr flags_t FLAGS_READ_ONLY = 0x1; /* lock read-only */
  static constexpr flags_t FLAGS_SET_SIZE    = 0x2;
  static constexpr flags_t FLAGS_CREATE_ONLY = 0x4;  /* only succeed if no existing k-v pair exist */
  static constexpr flags_t FLAGS_DONT_STOMP  = 0x8;  /* do not overwrite existing k-v pair */
  static constexpr flags_t FLAGS_NO_RESIZE   = 0x10; /* if size < existing size, do not resize */
  static constexpr flags_t FLAGS_MAX_VALUE   = 0x10;

  using unlock_flags_t = std::uint32_t;
  static constexpr unlock_flags_t UNLOCK_FLAGS_NONE = 0x0;
  static constexpr unlock_flags_t UNLOCK_FLAGS_FLUSH = 0x1; /* indicates for PM backends to flush */

  static constexpr pool_t POOL_ERROR = 0;

  enum class Capability {
    POOL_DELETE_CHECK, /*< checks if pool is open before allowing delete */
    RWLOCK_PER_POOL,   /*< pools are locked with RW-lock */
    POOL_THREAD_SAFE,  /*< pools can be shared across multiple client threads */
    WRITE_TIMESTAMPS,  /*< support for write timestamping */
  };

  enum class Op_type {
    WRITE, /* copy bytes into memory region */
    ZERO,  /* zero the memory region */
    INCREMENT_UINT64,
    CAS_UINT64,
  };


  enum Attribute : std::uint32_t {
    VALUE_LEN                = 1, /* length of a value associated with key */
    COUNT                    = 2, /* number of objects */
    CRC32                    = 3, /* get CRC32 of a value */
    AUTO_HASHTABLE_EXPANSION = 4, /* set to true if the hash table should expand */
    PERCENT_USED             = 5, /* get percent used pool capacity at current size */
    WRITE_EPOCH_TIME         = 6, /* epoch time at which the key-value pair was last
                                     written or locked with STORE_LOCK_WRITE */
    MEMORY_TYPE              = 7, /* type of memory */
    MEMORY_SIZE              = 8, /* size of pool or store in bytes */
  };

  enum {
    MEMORY_TYPE_DRAM        = 0x1,
    MEMORY_TYPE_PMEM_DEVDAX = 0x2,
    MEMORY_TYPE_UNKNOWN     = 0xFF,
  };

  enum lock_type_t {
    STORE_LOCK_NONE  = 0,
    STORE_LOCK_READ  = 1,
    STORE_LOCK_WRITE = 2,
  };

  enum {
    /* see common/errors.h */
    S_MORE           = 2,
    E_KEY_EXISTS     = E_ERROR_BASE - 1,
    E_KEY_NOT_FOUND  = E_ERROR_BASE - 2,
    E_POOL_NOT_FOUND = E_ERROR_BASE - 3,
    E_BAD_ALIGNMENT  = E_ERROR_BASE - 4,
    E_TOO_LARGE      = E_ERROR_BASE - 5, /* -55 */
    E_ALREADY_OPEN   = E_ERROR_BASE - 6,
  };

  std::string strerro(int e)
  {
    static std::map<int, std::string> errs {
      { S_MORE, "MORE" }
      , { E_KEY_EXISTS, "E_KEY_EXISTS" }
      , { E_KEY_NOT_FOUND, "E_KEY_NOT_FOUND" }
      , { E_POOL_NOT_FOUND, "E_POOL_NOT_FOUND" }
      , { E_BAD_ALIGNMENT, "E_BAD_ALIGNMENT" }
      , { E_TOO_LARGE, "E_TOO_LARGE" }
      , { E_ALREADY_OPEN, "E_ALREADY_OPEN" }
    };
    auto it = errs.find(e);
    return it == errs.end() ? ( "non-IKVStore error " + std::to_string(e) ) : it->second;
  }


  class Operation {
    Op_type _type;
    size_t  _offset;

   protected:
    Operation(Op_type type, size_t offset) : _type(type), _offset(offset) {}

   public:
    Op_type type() const noexcept { return _type; }
    size_t  offset() const noexcept { return _offset; }
  };

  class Operation_sized : public Operation {
    size_t _len;

   protected:
    Operation_sized(Op_type type, size_t offset_, size_t len) : Operation(type, offset_), _len(len) {}

   public:
    size_t size() const noexcept { return _len; }
  };

  class Operation_write : public Operation_sized {
    const void* _data;

   public:
    Operation_write(size_t offset, size_t len, const void* data)
        : Operation_sized(Op_type::WRITE, offset, len), _data(data)
    {
    }
    const void* data() const noexcept { return _data; }
  };

  /**
   * Determine thread safety of the component
   * Check capability of component
   *
   * @param cap Capability type
   *
   * @return THREAD_MODEL_XXXX
   */
  virtual int thread_safety() const = 0;

  /**
   * Check capability of component
   *
   * @param cap Capability type
   *
   * @return THREAD_MODEL_XXXX
   */
  virtual int get_capability(Capability cap) const { return -1; }

  /**
   * Create an object pool. If the pool exists and the FLAGS_CREATE_ONLY
   * is not provided, then the existing pool will be opened.  If
   * FLAGS_CREATE_ONLY is specified and the pool exists, POOL ERROR will be
   * returned.
   *
   * @param name Name of object pool
   * @param size Size of object pool in bytes
   * @param flags Creation flags
   * @param expected_obj_count [optional] Expected number of objects in pool
   * @param base_address_unused Needed for IKVStore and IMCAS unification
   *
   * @return Pool handle or POOL_ERROR
   */
  virtual pool_t create_pool(const std::string& name,
                             const size_t       size,
                             flags_t            flags              = 0,
                             uint64_t           expected_obj_count = 0,
                             const Addr         base_addr_unused = Addr{0})
  {
    PERR("create_pool not implemented");
    return POOL_ERROR;
  }

  virtual pool_t create_pool(const std::string& path,
                             const std::string& name,
                             const size_t       size,
                             flags_t            flags              = 0,
                             uint64_t           expected_obj_count = 0,
                             const Addr         base_addr_unused = Addr{0}) __attribute__((deprecated))
  {
    return create_pool(path + name, size, flags, expected_obj_count, base_addr_unused);
  }

  /**
   * Open an existing pool
   *
   * @param name Name of object pool
   * @param flags Open flags e.g., FLAGS_READ_ONLY
   * @param base_address_unused Needed for IKVStore and IMCAS unification
   *
   * @return Pool handle or POOL_ERROR if pool cannot be opened, or flags
   * unsupported
   */
  virtual pool_t open_pool(const std::string& name,
                           flags_t flags = 0,
                           const Addr base_addr_unused = Addr{0})
  {
    return POOL_ERROR;
  }

  virtual pool_t open_pool(const std::string& path,
                           const std::string& name,
                           flags_t flags = 0,
                           const Addr base_addr_unused = Addr{0}) __attribute__((deprecated))
  {
    return open_pool(path + name, flags, base_addr_unused);
  }

  /**
   * Close pool handle
   *
   * @param pool Pool handle
   *
   * @return S_OK on success, E_POOL_NOT_FOUND, E_ALREADY_OPEN if pool cannot be
   * closed due to open session.
   */
  virtual status_t close_pool(pool_t pool) = 0;

  /**
   * Delete an existing pool
   *
   * @param name Name of object pool
   *
   * @return S_OK on success, E_POOL_NOT_FOUND, E_ALREADY_OPEN if pool cannot be
   * deleted
   */
  virtual status_t delete_pool(const std::string& name) = 0;

  /**
   * Get mapped memory regions for pool.  This is used for pre-registration with
   * DMA engines.
   *
   * @param pool Pool handle
   * @param out_regions Backing file name (if any), Mapped memory regions
   *
   * @return S_OK on success or E_POOL_NOT_FOUND.  Components that do not
   * support this return E_NOT_SUPPORTED.
   */
  virtual status_t get_pool_regions(const pool_t pool, nupm::region_descriptor & out_regions)
  {
    return E_NOT_SUPPORTED; /* not supported in FileStore */
  }

  /**
   * Dynamically expand a pool.  Typically, this will add to the regions
   * belonging to a pool.
   *
   * @param pool Pool handle
   * @param increment_size Size in bytes to expand by
   * @param reconfigured_size [out] new size of pool
   *
   * @return S_OK on success or E_POOL_NOT_FOUND. Components that do not support
   * this return E_NOT_SUPPORTED (e.g. MCAS client)
   */
  virtual status_t grow_pool(const pool_t pool,
                             const size_t increment_size,
                             size_t& reconfigured_size)
  {
    PERR("grow_pool: not supported");
    return E_NOT_SUPPORTED;
  }

  /**
   * Write or overwrite an object value. If there already exists an
   * object with matching key, then it should be replaced
   * (i.e. reallocated) or overwritten.
   *
   * @param pool Pool handle
   * @param key Object key
   * @param value Value data
   * @param value_len Size of value in bytes
   *
   * @return S_OK or E_POOL_NOT_FOUND, E_KEY_EXISTS
   */
  virtual status_t put(const pool_t       pool,
                       const std::string& key,
                       const void*        value,
                       const size_t       value_len,
                       flags_t            flags = FLAGS_NONE)
  {
    return E_NOT_SUPPORTED;
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
   *
   * @return S_OK or E_POOL_NOT_FOUND, E_KEY_EXISTS
   */
  virtual status_t put_direct(const pool_t       pool,
                              const std::string& key,
                              const void*        value,
                              const size_t       value_len,
                              memory_handle_t    handle = HANDLE_NONE,
                              flags_t            flags  = FLAGS_NONE)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Resize memory for a value
   *
   * @param pool Pool handle
   * @param key Object key (should be unlocked)
   * @param new_size New size of value in bytes (can be more or less)
   *
   * @return S_OK on success, E_BAD_ALIGNMENT, E_POOL_NOT_FOUND,
   * E_KEY_NOT_FOUND, E_TOO_LARGE, E_ALREADY
   */
  virtual status_t resize_value(const pool_t       pool,
                                const std::string& key,
                                const size_t       new_size,
                                const size_t       alignment)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Read an object value
   *
   * @param pool Pool handle
   * @param key Object key
   * @param out_value Value data (if null, component will allocate memory)
   * @param out_value_len Size of value in bytes
   *
   * @return S_OK or E_POOL_NOT_FOUND, E_KEY_NOT_FOUND if key not found
   */
  virtual status_t get(const pool_t       pool,
                       const std::string& key,
                       void*&             out_value, /* release with free_memory() API */
                       size_t&            out_value_len) = 0;

  /**
   * Read an object value directly into client-provided memory.
   *
   * @param pool Pool handle
   * @param key Object key
   * @param out_value Client provided buffer for value
   * @param out_value_len [in] size of value memory in bytes [out] size of value
   * @param handle Memory registration handle
   *
   * @return S_OK, S_MORE if only a portion of value is read,
   * E_BAD_ALIGNMENT on invalid alignment, E_POOL_NOT_FOUND, or other
   * error code
   *
   * Note: S_MORE is reduncant, it could have been inferred from S_OK and
   * out_value_len [in] < out_value_len [out].
   */
  virtual status_t get_direct(pool_t             pool,
                              const std::string& key,
                              void*              out_value,
                              size_t&            out_value_len,
                              memory_handle_t    handle = HANDLE_NONE)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Get attribute for key or pool (see enum Attribute)
   *
   * @param pool Pool handle
   * @param attr Attribute to retrieve
   * @param out_value Vector of attribute values
   * @param key [optional] Key
   *
   * @return S_OK on success, E_POOL_NOT_FOUND, E_INVALID_ARG, E_KEY_NOT_FOUND
   */
  virtual status_t get_attribute(pool_t                 pool,
                                 Attribute              attr,
                                 std::vector<uint64_t>& out_value,
                                 const std::string*     key = nullptr) = 0;

  /**
   * Atomically (crash-consistent for pmem) swap keys (K,V)(K',V') -->
   * (K,V')(K',V).  Before calling this API, both KV-pairs must be
   * unlocked.
   *
   * @param pool Pool handle
   * @param key First key
   * @param key Second key
   *
   * @return S_OK on success, E_POOL_NOT_FOUND, E_KEY_NOT_FOUND, E_LOCKED
   * (unable to take both locks)
   */
  virtual status_t swap_keys(const pool_t pool,
                             const std::string key0,
                             const std::string key1)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Set attribute on a pool.
   *
   * @param pool Pool handle
   * @param attr Attribute to set
   * @param value Vector of values to set (for boolean 0=false, 1=true)
   * @param key [optional] key
   *
   * @return S_OK, E_INVALID_ARG (e.g. key==nullptr), E_POOL_NOT_FOUND
   */
  virtual status_t set_attribute(const pool_t                 pool,
                                 const Attribute              attr,
                                 const std::vector<uint64_t>& value,
                                 const std::string*           key = nullptr)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Allocate memory for zero copy DMA
   *
   * @param vaddr [out] allocated memory buffer
   * @param len [in] length of memory buffer in bytes
   * @param handle memory handle
   *
   */
  virtual status_t allocate_direct_memory(void*& vaddr,
                                          size_t len,
                                          memory_handle_t& handle)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Free memory for zero copy DMA
   *
   * @param handle handle to memory region to free
   *
   * @return S_OK on success
   */
  virtual status_t free_direct_memory(memory_handle_t handle)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Register memory for zero copy DMA
   *
   * @param vaddr Appropriately aligned memory buffer
   * @param len Length of memory buffer in bytes
   *
   * @return Memory handle or NULL on not supported.
   */
  virtual memory_handle_t register_direct_memory(void* vaddr,
                                                 size_t len)
  {
    return nullptr;
  }

  /**
   * Direct memory regions should be unregistered before the memory is released
   * on the client side.
   *
   * @param vaddr Address of region to unregister.
   *
   * @return S_OK on success
   */
  virtual status_t unregister_direct_memory(memory_handle_t handle)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Take a lock on an object. If the object does not exist and inout_value_len
   * is non-zero, create it with value space according to out_value_len (this
   * is very important for mcas context). If the object does not exist and
   * inout_value_len is zero, return E_KEY_NOT_FOUND.
   *
   * @param pool Pool handle
   * @param key Key
   * @param type STORE_LOCK_READ | STORE_LOCK_WRITE
   * @param out_value [out] Pointer to data
   * @param inout_value_len [in-out] Size of data in bytes
   * @param out_key [out]  Handle to key for unlock
   * @param out_key_ptr [out]  Optional request for key-string pointer (set to
   * nullptr if not required)
   *
   * @return S_OK, S_CREATED_OK (if created on demand), E_KEY_NOT_FOUND,
   * E_LOCKED (already locked), E_INVAL (e.g., no key & no length),
   * E_TOO_LARGE (cannot allocate space for lock), E_NOT_SUPPORTED
   * if unable to take lock or other error
   */
  virtual status_t lock(const pool_t       pool,
                        const std::string& key,
                        const lock_type_t  type,
                        void*&             out_value,
                        size_t&            inout_value_len,
                        key_t&             out_key_handle,
                        const char**       out_key_ptr = nullptr)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Unlock a key-value pair
   *
   * @param pool Pool handle
   * @param key_handle Handle (opaque) for key used to unlock
   *
   * @return S_OK, S_MORE (for async), E_INVAL or other error
   */
  virtual status_t unlock(const pool_t pool,
                          const key_t key_handle,
                          const unlock_flags_t flags = UNLOCK_FLAGS_NONE)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Update an existing value by applying a series of operations.
   * Together the set of operations make up an atomic transaction.
   * If the operation requires a result the operation type may provide
   * a method to accept the result. No operation currently requires
   * a result, but compare and swap probably would.
   *
   * @param pool Pool handle
   * @param key Object key
   * @param op_vector Operation vector
   * @param take_lock Set to true for automatic locking of object
   *
   * @return S_OK or error code
   */
  virtual status_t atomic_update(const pool_t                   pool,
                                 const std::string&             key,
                                 const std::vector<Operation*>& op_vector,
                                 bool                           take_lock = true)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Erase an object
   *
   * @param pool Pool handle
   * @param key Object key
   *
   * @return S_OK or error code (e.g. E_LOCKED)
   */
  virtual status_t erase(pool_t pool, const std::string& key) = 0;

  /**
   * Return number of objects in the pool
   *
   * @param pool Pool handle
   *
   * @return Number of objects
   */
  virtual size_t count(pool_t pool) = 0;

  /**
   * Apply functor to all objects in the pool
   *
   * @param pool Pool handle
   * @param function Functor to apply
   *
   * @return S_OK, E_POOL_NOT_FOUND
   */
  virtual status_t map(const pool_t pool,
                       std::function<int(const void* key,
                                         const size_t key_len,
                                         const void* value,
                                         const size_t value_len)> function)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Apply functor to all objects in the pool according
   * to given time constraints
   *
   * @param pool Pool handle
   * @param function Functor to apply (not in time order). If
   *                 functor returns < 0, then map aborts
   * @param t_begin Time must be after or equal. If set to zero, no constraint.
   * @param t_end Time must be before or equal. If set to zero, no constraint.
   *
   * @return S_OK, E_POOL_NOT_FOUND
   */
  virtual status_t map(const pool_t pool,
                       std::function<int(const void*              key,
                                         const size_t             key_len,
                                         const void*              value,
                                         const size_t             value_len,
                                         const common::tsc_time_t timestamp)> function,
                       const common::epoch_time_t t_begin,
                       const common::epoch_time_t t_end)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Apply functor to all keys only. Useful for file_store (now deprecated)
   *
   * @param pool Pool handle
   * @param function Functor
   *
   * @return S_OK, E_POOL_NOT_FOUND
   */
  virtual status_t map_keys(const pool_t pool, std::function<int(const std::string& key)> function)
  {
    return E_NOT_SUPPORTED;
  }

  /*
     auto iter = open_pool_iterator(pool);

     while(deref_pool_iterator(iter, ref, true) == S_OK)
       process_record(ref);

     close_pool_iterator(iter);
  */

  struct pool_reference_t {
  public:
    pool_reference_t()
      : key(nullptr), key_len(0), value(nullptr), value_len(0), timestamp() {}

    const void*          key;
    size_t               key_len;
    const void*          value;
    size_t               value_len;
    common::epoch_time_t timestamp; /* zero if not supported */
    
    inline std::string get_key() const {
      std::string k(static_cast<const char*>(key), key_len);
      return k;
    }

  };

  /**
   * Open pool iterator to iterate over objects in pool.
   *
   * @param pool Pool handle
   *
   * @return Pool iterator or nullptr
   */
  virtual pool_iterator_t open_pool_iterator(const pool_t pool)
  {
    return nullptr;
  }

  /**
   * Deference pool iterator position and optionally increment
   *
   * @param pool Pool handle
   * @param iter Pool iterator
   * @param t_begin Time must be after or equal. If set to zero, no constraint.
   * @param t_end Time must be before or equal. If set to zero, no constraint.
   * @param ref [out] Output reference record
   * @param ref [out] Set to true if within time bounds
   * @param increment Move iterator forward one position
   *
   * @return S_OK on success and valid reference, E_INVAL (bad iterator),
   *   E_OUT_OF_BOUNDS (when attempting to dereference out of bounds
   *   E_ITERATOR_DISTURBED (when writes have been made since last iteration)
   */
  virtual status_t deref_pool_iterator(const pool_t       pool,
                                       pool_iterator_t    iter,
                                       const common::epoch_time_t t_begin,
                                       const common::epoch_time_t t_end,
                                       pool_reference_t&  ref,
                                       bool&              time_match,
                                       bool               increment = true)
  {
    return E_NOT_IMPL;
  }

  /**
   * Unlock pool, release iterator and associated resources
   *
   * @param pool Pool handle
   * @param iter Pool iterator
   *
   * @return S_OK on success, E_INVAL (bad iterator)
   */
  virtual status_t close_pool_iterator(const pool_t pool,
                                       pool_iterator_t iter)
  {
    return E_NOT_IMPL;
  }

  /**
   * Free server-side allocated memory
   *
   * @param p Pointer to memory allocated through a get call
   *
   * @return S_OK on success
   */
  virtual status_t free_memory(void* p)
  {
    ::free(p);
    return S_OK;
  }

  /**
   * Allocate memory from pool
   *
   * @param pool Pool handle
   * @param size Size in bytes
   * @param alignment Alignment hint in bytes, 0 if no alignment is needed
   * @param out_addr Pointer to allocated region
   *
   * @return S_OK on success, E_BAD_ALIGNMENT, E_POOL_NOT_FOUND, E_NOT_SUPPORTED
   */
  virtual status_t allocate_pool_memory(const pool_t pool,
                                        const size_t size,
                                        const size_t alignment_hint,
                                        void*&       out_addr)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Free memory from pool
   *
   * @param pool Pool handle
   * @param addr Address of memory to free
   * @param size Size in bytes of allocation; if provided this accelerates
   * release
   *
   * @return S_OK on success, E_INVAL, E_POOL_NOT_FOUND, E_NOT_SUPPORTED
   */
  virtual status_t free_pool_memory(pool_t pool, const void* addr, size_t size = 0)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Flush memory from pool
   *
   * @param pool Pool handle
   * @param addr Address of memory to flush
   * @param size Size in bytes to flush
   *
   * @return S_OK on success, E_INVAL, E_POOL_NOT_FOUND, E_NOT_SUPPORTED
   */
  virtual status_t flush_pool_memory(pool_t pool, const void* addr, size_t size)
  {
    return E_NOT_SUPPORTED;
  }

  /**
   * Perform control invocation on component
   *
   * @param command String representation of command (component-interpreted)
   *
   * @return S_OK on success or error otherwise
   */
  virtual status_t ioctl(const std::string& command) { return E_NOT_SUPPORTED; }

  /**
   * Debug routine
   *
   * @param pool Pool handle
   * @param cmd Debug command
   * @param arg Parameter for debug operation
   */
  virtual void debug(pool_t pool, unsigned cmd, uint64_t arg) = 0;
};

class IKVStore_factory : public component::IBase {
 public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xface829f,0x0405,0x4c19,0x9898,0xa3,0xae,0x21,0x5a,0x3e,0xe8);
  // clang-format on

  virtual IKVStore* create(const std::string& owner, const std::string& param)
  {
    throw API_exception("IKVstore_factory::create(owner,param) not implemented");
  };

  virtual IKVStore* create(const std::string& owner, const std::string& param, const std::string& param2)
  {
    throw API_exception("IKVstore_factory::create(owner,param,param2) not implemented");
  }

  virtual IKVStore* create(unsigned           debug_level,
                           const std::string& owner,
                           const std::string& param,
                           const std::string& param2)
  {
    throw API_exception("IKVstore_factory::create(debug_level,owner,param,param2) not implemented");
  }

  using map_create = std::map<std::string, std::string>;

  static constexpr const char *k_src_addr = "src_addr";
  static constexpr const char *k_dest_addr = "dest_addr";
  static constexpr const char *k_dest_port = "dest_port";
  static constexpr const char *k_interface = "interface";
  static constexpr const char *k_provider = "provider";
  static constexpr const char *k_patience = "patience";

  static constexpr const char *k_debug = "debug";
  static constexpr const char *k_owner = "owner";
  static constexpr const char *k_name = "name";
  static constexpr const char *k_dax_config = "dax_config";

  /* this is the preferred create method - the others will be deprecated */
  virtual IKVStore* create(unsigned debug_level, const map_create& params)
  {
    throw API_exception("IKVstore_factory::create(debug_level,param-map) not implemented");
  }
};

#pragma GCC diagnostic pop

}  // namespace component
#endif
