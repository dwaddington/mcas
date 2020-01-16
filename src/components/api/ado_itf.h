/*
   Copyright [2019] [IBM Corporation]
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
 * Luna Xu (xuluna@ibm.com)
 *
 */

#ifndef __API_ADO_ITF_H__
#define __API_ADO_ITF_H__

#include <api/kvstore_itf.h>
#include <api/kvindex_itf.h>
#include <common/errors.h>
#include <common/types.h>
#include <component/base.h>

#include <functional>
#include <map>
#include <string>
#include <vector>
#include <tuple>

class Buffer_header;

namespace Component {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

class SLA;  /* to be defined - placeholder only */

enum class ADO_op {
  UNDEFINED = 0,
  CREATE = 1,
  OPEN = 2,
  ERASE = 3,
  VALUE_RESIZE = 4,
  ALLOCATE_POOL_MEMORY = 5,
  FREE_POOL_MEMORY = 6,
  SHUTDOWN = 7,
  POOL_DELETE = 8,
};


/**
 * Component for ADO process plugins
 *
 */
class IADO_plugin : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xacb20ef2,0xe796,0x4619,0x845d,0x4e,0x8e,0x6b,0x5c,0x35,0xaa);
  // clang-format on

  /* Used to define the set of buffers to return to client. The first
     buffer in the vector will be copied into the return message and
     freed. Subsequent are passed as references to the pool 
  */
  struct response_buffer_t {
    response_buffer_t(uint64_t offset_p, size_t len_p, bool pool_ref_p)
      : offset(offset_p), len(len_p), pool_ref(pool_ref_p) {}
    response_buffer_t(void * ptr_p, size_t len_p, bool pool_ref_p)
      : ptr(ptr_p), len(len_p), pool_ref(pool_ref_p) {}
    union {
      uint64_t offset;
      void * ptr;
    };
    uint64_t len;
    bool pool_ref;
  };

  struct response_buffer_vector_t : public std::vector<response_buffer_t>
  {
  public:
    ~response_buffer_vector_t() {
      for(auto& i : *this) {
        if(!i.pool_ref)
          ::free(i.ptr);
      }
    }
  };

public:  
  IADO_plugin()
    : _cb{}
  {
  }

  /**
   * Inform the plugin of memory mapped into the ADO process (normally pool
   * memory)
   *
   * @param shard_vaddr Virtual address as mapped in shard
   * @param local_vaddr Virtual address as mapped in local ADO process
   * @param len Size of mapping in bytes
   *
   * @return S_OK on success
   */
  virtual status_t register_mapped_memory(void *shard_vaddr,
                                          void *local_vaddr,
                                          size_t len) = 0;

  /**
   * Upcall to perform ADO operation on a specific key-value pair
   *
   * @param key Key for target
   * @param value Value memory associated to invoked key-value pair
   * @param value_len Length of value memory in bytes
   * @param detached_value Value memory associated to invoked key-value pair that has been created but is not attached (e.g., ADO_FLAG_DETACHED_PUT)
   * @param detached_value_len Length of detached value memory in bytes
   * @param in_work_request Open protocol request message
   * @param in_work_request_len Open protocol request message length
   * @param new_root Set to true if a new root value was created
   * @param response_buffers Buffers to transmit, in order, for response
   *
   * @return ADO plugin response
   */
  virtual status_t do_work(const uint64_t work_key,
                           const std::string &key,
                           void * value,
                           size_t value_len,
                           void * detached_value,
                           size_t detached_value_len,
                           const void *in_work_request, /* don't use iovec because of non-const */
                           const size_t in_work_request_len,
                           const bool new_root,
                           response_buffer_vector_t& response_buffers) = 0;

  
  /**
   * Upcall initial launch event
   *
   * @param auth_id Authentication identifier
   * @param pool_name Name of pool
   * @param pool_size Size of pool in bytes
   * @param pool_flags Pool creation/open flags
   * @param expected_obj_count Client-provided object count
   *
   */
  virtual void launch_event(const uint64_t auth_id,
                            const std::string& pool_name,
                            const size_t pool_size,
                            const unsigned int pool_flags,
                            const size_t expected_obj_count) { }

  /**
   * Upcall for operational events
   *
   * @param op Event
   */
  virtual void notify_op_event(ADO_op op) { } /* see ADO::OP_XXX */

  /* note:     FLAGS_CREATE_ONLY = 0x4, */
  enum {
    FLAGS_PERMANENT_LOCK = 0x1,
  };

  typedef struct {
    void* key;
    size_t key_len;
    void* value;
    size_t value_len;
  } kv_reference_t;

  /* Reference vector holds pointer to value memory held key-ptr,
     key-size, val-ptr, val-size for all pairs */
  class Reference_vector {
  public:
    Reference_vector(const Reference_vector& src)
      : _count(src._count),
        _value_memory_array(src._value_memory_array),
        _value_memory_size(src._value_memory_size) {}

    Reference_vector(const size_t count,
                     kv_reference_t * value_memory_array,
                     const size_t value_memory_size)
      : _count(count),
        _value_memory_array(value_memory_array),
        _value_memory_size(value_memory_size) {}
    
    Reference_vector(const size_t count,
                     void * value_memory_array,
                     const size_t value_memory_size)
      : _count(count),
        _value_memory_array(reinterpret_cast<kv_reference_t *>(value_memory_array)),
        _value_memory_size(value_memory_size) {}

    explicit Reference_vector()
      : _count(0), _value_memory_array(nullptr), _value_memory_size(0) {}
    
    Reference_vector &operator=(const Reference_vector&) = default;

    static size_t size_required(size_t num_pairs) {
      return sizeof(Reference_vector) + (num_pairs * sizeof(kv_reference_t));
    }
    
    inline size_t value_memory_size() const { return _value_memory_size; }
    inline const kv_reference_t * ref_array() const { return _value_memory_array; }
    inline size_t count() const { return _count; }
    inline void * value_memory() const { return reinterpret_cast<void*>(_value_memory_array); }
    
  private:
    size_t           _count;
    kv_reference_t * _value_memory_array;    
    size_t           _value_memory_size;
  } __attribute__((packed));

  struct Callback_table {

    /**
     * Create a new key-value pair
     *
     * @param work_id Work identifier from ADO invocation. This is
     *                used to manage the lock. If work_id is 0
     *                then a permanent lock is taken.
     * @param key_name Name of key
     * @param value_size Size of value
     * @param flags 
     * @param out_value_addr [out] Newly created value address
     *
     * @return : S_OK, E_ALREADY_EXISTS
     **/
    std::function<status_t(const uint64_t work_id,
                           const std::string &key_name,
                           const size_t value_size,
                           const int flags,
                           void *&out_value_addr)>
    create_key;
    
    /**
     * Open existing key-value pair
     *
     * @param work_id Work identifier from ADO invocation. This is
     *                used to manage the lock. If work_id is 0
     *                then a permanent lock is taken.
     * @param key_name Name of key
     * @param flags Optional IADO_plugin::FLAGS_PERMANENT_LOCK
     * @param out_value_addr [out] Value address
     * @param out_value_len [out] Size of value
     *
     * @return : S_OK, E_INVAL (result from IKVStore::lock)
     **/
    std::function<status_t(const uint64_t work_id,
                           const std::string &key_name,
                           const int flags,
                           void *&out_value_addr,
                           size_t &out_value_len)>
    open_key;

    /**
     * Erase existing key-value pair from store and index
     *
     * @param key_name Name of key
     *
     * @return : S_OK, E_INVAL or result from IKVStore::erase
     **/    
    std::function<status_t(const std::string &key_name)>
    erase_key;

    /**
     * Resize existing value (may perform memcpy)
     *
     * @param work_id Work identifier from ADO invocation
     * @param key_name Name of key
     * @param new_value_size Size to resize to
     * @param out_value_addr [out] New value address (relocation may have occurred)
     *
     * @return : S_OK, E_INVAL, or error from IKVStore::lock or IKVStore::resize
     **/    
    std::function<status_t(const uint64_t work_id,
                           const std::string& key_name,
                           const size_t new_value_size,
                           void *&out_new_value_addr)>
    resize_value;
    
    /**
     * Allocate pool memory. Memory is not attached to key.
     *
     * @param size Size to allocate in bytes
     * @param alignment_hint Hint for alignment (cannot be guaranteed)
     * @param out_value_addr [out] Pointer to allocated region
     *
     * @return : S_OK, E_INVAL, or result from IKVStore::allocate_pool_memory
     **/    
    std::function<status_t(const size_t size,
                           const size_t alignment_hint,
                           void *&out_new_addr)>
    allocate_pool_memory;

    /**
     * Free pool memory. Memory should not be referenced/attached.
     *
     * @param size Size of memory to free 
     * @param out_value_addr [out] Pointer to allocated region
     *
     * @return : S_OK, or result from IKVStore::free_pool_memory
     **/    
    std::function<status_t(const size_t size,
                           const void * addr)>
    free_pool_memory;
    
    /**
     * Get vector of all key-value references.  Optionally filter based on 
     * write timestamp. The vector is actually
     * held in value memory and should be free by ADO plugin.
     *
     * @param t_begin Optional time begin constraint (zero for no constraint)
     * @param t_end Optional time end constraint (zero for no constraint)
     * @param out_vector [out] Pointer to vector of references  
     *              held in pool memory which must be freed from ADO
     *
     * @return : S_OK, E_INSUFFICIENT_SPACE
     **/    
    std::function<status_t(const epoch_time_t t_begin,
                           const epoch_time_t t_end,
                           Reference_vector& out_vector
                           )>
    get_reference_vector;

    /**
     * Scan for key (requires index configured)
     *
     * @param work_id Work identifier from ADO invocation
     * @param key_expression Regular expression
     * @param begin_position Position to search from 
     * @param find_type (see kvindex_itf.h)
     * @param [out] Position of match or point at which max comparisons reached.
     * @param [out] Matched key
     *
     * @return : S_OK, E_NO_INDEX, E_MAX_REACHED (need to recall with new offset)
     **/    
    std::function<status_t(const std::string& key_expression,
                           const offset_t begin_position,
                           const IKVIndex::find_t find_type,
                           offset_t& out_matched_position,
                           std::string& out_matched_key)>
    find_key;


    /**
     * Get pool information, e.g., size, flags, expected_obj_count. 
     * Note: this information is currently for *only* when the pool was
     * created. If the pool is resized, or reopened, then some of this information
     * may be out of date.
     *
     * @param out_result [out] std::string in JSON format
     *
     * @return : S_OK or E_FAIL
     */
    std::function<status_t(std::string& out_result)>
    get_pool_info;

    
    /**
     * Iterate on pool key-value pairs 
     *
     * @param t_begin Optional time begin constraint (zero for no constraint)
     * @param t_end Optional time end constraint (zero for no constraint)
     * @param iterator [inout] Iterator handle. If zero, open iterator and advance
     *                 to the first element.
     * @param reference [out] Reference information for key-value pair
     *
     * @return S_OK on success and valid reference, E_INVAL (bad iterator), 
     *   E_OUT_OF_BOUNDS (when attempting to dereference out of bounds
     *   E_ITERATOR_DISTURBED (when writes have been made since last iteration)
     */
    std::function<status_t(const epoch_time_t t_begin,
                           const epoch_time_t t_end,
                           Component::IKVStore::pool_iterator_t& iterator,
                           Component::IKVStore::pool_reference_t& reference)>
    iterate;
  };


  /**
   * Register callbacks, so the plugin can perform KV-pair operations (sent to
   * shard to perform)
   *
   * @param create_key Callback function to create a new key in the current pool
   * @param erase_key Callback function to erase a key from the current pool
   */
  void register_callbacks(const Callback_table& callbacks) { _cb = callbacks; }

  /**
   * Request graceful shutdown. This will be called by ADO process on shutdown.
   *
   *
   * @return S_OK on success
   */
  virtual status_t shutdown() = 0;

protected:
  IADO_plugin::Callback_table _cb; /*< callback functions */
};

/**
 * ADO interface.  This is actually a proxy interface communicating with the
 * external process.
 *
 */
class IADO_proxy : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xbbbfa389,0x1665,0x4e5b,0xa1b1,0x3c,0xff,0x4a,0x5e,0xe2,0x63);
  // clang-format on

  using work_id_t = uint64_t; /*< work handle/identifier */

  /* ADO-to-SHARD (and vice versa) protocol */

  /**
   * Send boostrap event to ADO
   *
   * @param opened_existing True if open, false if create
   *
   * @return S_OK on success
   */  
  virtual status_t bootstrap_ado(bool opened_existing) = 0;


  /**
   * Send operation to ADO
   *
   * @return S_OK on success
   */  
  virtual status_t send_op_event(ADO_op op) = 0;


  /**
   * Send a memory map request to ADO
   *
   * @param token Token from MCAS expose_memory call
   * @param size Size of the memory
   * @param value_vaddr Virtual address as mapped by shard (may be different in
   * ADO)
   *
   * @return S_OK on success
   */
  virtual status_t send_memory_map(uint64_t token,
                                   size_t size,
                                   void *value_vaddr) = 0;

  /**
   * Send a work request to the ADO
   *
   * @param work_request_key Unique request identifier
   * @param value_addr Pointer to value (as mapped by shard)
   * @param value_len Length of value in bytes
   * @param detached_value_addr Pointer to detached value (as mapped by shard)
   * @param detached_value_len Length of detached value in bytes
   * @param invocation_data Data representing the work request (may be binary or
   * string)
   * @param invocation_len Length of data representing work
   * @param new_root Set true if a new root value was created
   *
   * @return S_OK on success
   */
  virtual status_t send_work_request(const uint64_t work_request_key,
                                     const std::string &work_key_str,
                                     const void *value_addr,
                                     const size_t value_len,
                                     const void * detached_value,
                                     const size_t detached_value_len,
                                     const void *invocation_data,
                                     const size_t invocation_len,
                                     const bool new_root) = 0;

  /**
   * Check for completion of work
   *
   * @param out_work_request_key [out] Work request identifier of completed work
   * @param out_status [out] Status
   * @param response_buffers [out] Response buffers
   *
   * @return True if bytes received
   */
  virtual bool check_work_completions(
       uint64_t& out_work_request_key,
       status_t& out_status,
       void *& response,
       size_t & response_len,
       Component::IADO_plugin::response_buffer_vector_t& response_buffers) = 0;
  
  /**
   * Get callback buffer withouth interpreting
   *
   * @param out_buffer Recieved buffer
   *
   * @return S_OK or E_EMPTY
   */
  virtual status_t recv_callback_buffer(Buffer_header *& out_buffer) = 0;

  /**
   * Free callback buffer
   *
   * @param buffer Recieved buffer
   *
   */
  virtual void free_callback_buffer(void * buffer) = 0;

  /**
   * Check for table operations (e.g., create_key)
   *
   * @param buffer Message buffer
   * @param work_request_id [out] Work request identifier of completed work
   * @param op [out] Operation type (see ado_proto.fbs)
   * @param key [out] Name of corresponding key
   * @param value_len [out] Size in bytes (optional)
   *
   * @return True if message interpreted as table op
   */
  virtual bool check_table_ops(const void * buffer,
                               uint64_t &work_request_id,
                               Component::ADO_op &op,
                               std::string &key,
                               size_t &value_len,
                               size_t &value_alignment,
                               void *& addr) = 0;

  /**
   * Check for index operations (e.g., find)
   *
   * @param buffer Message buffer
   * @param key_expression [out] Search expression
   * @param begin_pos [out] Position of match if successfull
   * @param find_type Type of search
   * @param max_comp Maximum numbfer of comparisons to make
   *
   * @return True if message interpreted as index op
   */
  virtual bool check_index_ops(const void * buffer,
                               std::string& key_expression,
                               offset_t& begin_pos,
                               int& find_type,
                               uint32_t max_comp) = 0;
  /**
   * Check for vector operations
   *
   * @param buffer Message buffer
   * @param t_begin Begin time constraint Zero for none.
   * @param t_end End time constraint. Zero for none.
   *
   * @return True if message interpreted as vector op
   */
  virtual bool check_vector_ops(const void * buffer,
                                epoch_time_t& t_begin,
                                epoch_time_t& t_end) = 0;

  /**
   * Check for vector operations
   *
   * @param buffer Message buffer
   *
   * @return True if message interpreted as pool info op
   */
  virtual bool check_pool_info_op(const void * buffer) = 0;


  /**
   * Check for iteration
   *
   * @param buffer Message buffer
   * @param t_begin Begin time constraint Zero for none.
   * @param t_end End time constraint. Zero for none.
   * @param iterator Iteration handle
   *
   * @return True if message interpreted as pool info op
   */
  virtual bool check_iterate(const void * buffer,
                             epoch_time_t& t_begin,
                             epoch_time_t& t_end,
                             Component::IKVStore::pool_iterator_t& iterator) = 0;

  /**
   * Check for op event responses
   *
   * @param buffer Message buffer
   * @param op Operation
   *
   * @return True if message interpreted as op event response
   */  
  virtual bool check_op_event_response(const void * buffer,
                                       Component::ADO_op& op) = 0;

  /**
   * Send response to ADO for table operation
   *
   * @param s Status code
   * @param value_addr [optional] Address of new
   * @param value_len [optional] Length of value in bytes
   *
   */
  virtual void send_table_op_response(const status_t status,
                                      const void *value_addr = nullptr,
                                      size_t value_len = 0) = 0;

  /**
   * Send response to ADO for index operation
   *
   * @param status Status code
   * @param matched_position Offset of the match
   * @param matched_key Key of the match
   *
   */
  virtual void send_find_index_response(const status_t status,
                                        const offset_t matched_position,
                                        const std::string& matched_key) = 0;

  /**
   * Send a vector of key-value pointers
   *
   * @param status Status code
   * @param rv Reference vector
   *
   */
  virtual void send_vector_response(const status_t status,
                                    const IADO_plugin::Reference_vector& rv) = 0;

  /**
   * Send iteration response 
   *
   * @param status Status code
   * @param iterator Iterator handle (updated)
   * @param reference Reference result
   */
  virtual void send_iterate_response(const status_t status,
                                     const Component::IKVStore::pool_iterator_t iterator,
                                     const Component::IKVStore::pool_reference_t reference) = 0;

  /**
   * Send a pool info response
   *
   */
  virtual void send_pool_info_response(const status_t status,
                                       const std::string& info) = 0;


  /**
   * Indicate whether ADO has shutdown
   *
   *
   * @return
   */
  virtual bool has_exited() = 0;

  /**
   * Request graceful shutdown
   *
   *
   * @return S_OK on success
   */
  virtual status_t shutdown() = 0;

  /**
   * Get pool id proxy is associated with
   *
   *
   * @return Pool id
   */
  virtual Component::IKVStore::pool_t pool_id() const = 0;

  /**
   * Get ado id proxy is associated with
   *
   *
   * @return ado id
   */
  virtual std::string ado_id() const = 0;

  /**
   * Get the name of the pool
   *
   * @return pool name
   */
  virtual const std::string& pool_name() const = 0;

  /**
   * Add a key-value pair for deferred unlock
   *
   * @param work_request_id
   * @param key
   */
  virtual void add_deferred_unlock(const uint64_t work_request_id,
                                   const Component::IKVStore::key_t key) = 0;

  /**
   * Updates a key-value pair deferred unlock
   *
   * @param work_request_id
   * @param key
   *
   * @return S_OK or E_NOT_FOUND
   */
  virtual status_t update_deferred_unlock(const uint64_t work_request_id,
                                          const Component::IKVStore::key_t key) = 0;

  /**
   * Retrive (and clear) keys that need to be unlock on associated pool
   *
   * @param work_request_id
   * @param keys Out vector of keys
   */
  virtual void get_deferred_unlocks(const uint64_t work_request_id,
                                    std::vector<Component::IKVStore::key_t> &keys) = 0;


  /**
   * Add a key-value pair for unlock after the life of the ADO
   *
   * @param key Key handle
   */
  virtual void add_life_unlock(const Component::IKVStore::key_t key) = 0;

  /**
   * Remove life lock
   *
   * @param key Key handle
   *
   * @return S_OK or E_NOT_FOUND
   */
  virtual status_t remove_life_unlock(const Component::IKVStore::key_t key) = 0;
  
  /**
   * Release life locks
   *
   */
  virtual void release_life_locks() = 0;
};

/**
 * ADO manager interface.  This is actually a proxy interface communicating with
 * an external process. The ADO manager has a "global" view of the system and
 * can coordinate / schedule resources that are being consumed by the ADO
 * processes.
 */
class IADO_manager_proxy : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xaaafa389,0x1665,0x4e5b,0xa1b1,0x3c,0xff,0x4a,0x5e,0xe2,0x63);
  // clang-format on

  using shared_memory_token_t =
      uint64_t; /*< token identifying shared memory for mcas module */

  /**
   * Launch ADO process.  This method must NOT block.
   *
   * @param auth_id Authentication identifier
   * @param itf KV-store interface
   * @param pool_id Pool identifier
   * @param filename Location of the executable
   * @param args Command line arguments to pass
   * @param shm_token Token to pass to ADO to use to map value memory into
   * process space.
   * @param value_memory_numa_zone NUMA zone to which the value memory resides
   * @param sla Placeholder for some type of SLA/QoS requirements specification.
   *
   * @return Proxy interface, with reference count 1. Use release_ref() to
   * destroy.
   */
  virtual IADO_proxy *create(const uint64_t auth_id,
                             Component::IKVStore* itf,
                             Component::IKVStore::pool_t pool_id,
                             const std::string &pool_name,
                             const size_t pool_size,
                             const unsigned int pool_flags,
                             const uint64_t expected_obj_count,
                             const std::string &filename,
                             std::vector<std::string> &args,
                             numa_node_t value_memory_numa_zone,
                             SLA *sla = nullptr) = 0;

  /**
   * Wait for process to exit.
   *
   * @param ado_proxy Handle to proxy object
   *
   * @return S_OK on success or E_BUSY.
   */
  virtual bool has_exited(IADO_proxy *ado_proxy) = 0;

  /**
   * Shutdown ADO process
   *
   * @param ado Interface pointer to proxy
   *
   * @return S_OK on success
   */
  virtual status_t shutdown(IADO_proxy *ado) = 0;
};

class IADO_manager_proxy_factory : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xfacfa389,0x1665,0x4e5b,0xa1b1,0x3c,0xff,0x4a,0x5e,0xe2,0x63);
  // clang-format on

  virtual IADO_manager_proxy *create(unsigned debug_level, int shard,
                                     std::string cores, float cpu_num) = 0;
};

class IADO_proxy_factory : public Component::IBase {
public:
  // clang-format off
  DECLARE_INTERFACE_UUID(0xfacbb389,0x1665,0x4e5b,0xa1b1,0x3c,0xff,0x4a,0x5e,0xe2,0x63);
  // clang-format on

  virtual IADO_proxy *create(const uint64_t auth_id,
                             Component::IKVStore* itf,
                             Component::IKVStore::pool_t pool_id,
                             const std::string &pool_name,
                             const size_t pool_size,
                             const unsigned int pool_flags,
                             const uint64_t expected_obj_count,
                             const std::string &filename,
                             std::vector<std::string> &args, std::string cores,
                             int memory, float cpu_num,
                             numa_node_t numa_zone) = 0;
};

#pragma GCC diagnostic pop

} // namespace Component

#endif // __API_ADO_ITF_H__
