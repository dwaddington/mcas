/*
   Copyright [2017-2020] [IBM Corporation]
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
#ifndef __mcas_PROTOCOL_H__
#define __mcas_PROTOCOL_H__

#pragma GCC diagnostic push
#ifdef __clang__
#pragma GCC diagnostic ignored "-Wunused-private-field"
#endif

#include <api/ado_itf.h>
#include <api/mcas_itf.h>
#include <common/dump_utils.h>
#include <common/exceptions.h>
#include <common/logging.h>
#include <common/utils.h>

#include <boost/numeric/conversion/cast.hpp>
#include <cassert>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <stdexcept>

//#define PROTOCOL_DEBUG
//#define RESPONSE_DATA_DEBUG

namespace mcas
{
namespace protocol
{
static constexpr unsigned PROTOCOL_VERSION = 0xFB;

enum MSG_TYPE : uint8_t {
  MSG_TYPE_HANDSHAKE       = 0x1,
  MSG_TYPE_HANDSHAKE_REPLY = 0x2,
  MSG_TYPE_CLOSE_SESSION   = 0x3,
  MSG_TYPE_STATS           = 0x4,
  MSG_TYPE_POOL_REQUEST    = 0x10,
  MSG_TYPE_POOL_RESPONSE   = 0x11,
  MSG_TYPE_IO_REQUEST      = 0x20,
  MSG_TYPE_IO_RESPONSE     = 0x21,
  MSG_TYPE_INFO_REQUEST    = 0x30,
  MSG_TYPE_INFO_RESPONSE   = 0x31,
  MSG_TYPE_ADO_REQUEST     = 0x40,
  MSG_TYPE_ADO_RESPONSE    = 0x41,
  MSG_TYPE_PUT_ADO_REQUEST = 0x42,
};

template <typename T>
auto max();

template <>
inline auto max<MSG_TYPE>()
{
  return std::numeric_limits<uint8_t>::max();
}

enum INFO_TYPE : uint32_t {
  /* must be above IKVStore::Attributes */
  INFO_TYPE_FIND_KEY  = 0xF0,
  INFO_TYPE_GET_STATS = 0xF1,
};

enum {
  PROTOCOL_V1 = 0x1, /*< Key-Value Store */
  PROTOCOL_V2 = 0x2, /*< Memory-Centric Active Storage */
};

/* _resvd flags */
enum {
  MSG_RESVD_SCBE   = 0x2, /* indicates short-circuit function (testing only) */
  MSG_RESVD_DIRECT = 0x4, /* indicate get_direct from client side */
};

enum OP_TYPE : uint8_t {
  OP_NONE   = 0,
  OP_CREATE = 1,
  OP_OPEN   = 2,
  OP_CLOSE  = 3,
  OP_PUT    = 4,
  OP_SET    = 4,
  OP_GET    = 5,
  OP_PUT_ADVANCE = 6,  // allocate space for subsequence put or partial put
  OP_PUT_SEGMENT = 7,
  OP_DELETE      = 8,
  OP_ERASE       = 8,
  OP_PREPARE     = 9,  // prepare for immediately following operation
  OP_COUNT       = 10,
  OP_CONFIGURE   = 11,
  OP_STATS       = 12,
  OP_SYNC        = 13,
  OP_ASYNC       = 14,
  OP_PUT_LOCATE  = 15,  // locate (or allocate) and lock value for DMA write
  OP_PUT_RELEASE = 16,  // free lock from DMA write
  OP_GET_LOCATE  = 17,  // locate and lock value for DMA read
  OP_GET_RELEASE = 18,  // free lock from DMA read
  OP_LOCATE      = 19,  // locate space for DMA access
  OP_RELEASE     = 20,  // release space located for DMA access
  OP_INVALID     = 0xFE, // not applicable
};

template <>
inline auto max<OP_TYPE>()
{
  return std::numeric_limits<uint8_t>::max();
}

/* unused */
#if 0
enum IO_TYPE : uint8_t {
  IO_READ  = 0x1,
  IO_WRITE = 0x2,
  IO_ERASE = 0x4,
  IO_MAX   = 0xFF,
};
#endif

/* Base for all messages */
class Message {
  uint64_t _auth_id;  // authorization token
  uint32_t _msg_len;  // message length in bytes
  uint8_t  _version;  // protocol version
  MSG_TYPE _type_id;  // message type id
  union {
    OP_TYPE _op;            // operation code (requests)
    uint8_t _status_delta;  // return -ve delta (responses)
  };
  uint8_t _resvd;  // reserved (but used for a couple of flags)

 public:
  Message(uint64_t auth_id, std::size_t msg_len, MSG_TYPE type_id, OP_TYPE op_param)
      : _auth_id(auth_id),
        _msg_len(boost::numeric_cast<decltype(_msg_len)>(msg_len)),
        _version(PROTOCOL_VERSION),
        _type_id(type_id),
        _resvd{}
  {
    _status_delta = 0;
    assert(op_param);
    _op = op_param;
    assert(this->_op);
  }

  /* modifers */
  void set_status(status_t rc_status)
  {
    const auto i  = static_cast<unsigned int>(abs(rc_status));
    _status_delta = boost::numeric_cast<uint8_t>(i);
  }

  void increase_msg_len(std::size_t sz) { _msg_len = boost::numeric_cast<decltype(_msg_len)>(_msg_len + sz); }

  void add_scbe() { _resvd |= MSG_RESVD_SCBE; }

  void set_direct()
  {
    /* indicate that this is a direct request */
    _resvd = MSG_RESVD_DIRECT;
  }

  /* observers */
  status_t get_status() const { return -1 * _status_delta; }

  void print() const
  {
    PLOG("Message:%p auth_id:%lu msg_len=%u type=%x", static_cast<const void*>(this), _auth_id, _msg_len,
         int(_type_id));
  }

  auto type_id() const { return _type_id; }  // message type id
  auto version() const { return _version; }
  auto msg_len() const { return _msg_len; }  // message length in bytes
  auto auth_id() const { return _auth_id; }
  auto op() const { return _op; }

  bool is_scbe() const { return bool(_resvd & MSG_RESVD_SCBE); }

  bool is_direct() const { return bool(_resvd & MSG_RESVD_DIRECT); }

  /* Convert the response to the expected type after verifying that the
     type_id field matches what is expected.
   */
  template <typename Type>
  const Type* ptr_cast() const
  {
    if (this->_type_id != Type::id)
      throw Protocol_exception("expected %s (0x%x) message - got 0x%x, len %lu", +Type::description, int(Type::id),
                               this->_type_id, this->_msg_len);

    return static_cast<const Type*>(this);
  }
};

namespace
{
inline const Message* message_cast(const void* b)
{
  auto pm = static_cast<const Message*>(b);
  if (!pm) {
    Protocol_exception e("expected message got nullptr");
#if 0
    throw e;
#else
    PWRN("%s", e.cause());
#endif
  }
  assert(pm->version() == PROTOCOL_VERSION);
  if (pm->version() != PROTOCOL_VERSION) {
    Protocol_exception e("expected protocol version 0x%x, got 0x%x", PROTOCOL_VERSION, pm->version());
#if 0
      throw e;
#else
    PWRN("%s", e.cause());
#endif
  }
  return pm;
}

}  // namespace

static_assert(sizeof(Message) == 16, "Unexpected Message data structure size");

/* Constructor definitions */

////////////////////////////////////////////////////////////////////////
// POOL OPERATIONS - create, delete

struct Message_pool_request : public Message {
  static constexpr auto        id          = MSG_TYPE_POOL_REQUEST;
  static constexpr const char* description = "Message_pool_request";

 private:
  using data_t = char; /* most trailing data is typed uint8_t, this is typed charr */
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }

 public:
  Message_pool_request(size_t             buffer_size,
                       uint64_t           auth_id,
                       uint64_t           request_id,
                       size_t             pool_size,
                       size_t             expected_object_count,
                       OP_TYPE            op_,
                       const std::string& pool_name,
                       uint32_t           flags_)
      : Message(auth_id, (sizeof *this), id, op_),
        _pool_size(pool_size),
        _expected_object_count(expected_object_count)
  {
    _flags = flags_;
    (void) request_id;  // unused
    assert(op_);
    assert(this->op());
    if (buffer_size < (sizeof *this)) throw std::length_error(description);
    auto max_data_len = buffer_size - (sizeof *this);

    size_t len = pool_name.length();
    if (len >= max_data_len) throw std::length_error(description);

    strncpy(data(), pool_name.c_str(), len);
    data()[len] = '\0';

    increase_msg_len(len + 1);
  }

  Message_pool_request(size_t buffer_size, uint64_t auth_id, uint64_t request_id, OP_TYPE op_, uint64_t pool_id_)
      : Message_pool_request(buffer_size, auth_id, request_id, 0, 0, op_, "", 0)
  {
    _pool_id = pool_id_;
  }

  const char* pool_name() const { return data(); }

  size_t _pool_size;                              /*< size of pool in bytes */
  auto   pool_size() const { return _pool_size; } /*< size of pool in bytes */
  size_t _expected_object_count;
  auto   expected_object_count() const { return _expected_object_count; }
  union {
    uint64_t _pool_id;
    uint32_t _flags;
  };
  auto pool_id() const { return _pool_id; }
  auto flags() const { return _flags; }
  /* data immediately follows */
} __attribute__((packed));

struct Message_pool_response : public Message {
  static constexpr auto        id          = MSG_TYPE_POOL_RESPONSE;
  static constexpr const char* description = "Message_pool_response";

 private:
  using data_t = uint8_t; /* some trailing data is typed uint8_t, some is typed
                             charu. This used to be char */
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
 public:
  Message_pool_response(uint64_t auth_id) : Message(auth_id, (sizeof *this), id, OP_INVALID) {}
#pragma GCC diagnostic pop

  uint64_t pool_id;
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// Operations which have a request_id
class Message_numbered_request : public Message {
  uint64_t _request_id; /*< id or sender timestamp counter */
  uint64_t _pool_id;

 public:
  Message_numbered_request(uint64_t    auth_id_,
                           std::size_t base_size_,
                           MSG_TYPE    id_,
                           OP_TYPE     op_,
                           uint64_t    request_id_,
                           uint64_t    pool_id_)
      : Message(auth_id_, base_size_, id_, op_),
        _request_id(request_id_),
        _pool_id(pool_id_)
  {
  }
  auto request_id() const { return _request_id; }
  auto pool_id() const { return _pool_id; }
};

class Message_numbered_response : public Message {
  uint64_t _request_id; /*< id or sender timestamp counter */
 public:
  Message_numbered_response(uint64_t auth_id_, std::size_t base_size_, MSG_TYPE id_, OP_TYPE op_, uint64_t request_id_)
      : Message(auth_id_, base_size_, id_, op_),
        _request_id(request_id_)
  {
  }
  auto request_id() const { return _request_id; }
};

////////////////////////////////////////////////////////////////////////
// IO OPERATIONS

struct Message_IO_request : public Message_numbered_request {
  static constexpr auto        id          = MSG_TYPE_IO_REQUEST;
  static constexpr const char* description = "Message_IO_request";
  using data_t                             = uint8_t; /* some trailing data is typed uint8_t, some is typed
                                                         charu. This used to be char */
 private:
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto cdata() const { return static_cast<const char*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_IO_request(size_t      buffer_size,
                     uint64_t    auth_id,
                     uint64_t    request_id_,
                     uint64_t    pool_id_,
                     OP_TYPE     op_,
                     const void* key_,
                     size_t      key_len_,
                     const void* value,
                     size_t      value_len_,
                     uint32_t    flags_)
      : Message_numbered_request(auth_id, (sizeof *this), MSG_TYPE_IO_REQUEST, op_, request_id_, pool_id_),
        _flags(flags_)
  {
    set_key_and_value(buffer_size, key_, key_len_, value, value_len_);
    increase_msg_len(key_len_ + value_len_ + 1);
  }
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_IO_request(uint64_t auth_id, uint64_t request_id_, uint64_t pool_id_, OP_TYPE op_, std::uint64_t target_)
      : Message_numbered_request(auth_id, (sizeof *this), id, op_, request_id_, pool_id_),
        addr(target_)
  {
  }
#pragma GCC diagnostic pop

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  /* For OP_LOCATE. max_offset is requested offset + requested size.
   * Borrow _key_len field to store the region id,
   * and _val_len field to store the max_offset
   */
  Message_IO_request(std::uint64_t auth_id_,
                     std::uint64_t request_id_,
                     uint64_t      pool_id_,
                     OP_TYPE       op_,
                     std::size_t   offset_,
                     std::size_t   size_)
      : Message_numbered_request(auth_id_, (sizeof *this), id, op_, request_id_, pool_id_),
        _key_len(offset_),
        _val_len(size_)
  {
  }
#pragma GCC diagnostic pop

  Message_IO_request(size_t             buffer_size,
                     uint64_t           auth_id,
                     uint64_t           request_id,
                     uint64_t           pool_id_,
                     OP_TYPE            op_,
                     const std::string& key,
                     const std::string& value,
                     uint32_t           flags_)
      : Message_IO_request(buffer_size,
                           auth_id,
                           request_id,
                           pool_id_,
                           op_,
                           key.data(),
                           key.size(),
                           value.data(),
                           value.size(),
                           flags_)
  {
  }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  /* key, and data_length */
  Message_IO_request(size_t      buffer_size,
                     uint64_t    auth_id,
                     uint64_t    request_id,
                     uint64_t    pool_id_,
                     OP_TYPE     op_,
                     const void* key,
                     size_t      key_len_,
                     size_t      value_len,
                     uint32_t    flags_)
      : Message_numbered_request(auth_id, (sizeof *this), id, op_, request_id, pool_id_),
        _flags(flags_)
  {
    set_key_value_len(buffer_size, key, key_len_, value_len);
    increase_msg_len(key_len_ + 1); /* we don't add value len, this will be in next buffer */
  }
#pragma GCC diagnostic pop

  Message_IO_request(size_t             buffer_size,
                     uint64_t           auth_id,
                     uint64_t           request_id,
                     uint64_t           pool_id_,
                     OP_TYPE            op_,
                     const std::string& key,
                     size_t             value_len,
                     uint32_t           flags_)
      : Message_IO_request(buffer_size, auth_id, request_id, pool_id_, op_, key.data(), key.size(), value_len, flags_)
  {
  }

  /*< version used for configure_pool command */
  Message_IO_request(size_t             buffer_size,
                     uint64_t           auth_id,
                     uint64_t           request_id_,
                     uint64_t           pool_id_,
                     OP_TYPE            op_,
                     const std::string& data)
      : Message_IO_request(buffer_size, auth_id, request_id_, pool_id_, op_, data.data(), data.size(), 0, 0)
  {
  }

  inline const uint8_t* key() const { return &data()[0]; }
  auto                  skey() const { return std::string(cdata(), _key_len); }
  inline const char*    cmd() const { return &cdata()[0]; }
  inline const uint8_t* value() const { return &data()[_key_len + 1]; }

  inline size_t get_key_len() const { return _key_len; }
  inline size_t get_value_len() const { return _val_len; }
  /* for OP_LOCATE */
  inline size_t get_offset() const { return _key_len; }
  inline size_t get_size() const { return _val_len; }

  void set_key_value_len(size_t buffer_size, const void* key, const size_t key_len_, const size_t value_len)
  {
    if (UNLIKELY((key_len_ + 1 + (sizeof *this)) > buffer_size))
      throw API_exception("%s::%s - insufficient buffer for "
                          "key-value_len pair (key_len=%lu) (val_len=%lu)",
                          +description, __func__, key_len_, value_len);

    std::memcpy(data(), key, key_len_); /* only copy key and set value length */
    data()[key_len_] = '\0';
    this->_val_len   = value_len;
    this->_key_len   = key_len_;
  }

  void set_key_and_value(const size_t buffer_size,
                         const void*  p_key,
                         const size_t p_key_len,
                         const void*  p_value,
                         const size_t p_value_len)
  {
    assert(buffer_size > 0);
    if (UNLIKELY((p_key_len + p_value_len + 1 + (sizeof *this)) > buffer_size))
      throw API_exception("%s::%s - insufficient buffer for "
                          "key-value pair (key_len=%lu) (val_len=%lu) (buffer_size=%lu)",
                          +description, __func__, p_key_len, p_value_len, buffer_size);

    std::memcpy(data(), p_key, p_key_len);
    data()[p_key_len] = '\0';
    std::memcpy(&data()[p_key_len + 1], p_value, p_value_len);
    this->_val_len = p_value_len;
    this->_key_len = p_key_len;
  }

  /* indicate that this is a direct request and register
   * how much space has been allocated on this side. For
   * get_direct this is allocated by the client
   */
  void set_direct(std::size_t val_len)
  {
    /* indicate that this is a direct request */
    Message::set_direct();
    _val_len = val_len;
  }
  void set_availabe_val_len_from_iob_len(std::size_t iob_length)
  {
    /* iob_length is the manimum possible length. Everything past this object is
     * available */
    _val_len = boost::numeric_cast<decltype(_val_len)>(iob_length - (sizeof *this));
  }
  static bool would_fit(std::size_t needed, std::size_t buffer_size)
  {
    return needed <= buffer_size - sizeof(Message_IO_request);
  }

  auto key_len() const { return _key_len; }
  auto flags() const { return _flags; }

  // fields
 private:
  uint64_t _key_len;
  uint64_t _val_len;

 public:
  uint64_t addr; /* PUT_RELEASE only */
 private:
  uint32_t _flags;
  uint32_t _padding;
  /* data immediately follows */
} __attribute__((packed));

class Message_IO_response : public Message_numbered_response {
  static constexpr uint64_t BIT_TWOSTAGE = 1ULL << 63;

 public:
  static constexpr auto        id          = MSG_TYPE_IO_RESPONSE;
  static constexpr const char* description = "Message_IO_response";
  /* data elements in response to OP_LOCATE */
  struct locate_element {
    std::uint64_t addr;
    std::uint64_t len;
  };

 private:
  using data_t = uint8_t; /* some trailing data is typed uint8_t, some is typed
                             charu. This used to be char */
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }

 public:
  auto cdata() const { return static_cast<const char*>(static_cast<const void*>(this + 1)); }
  auto edata() const { return static_cast<const locate_element*>(static_cast<const void*>(this + 1)); }
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_IO_response(size_t               // buffer_size
                      ,
                      uint64_t auth_id,
                      uint64_t request_id_)
      : Message_numbered_response(auth_id, (sizeof *this), id, OP_INVALID, request_id_),
        _data_len(0)
  {
  }
#pragma GCC diagnostic pop

  void copy_in_data(const void* in_data, size_t len)
  {
    assert(!is_set_twostage_bit());
    std::memcpy(data(), in_data, len);
    _data_len = len;
    increase_msg_len(_data_len);
  }

  void set_data_len_without_data(size_t len)
  {
    assert(!is_set_twostage_bit());
    _data_len = len;
  }

  size_t base_message_size() const { return (sizeof *this); }

  void set_twostage_bit() { _data_len |= BIT_TWOSTAGE; }
  bool is_set_twostage_bit() const { return _data_len & BIT_TWOSTAGE; }

  size_t data_length() const { return _data_len & (~BIT_TWOSTAGE); }

  size_t element_count() const
  {
    assert(data_length() % sizeof(locate_element) == 0);
    return data_length() / sizeof(locate_element);
  }

  // fields
 public:
  uint64_t _data_len; /* bit 63 is twostage flag */
 public:
  uint64_t addr; /* for PUT_LOCATE/GET_LOCATE response */
  uint64_t key;  /* for PUT_LOCATE/GET_LOCATE/LOCATE response */
  /* data immediately follows */
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// INFO REQUEST/RESPONSE

struct Message_INFO_request : public Message {
  static constexpr auto        id          = MSG_TYPE_INFO_REQUEST;
  static constexpr const char* description = "Message_INFO_request";

 private:
  using data_t = uint8_t; /* some trailing data is typed uint8_t, some is typed
                             charu. This used to be char */
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto cdata() const { return static_cast<const char*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_INFO_request(uint64_t auth_id, component::IKVStore::Attribute type_, std::uint64_t pool_id_)
      : Message(auth_id, (sizeof *this), id, OP_INVALID),
        _pool_id(pool_id_),
        _type(type_),
        key_len(0)
  {
  }
  Message_INFO_request(uint64_t auth_id, INFO_TYPE type_, uint64_t pool_id_)
      : Message(auth_id, (sizeof *this), id, OP_INVALID),
        _pool_id(pool_id_),
        _type(type_),
        key_len(0)
  {
  }
#pragma GCC diagnostic pop

  const char* key() const { return cdata(); }
  const char* c_str() const { return cdata(); }
  size_t      base_message_size() const { return (sizeof *this); }
  size_t      message_size() const { return (sizeof *this) + key_len + 1; }

  void set_key(const size_t buffer_size, const std::string& key)
  {
    key_len = key.length();
    if ((key_len + base_message_size() + 1) > buffer_size)
      throw API_exception("%s::%s - insufficient buffer for key (len=%lu)", +description, __func__,
                          std::size_t(key_len));

    std::memcpy(data(), key.c_str(), key_len);
    data()[key_len] = '\0';
  }

  // fields
  uint64_t _pool_id;
  auto     pool_id() const { return _pool_id; }
  uint32_t _type;
  auto     type() const { return _type; }
  uint32_t pad;
  uint64_t offset;
  uint64_t key_len;
  /* data immediately follows */
} __attribute__((packed));

struct Message_INFO_response : public Message {
  static constexpr auto        id          = MSG_TYPE_INFO_RESPONSE;
  static constexpr const char* description = "Message_INFO_response";

 private:
  using data_t = uint8_t;
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto cdata() const { return static_cast<const char*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_INFO_response(uint64_t authid) : Message(authid, (sizeof *this), id, OP_INVALID) {}
#pragma GCC diagnostic pop

  size_t      base_message_size() const { return (sizeof *this); }
  size_t      message_size() const { return (sizeof *this) + _value_len + 1; }
  const char* c_str() const { return cdata(); }

  void set_value(size_t buffer_size, const void* value, size_t len)
  {
    if (UNLIKELY((len + 1 + (sizeof *this)) > buffer_size))
      throw API_exception("%s::%s - insufficient buffer (value len=%lu)", +description, __func__, len);

    std::memcpy(data(), value, len); /* only copy key and set value length */
    data()[len] = '\0';
    _value_len  = len;
  }

  void set_value(std::size_t value_) { _value = value_; }

  auto value_numeric() const { return _value; }

  // fields
  union {
    size_t _value;
    size_t _value_len;
  };
  offset_t    offset;
  std::size_t value() const { return _value; }
  /* data immediately follows */
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// HANDSHAKE

struct Message_handshake : public Message {
  Message_handshake(uint64_t auth_id, uint64_t sequence)
      : Message(auth_id, (sizeof *this), id, OP_INVALID),
        seq(sequence),
        protocol(PROTOCOL_V1)
  {
  }

  static constexpr auto        id          = MSG_TYPE_HANDSHAKE;
  static constexpr const char* description = "Message_handshake";
  // fields
  uint64_t seq;
  uint8_t  protocol;

  void set_as_protocol() { protocol = PROTOCOL_V2; }

} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// HANDSHAKE REPLY

enum {
  HANDSHAKE_REPLY_FLAG_CERT = 0x1,
};

struct Message_handshake_reply : public Message {
  static constexpr auto        id          = MSG_TYPE_HANDSHAKE_REPLY;
  static constexpr const char* description = "Message_handshake_reply";

 private:
  using data_t = uint8_t;
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }
  auto x509_cert() const { return data(); }
  auto x509_cert() { return data(); }

 public:
  Message_handshake_reply(size_t         buffer_size,
                          uint64_t       auth_id,
                          uint64_t       sequence,
                          uint64_t       session_id_,
                          size_t         max_message_size_,
                          unsigned char* x509_cert_ptr,
                          uint32_t       x509_cert_len_)
      : Message(auth_id, (sizeof *this), id, OP_INVALID),
        seq(sequence),
        session_id(session_id_),
        max_message_size(max_message_size_),
        x509_cert_len(x509_cert_len_)
  {
    increase_msg_len(x509_cert_len_);
    if (msg_len() > buffer_size)
      throw Logic_exception("%s::%s - insufficient buffer for Message_handshake_reply", +description, __func__);
    if (x509_cert_ptr && (x509_cert_len_ > 0)) {
      std::memcpy(x509_cert(), x509_cert_ptr, x509_cert_len_);
    }
  }

  // fields
  uint64_t seq;
  uint64_t session_id;
  size_t   max_message_size; /* RDMA max message size in bytes */
  uint32_t x509_cert_len;
  /* x509_cert innediately follows */
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// CLOSE SESSION

struct Message_close_session : public Message {
  static constexpr auto        id          = MSG_TYPE_CLOSE_SESSION;
  static constexpr const char* description = "Message_close_session";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_close_session(uint64_t auth_id) : Message(auth_id, (sizeof *this), id, OP_INVALID) {}
#pragma GCC diagnostic pop

  // fields
  uint64_t seq;

} __attribute__((packed));

struct Message_stats : public Message {
  static constexpr auto        id          = MSG_TYPE_STATS;
  static constexpr const char* description = "Message_stats";

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_stats(uint64_t auth, const component::IMCAS::Shard_stats& shard_stats)
      : Message(auth, (sizeof *this), id, OP_INVALID),
        stats(shard_stats)
  {
  }
#pragma GCC diagnostic pop

  size_t message_size() const { return sizeof(Message_stats); }
  // fields
  /* TROUBLE: the stats are not packed */
  component::IMCAS::Shard_stats stats;
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// ADO MESSAGES

struct Message_ado_request : public Message_numbered_request {
  static constexpr auto        id          = MSG_TYPE_ADO_REQUEST;
  static constexpr const char* description = "Message_ado_request";

 private:
  using data_t = uint8_t;
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto cdata() const { return static_cast<const char*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_ado_request(size_t             buffer_size,
                      uint64_t           auth_id,
                      uint64_t           request_id,
                      uint64_t           pool_id_,
                      const std::string& key,
                      const void*        invocation_data,
                      size_t             invocation_data_len_,
                      uint32_t           flags_,
                      size_t             odvl = 4096)
      : Message_numbered_request(auth_id, (sizeof *this), id, OP_INVALID, request_id, pool_id_),
        ondemand_val_len(odvl),
        flags(flags_)

  {
    this->invocation_data_len = boost::numeric_cast<decltype(this->invocation_data_len)>(invocation_data_len_);
    key_len                   = key.size();
    increase_msg_len(key_len + invocation_data_len + 1);

    if (buffer_size < msg_len())
      throw API_exception("%s::%s - insufficient buffer for Message_ado_request", +description, __func__);

    std::memcpy(data(), key.c_str(), key.size());
    data()[key_len] = '\0';

    if (invocation_data_len > 0) std::memcpy(&data()[key_len + 1], invocation_data, invocation_data_len_);
  }
#pragma GCC diagnostic pop

  /* ca;er could use Message::msg_len */
  size_t         message_size() const { return msg_len(); }
  const char*    key() const { return cdata(); }
  const uint8_t* request() const { return (&data()[key_len + 1]); }
  size_t         request_len() const { return this->invocation_data_len; }
  bool           is_async() const { return flags & component::IMCAS::ADO_FLAG_ASYNC; }
  size_t         get_key_len() const { return key_len; }

  // fields
  uint64_t key_len; /*< does not include null terminator */
  uint64_t ondemand_val_len;
  uint32_t invocation_data_len; /*< does not include null terminator */
  uint32_t flags;
} __attribute__((packed));

struct Message_put_ado_request : public Message_numbered_request {
  static constexpr auto        id          = MSG_TYPE_PUT_ADO_REQUEST;
  static constexpr const char* description = "Message_put_ado_request";

 private:
  using data_t = uint8_t;
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto cdata() const { return static_cast<const char*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // missing initializers
  Message_put_ado_request(size_t             buffer_size,
                          uint64_t           auth_id,
                          uint64_t           request_id,
                          uint64_t           pool_id_,
                          const std::string& key,
                          const void*        invocation_data,
                          size_t             invocation_data_len_,
                          const void*        value,
                          size_t             value_len,
                          size_t             root_len,
                          uint32_t           flags_)
      : Message_numbered_request(auth_id, (sizeof *this), id, OP_INVALID, request_id, pool_id_),
        flags(flags_),
        _val_len(value_len),
        root_val_len(root_len)
  {
    assert(invocation_data);
    assert(invocation_data_len_ > 0);
    assert(value);
    assert(value_len > 0);

    this->invocation_data_len = boost::numeric_cast<decltype(this->invocation_data_len)>(invocation_data_len_);
    key_len                   = key.size();
    increase_msg_len(key_len + 1 + invocation_data_len + _val_len);

    if (buffer_size < message_size())
      throw API_exception("%s::%s - insufficient buffer for Message_ado_request", +description, __func__);

    std::memcpy(data(), key.c_str(), key.size());
    data()[key_len] = '\0'; /* ADO keys, unlike IO keys, are sent with a null terminator */
    byte* ptr       = static_cast<byte*>(&data()[key_len + 1]);

    /* copy in invocation data */
    std::memcpy(ptr, invocation_data, invocation_data_len_);

    /* copy in value_len */
    std::memcpy(ptr + invocation_data_len_, value, value_len);

    val_addr = reinterpret_cast<uint64_t>(value);
  }
#pragma GCC diagnostic pop
  /* callser could use Message::msg_len */
  std::size_t message_size() const { return msg_len(); }
  const char* key() const { return cdata(); }
  size_t      get_key_len() const { return key_len; }
  const void* request() const { return static_cast<const void*>(&data()[key_len + 1]); }
  const void* value() const
  {
    return static_cast<const void*>(static_cast<const byte*>(request()) + invocation_data_len);
  }
  size_t request_len() const { return this->invocation_data_len; }
  size_t value_len() const { return this->_val_len; }
  size_t root_len() const { return this->root_val_len; }
  bool   is_async() const { return flags & component::IMCAS::ADO_FLAG_ASYNC; }

  // fields
  uint64_t key_len;             /*< does not include null terminator */
  uint32_t invocation_data_len; /*< does not include null terminator */
  uint32_t flags;
  uint64_t _val_len;
  uint64_t val_addr;
  uint64_t root_val_len;
} __attribute__((packed));

struct Message_ado_response : public Message_numbered_response {
  using data_t = uint8_t;

 private:
  auto data() const { return static_cast<const data_t*>(static_cast<const void*>(this + 1)); }
  auto data() { return static_cast<data_t*>(static_cast<void*>(this + 1)); }

 public:
  static constexpr auto        id          = MSG_TYPE_ADO_RESPONSE;
  static constexpr const char* description = "Message_ado_response";

  Message_ado_response(size_t buffer_size, status_t status, uint64_t auth_id, uint64_t request_id)
      : Message_numbered_response(auth_id, (sizeof *this), id, OP_INVALID, request_id),
        max_buffer_size(buffer_size)
  {
    set_status(status);
  }

  inline size_t get_response_count() const { return response_count; }

  /* can use base::msg_len() */
  inline size_t message_size() const { return msg_len(); }

  /**
   * Add buffer to the network protocol response message. TODO how do
   * we check for overflow?
   */
  void append_response(void* buffer, size_t buffer_len, uint32_t layer_id)
  {
    assert(buffer);

    if (msg_len() + buffer_len > max_buffer_size) throw API_exception("Message_ado_response out of space");

    uint32_t* data_frame =
        static_cast<uint32_t*>(static_cast<void*>(&data()[response_len])); /* next position in data field */
#ifdef RESPONSE_DATA_DEBUG
    if (buffer_len > 0) {
      PLOG("Shard_ado: converting ADO-IPC response to NPC: %lu", buffer_len);
      hexdump(buffer, buffer_len);
    }
    else {
      PLOG("Shard_ado: converting ADO-IPC response to inline NPC: %p", buffer);
    }
#endif

    if (buffer_len > 0) {
      *data_frame       = boost::numeric_cast<uint32_t>(buffer_len); /* add size prefix */
      *(data_frame + 1) = layer_id;                                  /* add layer identifier */
      response_len += 8;                                             /* sizeof(uint32_t) * 2; */

      std::memcpy(&data()[response_len], buffer, buffer_len);
      response_len += boost::numeric_cast<uint32_t>(buffer_len);
      increase_msg_len(buffer_len + 8);
    }
    else {
      *data_frame       = boost::numeric_cast<uint32_t>(8); /* add size prefix, inline is 8 */
      *(data_frame + 1) = layer_id;                         /* add layer identifier */
      response_len += 8;                                    /* sizeof(uint32_t) * 2; */
      *(reinterpret_cast<void**>(&data()[response_len])) = buffer;
      increase_msg_len(16);
    }

    response_count++;
  }

  /**
   *  Get from response vector. This is called at the MCAS client side.
   */
  void client_get_response(uint32_t index, void*& out_data, size_t& out_data_len, uint32_t& out_layer_id) const
  {
    if (index >= response_count) {
      std::ostringstream s;
      s << "invalid response index " << index << " not less than " << int(response_count);
      throw std::range_error(s.str().c_str());
    }

    const byte* ptr = data();

    size_t pos = 0;
    while (index > 0) {
      auto data_frame = reinterpret_cast<const uint32_t*>(ptr + pos);
      pos += *data_frame + 8; /* sizeof(uint32_t) * 2 */
      index--;
    }

    out_data_len = *reinterpret_cast<const uint32_t*>(ptr + pos);
    out_layer_id = *reinterpret_cast<const uint32_t*>(ptr + pos + 4);
    out_data     = ::malloc(out_data_len);
    if (out_data == nullptr) {
      throw std::bad_alloc();
    }

    std::memcpy(out_data, ptr + pos + 8, out_data_len);

#ifdef RESPONSE_DATA_DEBUG
    PNOTICE("Client-side got buffer_len: %lu (%.*s)", out_data_len, boost::numeric_cast<int>(out_data_len),
            reinterpret_cast<char*>(out_data));
    hexdump(out_data, out_data_len);
#endif
  }

  // fields
  size_t   max_buffer_size;
  uint32_t flags          = 0;
  uint32_t response_len   = 0;
  uint8_t  response_count = 0;
  /* data immediately follows */
} __attribute__((packed));

static_assert(sizeof(Message_IO_request) % 8 == 0, "Message_IO_request should be 64bit aligned");
static_assert(sizeof(Message_IO_response) % 8 == 0, "Message_IO_request should be 64bit aligned");

}  // namespace protocol
namespace Protocol = protocol;
}  // namespace mcas

#pragma GCC diagnostic pop
#endif
