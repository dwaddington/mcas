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

#include "ado_ipc_proto.h" // hard-wired custom protocol
#include "ado_proto.h"
#include <common/logging.h>
#include <common/exceptions.h>
#include <common/dump_utils.h>
#include <common/time.h>
#include <boost/numeric/conversion/cast.hpp>
#include <cstring>
#include <cstddef>
#include <cstdlib>
#include <cinttypes> // PPIu32
#include <string>
#include <stdexcept> /* runtime_error */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

using namespace component;
using namespace mcas::ipc;

class Buffer_header
{
  /* size of the space returned by allocate_buffer()
   * not currently used, but would be handy if allocate_buffer
   * ever changed to return more than one size of buffer.
   */
  std::uint32_t _size;
  std::uint32_t _offset; // offset to the region used by flatbuffers
  const uint8_t *buf_begin() const { return static_cast<const uint8_t *>(static_cast<const void *>(this)); }
  uint8_t *buf_begin() { return static_cast<uint8_t *>(static_cast<void *>(this)); }
public:
  Buffer_header(std::uint32_t size_)
    : _size(size_)
    , _offset(0)
  {
  }
  const uint8_t * to_message() const
  {
    assert(_offset != 0);
    return buf_begin() + _offset;
  }
  uint8_t * to_message()
  {
    assert(_offset != 0);
    return buf_begin() + _offset;
  }
  const uint8_t *end() const
  {
    return buf_begin() + _size;
  }
  uint8_t *end()
  {
    return buf_begin() + _size;
  }
};

const uint8_t * ADO_protocol_builder::buffer_header_to_message(Buffer_header *bh)
{
  return bh->to_message();
}

ADO_protocol_builder::ADO_protocol_builder(unsigned option_DEBUG_, const std::string& channel_prefix,
                                           Role role)
  : option_DEBUG(option_DEBUG_)
  , _channel_prefix(channel_prefix)
  , _channel()
  , _channel_callback()
  , _b_mutex()
  , _buffer()
{
  /* connect UIPC channels */
  if(role == Role::CONNECT) {
  }
  else if(role == Role::ACCEPT) {
    _channel.open(_channel_prefix);

    std::lock_guard<std::mutex> g{_b_mutex};
    for ( auto i = 0; i != ADO_protocol_builder::QUEUE_SIZE; ++i ) {
      _buffer.emplace_back(buffer_allocate());
    }
    _channel_callback.open(_channel_prefix + "-cb");
  }
  else throw Logic_exception("bad role");
}

ADO_protocol_builder::~ADO_protocol_builder()
{
}

void ADO_protocol_builder::create_uipc_channels()
{
  _channel.create(_channel_prefix, MAX_MESSAGE_SIZE, QUEUE_SIZE);

  std::lock_guard<std::mutex> g{_b_mutex};
  for ( auto i = 0; i != ADO_protocol_builder::QUEUE_SIZE; ++i ) {
    _buffer.emplace_back(buffer_allocate());
  }
  _channel_callback.create(_channel_prefix + "-cb", MAX_MESSAGE_SIZE, QUEUE_SIZE);
}

void ADO_protocol_builder::send_bootstrap(const uint64_t auth_id,
                                          const std::string& pool_name,
                                          const size_t pool_size,
                                          const unsigned int pool_flags,
                                          const unsigned int memory_type,
                                          const uint64_t expected_obj_count,
                                          const bool open_existing)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Bootstrap_request(MAX_MESSAGE_SIZE,
                                            auth_id,
                                            pool_name,
                                            pool_size,
                                            pool_flags,
                                            memory_type,
                                            expected_obj_count,
                                            open_existing);
  send(buffer);
  PLOG("%s", "ADO_protocol_builder: sent bootstrap");
}

void ADO_protocol_builder::send_bootstrap_response()
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Chirp(chirp_t::HELLO);
  send(buffer);
}

/// --- op event

bool ADO_protocol_builder::recv_op_event_response(const Buffer_header * buffer,
                                                  ADO_op& op)
{
  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::OP_EVENT_RESPONSE) {
    auto msg = reinterpret_cast<const Op_event_response*>(buffer);
    op = msg->op;
    return true;
  }
  return false;
}

void ADO_protocol_builder::send_op_event_response(component::ADO_op event)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Op_event_response(event);
  /* it OK to send the response back on the callback queue.
     currently only work request completions are passed on the
     main queue for semantic reasons but we may be
     able to unify
  */
  send_callback(buffer);
}

void ADO_protocol_builder::send_op_event(component::ADO_op op)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Op_event(op);
  send(buffer);
}

void ADO_protocol_builder::send_cluster_event(const std::string& sender,
					      const std::string& type,
					      const std::string& content)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Cluster_event(MAX_MESSAGE_SIZE,
					sender,
					type,
					content);
  send(buffer);
}



void ADO_protocol_builder::send_shutdown()
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Chirp(chirp_t::SHUTDOWN);
  send(buffer);
}

void ADO_protocol_builder::send_shutdown_to_shard()
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Chirp(chirp_t::SHUTDOWN);
  send_callback(buffer);
}


void ADO_protocol_builder::send_memory_map(uint64_t memory_token,
                                           size_t memory_size,
                                           void * shard_address)
{
  auto buffer = get_buffer().release();

  new (buffer) Map_memory(MAX_MESSAGE_SIZE,
                          memory_token,
                          memory_size,
                          shard_address);

  send(buffer);
  PLOG("%s", "ADO_protocol_builder::send_memory_map OK");
}

void ADO_protocol_builder::send_memory_map_named(unsigned region_id,
  string_view pool_name,
  std::size_t offset,
  ::iovec iov)
{
  auto buffer = get_buffer().release();

  new (buffer) Map_memory_named(MAX_MESSAGE_SIZE,
                          region_id,
                          pool_name,
                          offset,
                          iov);

  send(buffer);
  PLOG("ADO_protocol_builder::%s OK", __func__);
}

bool ADO_protocol_builder::recv_index_op_request(const Buffer_header * buffer,
                                                 std::string& key_expression,
                                                 offset_t& begin_pos,
                                                 int& find_type)
{
  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::INDEX_REQUEST) {
    auto * wr = reinterpret_cast<const Index_request*>(buffer);
    key_expression.assign(wr->expr, wr->expr_len);
    begin_pos = wr->begin_pos;
    find_type = wr->find_type;
    return true;
  }

  return false;
}

// --- find index

void ADO_protocol_builder::send_find_index_request(const std::string& key_expression,
                                                   offset_t begin_position,
                                                   component::IKVIndex::find_t find_type)
{
  auto buffer = get_buffer().release();
  new (buffer) Index_request(MAX_MESSAGE_SIZE,
                             begin_position,
                             find_type,
                             key_expression);
  send_callback(buffer);
}

void ADO_protocol_builder::send_find_index_response(const status_t status,
                                                    const offset_t matched_position,
                                                    const std::string& matched_key)
{
  auto buffer = get_buffer().release();
  new (buffer) Index_response(MAX_MESSAGE_SIZE,
                              matched_position,
                              status,
                              matched_key);
  send_callback(buffer);
}

void ADO_protocol_builder::recv_find_index_response(status_t& status,
                                                    offset_t& out_matched_position,
                                                    std::string& out_matched_key)
{
  Buffer_header * buffer;
  auto st = poll_recv_callback(buffer);
  if ( st != S_OK )
    throw std::runtime_error("Bad response from recv_find_index_response");

  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::INDEX_RESPONSE) {
    auto * wr = reinterpret_cast<Index_response*>(buffer);
    status = wr->status;
    out_matched_position = wr->matched_pos;
    out_matched_key.assign(wr->matched_key, wr->matched_key_len);
  }
  else throw Logic_exception("recv_find_index_response got something else");

  free_ipc_buffer(buffer);
}


void ADO_protocol_builder::send_work_request(const uint64_t work_request_key,
                                             const char * key,
                                             const size_t key_len,
                                             const void * value,
                                             const size_t value_len,
                                             const void * detached_value,
                                             const size_t detached_value_len,
                                             const void * invocation_data,
                                             const size_t invocation_data_len,
                                             const bool new_root)
{
  auto buffer = get_buffer().release();
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__,__LINE__);

  assert(detached_value ? detached_value_len > 0 : true);

  if( 1 < option_DEBUG) {
    PMAJOR("SENDING Work_request: key=(%p=%.*s) value=%p value_len=%lu"
           " invocation_len=%lu detached_value=(%p,%lu) (%.*s) new=%d",
           static_cast<const void *>(key),
           int(key_len),
           key,
           value,
           value_len,
           invocation_data_len,
           detached_value,
           detached_value_len,
           boost::numeric_cast<int>(detached_value_len),
           reinterpret_cast<const char *>(detached_value),
           new_root);
  }

  new (buffer) Work_request(MAX_MESSAGE_SIZE,
                            work_request_key,
                            key,
                            key_len,
                            reinterpret_cast<uint64_t>(value),
                            value_len,
                            reinterpret_cast<uint64_t>(detached_value),
                            detached_value_len,
                            invocation_data,
                            invocation_data_len,
                            new_root);

  send(buffer);
}

void ADO_protocol_builder::send_work_response(status_t status,
                                              uint64_t work_key,
                                              const IADO_plugin::response_buffer_vector_t& response_buffers)
{
  auto buffer = get_buffer().release();
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__, __LINE__);

  new (buffer) Work_response(MAX_MESSAGE_SIZE,
                             work_key,
                             status,
                             response_buffers);

  send(buffer);
}

bool ADO_protocol_builder::
recv_from_ado_work_completion(uint64_t& work_key,
                              status_t& status,
                              component::IADO_plugin::response_buffer_vector_t& response_buffers)
{
  Buffer_header * buffer = nullptr;
  status_t s = recv(buffer);

  if(s == E_EMPTY) {
    return false;
  }

  /*---------------------------------------*/
  /* custom IPC message protocol           */
  /*---------------------------------------*/
  using namespace mcas::ipc;

  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::WORK_RESPONSE) {

    auto * wr = reinterpret_cast<Work_response*>(buffer);

    /* Change in behavior: old clear() would simple forget ptrs to malloc memory, new clear() will free them. */
    response_buffers.clear();
    work_key = wr->work_key;
    status = wr->status;
    wr->copy_responses(response_buffers);
  }
  else throw Logic_exception("invalid IPC message");

  free_ipc_buffer(buffer);
  return true;
}

status_t ADO_protocol_builder::recv_bootstrap_response()
{
  Buffer_header * buffer;

  auto st = poll_recv(buffer);

  /* TODO: check bootstrap response */
  if ( st == S_OK )
    free_ipc_buffer(buffer);

  return st;
}

void ADO_protocol_builder::send_table_op_create(const uint64_t work_request_id,
                                                const std::string& keystr,
                                                const size_t value_len,
                                                const std::uint64_t flags)
{
  auto buffer = get_buffer().release();
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__, __LINE__);

  new (buffer) Table_request(MAX_MESSAGE_SIZE,
                             work_request_id,
                             keystr,
                             value_len,
                             0, /* addr */
                             flags, /* align or flags */
                             component::ADO_op::CREATE);
  send_callback(buffer);
}

void ADO_protocol_builder::send_table_op_open(const uint64_t work_request_id,
                                              const std::string& keystr,
                                              const size_t value_len,
                                              const std::uint64_t flags)
{
  auto buffer = get_buffer().release();
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__, __LINE__);

  new (buffer) Table_request(MAX_MESSAGE_SIZE,
                             work_request_id,
                             keystr,
                             value_len,
                             0,
                             flags,
                             component::ADO_op::OPEN);
  send_callback(buffer);
}

void ADO_protocol_builder::send_table_op_resize(const uint64_t work_request_id,
                                                const std::string& keystr,
                                                const size_t new_value_len)
{
  auto buffer = get_buffer().release();
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__, __LINE__);

  new (buffer) Table_request(MAX_MESSAGE_SIZE,
                             work_request_id,
                             keystr,
                             new_value_len,
                             0,
                             0,
                             component::ADO_op::VALUE_RESIZE);
  send_callback(buffer);
}

void ADO_protocol_builder::send_table_op_erase(const std::string& keystr)
{
  auto buffer = get_buffer().release();
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__, __LINE__);

  new (buffer) Table_request(MAX_MESSAGE_SIZE,
                             0,
                             keystr,
                             0,
                             0,
                             0,
                             component::ADO_op::ERASE);
  send_callback(buffer);
}

void ADO_protocol_builder::send_table_op_allocate_pool_memory(const size_t size,
                                                              const size_t alignment)
{
  auto buffer = get_buffer().release();
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__, __LINE__);

  new (buffer) Table_request(MAX_MESSAGE_SIZE,
                             0,
                             size,
                             0,
                             alignment,
                             component::ADO_op::ALLOCATE_POOL_MEMORY);
  send_callback(buffer);
}

void ADO_protocol_builder::send_table_op_free_pool_memory(const void * ptr,
                                                          const size_t size)
{
  auto buffer = get_buffer().release();
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__, __LINE__);

  new (buffer) Table_request(MAX_MESSAGE_SIZE,
                             0,
                             size,
                             reinterpret_cast<uint64_t>(ptr),
                             0,
                             component::ADO_op::FREE_POOL_MEMORY);
  send_callback(buffer);
}



/// --- pool info
void ADO_protocol_builder::send_pool_info_response(const status_t status,
                                                   const std::string& info)
{
  auto buffer = get_buffer().release();
  new (buffer) Pool_info_response(MAX_MESSAGE_SIZE, status, info);
  send_callback(buffer);
}

bool ADO_protocol_builder::recv_pool_info_request(const Buffer_header * buffer)
{
  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::CHIRP) {
    auto * chirp = reinterpret_cast<const Chirp*>(buffer);
    return chirp->type == chirp_t::POOL_INFO_REQUEST;
  }
  return false;
}


void ADO_protocol_builder::send_pool_info_request()
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Chirp(chirp_t::POOL_INFO_REQUEST);
  send_callback(buffer);
}

void ADO_protocol_builder::recv_pool_info_response(status_t& status, std::string& out_response)
{
  Buffer_header * buffer;
  auto st = poll_recv_callback(buffer);
  if ( st != S_OK )
    throw std::runtime_error("bad response from recv_pool_info_response");

  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::POOL_INFO_RESPONSE) {
    auto * wr = reinterpret_cast<Pool_info_response*>(buffer);
    status = wr->status;
    out_response = wr->result;
  }
  else throw Logic_exception("recv_get_reference_vector_response got something else");

  free_ipc_buffer(buffer);
}


void ADO_protocol_builder::send_table_op_response(const status_t status,
                                                  const void * value_addr,
                                                  size_t value_len,
                                                  const char * key_addr,
                                                  component::IKVStore::key_t key_handle)
{
  auto buffer = get_buffer().release(); //::uipc_alloc_message(_channel);
  if(!buffer) throw General_exception("%s:%u out of buffers", __FILE__, __LINE__);

  new (buffer) Table_response(MAX_MESSAGE_SIZE,
                              status,
                              reinterpret_cast<uint64_t>(value_addr),
                              value_len,
                              reinterpret_cast<uint64_t>(key_addr),
                              reinterpret_cast<uint64_t>(key_handle));

  send_callback(buffer);
}

void ADO_protocol_builder::recv_table_op_response(status_t& status,
                                                  void *& value_addr,
                                                  size_t * p_out_value_len,
                                                  const char ** out_key_ptr,
                                                  component::IKVStore::key_t * out_key_handle)
{
  Buffer_header * buffer;
  auto st = poll_recv_callback(buffer);
  if ( st != S_OK )
    throw std::runtime_error("bad response from recv_table_op_response");

  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::TABLE_OP_RESPONSE) {
    auto * wr = reinterpret_cast<Table_response*>(buffer);


    status = wr->status;
    value_addr = reinterpret_cast<void*>(wr->value_addr);

    if(p_out_value_len)
      *p_out_value_len = wr->value_len;

    if(out_key_ptr)
      *out_key_ptr = reinterpret_cast<const char *>(wr->key_addr);

    if(out_key_handle)
      *out_key_handle = reinterpret_cast<component::IKVStore::key_t>(wr->key_handle);

    free_ipc_buffer(buffer);
  }
  else throw Logic_exception("recv_table_op_response got something else");
}




bool ADO_protocol_builder::recv_table_op_request(const Buffer_header * buffer,
                                                 uint64_t& work_request_id,
                                                 component::ADO_op& op,
                                                 std::string& key,
                                                 size_t& value_len,
                                                 size_t& align_or_flags,
                                                 void* & addr)
{
  if(mcas::ipc::Message::is_valid(buffer)) {
    switch(mcas::ipc::Message::type(buffer))
      {
      case(mcas::ipc::MSG_TYPE::TABLE_OP_REQUEST): {
        auto * msg = reinterpret_cast<const mcas::ipc::Table_request*>(buffer);
        op = msg->op;
        work_request_id = msg->work_key;
        key.assign(msg->key, msg->key_len);
        value_len = msg->value_len;
        align_or_flags = msg->align_or_flags;
        addr = reinterpret_cast<void*>(msg->addr);
        return true;
      }
      default: {
        return false;
      }
      }
  }

  return false;
}



/// --- vector

void ADO_protocol_builder::send_vector_request(const common::epoch_time_t t_begin,
                                               const common::epoch_time_t t_end)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Vector_request(t_begin, t_end);
  send_callback(buffer);
}

void ADO_protocol_builder::send_vector_response(const status_t status,
                                                const IADO_plugin::Reference_vector& rv)
{
  auto buffer = get_buffer().release();
  new (buffer) Vector_response(status, rv);
  send_callback(buffer);
}

void ADO_protocol_builder::recv_vector_response(status_t& status,
                                                IADO_plugin::Reference_vector& out_vector)
{
  Buffer_header * buffer;
  auto st = poll_recv_callback(buffer);
  if ( st != S_OK )
    throw std::runtime_error("bad response from recv_reference_vector_response");

  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::VECTOR_RESPONSE) {
    auto * wr = reinterpret_cast<Vector_response*>(buffer);
    status = wr->status;
    out_vector = wr->refs;
  }
  else throw Logic_exception("recv_get_reference_vector_response got something else");

  free_ipc_buffer(buffer);
}

bool ADO_protocol_builder::recv_vector_request(const Buffer_header * buffer,
                                               common::epoch_time_t& t_begin,
                                               common::epoch_time_t& t_end)
{
  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::VECTOR_REQUEST) {
    auto * req = reinterpret_cast<const Vector_request*>(buffer);
    t_begin = req->t_begin;
    t_end = req->t_end;
    return true;
  }
  return false;
}

/// --- vector

void ADO_protocol_builder::send_iterate_request(const common::epoch_time_t t_begin,
                                                const common::epoch_time_t t_end,
                                                component::IKVStore::pool_iterator_t iterator)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Iterate_request(t_begin,
                                          t_end,
                                          iterator);

  send_callback(buffer);
}

void ADO_protocol_builder::recv_iterate_response(status_t& status,
                                                 component::IKVStore::pool_iterator_t& iterator,
                                                 component::IKVStore::pool_reference_t& reference)
{
  Buffer_header * buffer;
  auto st = poll_recv_callback(buffer);
  if ( st != S_OK )
    throw std::runtime_error("bad response from recv_iterate_response");

  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::ITERATE_RESPONSE) {
    auto * wr = reinterpret_cast<Iterate_response*>(buffer);
    status = wr->status;
    iterator = wr->iterator;
    reference = wr->reference;
  }
  else throw Logic_exception("recv_iterate_response got something else");

  free_ipc_buffer(buffer);
}

bool ADO_protocol_builder::recv_iterate_request(const Buffer_header * buffer,
                                                common::epoch_time_t& t_begin,
                                                common::epoch_time_t& t_end,
                                                component::IKVStore::pool_iterator_t& iterator)
{
  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::ITERATE_REQUEST) {
    auto * req = reinterpret_cast<const Iterate_request*>(buffer);
    t_begin = req->t_begin;
    t_end = req->t_end;
    iterator = req->iterator;
    return true;
  }
  iterator = nullptr;
  return false;
}

void ADO_protocol_builder::send_iterate_response(const status_t rc,
                                                 component::IKVStore::pool_iterator_t iterator,
                                                 component::IKVStore::pool_reference_t reference)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Iterate_response(rc, iterator, reference);
  send_callback(buffer);
}


/// --unlock
void ADO_protocol_builder::send_unlock_request(const uint64_t work_id,
                                               const component::IKVStore::key_t key_handle)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Unlock_request(work_id, key_handle);

  send_callback(buffer);
}

bool ADO_protocol_builder::recv_unlock_response(status_t& status)
{
  Buffer_header * buffer;
  auto st = poll_recv_callback(buffer);
  if ( st != S_OK )
    throw std::runtime_error("bad response from recv_unlock_response");

  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::CHIRP) {
    auto * chirp = reinterpret_cast<const Chirp*>(buffer);
    status = chirp->status;
    return chirp->type == chirp_t::UNLOCK_RESPONSE;
  }
  return false;
}

bool ADO_protocol_builder::recv_unlock_request(const Buffer_header * buffer,
                                               uint64_t& work_id,
                                               component::IKVStore::key_t& key_handle)
{
  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::UNLOCK_REQUEST) {
    auto * req = reinterpret_cast<const Unlock_request*>(buffer);
    work_id = req->work_id;
    key_handle = req->key_handle;
    assert(work_id);
    assert(key_handle);
    return true;
  }
  return false;
}

void ADO_protocol_builder::send_unlock_response(const status_t status)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Chirp(chirp_t::UNLOCK_RESPONSE, status);
  send_callback(buffer);
}


void ADO_protocol_builder::send_configure_request(const uint64_t options)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Configure_request(options);

  send_callback(buffer);
}

bool ADO_protocol_builder::recv_configure_request(const Buffer_header * buffer,
                                                  uint64_t& options)
{
  if(mcas::ipc::Message::is_valid(buffer) &&
       mcas::ipc::Message::type(buffer) == MSG_TYPE::CONFIGURE_REQUEST) {
      auto * req = reinterpret_cast<const Configure_request*>(buffer);
      options = req->options;
      return true;
    }
    return false;
}

void ADO_protocol_builder::send_configure_response(const status_t status)
{
  auto buffer = get_buffer().release();
  new (buffer) mcas::ipc::Chirp(chirp_t::CONFIGURE_RESPONSE, status);
  send_callback(buffer);
}

bool ADO_protocol_builder::recv_configure_response(status_t& status)
{
  Buffer_header * buffer;
  auto st = poll_recv_callback(buffer);
  if ( st != S_OK )
    throw std::runtime_error("bad response from recv_configure_response");

  if(mcas::ipc::Message::is_valid(buffer) &&
     mcas::ipc::Message::type(buffer) == MSG_TYPE::CHIRP) {
    auto * chirp = reinterpret_cast<const Chirp*>(buffer);
    status = chirp->status;
    return chirp->type == chirp_t::CONFIGURE_RESPONSE;
  }
  return false;

}
