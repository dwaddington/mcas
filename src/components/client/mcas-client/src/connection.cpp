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

#include "connection.h"

#include <city.h>
#include <common/cycles.h>
#include <common/utils.h>
#include <unistd.h>

#include <memory>

#include "protocol.h"

//#define DEBUG_NPC_RESPONSES

using namespace Component;

namespace
{
/*
 * First, cast the response buffer to Message (checking version).
 * Second, cast the Message to a specific message type (checking message type).
 */
template <typename Type>
const Type *response_ptr(void *b)
{
  const auto *const msg = mcas::Protocol::message_cast(b);
  return msg->ptr_cast<Type>();
}
}  // namespace

namespace mcas
{
namespace Client
{
struct buffer_pair_t {
  buffer_pair_t(Client::Fabric_transport::buffer_t *_iobs, Client::Fabric_transport::buffer_t *_iobr)
      : iobs(_iobs), iobr(_iobr)
  {
  }
  Client::Fabric_transport::buffer_t *iobs;
  Client::Fabric_transport::buffer_t *iobr;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++" // missing initializers
Connection_handler::Connection_handler(Connection_base::Transport *connection)
  : Connection_base(connection),
  _max_inject_size(connection->max_inject_size())
{
  char *env = getenv("SHORT_CIRCUIT_BACKEND");
  if (env && env[0] == '1') {
    _options.short_circuit_backend = true;
  }
}
#pragma GCC diagnostic pop

Connection_handler::~Connection_handler() { PLOG("Connection_handler::dtor (%p)", static_cast<const void *>(this)); }

/* The various embedded returns and throws suggest that the allocated
 * iobs should be automatically freed to avoid leaks.
 */

class iob_free {
  Connection_handler *_h;

 public:
  iob_free(Connection_handler *h_) : _h(h_) {}
  void operator()(Connection_handler::buffer_t *iob) { _h->free_buffer(iob); }
};

Connection_handler::pool_t Connection_handler::open_pool(const std::string name, const unsigned int flags)
{
  API_LOCK();

  PMAJOR("open pool: %s", name.c_str());

  /* send pool request message */

  /* Use unique_ptr to ensure that the dynamically buffers are freed.
   * unique_ptr protects against the code forgetting to call free_buffer,
   * which it usually did when the function exited by a throw or a
   * non-terminal return.
   *
   * The type std::unique_ptr<buffer_t, iob_free> is a bit ugly, and
   * could be simplified by a typedef, e.g.
   *   using buffer_ptr_t = std::unique_ptr<buffer_t, iob_free>
   * but it can stay as it is for now to reduce the levels of indirection
   * necessary to understand what it does.
   */
  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  IKVStore::pool_t pool_id;

  assert(&*iobr != &*iobs);

  // temp
  memset(iobr->base(), 0xbb, iobr->length());

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_pool_request(iobs->length(), auth_id(), /* auth id */
                                                                             ++_request_id, 0,          /* size */
                                                                             0, /* expected obj count */
                                                                             mcas::Protocol::OP_OPEN, name);

    iobs->set_length(msg->msg_len);

    /* The &* notation extracts a raw pointer form the "unique_ptr".
     * The difference is that the standard pointer does not imply
     * ownership; the function is simply "borrowing" the pointer.
     * Here, &*iobr is equivalent to iobr->get(). The choice is a
     * matter of style. &* uses two fewer tokens.
     */
    /* the sequence "post_recv, sync_inject_send, wait_for_completion"
     * is common enough that it probably deserves its own function.
     * But we may find that the entire single-exchange pattern as seen
     * in open_pool, create_pool and several others could be placed in
     * a single template function.
     */

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr); /* await response */

    const auto response_msg = response_ptr<const mcas::Protocol::Message_pool_response>(iobr->base());

    pool_id = response_msg->pool_id;
  }
  catch (...) {
    pool_id = IKVStore::POOL_ERROR;
  }
  return pool_id;
}

Connection_handler::pool_t Connection_handler::create_pool(const std::string  name,
                                                           const size_t       size,
                                                           const unsigned int flags,
                                                           const uint64_t     expected_obj_count)
{
  API_LOCK();

  /* send pool request message */
  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  IKVStore::pool_t pool_id;

  try {
    const auto msg = new (iobs->base())
        Protocol::Message_pool_request(iobs->length(), auth_id(), /* auth id */
                                       ++_request_id, size, expected_obj_count, mcas::Protocol::OP_CREATE, name);
    assert(msg->op);
    msg->flags = flags;

    iobs->set_length(msg->msg_len);

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr);

    const auto response_msg = response_ptr<const mcas::Protocol::Message_pool_response>(iobr->base());

    pool_id = response_msg->pool_id;
  }
  catch (...) {
    pool_id = IKVStore::POOL_ERROR;
  }

  /* Note: most request/response pairs return status. This one returns a pool_id
   * instead. */
  return pool_id;
}

status_t Connection_handler::close_pool(const pool_t pool)
{
  API_LOCK();
  /* send pool request message */
  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto msg  = new (iobs->base())
      mcas::Protocol::Message_pool_request(iobs->length(), auth_id(), ++_request_id, mcas::Protocol::OP_CLOSE);
  msg->pool_id = pool;

  iobs->set_length(msg->msg_len);

  post_recv(&*iobr);
  sync_inject_send(&*iobs);
  try {
    wait_for_completion(&*iobr);
  }
  catch (...) {
    return E_FAIL;
  }

  const auto response_msg = response_ptr<const mcas::Protocol::Message_pool_response>(iobr->base());

  const auto status = response_msg->get_status();
  return status;
}

status_t Connection_handler::delete_pool(const std::string &name)

{
  if (name.empty()) return E_INVAL;

  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);

  const auto msg = new (iobs->base()) mcas::Protocol::Message_pool_request(iobs->length(), auth_id(), ++_request_id,
                                                                           0,  // size
                                                                           0,  // exp obj count
                                                                           mcas::Protocol::OP_DELETE, name);
  iobs->set_length(msg->msg_len);

  post_recv(&*iobr);
  sync_inject_send(&*iobs);
  try {
    wait_for_completion(&*iobr);
  }
  catch (...) {
    return E_FAIL;
  }

  const auto response_msg = response_ptr<const mcas::Protocol::Message_pool_response>(iobr->base());

  return response_msg->get_status();
}

status_t Connection_handler::delete_pool(const IMCAS::pool_t pool)
{
  if (!pool) return E_INVAL;

  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);

  const auto msg = new (iobs->base())
      mcas::Protocol::Message_pool_request(iobs->length(), auth_id(), ++_request_id, mcas::Protocol::OP_DELETE);
  msg->pool_id = pool;
  iobs->set_length(msg->msg_len);

  post_recv(&*iobr);
  sync_inject_send(&*iobs);
  try {
    wait_for_completion(&*iobr);
  }
  catch (...) {
    return E_FAIL;
  }

  const auto response_msg = response_ptr<const mcas::Protocol::Message_pool_response>(iobr->base());

  return response_msg->get_status();
}

status_t Connection_handler::configure_pool(const IMCAS::pool_t pool, const std::string &json)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);

  const auto msg = new (iobs->base()) mcas::Protocol::Message_IO_request(iobs->length(), auth_id(), ++_request_id, pool,
                                                                         mcas::Protocol::OP_CONFIGURE,  // op
                                                                         json);
  if ((json.length() + sizeof(mcas::Protocol::Message_IO_request)) > Buffer_manager<IFabric_client>::BUFFER_LEN)
    return IKVStore::E_TOO_LARGE;

  iobs->set_length(msg->msg_len);

  post_recv(&*iobr);
  sync_inject_send(&*iobs);
  try {
    wait_for_completion(&*iobr);
  }
  catch (...) {
    return E_FAIL;
  }

  const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(iobr->base());

  if (option_DEBUG)
    PLOG("got response from CONFIGURE operation: status=%d request_id=%lu", response_msg->get_status(),
         response_msg->request_id);

  return response_msg->get_status();
}

/**
 * Memcpy version; both key and value are copied
 *
 */
status_t Connection_handler::put(const pool_t       pool,
                                 const std::string  key,
                                 const void *       value,
                                 const size_t       value_len,
                                 const unsigned int flags)
{
  if (value == nullptr || value_len == 0) return E_INVAL;
  return put(pool, key.c_str(), key.length(), value, value_len, flags);
}

status_t Connection_handler::put(const pool_t       pool,
                                 const void *       key,
                                 const size_t       key_len,
                                 const void *       value,
                                 const size_t       value_len,
                                 const unsigned int flags)
{
  API_LOCK();

  if (option_DEBUG)
    PINF("put: %.*s (key_len=%lu) (value_len=%lu)", int(key_len), static_cast<const char *>(key), key_len, value_len);

  /* check key length */
  if ((key_len + value_len + sizeof(mcas::Protocol::Message_IO_request)) > Buffer_manager<IFabric_client>::BUFFER_LEN) {
    PWRN("mcas_client::put value length (%lu) too long. Use put_direct.", value_len);
    return IKVStore::E_TOO_LARGE;
  }

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);

  status_t status;

  try {
    const auto msg =
        new (iobs->base()) mcas::Protocol::Message_IO_request(iobs->length(), auth_id(), ++_request_id, pool,
                                                              mcas::Protocol::OP_PUT,  // op
                                                              key, key_len, value, value_len, flags);

    if (_options.short_circuit_backend) msg->resvd |= mcas::Protocol::MSG_RESVD_SCBE;

    iobs->set_length(msg->msg_len);

    post_recv(&*iobr);
    sync_send(&*iobs); /* this will clean up iobs */
    wait_for_completion(&*iobr);

    const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(iobr->base());

    if (option_DEBUG)
      PLOG("got response from PUT operation: status=%d request_id=%lu", response_msg->get_status(),
           response_msg->request_id);

    status = response_msg->get_status();
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

status_t Connection_handler::two_stage_put_direct(const pool_t                 pool,
                                                  const void *                 key,
                                                  const size_t                 key_len,
                                                  const void *                 value,
                                                  const size_t                 value_len,
                                                  const IMCAS::memory_handle_t handle,
                                                  const unsigned int           flags)
{
  using namespace mcas;

  assert(pool);

  assert(value_len <= _max_message_size);
  assert(value_len > 0);

  {
    const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
    const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);

    /* send advance leader message */
    const auto request_id = ++_request_id;
    const auto msg        = new (iobs->base()) Protocol::Message_IO_request(iobs->length(), auth_id(), request_id, pool,
                                                                     Protocol::OP_PUT_ADVANCE,  // op
                                                                     key, key_len, value_len, flags);
    msg->flags            = flags;
    iobs->set_length(msg->msg_len);

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    try {
      wait_for_completion(&*iobr);
    }
    catch (...) {
      return E_FAIL;
    }

    const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(iobr->base());

    /* wait for response from header before posting the value */

    if (option_DEBUG) PMAJOR("got response (status=%u) from put direct header", response_msg->get_status());

    /* if response is not OK, don't follow with the value */
    if (response_msg->get_status() != S_OK) {
      return msg->get_status();
    }
  }

  /* send value */
  buffer_t *value_buffer = reinterpret_cast<buffer_t *>(handle);
  value_buffer->set_length(value_len);
  assert(value_buffer->check_magic());

  if (option_DEBUG)
    PLOG("value_buffer: (iov_len=%lu, region=%p, desc=%p)", value_buffer->iov->iov_len,
         static_cast<const void *>(value_buffer->region), value_buffer->desc);

  sync_send(value_buffer);  // client owns buffer

  if (option_DEBUG) {
    PINF("two_stage_put_direct: complete");
  }

  return S_OK;
}

status_t Connection_handler::put_direct(const pool_t                 pool,
                                        const std::string &          key,
                                        const void *                 value,
                                        const size_t                 value_len,
                                        const IMCAS::memory_handle_t handle,
                                        const unsigned int           flags)
{
  API_LOCK();

  if (handle == IKVStore::HANDLE_NONE) {
    PWRN("put_direct: memory handle should be provided");
    return E_BAD_PARAM;
  }

  assert(_max_message_size);

  if (pool == 0) {
    PWRN("put_direct: invalid pool identifier");
    return E_INVAL;
  }

  buffer_t *value_buffer = reinterpret_cast<buffer_t *>(handle);
  value_buffer->set_length(value_len);

  if (!value_buffer->check_magic()) {
    PWRN("put_direct: memory handle is invalid");
    return E_INVAL;
  }

  status_t status;

  try {
    const auto key_len = key.length();
    if ((key_len + value_len + sizeof(mcas::Protocol::Message_IO_request)) >
        Buffer_manager<IFabric_client>::BUFFER_LEN) {
      /* check value is not too large for underlying transport */
      if (value_len > _max_message_size) {
        PWRN("put_direct: message size too large");
        return IKVStore::E_TOO_LARGE;
      }

      /* for large puts, where the receiver will not have
       * sufficient buffer space, we use a two-stage protocol */
      return two_stage_put_direct(pool, key.c_str(), key_len, value, value_len, handle, flags);
    }

    const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
    const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);

    if (option_DEBUG) {
      PLOG("put_direct: key=(%.*s) key_len=%lu value=(%.20s...) value_len=%lu", int(key_len),
           key.c_str(), key_len, static_cast<const char *>(value), value_len);

      PLOG("value_buffer: (iov_len=%lu, mr=%p, desc=%p)", value_buffer->iov->iov_len,
           static_cast<const void *>(value_buffer->region), value_buffer->desc);
    }

    const auto msg =
        new (iobs->base()) mcas::Protocol::Message_IO_request(iobs->length(), auth_id(), ++_request_id, pool,
                                                              mcas::Protocol::OP_PUT,  // op
                                                              key.c_str(), key_len, value_len, flags);

    if (_options.short_circuit_backend) msg->resvd |= mcas::Protocol::MSG_RESVD_SCBE;

    msg->flags = flags;

    iobs->set_length(msg->msg_len);

    post_recv(&*iobr);
    sync_send(&*iobs, value_buffer); /* send two concatentated buffers in single DMA */
    wait_for_completion(&*iobr);     /* get response */

    const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(iobr->base());

    if (option_DEBUG) PLOG("got response from PUT_DIRECT operation: status=%d", msg->get_status());

    status = response_msg->get_status();
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

status_t Connection_handler::async_put(const IMCAS::pool_t    pool,
                                       const void *           key,
                                       const size_t           key_len,
                                       const void *           value,
                                       const size_t           value_len,
                                       IMCAS::async_handle_t &out_handle,
                                       const unsigned int     flags)
{
  API_LOCK();

  if (option_DEBUG)
    PINF("async_put: %.*s (key_len=%lu) (value_len=%lu)", int(key_len), static_cast<const char *>(key), key_len, value_len);

  /* check key length */
  if ((key_len + value_len + sizeof(mcas::Protocol::Message_IO_request)) > Buffer_manager<IFabric_client>::BUFFER_LEN) {
    PWRN("mcas_client::async_put value length (%lu) too long. Use async_put_direct.", value_len);
    return IKVStore::E_TOO_LARGE;
  }

  buffer_t *iobs = allocate();
  buffer_t *iobr = allocate();

  try {
    const auto msg =
        new (iobs->base()) mcas::Protocol::Message_IO_request(iobs->length(), auth_id(), ++_request_id, pool,
                                                              mcas::Protocol::OP_PUT,  // op
                                                              key, key_len, value, value_len, flags);

    iobs->set_length(msg->msg_len);

    /* post both send and receive */
    post_recv(&*iobr);
    post_send(iobs->iov, iobs->iov + 1, &iobs->desc, iobs);

    out_handle = reinterpret_cast<IMCAS::async_handle_t>(new buffer_pair_t(iobs, iobr));

    return S_OK;
  }
  catch (...) {
    throw Logic_exception("async_put: network posting failed unexpectedly.");
  }

  return E_FAIL;
}

status_t Connection_handler::check_async_completion(IMCAS::async_handle_t &handle)
{
  auto bptrs = reinterpret_cast<buffer_pair_t *>(handle);
  assert(bptrs->iobs || bptrs->iobr);

  if (bptrs->iobs) {
    if (test_completion(bptrs->iobs) == false)
      return E_BUSY;
    else {
      free_buffer(bptrs->iobs);
      bptrs->iobs = nullptr;
    }
  }

  if (bptrs->iobr) {
    if (test_completion(bptrs->iobr) == false)
      return E_BUSY;
    else {
      assert(bptrs->iobs == nullptr);

      const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(bptrs->iobr->base());

      if (option_DEBUG)
        PLOG("got response from ASYNC PUT/ERASE operation: status=%d request_id=%lu", response_msg->get_status(),
             response_msg->request_id);

      status_t status = response_msg->get_status();
      free_buffer(bptrs->iobr);
      delete bptrs;
      return status;
    }
  }
  throw Logic_exception("unexpected control flow");
  return E_FAIL;
}

status_t Connection_handler::get(const pool_t pool, const std::string &key, std::string &value)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg =
        new (iobs->base()) mcas::Protocol::Message_IO_request(iobs->length(), auth_id(), ++_request_id, pool,
                                                              mcas::Protocol::OP_GET,  // op
                                                              key, "", 0);

    if (_options.short_circuit_backend) msg->resvd |= mcas::Protocol::MSG_RESVD_SCBE;

    iobs->set_length(msg->msg_len);

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr);

    const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(iobr->base());

    if (option_DEBUG) PLOG("got response from GET operation: status=%d (%s)", msg->get_status(), response_msg->data);

    status = response_msg->get_status();
    value.reserve(response_msg->data_len + 1);
    value.insert(0, response_msg->data, response_msg->data_len);
    assert(response_msg->data);
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

status_t Connection_handler::get(const pool_t pool, const std::string &key, void *&value, size_t &value_len)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg =
        new (iobs->base()) mcas::Protocol::Message_IO_request(iobs->length(), auth_id(), ++_request_id, pool,
                                                              mcas::Protocol::OP_GET,  // op
                                                              key.c_str(), key.length(), 0);

    /* indicate how much space has been allocated on this side. For
       get this is based on buffer size
    */
    msg->val_len = iobs->original_length - sizeof(mcas::Protocol::Message_IO_response);

    if (_options.short_circuit_backend) msg->resvd |= mcas::Protocol::MSG_RESVD_SCBE;

    iobs->set_length(msg->msg_len);

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr); /* TODO; could we issue the recv and send together? */

    const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(iobr->base());

    if (option_DEBUG)
      PLOG("got response from GET operation: status=%d request_id=%lu "
           "data_len=%lu",
           response_msg->get_status(), response_msg->request_id, response_msg->data_length());

    if (response_msg->get_status() != S_OK) return response_msg->get_status();

    if (option_DEBUG) PLOG("message value:(%s)", response_msg->data);

    if (response_msg->is_set_twostage_bit()) {
      /* two-stage get */
      const auto data_len = response_msg->data_length() + 1;
      value               = ::aligned_alloc(MiB(2), data_len);
      madvise(value, data_len, MADV_HUGEPAGE);

      auto region = register_memory(value, data_len); /* we could have some pre-registered? */
      auto desc   = get_memory_descriptor(region);

      iovec iov{value, data_len - 1};
      post_recv(&iov, (&iov) + 1, &desc, &iov);

      /* synchronously wait for receive to complete */
      wait_for_completion(&iov);

      deregister_memory(region);
    }
    else {
      /* copy off value from IO buffer */
      value     = ::malloc(response_msg->data_len + 1);
      value_len = response_msg->data_len;
      memcpy(value, response_msg->data, response_msg->data_len);
      static_cast<char *>(value)[response_msg->data_len] = '\0';
    }

    status = response_msg->get_status();
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

status_t Connection_handler::get_direct(const pool_t                 pool,
                                        const std::string &          key,
                                        void *                       value,
                                        size_t &                     out_value_len,
                                        const IMCAS::memory_handle_t handle)
{
  API_LOCK();

  if (!value || out_value_len == 0 || handle == 0) {
    PWRN("bad parameter value=%p out_value_len=%lu handle=%p", value, out_value_len, static_cast<const void *>(handle));
    return E_BAD_PARAM;
  }

  buffer_t *value_iob = reinterpret_cast<buffer_t *>(handle);
  if (!value_iob->check_magic()) {
    PWRN("bad handle parameter to get_direct");
    return E_BAD_PARAM;
  }

  /* check value is not too large for underlying transport */
  if (out_value_len > _max_message_size) return IKVStore::E_TOO_LARGE;

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  status_t status;
  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_IO_request(
        iobs->length(), auth_id(), ++_request_id, pool, mcas::Protocol::OP_GET, key.c_str(), key.length(), 0);

    /* indicate that this is a direct request and register
       how much space has been allocated on this side. For
       get_direct this is allocated by the client */
    msg->resvd   = Protocol::MSG_RESVD_DIRECT;
    msg->val_len = out_value_len;

    iobs->set_length(msg->msg_len);

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr); /* get response */

    const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(iobr->base());

    if (option_DEBUG)
      PLOG("get_direct: got initial response (two_stage=%s)", response_msg->is_set_twostage_bit() ? "true" : "false");

    /* insufficent space should have been dealt with already */
    assert(out_value_len >= response_msg->data_length());

    status = response_msg->get_status();

    /* if response not S_OK, do not do anything else */
    if (status != S_OK) {
      return status;
    }

    /* set out_value_len to receiving length */
    out_value_len = response_msg->data_length();

    if (response_msg->is_set_twostage_bit()) {
      /* two-stage get */
      post_recv(value_iob);

      /* synchronously wait for receive to complete */
      wait_for_completion(value_iob);
    }
    else {
      memcpy(value, response_msg->data, response_msg->data_len);
    }

    status = response_msg->get_status();
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

status_t Connection_handler::erase(const pool_t pool, const std::string &key)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_IO_request(
        iobs->length(), auth_id(), ++_request_id, pool, mcas::Protocol::OP_ERASE, key.c_str(), key.length(), 0);

    iobs->set_length(msg->msg_len);

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr);

    const auto response_msg = response_ptr<const mcas::Protocol::Message_IO_response>(iobr->base());

    if (option_DEBUG)
      PLOG("got response from ERASE operation: status=%d request_id=%lu "
           "data_len=%lu",
           response_msg->get_status(), response_msg->request_id, response_msg->data_length());
    status = response_msg->get_status();
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

status_t Connection_handler::async_erase(const IMCAS::pool_t    pool,
                                         const std::string &    key,
                                         IMCAS::async_handle_t &out_handle)
{
  API_LOCK();

  buffer_t *iobs = allocate();
  buffer_t *iobr = allocate();

  assert(iobs);
  assert(iobr);

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_IO_request(
        iobs->length(), auth_id(), ++_request_id, pool, mcas::Protocol::OP_ERASE, key.c_str(), key.length(), 0);

    iobs->set_length(msg->msg_len);

    /* post both send and receive */
    post_recv(&*iobr);
    post_send(iobs->iov, iobs->iov + 1, &iobs->desc, iobs);

    out_handle = reinterpret_cast<IMCAS::async_handle_t>(new buffer_pair_t(iobs, iobr));
  }
  catch (...) {
    throw Logic_exception("async_erase: network posting failed unexpectedly.");
  }

  return S_OK;
}

size_t Connection_handler::count(const pool_t pool)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_INFO_request(auth_id());
    msg->pool_id   = pool;
    msg->type      = IKVStore::Attribute::COUNT;
    iobs->set_length(msg->base_message_size());

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr);

    const auto response_msg = response_ptr<const mcas::Protocol::Message_INFO_response>(iobr->base());

    return response_msg->value;
  }
  catch (...) {
    return 0;
  }
}

status_t Connection_handler::get_attribute(const IKVStore::pool_t    pool,
                                           const IKVStore::Attribute attr,
                                           std::vector<uint64_t> &   out_attr,
                                           const std::string *       key)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_INFO_request(auth_id());
    msg->pool_id   = pool;

    msg->type = attr;
    if (key) msg->set_key(iobs->length(), *key);

    iobs->set_length(msg->message_size());

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr);

    const auto response_msg = response_ptr<const mcas::Protocol::Message_INFO_response>(iobr->base());

    out_attr.clear();
    out_attr.push_back(response_msg->value);
    status = response_msg->get_status();
  }
  catch (...) {
    status = E_FAIL;
  }
  return status;
}

status_t Connection_handler::get_statistics(IMCAS::Shard_stats &out_stats)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_INFO_request(auth_id());
    msg->pool_id   = 0;
    msg->type      = mcas::Protocol::INFO_TYPE_GET_STATS;
    iobs->set_length(msg->message_size());

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr);

    const auto response_msg = response_ptr<const mcas::Protocol::Message_stats>(iobr->base());

    status = response_msg->get_status();
#pragma GCC diagnostic push
#if 9 <= __GNUC__
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif
    out_stats = response_msg->stats;
#pragma GCC diagnostic pop
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

status_t Connection_handler::find(const IMCAS::pool_t pool,
                                  const std::string & key_expression,
                                  const offset_t      offset,
                                  offset_t &          out_matched_offset,
                                  std::string &       out_matched_key)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_INFO_request(auth_id());
    msg->pool_id   = pool;
    msg->type      = mcas::Protocol::INFO_TYPE_FIND_KEY;
    msg->offset    = offset;

    msg->set_key(iobs->length(), key_expression);
    iobs->set_length(msg->message_size());

    post_recv(&*iobr);
    sync_inject_send(&*iobs);
    wait_for_completion(&*iobr);

    const auto response_msg = response_ptr<const mcas::Protocol::Message_INFO_response>(iobr->base());

    status = response_msg->get_status();

    if (status == S_OK) {
      out_matched_key    = response_msg->c_str();
      out_matched_offset = response_msg->offset;
    }
  }
  catch (...) {
    status = E_FAIL;
  }
  return status;
}

status_t Connection_handler::invoke_ado(const IKVStore::pool_t            pool,
                                        const std::string &               key,
                                        const void *                      request,
                                        const size_t                      request_len,
                                        const unsigned int                flags,
                                        std::vector<IMCAS::ADO_response> &out_response,
                                        const size_t                      value_size)
{
  API_LOCK();

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);

  status_t status;

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_ado_request(
        iobs->length(), auth_id(), ++_request_id, pool, key, request, request_len, flags, value_size);
    iobs->set_length(msg->message_size());

    if (flags & IMCAS::ADO_FLAG_ASYNC) {
      sync_send(&*iobs);
      /* do not wait for response */
      return S_OK;
    }

    const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
    assert(iobr);

    post_recv(&*iobr);
    sync_send(&*iobs);
    wait_for_completion(&*iobr); /* wait for response */

    const auto response_msg = response_ptr<const mcas::Protocol::Message_ado_response>(iobr->base());

    status = response_msg->get_status();

    if (status == S_OK) {
      out_response.clear();

      /* unmarshal responses */
      for (uint32_t i = 0; i < response_msg->get_response_count(); i++) {
        void *   out_data     = nullptr;
        size_t   out_data_len = 0;
        uint32_t out_layer_id = 0;
        response_msg->client_get_response(i, out_data, out_data_len, out_layer_id);

#ifdef DEBUG_NPC_RESPONSES
        PLOG("Response:");
        hexdump(out_data, out_data_len);
#endif
        out_response.emplace_back(out_data, out_data_len, out_layer_id);
      }
    }
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

status_t Connection_handler::invoke_put_ado(const IKVStore::pool_t            pool,
                                            const std::string &               key,
                                            const void *                      request,
                                            const size_t                      request_len,
                                            const void *                      value,
                                            const size_t                      value_len,
                                            const size_t                      root_len,
                                            const unsigned int                flags,
                                            std::vector<IMCAS::ADO_response> &out_response)
{
  API_LOCK();

  if (request_len == 0) return E_INVAL;

  const auto iobs = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
  assert(iobs);

  out_response.clear();

  status_t status;

  try {
    const auto msg = new (iobs->base()) mcas::Protocol::Message_put_ado_request(
        iobs->length(), auth_id(), ++_request_id, pool, key, request, request_len, value, value_len, root_len, flags);

    iobs->set_length(msg->message_size());

    if (flags & IMCAS::ADO_FLAG_ASYNC) {
      sync_send(&*iobs);
      /* do not wait for response */
      return S_OK;
    }

    const auto iobr = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
    assert(iobr);

    post_recv(&*iobr);
    sync_send(&*iobs);
    wait_for_completion(&*iobr); /* wait for response */

    const auto response_msg = response_ptr<const mcas::Protocol::Message_ado_response>(iobr->base());

    status = response_msg->get_status();

    if (status == S_OK) {
      out_response.clear();

      /* unmarshal responses */
      for (uint32_t i = 0; i < response_msg->get_response_count(); i++) {
        void *   out_data     = nullptr;
        size_t   out_data_len = 0;
        uint32_t out_layer_id = 0;
        response_msg->client_get_response(i, out_data, out_data_len, out_layer_id);

#ifdef DEBUG_NPC_RESPONSES
        PLOG("Response:");
        hexdump(out_data, out_data_len);
#endif

        out_response.emplace_back(out_data, out_data_len, out_layer_id);
      }
    }
  }
  catch (...) {
    status = E_FAIL;
  }

  return status;
}

int Connection_handler::tick()
{
  using namespace mcas::Protocol;

  switch (_state) {
    case INITIALIZE: {
      set_state(HANDSHAKE_SEND);
      break;
    }
    case HANDSHAKE_SEND: {
      const auto iob = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
      auto       msg = new (iob->base()) mcas::Protocol::Message_handshake(auth_id(), 1);
      msg->set_status(S_OK);
      iob->set_length(msg->msg_len);
      post_send(iob->iov, iob->iov + 1, &iob->desc, &*iob);

      try {
        wait_for_completion(&*iob);
      }
      catch (...) {
        set_state(STOPPED);
      }

      set_state(HANDSHAKE_GET_RESPONSE);
      break;
    }
    case HANDSHAKE_GET_RESPONSE: {
      const auto iob = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
      post_recv(iob->iov, iob->iov + 1, &iob->desc, &*iob);

      try {
        wait_for_completion(&*iob);
      }
      catch (...) {
        set_state(STOPPED);
      }

      set_state(READY);

      _max_message_size = max_message_size(); /* from fabric component */
      break;
    }
    case READY: {
      return 0;
      break;
    }
    case SHUTDOWN: {
      const auto iob = std::unique_ptr<buffer_t, iob_free>(allocate(), this);
      auto       msg = new (iob->base()) mcas::Protocol::Message_close_session(reinterpret_cast<uint64_t>(this));

      iob->set_length(msg->msg_len);
      post_send(iob->iov, iob->iov + 1, &iob->desc, &*iob);

      try {
        wait_for_completion(&*iob);
      }
      catch (...) {
      }

      set_state(STOPPED);
      PLOG("mcas_client: connection %p shutdown.", static_cast<const void *>(this));
      return 0;
    }
    case STOPPED: {
      assert(0);
      return 0;
    }
  }  // end switch

  return 1;
}

}  // namespace Client
}  // namespace mcas
