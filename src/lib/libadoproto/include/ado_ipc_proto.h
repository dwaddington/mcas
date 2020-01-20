/*
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
#ifndef __ADO_IPC_PROTOCOL_H__
#define __ADO_IPC_PROTOCOL_H__

#include <boost/numeric/conversion/cast.hpp>
#include <api/ado_itf.h>
#include <common/exceptions.h>
#include <common/dump_utils.h>
#include <vector>
#include <string.h>
#include <stdexcept>
#include <cstring>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wpedantic"

namespace mcas
{

namespace ipc
{
enum {
  MSG_TYPE_BOOTSTRAP_REQUEST = 1,
  MSG_TYPE_WORK_REQUEST  = 2,
  MSG_TYPE_WORK_RESPONSE = 3,
  MSG_TYPE_TABLE_OP_REQUEST = 4,
  MSG_TYPE_TABLE_OP_RESPONSE = 5,
  MSG_TYPE_INDEX_REQUEST = 6,
  MSG_TYPE_INDEX_RESPONSE = 7,
  MSG_TYPE_CHIRP = 8,
  MSG_TYPE_MAP_MEMORY = 9,
  MSG_TYPE_VECTOR_REQUEST = 10,
  MSG_TYPE_VECTOR_RESPONSE = 11,
  MSG_TYPE_POOL_INFO_RESPONSE = 12,
  MSG_TYPE_OP_EVENT = 13,
  MSG_TYPE_OP_EVENT_RESPONSE = 14,
  MSG_TYPE_ITERATE_REQUEST = 15,
  MSG_TYPE_ITERATE_RESPONSE = 16,
};

typedef enum {
  HELLO = 1,
  HEARTBEAT,
  OK,
  SHUTDOWN,
  PANIC,
  POOL_INFO_REQUEST,
} chirp_t;

// typedef enum {
//   CREATE,
//   OPEN,
//   ERASE,
//   VALUE_RESIZE,
//   ALLOCATE_POOL_MEMORY,
//   FREE_POOL_MEMORY
// } table_op_enum;

static constexpr uint16_t MAGIC = 0xDEAF;

struct Message {

  Message(uint16_t type_id_p) : magic(MAGIC), type_id(type_id_p)
  {
  }

  template <typename Type>
  const Type *ptr_cast() const
  {
    if (this->type_id != Type::id)
      throw Protocol_exception("expected %s (0x%x) message - got 0x%x",
                               Type::description, Type::id,
                               this->type_id);
    return static_cast<const Type *>(this);
  }

  inline static bool is_valid(const void * buffer) {
    return *(reinterpret_cast<const uint16_t*>(buffer)) == MAGIC;
  }

  inline static uint16_t type(const void * buffer) {
    return (reinterpret_cast<const Message*>(buffer))->type_id;
  }
  
  uint16_t  magic;
  uint16_t  type_id;  // message type id
} __attribute__((packed));

//-------------

struct Chirp : public Message {
  static constexpr uint8_t id = MSG_TYPE_CHIRP;
  static constexpr const char *description = "mcas::ipc::Chirp";

  Chirp(chirp_t _type) : Message(id), type(_type)
  {
  }

  chirp_t type;
} __attribute__((packed));

//-------------

struct Op_event : public Message {
  static constexpr uint8_t id = MSG_TYPE_OP_EVENT;
  static constexpr const char *description = "mcas::ipc::Op_event";

  Op_event(Component::ADO_op _op) : Message(id), op(_op)
  {
  }

  Component::ADO_op op;
} __attribute__((packed));

//-------------

struct Op_event_response : public Message {
  static constexpr uint8_t id = MSG_TYPE_OP_EVENT_RESPONSE;
  static constexpr const char *description = "mcas::ipc::Op_event_response";

  Op_event_response(Component::ADO_op _op) : Message(id), op(_op)
  {
  }

  Component::ADO_op op;
} __attribute__((packed));

//-------------

struct Bootstrap_request : public Message {
  static constexpr uint8_t id = MSG_TYPE_BOOTSTRAP_REQUEST;
  static constexpr const char *description = "mcas::ipc::Bootstrap_request";

  Bootstrap_request(size_t buffer_size,
                    const uint64_t _auth_id,
                    const std::string& _pool_name,
                    const size_t _pool_size,
                    const unsigned int _pool_flags,
                    const uint64_t _expected_obj_count,
                    const bool _open_existing)
    : Message(id),
      auth_id(_auth_id),
      pool_size(_pool_size),
      expected_obj_count(_expected_obj_count),
      pool_name_len(_pool_name.size()),
      pool_flags(_pool_flags),
      open_existing(_open_existing)
  {
    if((sizeof(Bootstrap_request) + _pool_name.size() + 1) > buffer_size)
      throw std::length_error(description);

    ::memcpy(pool_name, _pool_name.data(), _pool_name.size());
    pool_name[_pool_name.size()] = '\0';
  }

  uint64_t     auth_id;
  size_t       pool_size;
  uint64_t     expected_obj_count;
  size_t       pool_name_len;
  unsigned int pool_flags;
  bool         open_existing;
  char         pool_name[];
  
} __attribute__((packed));


//-------------

struct Map_memory : public Message {
  static constexpr uint8_t id = MSG_TYPE_MAP_MEMORY;
  static constexpr const char *description = "mcas::ipc::Map_memory";

  Map_memory(size_t buffer_size,
             uint64_t _token,
             size_t _size,
             void * _shard_addr)
    : Message(id), token(_token), size(_size), shard_addr(_shard_addr)
  {
    if(sizeof(Map_memory) > buffer_size)
      throw std::length_error(description);
  }
  
  uint64_t token;
  size_t   size;
  void *   shard_addr;
  
} __attribute__((packed));

//-------------

struct Work_request : public Message {
  static constexpr uint8_t id = MSG_TYPE_WORK_REQUEST;
  static constexpr const char *description = "mcas::ipc::Work_request";

  Work_request(size_t buffer_size,
               const uint64_t _work_key,
               const std::string& _key_string,
               const uint64_t _value_addr,
               const uint64_t _value_len,
               const uint64_t _detached_value_addr,
               const uint64_t _detached_value_len,
               const void * _invocation_data,
               const size_t _invocation_data_len,
               const bool _new_root)
    : Message(id),
      work_key(_work_key),
      key_len(_key_string.length()),
      value_addr(_value_addr),
      value_len(_value_len),
      detached_value_addr(_detached_value_addr),
      detached_value_len(_detached_value_len),
      invocation_data_len(_invocation_data_len),
      new_root(_new_root)
  {
    assert(detached_value_addr ? detached_value_len > 0 : true);
    // bounds check
    if((_invocation_data_len + _key_string.length() + sizeof(Work_request)) > buffer_size)
      throw std::length_error(description);
    ::memcpy(data, _key_string.data(), _key_string.length());
    ::memcpy(&data[key_len], _invocation_data, invocation_data_len);
  }           

  inline size_t get_key_len() const { return key_len; }
  inline std::string get_key() const { return std::string(&data[0], key_len); }
  inline size_t get_invocation_data_len() const { return invocation_data_len; }
  inline const char * get_invocation_data() const { return &data[key_len]; }
  inline void * get_value_addr() const { return reinterpret_cast<void *>(value_addr); }
  inline void * get_detached_value_addr() const { return reinterpret_cast<void *>(detached_value_addr); }
  
  uint64_t work_key;
  uint64_t key_len;
  uint64_t value_addr;
  uint64_t value_len;
  uint64_t detached_value_addr;
  uint64_t detached_value_len;
  uint64_t invocation_data_len;
  bool     new_root;
  char     data[];
  
} __attribute__((packed));



//-------------

struct Work_response : public Message {
  static constexpr uint8_t id = MSG_TYPE_WORK_RESPONSE;
  static constexpr const char *description = "mcas::ipc::Work_response";

  Work_response(size_t buffer_size,
                const uint64_t _work_key,
                const status_t _status,
                Component::IADO_plugin::response_buffer_vector_t& response_buffers)
    : Message(id), work_key(_work_key), status(_status)
  {
    assert(buffer_size > 0);

    char * data_ptr = data;
    size_t buffer_used = sizeof(Work_response);
    
    /* first copy over response buffers */
    response_len = 0;
    for(auto i=response_buffers.begin(); i!=response_buffers.end(); i++) {
      const Component::IADO_plugin::response_buffer_t& b = *i;
      if(!b.pool_ref) {
        ::memcpy(data_ptr, b.ptr, b.len);
        data_ptr += b.len;
        response_len += b.len;

        buffer_used += response_len;
        if(buffer_used > buffer_size)
          throw std::length_error(description);
      }
    }

    
    /* then copy pool references  */
    pool_buffer_count = 0;
    for(auto i=response_buffers.begin(); i!=response_buffers.end(); i++) {
      const Component::IADO_plugin::response_buffer_t& b = *i;
      if(b.pool_ref) {
        auto pbref = reinterpret_cast<uint64_t*>(data_ptr);
        pbref[0] = b.offset;
        pbref[1] = b.len;
        data_ptr += 16; // move two 64bit integers forward
        pool_buffer_count ++;
      }
    }
    buffer_used += pool_buffer_count * 16;
    if(buffer_used > buffer_size)
      throw std::length_error(description);

  }

  inline const void * get_response() const {
    return data;
  }

  size_t get_message_size() const { return sizeof(Work_response) + response_len; }
  
  void get_pool_buffers(Component::IADO_plugin::response_buffer_vector_t& out_vector) const {
    const uint64_t * pb = reinterpret_cast<const uint64_t*>(data + response_len);
    for(uint32_t i=0; i<pool_buffer_count; i++) {
      out_vector.push_back({pb[0],pb[1],true});
      pb+=2;
    }
  }
  
  uint64_t work_key;
  int32_t  status;
  uint32_t pool_buffer_count;
  uint64_t response_len;  
  char     data[]; /* pool buffer vector, followed by response data */
} __attribute__((packed));


//-------------

struct Table_request : public Message {
  static constexpr uint8_t id = MSG_TYPE_TABLE_OP_REQUEST;
  static constexpr const char *description = "mcas::ipc::Table_request";

  Table_request(size_t buffer_size,
                uint64_t _work_key,
                const std::string& _key,
                uint64_t _value_len,
                uint64_t _addr,
                uint64_t _align_or_flags,
                Component::ADO_op _op)
    : Message(id), work_key(_work_key),value_len(_value_len),
      key_len(_key.length()), addr(_addr),
      align_or_flags(_align_or_flags), op(_op)
  {
    if((_key.size() + sizeof(Table_request)) > buffer_size)
      throw std::length_error(description);
    ::memcpy(key, _key.data(), _key.size());
  }

  Table_request(size_t buffer_size,
                uint64_t _work_key,
                uint64_t _value_len,
                uint64_t _addr,
                uint64_t _align_or_flags,
                Component::ADO_op _op)
    : Message(id), work_key(_work_key),value_len(_value_len),
      key_len(0), addr(_addr),
      align_or_flags(_align_or_flags), op(_op)
  {
    if(sizeof(Table_request) > buffer_size)
      throw std::length_error(description);
  }

  uint64_t          work_key;
  uint64_t          value_len;
  uint64_t          key_len;
  uint64_t          addr;
  uint64_t          align_or_flags;
  Component::ADO_op op;
  char              key[];

} __attribute__((packed));


//-------------

struct Table_response : public Message {
  static constexpr uint8_t id = MSG_TYPE_TABLE_OP_RESPONSE;
  static constexpr const char *description = "mcas::ipc::Table_response";

  Table_response(size_t buffer_size,
                 status_t _status,
                 uint64_t _value_addr,
                 uint64_t _value_len)
    : Message(id), value_addr(_value_addr), value_len(_value_len), status(_status)
  {
    if(sizeof(Table_response) > buffer_size)
      throw std::length_error(description);
  }

  uint64_t value_addr;
  uint64_t value_len;
  status_t status;
  
} __attribute__((packed));                


//-------------

struct Index_request : public Message {
  static constexpr uint8_t id = MSG_TYPE_INDEX_REQUEST;
  static constexpr const char *description = "mcas::ipc::Index_request";

  Index_request(size_t buffer_size,
                uint64_t _begin_pos,
                Component::IKVIndex::find_t _find_type,
                const std::string& _expr)
    : Message(id), begin_pos(_begin_pos),
      find_type(_find_type)
  {
    if((sizeof(Index_request) + _expr.size()) > buffer_size)
      throw std::length_error(description);

    expr_len = _expr.size();
    ::memcpy(expr, _expr.data(), expr_len);
  }

  uint64_t begin_pos;
  uint64_t expr_len;
  Component::IKVIndex::find_t find_type;
  char expr[];
  
} __attribute__((packed));


//-------------

struct Index_response : public Message {
  static constexpr uint8_t id = MSG_TYPE_INDEX_RESPONSE;
  static constexpr const char *description = "mcas::ipc::Index_response";

  Index_response(size_t buffer_size,
                 uint64_t _matched_pos,
                 status_t _status,
                 const std::string _matched_key)
    : Message(id), matched_pos(_matched_pos),
      matched_key_len(_matched_key.size()), status(_status)
  {
    if((sizeof(Index_response) + _matched_key.size()) > buffer_size)
      throw std::length_error(description);

    ::memcpy(matched_key, _matched_key.data(), matched_key_len);
  }

  uint64_t matched_pos;
  uint64_t matched_key_len;
  status_t status;
  char matched_key[];
  
} __attribute__((packed));


//-------------

struct Vector_request : public Message {
  static constexpr uint8_t id = MSG_TYPE_VECTOR_REQUEST;
  static constexpr const char *description = "mcas::ipc::Vector_request";

  Vector_request(const epoch_time_t _t_begin,
                 const epoch_time_t _t_end) 
    : Message(id), t_begin(_t_begin), t_end(_t_end)
  {
  }

  epoch_time_t t_begin;
  epoch_time_t t_end;
  
} __attribute__((packed));

//-------------

struct Vector_response : public Message {
  static constexpr uint8_t id = MSG_TYPE_VECTOR_RESPONSE;
  static constexpr const char *description = "mcas::ipc::Vector_request";

  Vector_response(status_t _status,
                  const Component::IADO_plugin::Reference_vector& _refs)
    : Message(id), status(_status), refs(_refs)
  {
  }

  status_t                                 status;
  Component::IADO_plugin::Reference_vector refs;
  
} __attribute__((packed));


//-------------

struct Pool_info_response : public Message {
  static constexpr uint8_t id = MSG_TYPE_POOL_INFO_RESPONSE;
  static constexpr const char *description = "mcas::ipc::Pool_info_request";

  Pool_info_response(size_t buffer_size,
                     status_t _status,
                     const std::string& _result)
    : Message(id), status(_status)
  {
    if((sizeof(Pool_info_response) + _result.size() + 1) > buffer_size)
      throw std::length_error(description);

    ::memcpy(result, _result.data(), _result.size());
    result[_result.size()] = '\0';
  }

  status_t status;
  size_t   result_len;
  char     result[];
  
} __attribute__((packed));


//-------------

struct Iterate_request : public Message {
  static constexpr uint8_t id = MSG_TYPE_ITERATE_REQUEST;
  static constexpr const char *description = "mcas::ipc::Iterate_request";

  Iterate_request(const epoch_time_t _t_begin,
                  const epoch_time_t _t_end,
                  Component::IKVStore::pool_iterator_t _iterator)
    : Message(id), t_begin(_t_begin), t_end(_t_end), iterator(_iterator)
  {
  }

  const epoch_time_t t_begin;
  const epoch_time_t t_end;
  Component::IKVStore::pool_iterator_t iterator;
  
} __attribute__((packed));


struct Iterate_response : public Message {
  static constexpr uint8_t id = MSG_TYPE_ITERATE_RESPONSE;
  static constexpr const char *description = "mcas::ipc::Iterate_response";

  Iterate_response(status_t _status,
                   const Component::IKVStore::pool_iterator_t _iterator,
                   const Component::IKVStore::pool_reference_t& _reference)
    : Message(id), status(_status), iterator(_iterator), reference(_reference)
  {
  }
  
  status_t                              status;
  Component::IKVStore::pool_iterator_t  iterator;
  Component::IKVStore::pool_reference_t reference;
  
} __attribute__((packed));



} // ipc
} // mcas

#pragma GCC diagnostic pop

#endif
