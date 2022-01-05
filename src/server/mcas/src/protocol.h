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
#include <common/pointer_cast.h>
#include <common/string_view.h>
#include <common/utils.h>

#include <boost/numeric/conversion/cast.hpp>
#include <cassert>
#include <cstring>
#include <limits>
#include <sstream>
#include <stdexcept>

//#define PROTOCOL_DEBUG
//#define RESPONSE_DATA_DEBUG

namespace mcas
{
namespace protocol
{
static constexpr unsigned PROTOCOL_VERSION = 0xFB;

enum class MSG_TYPE : uint8_t {
  HANDSHAKE       = 0x1,
  HANDSHAKE_REPLY = 0x2,
  CLOSE_SESSION   = 0x3,
  /* Note: an INFO_REQUEST with INFO_TYPE::GET_STATS will be answered by STATS, not INFO_RESPONSE */
  STATS           = 0x4,
  NO_MSG          = 0x5,
  PING            = 0x6,
  PONG            = 0x7,
  POOL_REQUEST    = 0x10,
  POOL_RESPONSE   = 0x11,
  IO_REQUEST      = 0x20,
  IO_RESPONSE     = 0x21,
  INFO_REQUEST    = 0x30,
  INFO_RESPONSE   = 0x31,
  ADO_REQUEST     = 0x40,
  ADO_RESPONSE    = 0x41,
  PUT_ADO_REQUEST = 0x42,
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


enum class AUTH_TYPE {
  AUTH_NONE = 0x0,
  AUTH_TLS_HMAC = 0x1,
  AUTH_TLS_FULL = 0x2,
};

/* _resvd flags */
enum {
  MSG_RESVD_SCBE   = 0x2, /* indicates short-circuit function (testing only) */
  MSG_RESVD_DIRECT = 0x4, /* indicate get_direct from client side */
};

enum class OP_TYPE : uint8_t {
  NONE   = 0,
  CREATE = 1,
  OPEN   = 2,
  CLOSE  = 3,
  PUT    = 4,
  SET    = 4,
  GET    = 5,
  PUT_ADVANCE = 6,  // allocate space for subsequence put or partial put
  PUT_SEGMENT = 7,
  DELETE      = 8,
  ERASE       = 8,
  PREPARE     = 9,  // prepare for immediately following operation
  COUNT       = 10,
  CONFIGURE   = 11,
  STATS       = 12,
  SYNC        = 13,
  ASYNC       = 14,
  PUT_LOCATE  = 15,  // locate (or allocate) and lock value for DMA write
  PUT_RELEASE = 16,  // free lock from DMA write
  GET_LOCATE  = 17,  // locate and lock value for DMA read
  GET_RELEASE = 18,  // free lock from DMA read
  LOCATE      = 19,  // locate space for DMA access
  RELEASE     = 20,  // release space located for DMA access
  RELEASE_WITH_FLUSH = 21,  // flush and release space located for DMA access
  INVALID     = 0xFE, // not applicable
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

 protected:
  using string_view_byte = common::basic_string_view<common::byte>;
  using string_view_key = string_view_byte;
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
    PLOG("Message:%p auth_id:%lu msg_len=%u type=%x",
         static_cast<const void*>(this), _auth_id, _msg_len,
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
  static constexpr auto        id          = MSG_TYPE::POOL_REQUEST;
  static constexpr const char* description = "Message_pool_request";

 private:
  using data_t = char; /* most trailing data is typed uint8_t, this is typed charr */
  auto data() const {
    return common::pointer_cast<const data_t>(this + 1);
  }
  auto data() {
    return common::pointer_cast<data_t>(this + 1);
  }

  Message_pool_request(size_t             buffer_size,
                       uint64_t           auth_id,
                       size_t             pool_size,
                       size_t             expected_object_count,
                       OP_TYPE            op_,
                       common::string_view pool_name,
                       uint64_t           pool_id_,
                       uint32_t           flags_,
                       const addr_t       base_addr_)
      : Message(auth_id, (sizeof *this), id, op_),
        _pool_size(pool_size),
        _expected_object_count(expected_object_count),
        _base_addr(base_addr_),
        _pool_id(pool_id_),
        _flags(flags_)
  {
    assert(op_);
    assert(this->op());
    if (buffer_size < (sizeof *this)) throw std::length_error(description);
    auto max_data_len = buffer_size - (sizeof *this);

    size_t len = pool_name.length();
    if (len >= max_data_len) throw std::length_error(description);

    strncpy(data(), pool_name.data(), len);
    data()[len] = '\0';

    increase_msg_len(len + 1);
  }

 public:
  Message_pool_request(size_t             buffer_size,
                       uint64_t           auth_id,
                       uint64_t           /* request_id */,
                       size_t             pool_size,
                       size_t             expected_object_count,
                       OP_TYPE            op_,
                       common::string_view pool_name,
                       uint32_t           flags_,
                       const addr_t       base_addr_)
      : Message_pool_request(buffer_size,
                             auth_id,
                             pool_size,
                             expected_object_count,
                             op_,
                             pool_name,
                             0,
                             flags_,
                             base_addr_)
  {
  }

  Message_pool_request(size_t buffer_size,
                       uint64_t auth_id,
                       uint64_t /* request_id */,
                       OP_TYPE op_,
                       uint64_t pool_id_)
      : Message_pool_request(buffer_size,
                             auth_id,
                             0,
                             0,
                             op_,
                             "",
                             pool_id_,
                             0,
                             0)
  {
  }

  const char* pool_name() const { return data(); }

  auto pool_size() const { return _pool_size; } /*< size of pool in bytes */
  auto expected_object_count() const { return _expected_object_count; }
  auto pool_id() const { return _pool_id; }
  auto flags() const { return boost::numeric_cast<uint32_t>(_flags); }
  auto base_addr() const { return _base_addr; }
  
private:
  size_t       _pool_size; /*< size of pool in bytes */
  size_t       _expected_object_count;
  addr_t       _base_addr; /*< virtual base address to use in ADO space */
  uint64_t     _pool_id;
  uint64_t     _flags;
} __attribute__((packed));


struct Message_pool_response : public Message {
  static constexpr auto        id          = MSG_TYPE::POOL_RESPONSE;
  static constexpr const char* description = "Message_pool_response";

 private:
  using data_t = uint8_t; /* some trailing data is typed uint8_t, some is typed
                             charu. This used to be char */
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }
  auto data() { return common::pointer_cast<data_t>(this + 1); }
 public:
  Message_pool_response(uint64_t auth_id)
		: Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID)
		, pool_id()
#if CW_TEST
		, scratchpad_base()
		, scratchpad_size()
		, scratchpad_rma_key()
#endif
	{}

  uint64_t pool_id;
#if CW_TEST
	uint64_t scratchpad_base;
	uint64_t scratchpad_size;
	uint64_t scratchpad_rma_key;
#endif
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// Operations which have a request_id
class Message_numbered_request : public Message {
  uint64_t _request_id; /*< id or sender timestamp counter */
  uint64_t _pool_id;
 protected:
  template <typename T>
    using basic_string_view = common::basic_string_view<T>;
  using byte = common::byte;

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
  static constexpr auto        id          = MSG_TYPE::IO_REQUEST;
  static constexpr const char* description = "Message_IO_request";
  using data_t                             = uint8_t; /* some trailing data is typed uint8_t, some is typed
                                                         char. This used to be char */
  using string_view = common::string_view;
  using string_view_byte = common::basic_string_view<common::byte>;
  using string_view_key = string_view_byte;
  using string_view_value = string_view_byte;

 private:
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }
  auto cdata() const { return common::pointer_cast<const char>(this + 1); }
  auto data() { return common::pointer_cast<data_t>(this + 1); }
  auto cache_aligned_value_offset() const { return round_up_t(_key_len, 64); }

 public:
  Message_IO_request(size_t      buffer_size,
                     uint64_t    auth_id,
                     uint64_t    request_id_,
                     uint64_t    pool_id_,
                     OP_TYPE     op_,
                     string_view_key    key_,
                     const void* value,
                     size_t      value_len_,
                     uint32_t    flags_)
      : Message_numbered_request(auth_id, (sizeof *this), MSG_TYPE::IO_REQUEST, op_, request_id_, pool_id_),
        _key_len(),
        _val_len(),
        addr(),
        _flags(flags_),
        _padding()
  {
    set_key_and_value(buffer_size, key_, value, value_len_);
    increase_msg_len(cache_aligned_value_offset() + value_len_);
  }

  Message_IO_request(uint64_t auth_id, uint64_t request_id_, uint64_t pool_id_, OP_TYPE op_, std::uint64_t target_)
      : Message_numbered_request(auth_id, (sizeof *this), id, op_, request_id_, pool_id_),
        _key_len(),
        _val_len(),
        addr(target_),
        _flags(0),
        _padding()
  {
  }

  /* For OP_TYPE::LOCATE. max_offset is requested offset + requested size.
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
        _val_len(size_),
        addr(),
        _flags(0),
        _padding()
  {
  }

  Message_IO_request(size_t             buffer_size,
                     uint64_t           auth_id,
                     uint64_t           request_id,
                     uint64_t           pool_id_,
                     OP_TYPE            op_,
                     string_view_key    key,
                     string_view_value  value,
                     uint32_t           flags_)
      : Message_IO_request(buffer_size,
                           auth_id,
                           request_id,
                           pool_id_,
                           op_,
                           key,
                           value.data(),
                           value.size(),
                           flags_)
  {
  }

  /* key, and data_length */
  Message_IO_request(size_t      buffer_size,
                     uint64_t    auth_id,
                     uint64_t    request_id,
                     uint64_t    pool_id_,
                     OP_TYPE     op_,
                     string_view_key key_,
                     size_t      value_len,
                     uint32_t    flags_)
      : Message_numbered_request(auth_id, (sizeof *this), id, op_, request_id, pool_id_),
        _key_len(),
        _val_len(),
        addr(),
        _flags(flags_),
        _padding()
  {
    set_key_value_len(buffer_size, key_, value_len);
    increase_msg_len(key_.size()); /* we don't add value len, this will be in next buffer */
  }
#if 0
  Message_IO_request(size_t             buffer_size,
                     uint64_t           auth_id,
                     uint64_t           request_id,
                     uint64_t           pool_id_,
                     OP_TYPE            op_,
                     string_view_key    key,
                     size_t             value_len,
                     uint32_t           flags_)
      : Message_IO_request(buffer_size, auth_id, request_id, pool_id_, op_, key.data(), key.size(), value_len, flags_)
  {
  }
#endif
  /*< version used for configure_pool command */
  Message_IO_request(size_t             buffer_size,
                     uint64_t           auth_id,
                     uint64_t           request_id_,
                     uint64_t           pool_id_,
                     OP_TYPE            op_,
                     string_view        data)
      : Message_IO_request(buffer_size, auth_id, request_id_, pool_id_, op_, string_view_key(common::pointer_cast<string_view_key::value_type>(data.data()), data.size()), 0, 0)
  {
  }

  common::string_view   key() const { return common::string_view(cdata(), _key_len); }
  common::string_view   cmd() const { return common::string_view(cdata(), _key_len); }
  inline const uint8_t* value() const { return &data()[cache_aligned_value_offset()]; }

  auto key_len() const { return _key_len; }
  inline size_t value_len() const { return _val_len; }
  /* for OP_TYPE::LOCATE, the (offset, size) pair are in (_key_len, _val_len) */
  inline size_t get_locate_offset() const { return _key_len; }
  inline size_t get_locate_end() const { return _key_len + _val_len; }

  void set_key_value_len(size_t buffer_size_, string_view_key key_, const size_t value_len_)
  {
    if (UNLIKELY((key_.size() + (sizeof *this)) > buffer_size_))
      throw API_exception("%s::%s - insufficient buffer for "
                          "key-value_len pair (key_len=%lu) (val_len=%lu)",
                          +description, __func__, key_.size(), value_len_);

    std::memcpy(data(), key_.data(), key_.size()); /* only copy key and set value length */
    this->_val_len   = value_len_;
    this->_key_len   = key_.size();
  }

  void set_key_and_value(const size_t buffer_size,
                         const string_view_key p_key,
                         const void*  p_value,
                         const size_t p_value_len)
  {
    assert(buffer_size > 0);
    if (UNLIKELY((p_key.size() + p_value_len + (sizeof *this)) > buffer_size))
      throw API_exception("%s::%s - insufficient buffer for "
                          "key-value pair (key_len=%lu) (val_len=%lu) (buffer_size=%lu)",
                          +description, __func__, p_key.size(), p_value_len, buffer_size);

    std::memcpy(data(), p_key.data(), p_key.size());
    this->_val_len = p_value_len;
    this->_key_len = p_key.size();
    std::memcpy(&data()[cache_aligned_value_offset()], p_value, p_value_len);
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
  static constexpr auto        id          = MSG_TYPE::IO_RESPONSE;
  static constexpr const char* description = "Message_IO_response";
  /* data elements in response to OP_TYPE::LOCATE */
  struct locate_element {
    std::uint64_t addr;
    std::uint64_t len;
  };

 private:
  using data_t = uint8_t; /* some trailing data is typed uint8_t, some is typed
                             charu. This used to be char */
 public: /* exposed to allow direct copy to data buffer by speculative get_direct */
  auto data() { return common::pointer_cast<data_t>(this + 1); }

 public:
  auto cdata() const { return common::pointer_cast<const char>(this + 1); }
  auto edata() const { return common::pointer_cast<const locate_element>(this + 1); }
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }

  Message_IO_response(size_t /* buffer_size */,
                      uint64_t auth_id,
                      uint64_t request_id_)
      : Message_numbered_response(auth_id, (sizeof *this), id, OP_TYPE::INVALID, request_id_),
        _data_len(0),
        addr(),
        key(),
        pad{}
  {
  }

  void set_data_len(size_t len)
  {
    _data_len = len;
    increase_msg_len(_data_len);
  }

  void copy_in_data(const void* in_data, size_t len)
  {
    assert(!is_set_twostage_bit());
    std::memcpy(data(), in_data, len);
    set_data_len(len);
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
  char pad[16];  /* pad to cache line multiple (64) */
  /* data immediately follows */
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// INFO REQUEST/RESPONSE

struct Message_INFO_request : public Message {
  static constexpr auto        id          = MSG_TYPE::INFO_REQUEST;
  static constexpr const char* description = "Message_INFO_request";

 private:
  using data_t = uint8_t; /* some trailing data is typed uint8_t, some is typed
                             char. This used to be char */
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }
  auto cdata() const { return common::pointer_cast<const char>(this + 1); }
  auto data() { return common::pointer_cast<data_t>(this + 1); }

 public:
  Message_INFO_request(uint64_t auth_id, component::IKVStore::Attribute type_, std::uint64_t pool_id_)
      : Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID),
        _pool_id(pool_id_),
        _type(uint32_t(type_)),
        pad(),
        offset(),
        key_len(0)
  {
  }

  Message_INFO_request(uint64_t auth_id, INFO_TYPE type_, uint64_t pool_id_)
      : Message_INFO_request(auth_id, type_, pool_id_, 0)
  {
  }

  Message_INFO_request(uint64_t auth_id, INFO_TYPE type_, uint64_t pool_id_, offset_t offset_)
      : Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID),
        _pool_id(pool_id_),
        _type(type_),
        pad(),
        offset(offset_),
        key_len(0)
  {
  }

  common::string_view key() const { return common::string_view(cdata(), key_len); }
 private:
  const char* c_str() const { return cdata(); }
 public:
  size_t      base_message_size() const { return (sizeof *this); }
  size_t      message_size() const { return (sizeof *this) + key_len + 1; }

  void set_key(const size_t buffer_size, string_view_key key)
  {
    key_len = key.length();
    if ((key_len + base_message_size() + 1) > buffer_size)
      throw API_exception("%s::%s - insufficient buffer for key (len=%lu)", +description, __func__,
                          std::size_t(key_len));

    std::memcpy(data(), key.data(), key_len);
    data()[key_len] = '\0';
  }

  auto     pool_id() const { return _pool_id; }
  auto     type() const { return _type; }
  // fields
  uint64_t _pool_id;
  uint32_t _type;
  uint32_t pad;
  uint64_t offset;
  uint64_t key_len;
  /* data immediately follows */
} __attribute__((packed));

struct Message_INFO_response : public Message {
  static constexpr auto        id          = MSG_TYPE::INFO_RESPONSE;
  static constexpr const char* description = "Message_INFO_response";

 private:
  using data_t = uint8_t;
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }
  auto cdata() const { return common::pointer_cast<const char>(this + 1); }
  auto data() { return common::pointer_cast<data_t>(this + 1); }

  Message_INFO_response(uint64_t authid_, offset_t offset_) : Message(authid_, (sizeof *this), id, OP_TYPE::INVALID), _v{}, _offset(offset_) {}
  void set_value(size_t buffer_size, const void* value, size_t len)
  {
    if (UNLIKELY((len + 1 + (sizeof *this)) > buffer_size))
      throw API_exception("%s::%s - insufficient buffer (value len=%lu)", +description, __func__, len);

    std::memcpy(data(), value, len); /* only copy key and set value length */
    data()[len] = '\0';
    _v._value_len  = len;
  }

 public:
  Message_INFO_response(uint64_t authid_) : Message_INFO_response(authid_, offset_t()) {}

  size_t      base_message_size() const { return (sizeof *this); }
  size_t      message_size() const { return (sizeof *this) + _v._value_len + 1; }
  const char* c_str() const { return cdata(); }

  void set_value(size_t buffer_size, const void* value, size_t len, offset_t offset_)
  {
    set_value(buffer_size, value, len);
    _offset = offset_;
  }

  void set_value(std::size_t value_) { _v._value = value_; }

  auto value_numeric() const { return _v._value; }
  std::size_t value() const { return _v._value; }
  offset_t Offset() const { return _offset; }

  // fields
  /* The type of the request (to which this is a response) determines the field */
private:
  union {
    size_t _value;
    size_t _value_len;
  } _v;
  offset_t   _offset;
  /* data immediately follows */
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// HANDSHAKE

struct Message_handshake : public Message {
  Message_handshake(uint64_t auth_id, uint64_t sequence
#if CW_TEST
		, std::uint64_t test_data_size_
#endif
  )
      : Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID),
        seq(sequence),
        protocol(PROTOCOL_V1),
        security_tls_auth(0)
#if CW_TEST
		, test_data_size(test_data_size_)
#endif
  {
  }

  static constexpr auto        id          = MSG_TYPE::HANDSHAKE;
  static constexpr const char* description = "Message_handshake";
  // fields
  uint64_t seq;
  uint8_t  protocol;
  bool security_tls_auth  : 1;
#if CW_TEST
  uint64_t test_data_size;
#endif
  /* add more fields for HMAC, encryption etc. */

  inline void set_as_protocol() { protocol = PROTOCOL_V2; }
  inline void set_tls_auth() {  security_tls_auth = true; }
  inline bool is_tls_auth() const { return security_tls_auth; }

} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// HANDSHAKE REPLY

enum {
  HANDSHAKE_REPLY_FLAG_CERT = 0x1,
};

struct Message_handshake_reply : public Message {
  static constexpr auto        id          = MSG_TYPE::HANDSHAKE_REPLY;
  static constexpr const char* description = "Message_handshake_reply";

 private:
  using data_t = uint8_t;
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }
  auto data() { return common::pointer_cast<data_t>(this + 1); }

 public:
  Message_handshake_reply(size_t         buffer_size,
                          uint64_t       auth_id,
                          uint64_t       sequence,
                          uint64_t       session_id_,
                          size_t         max_message_size_,
                          bool           start_tls_
#if CW_TEST
		, uint64_t vaddr_
		, uint64_t key_
#endif
	)
      : Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID),
        seq(sequence),
        session_id(session_id_),
        max_message_size(max_message_size_),
        start_tls(start_tls_)
#if CW_TEST
		, vaddr(vaddr_)
		, key(key_)
#endif
  {
    if (msg_len() > buffer_size)
      throw Logic_exception("%s::%s - insufficient buffer for Message_handshake_reply", +description, __func__);
  }

  // fields
  uint64_t seq;
  uint64_t session_id;
  size_t   max_message_size; /* RDMA max message size in bytes */
  bool     start_tls : 1;
#if CW_TEST
	uint64_t vaddr; /* perf test area */
	uint64_t key; /* perf test key */
#endif
  /* x509_cert innediately follows */
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// CLOSE SESSION

struct Message_close_session : public Message {
  static constexpr auto        id          = MSG_TYPE::CLOSE_SESSION;
  static constexpr const char* description = "Message_close_session";

  Message_close_session(uint64_t auth_id) : Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID), seq() {}

  // fields
  uint64_t seq;

} __attribute__((packed));

struct Message_stats : public Message {
  static constexpr auto        id          = MSG_TYPE::STATS;
  static constexpr const char* description = "Message_stats";

  Message_stats(uint64_t auth, const component::IMCAS::Shard_stats& shard_stats)
      : Message(auth, (sizeof *this), id, OP_TYPE::INVALID),
        stats(shard_stats)
  {
  }

  size_t message_size() const { return sizeof(Message_stats); }
  // fields
  /* TROUBLE: the stats are not packed */
  component::IMCAS::Shard_stats stats;
} __attribute__((packed));

// Debugging: client sends "none". server ignores it.
// Message_none

struct Message_none : public Message {
  static constexpr auto        id          = MSG_TYPE::NO_MSG;
  static constexpr const char* description = "Message_none";

  Message_none(uint64_t auth_id) : Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID) {}

  // fields
} __attribute__((packed));

// Debugging: client sends a ping; server should respond with a pong. No data.
// Message_ping

struct Message_ping : public Message {
  static constexpr auto        id          = MSG_TYPE::PING;
  static constexpr const char* description = "Message_ping";

  Message_ping(uint64_t auth_id) : Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID) {}

  // fields
} __attribute__((packed));

// Message_pong

struct Message_pong : public Message {
  static constexpr auto        id          = MSG_TYPE::PONG;
  static constexpr const char* description = "Message_pong";

  Message_pong(uint64_t auth_id) : Message(auth_id, (sizeof *this), id, OP_TYPE::INVALID) {}

  // fields
} __attribute__((packed));

////////////////////////////////////////////////////////////////////////
// ADO MESSAGES

struct Message_ado_request : public Message_numbered_request {
  static constexpr auto        id          = MSG_TYPE::ADO_REQUEST;
  static constexpr const char* description = "Message_ado_request";

 private:
  using data_t = uint8_t;
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }
  auto cdata() const { return common::pointer_cast<const char>(this + 1); }
  auto data() { return common::pointer_cast<data_t>(this + 1); }

 public:
  Message_ado_request(size_t             buffer_size,
                      uint64_t           auth_id,
                      uint64_t           request_id,
                      uint64_t           pool_id_,
                      const basic_string_view<byte> key,
                      const basic_string_view<byte> invocation_data,
                      uint32_t           flags_,
                      size_t             odvl = 4096)
      : Message_numbered_request(auth_id, (sizeof *this), id, OP_TYPE::INVALID, request_id, pool_id_),
        key_len(),
        _ondemand_val_len(odvl),
        invocation_data_len(),
        _flags(flags_)
  {
    this->invocation_data_len = boost::numeric_cast<decltype(this->invocation_data_len)>(invocation_data.size());
    key_len                   = key.size();
    increase_msg_len(key_len + invocation_data.size() + 1);

    if (buffer_size < msg_len())
      throw API_exception("%s::%s - insufficient buffer for Message_ado_request", +description, __func__);

    std::memcpy(data(), key.begin(), key.size());
    data()[key_len] = '\0';

    if (invocation_data_len > 0) std::memcpy(&data()[key_len + 1], invocation_data.begin(), invocation_data.size());
  }

  /* ca;er could use Message::msg_len */
  size_t         message_size() const { return msg_len(); }
  common::string_view key() const { return common::string_view(cdata(), key_len); }
  common::basic_string_view<uint8_t> request() const { return common::basic_string_view<uint8_t>(&data()[key_len + 1], request_len()); }
 private:
  size_t         request_len() const { return this->invocation_data_len; }
 public:
  bool           is_async() const { return _flags & component::IMCAS::ADO_FLAG_ASYNC; }
  size_t         get_key_len() const { return key_len; }
  uint32_t       flags() const { return _flags; }
  uint64_t       ondemand_val_len() const { return _ondemand_val_len; }
 private:
  // fields
  uint64_t key_len; /*< does not include null terminator */
  uint64_t _ondemand_val_len;
  uint32_t invocation_data_len; /*< does not include null terminator */
  uint32_t _flags;
} __attribute__((packed));

struct Message_put_ado_request : public Message_numbered_request {
  static constexpr auto        id          = MSG_TYPE::PUT_ADO_REQUEST;
  static constexpr const char* description = "Message_put_ado_request";

 private:
  using data_t = byte;
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }
  auto cdata() const { return common::pointer_cast<const char>(this + 1); }
  auto data() { return common::pointer_cast<data_t>(this + 1); }

 public:
  Message_put_ado_request(size_t             buffer_size,
                          uint64_t           auth_id,
                          uint64_t           request_id,
                          uint64_t           pool_id_,
                          const basic_string_view<byte> key,
                          const basic_string_view<byte> invocation_data,
                          const basic_string_view<byte> value,
                          size_t             root_len,
                          uint32_t           flags_)
      : Message_numbered_request(auth_id, (sizeof *this), id, OP_TYPE::INVALID, request_id, pool_id_),
        key_len(key.size()),
        invocation_data_len(),
        flags(flags_),
        _val_len(value.size()),
        val_addr(),
        root_val_len(root_len)
  {
    assert(invocation_data.begin());
    assert(invocation_data.size() > 0);
    assert(value.begin());
    assert(value.size() > 0);

    this->invocation_data_len = boost::numeric_cast<decltype(this->invocation_data_len)>(invocation_data.size());
    increase_msg_len(key_len + 1 + invocation_data.size() + _val_len);

    if (buffer_size < message_size())
      throw API_exception("%s::%s - insufficient buffer for Message_ado_request", +description, __func__);

    std::memcpy(data(), key.begin(), key.size());
    data()[key_len] = byte('\0'); /* ADO keys, unlike IO keys, are sent with a null terminator */
    byte* ptr       = static_cast<byte*>(&data()[key_len + 1]);

    /* copy in invocation data */
    std::memcpy(ptr, invocation_data.begin(), invocation_data.size());

    /* copy in value_len */
    std::memcpy(ptr + invocation_data.size(), value.begin(), value.size());

    val_addr = reinterpret_cast<uint64_t>(value.begin());
  }
  /* caller could use Message::msg_len */
  std::size_t message_size() const { return msg_len(); }
  common::string_view key() const { return common::string_view(cdata(), key_len); }
  const void* request() const { return static_cast<const void*>(&data()[key_len + 1]); }
  const void* value() const
  {
    return static_cast<const byte*>(request()) + invocation_data_len;
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
  auto data() const { return common::pointer_cast<const data_t>(this + 1); }
  auto data() { return common::pointer_cast<data_t>(this + 1); }

 public:
  static constexpr auto        id          = MSG_TYPE::ADO_RESPONSE;
  static constexpr const char* description = "Message_ado_response";

  Message_ado_response(size_t buffer_size, status_t status, uint64_t auth_id, uint64_t request_id)
      : Message_numbered_response(auth_id, (sizeof *this), id, OP_TYPE::INVALID, request_id),
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
        common::pointer_cast<uint32_t>(static_cast<void*>(&data()[response_len])); /* next position in data field */
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

static_assert(sizeof(Message_IO_request) % 64 == 0, "Message_IO_request size should be a cache line (64-byte) multiple to align following data");
static_assert(sizeof(Message_IO_response) % 64 == 0, "Message_IO_response size should be a cache line (64-byte) multiple to align following data");

}  // namespace protocol
namespace Protocol = protocol;
}  // namespace mcas

#pragma GCC diagnostic pop
#endif
