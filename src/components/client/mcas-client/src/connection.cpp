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

#include "async_buffer_set_get_locate.h"
#include "async_buffer_set_put_locate.h"
#include "async_buffer_set_invoke.h"
#include "async_buffer_set_get_direct_offset.h"
#include "async_buffer_set_put_direct_offset.h"
#if CW_TEST
#include <cw/cw_common.h>
#include <cw/cw_fabric_test.h>
#endif
#include "memory_registered.h"
#include "protocol.h"
#include "range.h"

#include <city.h>
#include <common/cycles.h>
#include <common/delete_copy.h>
#include <common/to_string.h>
#include <common/utils.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <numeric> /* accumulate */
#include <thread> /* sleep_for */

#include <rapidjson/error/en.h>
#include <rapidjson/filereadstream.h>
#include <rapidjson/prettywriter.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include <rapidjson/schema.h>
#pragma GCC diagnostic pop
#include <rapidjson/stringbuffer.h>
#include <rapidjson/error/error.h>  // rapidjson::ParseErrorCode

static constexpr const unsigned TLS_DEBUG_LEVEL = 3;

/* environment variables */
static constexpr const char* ENVIRONMENT_VARIABLE_CERT = "CERT";
static constexpr const char* ENVIRONMENT_VARIABLE_KEY = "KEY";
static constexpr const char* ENVIRONMENT_VARIABLE_SC = "SHORT_CIRCUIT_BACKEND";

/* static constructor called once */
static void print_logs(int level, const char* msg) { printf("GnuTLS [%d]: %s", level, msg); }

static void __attribute__((constructor)) Global_ctor()
{
  gnutls_global_init();
  gnutls_global_set_log_level(0); /* 0-9 higher more verbose */
  gnutls_global_set_log_function(print_logs);
}

static constexpr const char * cipher_suite = "NORMAL:+AEAD";

namespace mcas
{
[[noreturn]] void throw_parse_exception(rapidjson::ParseErrorCode code, const char *msg, size_t offset);
}

//#define DEBUG_NPC_RESPONSES

using namespace component;

using memory_registered_fabric = mcas::memory_registered<IFabric_client>;

struct remote_fail : public std::runtime_error {
private:
  int _status;

public:
  remote_fail(int status_) : runtime_error("remote fail"), _status(status_) {}
  int status() const { return _status; }
};

using Connection = mcas::client::Connection;

Connection::Connection(const unsigned              debug_level,
                                       Connection_base::Transport *connection,
                                       Connection_base::buffer_manager &bm_,
                                       const unsigned              patience,
#if CW_TEST
	const cw::test_data test_data_,
#endif
                                       const common::string_view   other)
  : Connection_base(debug_level, connection, bm_, patience)
#if CW_TEST
	, _connection_start(clock_type::now())
	, _connection_delta(_connection_start)
	, _rm_out{}
	, _rm_in{}
	, _client{}
	, _ct_w()
	, _ct_wr_real_to_test()
	, _ct_wr_test_to_real()
	, _ct_rd_test_from_real()
#if 0
	, _scratchpad{}
	, _scratchpad_rma_key{}
#endif
#endif
#ifdef THREAD_SAFE_CLIENT
    , _api_lock{}
#endif
    , _exit{false},
    _request_id{0},
    _max_message_size{0},
    _max_inject_size(connection->max_inject_size()),
    _options()
	, _xcred{}
	, _priority{}
	, _session{}
	, _tls_buffer{}
#if CW_TEST
	, _test_data(test_data_)
#endif
{
  char *env = ::getenv(ENVIRONMENT_VARIABLE_SC);
  if (env && env[0] == '1') {
    _options.short_circuit_backend = true;
  }

  if(::getenv(ENVIRONMENT_VARIABLE_KEY) && ::getenv(ENVIRONMENT_VARIABLE_CERT))
    _options.tls = true;

  if(other.data()) {
    try {
      rapidjson::Document doc;
      doc.Parse(other.data());
      if ( doc.HasParseError() )
      {
        throw std::domain_error{std::string{"JSON parse error \""} + rapidjson::GetParseError_En(doc.GetParseError()) + "\" at " + std::to_string(doc.GetErrorOffset())};
      }
      auto security = doc.FindMember("security");

      /* set security options */
      if(security != doc.MemberEnd() && doc["security"].IsString()) {
        std::string option(doc["security"].GetString());

        //PNOTICE("!!!!! option=%s", option.c_str());
        /* tls:auth indicates TLS authentication */
        if(option == "tls:auth") {
          _options.tls = true;
        }
      }
    }
    catch (...) {
      throw API_exception("extra configuration string parse failed");
    }
  }

}

Connection::~Connection()
{
  PLOG("%s: (%p)", __func__, common::p_fmt(this));
#if CW_TEST
	FLOG("wr {} ", _ct_w.out("usec"));
	FLOG("wr_real_to_test {}", _ct_wr_real_to_test.out("usec"));
	FLOG("wr_test_to_real {}", _ct_wr_test_to_real.out("usec"));
	FLOG("rd_test_from_real {}", _ct_rd_test_from_real.out("usec"));
#endif
}

void Connection::send_complete(void *param, buffer_t *iob)
{
  PLOG("%s param %p iob %p", __func__, param, common::p_fmt(iob));
}

void Connection::recv_complete(void *param, buffer_t *iob)
{
  PLOG("%s param %p iob %p", __func__, param, common::p_fmt(iob));
}

void Connection::write_complete(void *param, buffer_t *iob)
{
  PLOG("%s param %p iob %p", __func__, param, common::p_fmt(iob));
}

void Connection::read_complete(void *param, buffer_t *iob)
{
  PLOG("%s param %p iob %p", __func__, param, common::p_fmt(iob));
}

auto Connection::open_pool(const string_view name,
                                                         const unsigned int flags,
                                                         const addr_t base) ->
#if CW_TEST
 std::tuple<Connection::pool_t, uint64_t, uint64_t>
#else
 Connection::pool_t
#endif
{
  API_LOCK();

  PMAJOR("Open pool: %*.s (flags=%u, base=0x%lx)", int(name.size()), name.data(), flags, base);

  /* send pool request message */

  /* Use unique_ptr to ensure that the dynamically buffers are freed.
   * unique_ptr protects against the code forgetting to call free_buffer,
   * which it usually did when the function exited by a throw or a
   * non-terminal return.
   *
   */
  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  IKVStore::pool_t pool_id;
#if CW_TEST
	uint64_t scratchpad_base {};
	uint64_t scratchpad_rma_key {};
#endif

  assert(&*iobr != &*iobs);

  try {
    const auto msg =
      new (iobs->base())
      mcas::protocol::Message_pool_request(iobs->length(), auth_id(), /* auth id */
                                           request_id(), 0,           /* size */
                                           0,                         /* expected obj cnt */
                                           mcas::protocol::OP_TYPE::OPEN,
                                           name,
                                           0, /* flags */
                                           base /* base virtual address */
                                           );

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
    sync_inject_send(&*iobs, msg, __func__);
    wait_for_completion(iobr->to_context()); /* await response */

    const auto response_msg = msg_recv<const mcas::protocol::Message_pool_response>(&*iobr, __func__);

    pool_id = response_msg->pool_id;
#if CW_TEST
		scratchpad_base = response_msg->scratchpad_base;
		scratchpad_rma_key = response_msg->scratchpad_rma_key;
#if 0
		scratchpad =
			{
				common::make_byte_span(
					reinterpret_cast<void *>(response_msg->scratchpad_base)
					, response_msg->scratchpad_size
				)
				, response_msg->scratchpad_rma_key
			};
		_client->set_vaddr(response_msg->scratchpad_base);
#endif
#endif

  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    pool_id = IKVStore::POOL_ERROR;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    pool_id = IKVStore::POOL_ERROR;
  }
#if CW_TEST
  return { pool_id, scratchpad_base, scratchpad_rma_key };
#else
  return pool_id;
#endif
}

auto Connection::create_pool(const string_view  name_,
                                const size_t       size_,
                                const unsigned int flags_,
                                const uint64_t     expected_obj_count_,
                                const addr_t       base_
) ->
#if CW_TEST
 std::tuple<Connection::pool_t, uint64_t, uint64_t>
#else
 Connection::pool_t
#endif
{
  API_LOCK();

  PMAJOR("Create pool: %.*s (flags=%u, base=0x%lx)", int(name_.size()), name_.data(), flags_, base_);

  /* send pool request message */
  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  IKVStore::pool_t pool_id;
#if CW_TEST
	uint64_t scratchpad_base {};
	uint64_t scratchpad_rma_key {};
#endif

  try {
    const auto msg = new (iobs->base())
      protocol::Message_pool_request(iobs->length(),
                                     auth_id(),
                                     request_id(),
                                     size_,
                                     expected_obj_count_,
                                     mcas::protocol::OP_TYPE::CREATE,
                                     name_,
                                     flags_,
                                     base_);
    assert(msg->op());

    post_recv(&*iobr);
    sync_inject_send(&*iobs, msg, __func__);
    wait_for_completion(iobr->to_context());

    const auto response_msg = msg_recv<const mcas::protocol::Message_pool_response>(&*iobr, __func__);

    pool_id = response_msg->pool_id;
#if CW_TEST
		scratchpad_base = response_msg->scratchpad_base;
		scratchpad_rma_key = response_msg->scratchpad_rma_key;
#if 0
		scratchpad =
			{
				common::make_byte_span(
					reinterpret_cast<void *>(response_msg->scratchpad_base)
					, response_msg->scratchpad_size
				)
				, response_msg->scratchpad_rma_key
			};
	_client->set_vaddr(response_msg->scratchpad_base);
#endif
#endif
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    pool_id = IKVStore::POOL_ERROR;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    pool_id = IKVStore::POOL_ERROR;
  }

  /* Note: most request/response pairs return status. This one returns a pool_id
   * instead. */
#if CW_TEST
	if ( pool_id != KVStore::POOL_ERROR )
	{
		{
			/* run a performance test on the pool: read 8MiB, then write 8 Mib 10K times */
			std::size_t sz = 1UL << 23;
			auto base = scratchpad_base - scratchpad_base % 0x10000;
			read_test_from_real(sz, base, scratchpad_rma_key);
			for ( auto i = 0; i != 10000; ++i )
			{
				write_test_to_real(sz, base, scratchpad_rma_key);
			}
			FLOGM("perf check: wr_test_to_real aligned at {} {}", reinterpret_cast<void *>(base), _ct_wr_test_to_real.out("usec"));
		}
		{
			/* run a performance test on the pool: read 8MiB, then write 8 Mib 10K times */
			std::size_t sz = 1UL << 23;
			read_test_from_real(sz, scratchpad_base, scratchpad_rma_key);
			for ( auto i = 0; i != 10000; ++i )
			{
				write_test_to_real(sz, scratchpad_base, scratchpad_rma_key);
			}
			FLOGM("perf check: wr_test_to_real {}", _ct_wr_test_to_real.out("usec"));
		}
	}
  return { pool_id, scratchpad_base, scratchpad_rma_key };
#else
  return pool_id;
#endif
}

status_t Connection::close_pool(const pool_t pool)
{
  PMAJOR("Close pool: 0x%lx", pool);

  API_LOCK();
  /* send pool request message */
  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  const auto msg  = new (iobs->base())
    mcas::protocol::Message_pool_request(iobs->length(), auth_id(), request_id(), mcas::protocol::OP_TYPE::CLOSE, pool);

  post_recv(&*iobr);
  sync_inject_send(&*iobs, msg, __func__);
  try {
    wait_for_completion(iobr->to_context());

    const auto response_msg = msg_recv<const mcas::protocol::Message_pool_response>(&*iobr, __func__);

    const auto status = response_msg->get_status();
    return status;
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    return E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return E_FAIL;
  }
}

status_t Connection::delete_pool(const string_view name)
{
  if (name.empty()) return E_INVAL;

  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();

  const auto msg = new (iobs->base())
    mcas::protocol::Message_pool_request(iobs->length(),
                                         auth_id(),
                                         request_id(),
                                         0, // size
                                         0, // exp obj count
                                         mcas::protocol::OP_TYPE::DELETE,
                                         name,
                                         0, // flags
                                         0); // base

  post_recv(&*iobr);
  sync_inject_send(&*iobs, msg, __func__);
  try {
    wait_for_completion(iobr->to_context());

    const auto response_msg = msg_recv<const mcas::protocol::Message_pool_response>(&*iobr, __func__);

    return response_msg->get_status();
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    return E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return E_FAIL;
  }
}

status_t Connection::delete_pool(const IMCAS::pool_t pool)
{
  if (!pool) return E_INVAL;

  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();

  const auto msg = new (iobs->base())
    mcas::protocol::Message_pool_request(iobs->length(), auth_id(), request_id(), mcas::protocol::OP_TYPE::DELETE, pool);

  post_recv(&*iobr);
  sync_inject_send(&*iobs, msg, __func__);
  try {
    wait_for_completion(iobr->to_context());

    const auto response_msg = msg_recv<const mcas::protocol::Message_pool_response>(&*iobr, __func__);

    return response_msg->get_status();
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    return E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return E_FAIL;
  }
}

status_t Connection::configure_pool(const IMCAS::pool_t pool, const string_view json)
{
  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();

  if (!mcas::protocol::Message_IO_request::would_fit(json.length(), iobs->original_length())) {
    return IKVStore::E_TOO_LARGE;
  }

  const auto msg = new (iobs->base()) mcas::protocol::Message_IO_request(iobs->length(), auth_id(), request_id(), pool,
                                                                         mcas::protocol::OP_TYPE::CONFIGURE,  // op
                                                                         json);

  post_recv(&*iobr);
  sync_inject_send(&*iobs, msg, __func__);
  try {
    wait_for_completion(iobr->to_context());
    const auto response_msg = msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, __func__);

    return response_msg->get_status();
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    return E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return E_FAIL;
  }
}

/**
 * Memcpy version; both key and value are copied
 *
 */
status_t Connection::put(const pool_t       pool,
                                 const string_view_key key,
                                 const void *       value,
                                 const size_t       value_len,
                                 const unsigned int flags)
{
  TM_ROOT();
  if (value == nullptr || value_len == 0) return E_INVAL;
  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();

  if (debug_level() > 1)
    PINF("put: %.*s (key_len=%lu) (value_len=%lu)", int(key.size()), common::pointer_cast<char>(key.data()), key.size(), value_len);

  /* check key length */
  if (!mcas::protocol::Message_IO_request::would_fit(key.size() + value_len, iobs->original_length())) {
    PWRN("mcas_client::%s value length (%lu) too long. Use put_direct.", __func__, value_len);
    return IKVStore::E_TOO_LARGE;
  }

  status_t status;

  try {
    const auto msg =
      new (iobs->base()) mcas::protocol::Message_IO_request(iobs->length(), auth_id(), request_id(), pool,
                                                            mcas::protocol::OP_TYPE::PUT,  // op
                                                            key, value, value_len, flags);

    if (_options.short_circuit_backend) msg->add_scbe();

    iobs->set_length(msg->msg_len());

    post_recv(&*iobr);
    sync_send(&*iobs, msg, __func__); /* this will clean up iobs */
    {
      TM_SCOPE(wait_recv)
      wait_for_completion(iobr->to_context());
    }

    const auto response_msg = msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, __func__);

    status = response_msg->get_status();
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    status = E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    status = E_FAIL;
  }

  return status;
}

auto Connection::locate(const pool_t pool_, const std::size_t offset_, const std::size_t size_)
  -> std::tuple<uint64_t, std::vector<locate_element>>
{
  const auto iobr = make_iob_ptr_recv();
  const auto iobs = make_iob_ptr_send();

  /* send advance leader message */
  const auto msg = new (iobs->base())
    protocol::Message_IO_request(auth_id(), pool_, request_id(), protocol::OP_TYPE::LOCATE, offset_, size_);

  post_recv(&*iobr);
  sync_inject_send(&*iobs, msg, __func__);
  /* wait for response from header before posting the value */
  wait_for_completion(iobr->to_context());

  const auto response = msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, __func__);

  if (response->get_status() != S_OK) {
    throw remote_fail(response->get_status());
  }

  auto                        cursor = response->edata();
  std::vector<locate_element> addr_list(cursor, cursor + response->element_count());

  return std::tuple<uint64_t, std::vector<locate_element>>(response->key, std::move(addr_list));
}

namespace
{
  std::size_t values_size(gsl::span<const common::const_byte_span> values_)
  {
    return std::accumulate(
      values_.begin(), values_.end()
      , 0
      , [] (std::size_t a, const common::const_byte_span &s) -> std::size_t
        {
          return a + size(s);
        }
    );
  }
}

IMCAS::async_handle_t Connection::put_locate_async(TM_ACTUAL const pool_t                        pool,
                                            string_view_key key,
                                                           const gsl::span<const common::const_byte_span> values,
                                                           component::Registrar_memory_direct *rmd_,
                                                           gsl::span<const component::IKVStore::memory_handle_t> mem_handles_,
                                                           const unsigned                      flags)
{
  TM_SCOPE()
  auto iobr = make_iob_ptr_recv();
  auto iobs = make_iob_ptr_send();

  /* send locate message */
  const auto msg = new (iobs->base()) protocol::Message_IO_request(
                                                                   iobs->length(), auth_id(), request_id(), pool, protocol::OP_TYPE::PUT_LOCATE, key, values_size(values), flags);
  iobs->set_length(msg->msg_len());

  post_recv(&*iobr);
  post_send({iobs->iov, iobs->iov + 1}, iobs->desc, iobs->to_context(), msg, __func__);

  /*
   * The entire put_locate protocol involves five completions at the client:
   *   send PUT_LOCATE request
   *   recv PUT_LOCATE response
   *   write DMA
   *   send PUT_RELEASE request
   *   recv PUT_RELEASE response
   */
  return
    static_cast<IMCAS::async_handle_t>(
      new async_buffer_set_put_locate(
        TM_REF debug_level()
        , rmd_
        , std::move(iobs)
        , std::move(iobr)
        , make_iob_ptr_send()
        , make_iob_ptr_recv()
        , pool, auth_id()
        , values
        , mem_handles_
      )
    );
}

IMCAS::async_handle_t Connection::get_direct_offset_async(const pool_t                        pool_,
                                                                  const std::size_t                   offset_,
                                                                  void *const                         buffer_,
                                                                  std::size_t &                       len_,
                                                                  component::Registrar_memory_direct *rmd_,
                                                                  void *const                         desc_)
{
  TM_ROOT()
  auto iobr = make_iob_ptr_recv();
  auto iobs = make_iob_ptr_send();

  /* send advance leader message */
  const auto msg = new (iobs->base())
    protocol::Message_IO_request(auth_id(), request_id(), pool_, protocol::OP_TYPE::LOCATE, offset_, len_);
  iobs->set_length(msg->msg_len());

  post_recv(&*iobr);
  post_send({iobs->iov, iobs->iov + 1}, iobs->desc, iobs->to_context(), msg, __func__);

  /*
   * The entire get_direct_offset protocol involves five completions at the
   * client: send LOCATE request recv LOCATE response read DMA send RELEASE
   * request recv RELEASE response
   */
  return
    static_cast<IMCAS::async_handle_t>(new async_buffer_set_get_direct_offset(
                                                                                                         TM_REF debug_level(), rmd_, std::move(iobs), std::move(iobr), iob_ptr(nullptr, this), make_iob_ptr_send(),
                                                                                                         make_iob_ptr_recv(), pool_, auth_id(), offset_, buffer_, len_, desc_));
}

IMCAS::async_handle_t Connection::put_direct_offset_async(const pool_t                        pool_,
                                                                  const std::size_t                   offset_,
                                                                  const void *const                   buffer_,
                                                                  std::size_t &                       length_,
                                                                  component::Registrar_memory_direct *rmd_,
                                                                  void *const                         desc_)
{
  TM_ROOT()
  auto iobr = make_iob_ptr_recv();
  auto iobs = make_iob_ptr_send();

  /* send locate message */
  const auto msg = new (iobs->base())
    protocol::Message_IO_request(auth_id(), request_id(), pool_, protocol::OP_TYPE::LOCATE, offset_, length_);
  iobs->set_length(msg->msg_len());

  post_recv(&*iobr);
  post_send({iobs->iov, iobs->iov + 1}, iobs->desc, iobs->to_context(), msg, __func__);

  /*
   * The entire get_direct_offset protocol involves five completions at the
   * client: send LOCATE request recv LOCATE response read DMA send RELEASE
   * request recv RELEASE response
   */
  return
    static_cast<IMCAS::async_handle_t>(new async_buffer_set_put_direct_offset(
      TM_REF debug_level(), rmd_, std::move(iobs), std::move(iobr), iob_ptr(nullptr, this), make_iob_ptr_send(),
      make_iob_ptr_recv(), pool_, auth_id(), offset_, buffer_, length_, desc_));
}

IMCAS::async_handle_t
Connection::get_locate_async(TM_ACTUAL const pool_t                        pool,
                                     string_view_key                     key,
                                     void *const                         value,
                                     size_t &                            value_len,
                                     component::Registrar_memory_direct *rmd_,
                                     void *const                         desc_,
                                     const unsigned                      flags)
{
	TM_SCOPE()
	auto iobr = make_iob_ptr_recv();
	auto iobs = make_iob_ptr_send();
	const auto buffer_len = value_len;

	/* send advance leader message */
	const auto msg = new (iobs->base()) protocol::Message_IO_request(
                                                                   iobs->length(), auth_id(), request_id(), pool, protocol::OP_TYPE::GET_LOCATE, key, value_len, flags);
	{
		TM_SCOPE(1)
		iobs->set_length(msg->msg_len());

		post_recv(&*iobr);
		post_send({iobs->iov, iobs->iov + 1}, iobs->desc, iobs->to_context(), msg, __func__);

		{
			TM_SCOPE(wait_send)
			wait_for_completion(iobs->to_context());
		}
		iobs.reset(nullptr);

		{
				TM_SCOPE(wait_recv)
				wait_for_completion(iobr->to_context());
		}
	}
	const auto response_msg = msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, "ASYNC GET_LOCATE");
	auto status = response_msg->get_status();
	if (status != S_OK) {
		throw remote_fail(status);
	}

	value_len = response_msg->data_length();
	auto addr = response_msg->addr;
	auto memory_key = response_msg->key;
	iobs.reset(nullptr);
	auto transfer_len = std::min(buffer_len, value_len);

	{
		TM_SCOPE(2)
		/*
		 * The entire get_locate protocol involves five completions at the client:
		 *   send GET_LOCATE request
		 *   recv GET_LOCATE response
		 *   read DMA
		 *   send GET_RELEASE request
		 *   recv GET_RELEASE response
		 */
		return
			static_cast<IMCAS::async_handle_t>(new async_buffer_set_get_locate(
				TM_REF
				debug_level(), rmd_
#if 0
				, make_iob_ptr_read()
#endif
				, make_iob_ptr_send(),
				make_iob_ptr_recv(), pool, auth_id(), value, transfer_len, this, desc_, addr, memory_key));
	}
}

status_t Connection::spin_for_async_completion(async_start_type st_)
{
  if ( st_.status == S_OK )
  {
    auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(_patience);
    TM_SCOPE(spin)
    do {
      if ( deadline < std::chrono::system_clock::now() )
      {
        throw Program_exception("time out: waited %lu seconds for completion", _patience);
      }
      st_.status = check_async_completion(TM_REF st_.handle);
    } while (st_.status == E_BUSY);
  }
  return st_.status;
}

status_t Connection::put_direct(pool_t                               pool_,
                      const string_view_key                key_,
                      gsl::span<const common::const_byte_span> values_,
                      component::Registrar_memory_direct * rmd_,
                      gsl::span<const component::IMCAS::memory_handle_t> handles_,
                      const component::IMCAS::flags_t      flags_)
{
  TM_ROOT()
  return spin_for_async_completion(async_put_direct(pool_, key_, values_, rmd_, handles_, flags_));
}

status_t Connection::async_put(const IMCAS::pool_t    pool,
                      const string_view_key                key_,
                                       const void *           value,
                                       const size_t           value_len,
                                       IMCAS::async_handle_t &out_handle,
                                       const IMCAS::flags_t   flags)
{
  API_LOCK();

  if (debug_level() > 1)
    PINF("%s: %.*s (key_len=%lu) (value_len=%lu)", __func__, int(key_.size()), common::pointer_cast<char>(key_.data()), key_.size(),
         value_len);

  auto iobr = make_iob_ptr_recv();
  auto iobs = make_iob_ptr_send();

  /* check key length */
  if (!mcas::protocol::Message_IO_request::would_fit(key_.size() + value_len, iobs->original_length())) {
    PWRN("mcas_client::%s value length (%lu) too long. Use async_put_direct.", __func__, value_len);
    return IKVStore::E_TOO_LARGE;
  }

  try {
    const auto msg =
      new (iobs->base()) mcas::protocol::Message_IO_request(iobs->length(), auth_id(), request_id(), pool,
                                                            mcas::protocol::OP_TYPE::PUT,  // op
                                                            key_, value, value_len, flags);

    iobs->set_length(msg->msg_len());

    /* post both send and receive */
    post_recv(&*iobr);
    post_send({iobs->iov, iobs->iov + 1}, iobs->desc, iobs->to_context(), msg, __func__);

    out_handle = new async_buffer_set_simple(debug_level(), std::move(iobs), std::move(iobr));

    return S_OK;
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    throw Logic_exception("%s: network posting failed unexpectedly.", __func__);
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    throw Logic_exception("%s: network posting failed unexpectedly.", __func__);
  }

  return E_FAIL;
}

auto Connection::async_put_direct(
  const IMCAS::pool_t                        pool_,
  const string_view_key                      key_,
  const gsl::span<const common::const_byte_span> values_,
  component::Registrar_memory_direct *       rmd_,
  const gsl::span<const component::IKVStore::memory_handle_t> mem_handles_,
  const component::IMCAS::flags_t            flags_
) -> async_start_type
{
  component::IMCAS::async_handle_t async_handle = component::IMCAS::ASYNC_HANDLE_INIT;
  auto status = async_put_direct(pool_, key_, values_, async_handle, rmd_, mem_handles_, flags_);
  return {status, async_handle};
}

status_t Connection::async_put_direct(const IMCAS::pool_t                        pool_,
                                              const string_view_key                      key_,
                                              const gsl::span<const common::const_byte_span>   values_,
                                              component::IMCAS::async_handle_t &         out_async_handle_,
                                              component::Registrar_memory_direct *       rmd_,
                                              const gsl::span<const component::IKVStore::memory_handle_t> mem_handles_,
                                              const component::IMCAS::flags_t            flags_)
{
  TM_ROOT()
  API_LOCK();

  assert(_max_message_size);

  if (pool_ == 0) {
    PWRN("%s: invalid pool identifier", __func__);
    return E_INVAL;
  }

  for ( const auto & v : values_ )
  {
    if ( ::size(v) && ! ::base(v) ) {
      PWRN("%s: bad parameter value=%p value_len=%zu", __func__, ::base(v), ::size(v) );
      return E_BAD_PARAM;
    }
  }

  try {
    auto iobr = make_iob_ptr_recv();
    auto iobs = make_iob_ptr_send();

#if 1
    /* for large puts, where the receiver will not have
     * sufficient buffer space, we use put locate (DMA write) protocol */
    out_async_handle_ = put_locate_async(TM_REF
                                         pool_, key_, values_, rmd_,
                                         mem_handles_, flags_);
#else
    if (values_.size() == 1 /* A simplification. We could change the small put code to handle multiple source */
        &&
        (_force_direct == false) &&
        (mcas::protocol::Message_IO_request::would_fit(key_len_ + ::size(values_.front()), iobs->original_length())) &&
        (mem_handles_.size() != 0 && mem_handles_.front() != IKVStore::HANDLE_NONE)) {

      /* Fast path: small size and memory already registered */
      CPLOG(1, "%s: using small send for direct put key=(%.*s) key_len=%lu value=(%.20s...) value_len=%lu", __func__, int(key_len_),
            static_cast<const char *>(key_), key_len_, static_cast<const char *>(::base(values_.front())), ::size(values_.front()));

      const auto msg =
        new (iobs->base()) mcas::protocol::Message_IO_request(iobs->length(), auth_id(), request_id(), pool_,
                                                              mcas::protocol::OP_TYPE::PUT,  // op
                                                              key_, key_len_, size(values_.front()), flags_);

      if (_options.short_circuit_backend) msg->add_scbe();

      post_recv(&*iobr);

      iobs->set_length(msg->msg_len());
      iobs->iov[1].iov_base = const_cast<void*>(::base(values_.front()));
      iobs->iov[1].iov_len =  size(values_.front());
      iobs->desc[1] = static_cast<buffer_base *>(mem_handles_.front())->get_desc();
      post_send(iobs->iov, iobs->iov + 2, iobs->desc, &*iobs, msg, __func__); /* send two concatentated buffers in single DMA */

      out_async_handle_ = new async_buffer_set_simple(debug_level(), std::move(iobs), std::move(iobr));
    }
    else
    {
        /* for large puts, where the receiver will not have
         * sufficient buffer space, we use put locate (DMA write) protocol
         */
        out_async_handle_ = put_locate_async(TM_REF pool_, key_, key_len_, values_, rmd_,
                                             mem_handles_, flags_);
    }
#endif
    return S_OK;
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    return E_FAIL;
  }
  catch (const remote_fail &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return e.status();
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return E_FAIL;
  }
}

auto Connection::async_get_direct(
  const IMCAS::pool_t                        pool_,
  const string_view_key                      key_,
  void *const                                value_,
  size_t &                                   value_len_,
  component::Registrar_memory_direct *       rmd_,
  const component::IKVStore::memory_handle_t mem_handle_,
  const component::IMCAS::flags_t            flags_
) -> async_start_type
{
  component::IMCAS::async_handle_t async_handle = component::IMCAS::ASYNC_HANDLE_INIT;
  auto status = async_get_direct(pool_, key_, value_, value_len_, async_handle, rmd_, mem_handle_, flags_);
  return {status, async_handle};
}

status_t Connection::async_get_direct(TM_ACTUAL const IMCAS::pool_t                        pool_,
                                              const string_view_key                      key_,
                                              void *const                                value_,
                                              size_t &                                   value_len_,
                                              component::IMCAS::async_handle_t &         out_async_handle_,
                                              component::Registrar_memory_direct *       rmd_,
                                              const component::IKVStore::memory_handle_t mem_handle_,
                                              const component::IMCAS::flags_t            flags_)
{
  TM_SCOPE()
  API_LOCK();

  if (value_len_ && !value_) {
    PWRN("%s: bad parameter value=%p value_len=%zu", __func__, value_, value_len_);
    return E_BAD_PARAM;
  }

  assert(_max_message_size);

  if (pool_ == 0) {
    PWRN("%s: invalid pool identifier", __func__);
    return E_INVAL;
  }

  try {
    auto iobr = make_iob_ptr_recv();
    auto iobs = make_iob_ptr_send();

    out_async_handle_ = get_locate_async(TM_REF
                                         pool_, key_, value_, value_len_, rmd_,
                                         mem_handle_ == IKVStore::HANDLE_NONE ? nullptr : static_cast<buffer_base *>(mem_handle_)->get_desc(), flags_);
    return S_OK;
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    return E_FAIL;
  }
  catch (const remote_fail &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return e.status();
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return E_FAIL;
  }
}

status_t Connection::check_async_completion(TM_ACTUAL IMCAS::async_handle_t &handle)
{
  API_LOCK();
  auto bptrs = static_cast<async_buffer_set_t *>(handle);
  assert(bptrs);

  int status = E_BUSY;
  try
    {
      status = bptrs->move_along(TM_REF this);
      /* status will be one of
       * E_BUSY: the operattion is not finished, but the call to move_along may have
       * caused progress other: the operation has finished
       */
    }
  catch ( const Exception &e )
    {
      PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
      status = E_FAIL;
    }
  catch ( const std::exception &e )
    {
      PLOG("%s %s fail %s", __FILE__, __func__, e.what());
      status = E_FAIL;
    }

  if (status != E_BUSY) {
    delete bptrs;
  }

  return status;
}

status_t Connection::get(const pool_t pool, const string_view_key key, std::string &value)
{
  TM_ROOT()
  API_LOCK();

  if (debug_level() > 1) {
    PINF("get: %.*s (key_len=%lu)", int(key.size()), common::pointer_cast<char>(key.data()), key.size());
  }

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg =
      new (iobs->base()) mcas::protocol::Message_IO_request(iobs->length(), auth_id(), request_id(), pool,
                                                            mcas::protocol::OP_TYPE::GET,  // op
                                                            key, string_view_value(), 0);

    if (_options.short_circuit_backend) msg->add_scbe();

    post_recv(&*iobr);
    sync_inject_send(&*iobs, msg, __func__);
    wait_for_completion(iobr->to_context());

    const auto response_msg = msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, __func__);
#if 0
    /* NOTE: g++ might or might not be smart enough to skip the string argument evaluation if the
     *  debug_level() check in msg_recv_log will suppress use of the string.
     * If it is not, performance will suffer.
     */
    msg_recv_log(response_msg, __func__ + std::string(" ") + std::string(response_msg->data(), response_msg->data_length()));
#endif
                 status = response_msg->get_status();
                 value.reserve(response_msg->data_length() + 1);
                 value.insert(0, response_msg->cdata(), response_msg->data_length());
                 assert(response_msg->data());
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    status = E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    status = E_FAIL;
  }

  return status;
}

status_t Connection::get(const pool_t pool, const string_view_key key, void *&value, size_t &value_len)
{
  TM_ROOT()
  API_LOCK();

  if (debug_level() > 1) {
    PINF("get: %.*s (key_len=%lu)", int(key.size()), common::pointer_cast<char>(key.data()), key.size());
  }

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg =
      new (iobs->base()) mcas::protocol::Message_IO_request(iobs->length(), auth_id(), request_id(), pool,
                                                            mcas::protocol::OP_TYPE::GET,  // op
                                                            key, 0, 0);

    /* indicate how much space has been allocated on this side. For
       get this is based on buffer size
    */
    msg->set_availabe_val_len_from_iob_len(iobs->original_length());

    if (_options.short_circuit_backend) msg->add_scbe();

    post_recv(&*iobr);
    sync_inject_send(&*iobs, msg, __func__);
    {
      TM_SCOPE(wait_recv)
      wait_for_completion(iobr->to_context()); /* TODO; could we issue the recv and send together? */
    }

    const auto response_msg = msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, __func__);

    if (response_msg->get_status() != S_OK) return response_msg->get_status();

    CPLOG(1, "%s: message value (%.*s) size=%lu", __func__, int(response_msg->data_length()), response_msg->data(), response_msg->data_length());

    if (response_msg->is_set_twostage_bit()) {
      /* two-stage get */
      const auto data_len = response_msg->data_length() + 1;
      value               = ::aligned_alloc(MiB(2), data_len);
      if (value == nullptr) {
        throw std::bad_alloc();
      }
      madvise(value, data_len, MADV_HUGEPAGE);

      auto  region = make_memory_registered(common::make_const_byte_span(value, data_len)); /* we could have some pre-registered? */
      void *desc[] = {region.get_memory_descriptor()};

      ::iovec iov[]{{value, data_len - 1}};
      ::fi_context2 ctxt;
      post_recv(iov, std::begin(desc), &ctxt);

      /* synchronously wait for receive to complete */
      wait_for_completion(&ctxt);
      CPLOG(1, "%s Received value from two stage get", __func__);
    }
    else {
      TM_SCOPE(alloc)
      /* copy off value from IO buffer */
      value = ::malloc(response_msg->data_length() + 1);
      if (value == nullptr) {
        throw std::bad_alloc();
      }

        TM_SCOPE(copy)
      value_len = response_msg->data_length();
      std::memcpy(value, response_msg->data(), response_msg->data_length());
      static_cast<char *>(value)[response_msg->data_length()] = '\0';

    }

    status = response_msg->get_status();
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    status = E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    status = E_FAIL;
  }

  return status;
}

status_t Connection::get_direct(const pool_t                              pool_,
                                        const string_view_key                     key_,
                                        void *const                               value_,
                                        size_t &                                  value_len_,
                                        component::Registrar_memory_direct *const rmd_,
                                        const IMCAS::memory_handle_t              mem_handle_)
{
  TM_ROOT()
  return spin_for_async_completion(async_get_direct(TM_REF pool_, key_, value_, value_len_, rmd_, mem_handle_, 0));
}

status_t Connection::get_direct_offset(const pool_t                              pool_,
                                               const std::size_t                         offset_,
                                               std::size_t &                             length_,
                                               void *const                               buffer_,
                                               component::Registrar_memory_direct *const rmd_,
                                               const component::IMCAS::memory_handle_t   mem_handle_)
{
  TM_ROOT()
  return spin_for_async_completion(async_get_direct_offset(pool_, offset_, length_, buffer_, rmd_, mem_handle_));
}

status_t Connection::put_direct_offset(const pool_t                              pool_,
                                               const std::size_t                         offset_,
                                               std::size_t &                             size_,
                                               const void *const                         buffer_,
                                               component::Registrar_memory_direct *const rmd_,
                                               const component::IMCAS::memory_handle_t   mem_handle_)
{
  TM_ROOT()
  return spin_for_async_completion(async_put_direct_offset(pool_, offset_, size_, buffer_, rmd_, mem_handle_));
}

auto Connection::async_get_direct_offset(
  const pool_t                              pool_,
  const std::size_t                         offset_,
  std::size_t &                             length_,
  void *const                               buffer_,
  component::Registrar_memory_direct *const rmd_,
  const component::IMCAS::memory_handle_t   mem_handle_
) -> async_start_type
{
  component::IMCAS::async_handle_t async_handle = component::IMCAS::ASYNC_HANDLE_INIT;
  auto status = async_get_direct_offset(pool_, offset_, length_, buffer_, async_handle, rmd_, mem_handle_);
  return {status, async_handle};
}

status_t Connection::async_get_direct_offset(const pool_t                              pool_,
                                                     const std::size_t                         offset_,
                                                     std::size_t &                             length_,
                                                     void *const                               buffer_,
                                                     IMCAS::async_handle_t &                   out_async_handle_,
                                                     component::Registrar_memory_direct *const rmd_,
                                                     const component::IMCAS::memory_handle_t   mem_handle_)
{
  if(length_ == 0)
    throw API_exception("%s: variant of get_direct_offset called with zero length", __func__);

  API_LOCK();

  if (length_ && !buffer_) {
    PWRN("%s: bad parameter buffer=%p size=%zu", __func__, buffer_, length_);
    return E_BAD_PARAM;
  }

  try {
    out_async_handle_ = get_direct_offset_async(pool_, offset_, buffer_, length_, rmd_,
                                                mem_handle_ == IMCAS::MEMORY_HANDLE_NONE ? nullptr : static_cast<buffer_base *>(mem_handle_)->get_desc());
    return S_OK;
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    return E_FAIL;
  }
  catch (const remote_fail &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return e.status();
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return E_FAIL;
  }
}

auto Connection::async_put_direct_offset(
  const pool_t                              pool_,
  const std::size_t                         offset_,
  std::size_t &                             length_,
  const void *const                         buffer_,
  component::Registrar_memory_direct *const rmd_,
  const component::IMCAS::memory_handle_t   mem_handle_
) -> async_start_type
{
  component::IMCAS::async_handle_t async_handle = component::IMCAS::ASYNC_HANDLE_INIT;
  auto status = async_put_direct_offset(pool_, offset_, length_, buffer_, async_handle, rmd_, mem_handle_);
  return {status, async_handle};
}

status_t Connection::async_put_direct_offset(const pool_t                              pool_,
                                                     const std::size_t                         offset_,
                                                     std::size_t &                             length_,
                                                     const void *const                         buffer_,
                                                     IMCAS::async_handle_t &                   out_async_handle_,
                                                     component::Registrar_memory_direct *const rmd_,
                                                     const component::IMCAS::memory_handle_t   mem_handle_)
{
  if(length_ == 0)
    throw API_exception("%s: variant of put_direct_offset called with zero length", __func__);

  API_LOCK();

  if (length_ && !buffer_) {
    PWRN("%s: bad parameter buffer=%p size=%zu", __func__, buffer_, length_);
    return E_BAD_PARAM;
  }

  try {
    out_async_handle_ = put_direct_offset_async(pool_, offset_, buffer_, length_, rmd_,
                                                mem_handle_ == IMCAS::MEMORY_HANDLE_NONE ? nullptr : static_cast<buffer_base *>(mem_handle_)->get_desc());
    return S_OK;
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    throw Logic_exception("%s: network posting failed unexpectedly.", __func__);
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    throw Logic_exception("%s: network posting failed unexpectedly.", __func__);
  }
  return E_FAIL;
}

status_t Connection::erase(const pool_t pool, const string_view_key key)
{
  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg =
      new (iobs->base()) mcas::protocol::Message_IO_request(
        iobs->length(), auth_id(), request_id(), pool, mcas::protocol::OP_TYPE::ERASE, key, 0, 0
      );

    post_recv(&*iobr);
    sync_inject_send(&*iobs, msg, __func__);
    wait_for_completion(iobr->to_context());

    const auto response_msg = msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, __func__);

    status = response_msg->get_status();
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    status = E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    status = E_FAIL;
  }

  return status;
}

status_t Connection::async_erase(const IMCAS::pool_t    pool,
                                         const string_view_key key,
                                         IMCAS::async_handle_t &out_async_handle)
{
  API_LOCK();

  auto iobs = make_iob_ptr_send();
  auto iobr = make_iob_ptr_recv();

  assert(iobs);
  assert(iobr);

  try {
    const auto msg = new (iobs->base()) mcas::protocol::Message_IO_request(iobs->length(),
                                                                           auth_id(),
                                                                           request_id(),
                                                                           pool,
                                                                           mcas::protocol::OP_TYPE::ERASE,
                                                                           key,
                                                                           0, 0);

    iobs->set_length(msg->msg_len());

    /* post both send and receive */
    post_recv(&*iobr);
    post_send({iobs->iov, iobs->iov + 1}, iobs->desc, iobs->to_context(), msg, __func__);

    out_async_handle = new async_buffer_set_simple(debug_level(), std::move(iobs), std::move(iobr));
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    throw Logic_exception("%s: network posting failed unexpectedly.", __func__);
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    throw Logic_exception("%s: network posting failed unexpectedly.", __func__);
  }

  return S_OK;
}

size_t Connection::count(const pool_t pool)
{
  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  try {
    const auto msg =
      new (iobs->base()) mcas::protocol::Message_INFO_request(auth_id(), IKVStore::Attribute::COUNT, pool);

    post_recv(&*iobr);
    sync_inject_send(&*iobs, msg, msg->base_message_size(), __func__);
    wait_for_completion(iobr->to_context());

    const auto response_msg = msg_recv<const mcas::protocol::Message_INFO_response>(&*iobr, __func__);

    return response_msg->value();
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    return 0;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    return 0;
  }
}

status_t Connection::get_attribute(const IKVStore::pool_t    pool,
                                           const IKVStore::Attribute attr,
                                           std::vector<uint64_t> &   out_attr,
                                           const string_view_key key)
{
  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg = new (iobs->base()) mcas::protocol::Message_INFO_request(auth_id(), attr, pool);

    if (key.data()) msg->set_key(iobs->length(), key);

    post_recv(&*iobr);
    sync_inject_send(&*iobs, msg, msg->message_size(), __func__);

    wait_for_completion(iobr->to_context());
    const auto response_msg = msg_recv<const mcas::protocol::Message_INFO_response>(&*iobr, __func__);

    out_attr.clear();
    out_attr.push_back(response_msg->value());
    status = response_msg->get_status();
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    status = E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    status = E_FAIL;
  }
  return status;
}

status_t Connection::get_statistics(IMCAS::Shard_stats &out_stats)
{
  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg =
      new (iobs->base()) mcas::protocol::Message_INFO_request(auth_id(), mcas::protocol::INFO_TYPE_GET_STATS, 0);

    post_recv(&*iobr);
    sync_inject_send(&*iobs, msg, msg->message_size(), __func__);

    wait_for_completion(iobr->to_context());
    const auto response_msg = msg_recv<const mcas::protocol::Message_stats>(&*iobr, __func__);

    status = response_msg->get_status();
#pragma GCC diagnostic push
#if defined __clang__ || 9 <= __GNUC__
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"
#endif
    out_stats = response_msg->stats;
#pragma GCC diagnostic pop
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    status = E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    status = E_FAIL;
  }

  return status;
}

status_t Connection::find(const IMCAS::pool_t pool,
                                  string_view         key_expression,
                                  const offset_t      offset,
                                  offset_t &          out_matched_offset,
                                  std::string &       out_matched_key)
{
  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  const auto iobr = make_iob_ptr_recv();
  assert(iobs);
  assert(iobr);

  status_t status;

  try {
    const auto msg =
      new (iobs->base()) mcas::protocol::Message_INFO_request(auth_id(),
                                                              mcas::protocol::INFO_TYPE_FIND_KEY,
                                                              pool,
                                                              offset);

    /* The key expression concatenates prefix, which is characters, and key bytes */
    msg->set_key(iobs->length(), string_view_key(common::pointer_cast<string_view_key::value_type>(key_expression.data()), key_expression.size()));

    post_recv(&*iobr);
    sync_inject_send(&*iobs, msg, msg->message_size(), __func__);

    wait_for_completion(iobr->to_context());
    const auto response_msg = msg_recv<const mcas::protocol::Message_INFO_response>(&*iobr, "FIND");

    status = response_msg->get_status();

    if (status == S_OK) {
      out_matched_key    = response_msg->c_str();
      out_matched_offset = response_msg->Offset();
    }
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    status = E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    status = E_FAIL;
  }
  return status;
}

status_t Connection::receive_and_process_ado_response(
  const iob_ptr & iobr_
  , std::vector<IMCAS::ADO_response> & out_response_
)
{
  const auto response_msg = msg_recv<const mcas::protocol::Message_ado_response>(&*iobr_, __func__);
  status_t status = response_msg->get_status();

  out_response_.clear();
  if (status == S_OK) {
    /* unmarshal responses */
    for (uint32_t i = 0; i < response_msg->get_response_count(); i++) {
      void *   out_data     = nullptr;
      size_t   out_data_len = 0;
      uint32_t out_layer_id = 0;
      response_msg->client_get_response(i, out_data, out_data_len, out_layer_id);

#if defined DEBUG_NPC_RESPONSES
      if (out_data_len > 0) {
        PLOG("%s: Response:", __func__);
        hexdump(out_data, out_data_len);
      }
      else {
        PLOG("%s: Response (inline): %p", __func__, out_data);
      }
#endif
      out_response_.emplace_back(out_data, out_data_len, out_layer_id);
    }
  }
  else {
    if (response_msg->get_response_count() > 0) {
      void *   err_msg      = nullptr;
      size_t   err_msg_len  = 0;
      uint32_t out_layer_id = 0;
      response_msg->client_get_response(0, err_msg, err_msg_len, out_layer_id);
      PLOG("%s:%u ADO response status %d %.*s", __FILE__, __LINE__, status, int(err_msg_len),
           static_cast<const char *>(err_msg));
      ::free(err_msg);
    }
  }
  return status;
}

template <typename MT>
  status_t Connection::invoke_ado_common(
    const iob_ptr & iobs_
    , const MT *msg_
    , std::vector<IMCAS::ADO_response>& out_response_
    , unsigned int flags
  )
  {
    iobs_->set_length(msg_->message_size());

    if (flags & IMCAS::ADO_FLAG_ASYNC) {
      sync_send(&*iobs_, msg_, __func__);
      /* do not wait for response */
      return S_OK;
    }

    const auto iobr = make_iob_ptr_recv();
    assert(iobr);

    post_recv(&*iobr);
    sync_send(&*iobs_, msg_, __func__);
    wait_for_completion(iobr->to_context()); /* wait for response */

    return receive_and_process_ado_response(iobr, out_response_);
  }

status_t Connection::invoke_ado(const IKVStore::pool_t            pool,
                                        const string_view_key     key,
                                        const string_view_request     request,
                                        const unsigned int                flags,
                                        std::vector<IMCAS::ADO_response>& out_response,
                                        const size_t                      value_size)
{
  API_LOCK();

  const auto iobs = make_iob_ptr_send();
  assert(iobs);

  status_t status;

  try {
    const auto msg = new (iobs->base())
      mcas::protocol::Message_ado_request(iobs->length(),
                                          auth_id(),
                                          request_id(),
                                          pool,
                                          key,
                                          request,
                                          flags,
                                          value_size);
    status = invoke_ado_common(iobs, msg, out_response, flags);
  }
  catch (const Exception &e) {
    PLOG("%s:%u ADO response Exception %s", __FILE__, __LINE__, e.cause());
    status =  E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s:%u ADO response exception %s", __FILE__, __LINE__, e.what());
    status =  E_FAIL;
  }
  return status;
}

status_t Connection::invoke_ado_async(const component::IMCAS::pool_t               pool,
                                              const string_view_key                key,
                                              const string_view_request                request,
                                              const component::IMCAS::ado_flags_t          flags,
                                              std::vector<component::IMCAS::ADO_response>& out_response,
                                              component::IMCAS::async_handle_t &           out_async_handle,
                                              const size_t                                 value_size)
{
  API_LOCK();

  auto iobs = make_iob_ptr_send();
  auto iobr = make_iob_ptr_recv();

  assert(iobs);
  assert(iobr);

  try {
    const auto msg = new (iobs->base()) mcas::protocol::Message_ado_request(iobs->length(),
                                                                            auth_id(),
                                                                            request_id(),
                                                                            pool,
                                                                            key,
                                                                            request,
                                                                            flags,
                                                                            value_size);
    iobs->set_length(msg->message_size());

    post_recv(&*iobr);
    post_send(&*iobs, msg, __func__);

    out_async_handle = new async_buffer_set_invoke(debug_level(), std::move(iobs), std::move(iobr), &out_response);

    return S_OK;
  }
  catch (const Exception &e) {
    PLOG("%s:%u ADO response Exception %s", __FILE__, __LINE__, e.cause());
    return E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s:%u ADO response exception %s", __FILE__, __LINE__, e.what());
    return E_FAIL;
  }
  catch (...) {
    return E_FAIL;
  }
}

status_t Connection::invoke_put_ado(const IKVStore::pool_t            pool,
                                            const string_view_key     key,
                                            const string_view_request     request,
                                            const string_view_value     value,
                                            const size_t                      root_len,
                                            const unsigned int                flags,
                                            std::vector<IMCAS::ADO_response>& out_response)
{
  API_LOCK();

  if (request.size() == 0) return E_INVAL;

  const auto iobs = make_iob_ptr_send();
  assert(iobs);

  status_t status;

  try {
    const auto msg = new (iobs->base()) mcas::protocol::Message_put_ado_request(iobs->length(),
                                                                                auth_id(),
                                                                                request_id(),
                                                                                pool,
                                                                                key,
                                                                                request,
                                                                                value,
                                                                                root_len,
                                                                                flags);

    status = invoke_ado_common(iobs, msg, out_response, flags);
  }
  catch (const Exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.cause());
    status = E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s %s fail %s", __FILE__, __func__, e.what());
    status = E_FAIL;
  }

  return status;
}

status_t Connection::invoke_put_ado_async(const component::IMCAS::pool_t                  pool,
                                                  const string_view_key                   key,
                                                  const string_view_request                   request,
                                                  const string_view_value                   value,
                                                  const size_t                                    root_len,
                                                  const component::IMCAS::ado_flags_t             flags,
                                                  std::vector<component::IMCAS::ADO_response>&    out_response,
                                                  component::IMCAS::async_handle_t&               out_async_handle)
{
  API_LOCK();

  auto iobs = make_iob_ptr_send();
  auto iobr = make_iob_ptr_recv();

  assert(iobs);
  assert(iobr);

  try {
    const auto msg = new (iobs->base()) mcas::protocol::Message_put_ado_request(iobs->length(),
                                                                                auth_id(),
                                                                                request_id(),
                                                                                pool,
                                                                                key,
                                                                                request,
                                                                                value,
                                                                                root_len,
                                                                                flags);
    iobs->set_length(msg->message_size());

    post_recv(&*iobr);
    post_send(&*iobs, msg, __func__);

    out_async_handle = new async_buffer_set_invoke(debug_level(), std::move(iobs), std::move(iobr), &out_response);

    return S_OK;
  }
  catch (const Exception &e) {
    PLOG("%s:%u ADO response Exception %s", __FILE__, __LINE__, e.cause());
    return E_FAIL;
  }
  catch (const std::exception &e) {
    PLOG("%s:%u ADO response exception %s", __FILE__, __LINE__, e.what());
    return E_FAIL;
  }
  catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

auto Connection::make_iob_ptr(buffer_t::completion_t completion_) -> iob_ptr
{
  return iob_ptr(allocate(completion_), this);
}

auto Connection::make_iob_ptr_recv() -> iob_ptr
{
  return make_iob_ptr(recv_complete);
}

auto Connection::make_iob_ptr_send() -> iob_ptr
{
  return make_iob_ptr(send_complete);
}

auto Connection::make_iob_ptr_write() -> iob_ptr
{
  return make_iob_ptr(write_complete);
}

auto Connection::make_iob_ptr_read() -> iob_ptr
{
  return make_iob_ptr(read_complete);
}

#if CW_TEST
	void Connection::run_intermittent_ping_pong(unsigned interval_)
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiv-by-zero"
		if ( interval_ && _ct_w.count() % interval_ == 0 )
#pragma GCC diagnostic pop
		{
			auto iobrp = make_iob_ptr_recv();
			auto iobsp = make_iob_ptr_send();

			/* send "ping" message */
			const auto msg = new (iobsp->base()) protocol::Message_ping(auth_id());
			iobsp->set_length(msg->msg_len());
			post_recv(&*iobrp);
			sync_inject_send(&*iobsp, msg, __func__);
			wait_for_completion(iobrp->to_context()); /* await response */
		}
	}

	void Connection::run_test_count_rdma(common::string_view id_)
	{
		auto t_start = std::chrono::steady_clock::now();

		for ( auto i = _test_data.count(); i != 0; --i )
		{
			run_one_test_element_rdma();
		}
		auto t_duration = std::chrono::steady_clock::now() - t_start;

		auto data_size_total = _test_data.count() * _test_data.size();
		FLOG("test {}: data rate {} bytes in {} seconds {} GB/sec", id_, data_size_total, cw::double_seconds(t_duration), double(data_size_total) / 1e9 / cw::double_seconds(t_duration));
		FLOG("wr {}", _ct_w.out("usec"));
		FLOGM("local {} -> remote {}", _client->write_from_test_local_addr(), _client->write_to_test_remote_addr());
	}

	void Connection::run_intermittent_sleep() const
	{
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdiv-by-zero"
		if ( _test_data.sleep_interval() && _ct_w.count() % _test_data.sleep_interval() == 0 )
#pragma GCC diagnostic pop
		{
			std::this_thread::sleep_for(_test_data.sleep_time());
		}
	}

	void Connection::client_set_vaddr_key(uint64_t vaddr, uint64_t key)
	{
		run_test_count_rdma("DRAM");
		auto old_vaddr_key = _client->set_vaddr_key(vaddr, key);
		run_test_count_rdma("PMEM");
		_client->set_vaddr_key(old_vaddr_key);
		run_test_count_rdma("DRAM again");
		_client->set_vaddr_key(vaddr, key);
		run_test_count_rdma("PMEM again");
	}

	void Connection::run_one_test_element_rdma()
	{
		using namespace cw;
		run_intermittent_ping_pong(_test_data.pre_ping_pong_interval());
		timer t_write;
		// auto st = client.read(fabric_fabric::data_size);
		auto st = _client->write_uninitialized(_test_data.size());
		if ( st != ::S_OK ) { throw std::runtime_error(__func__ + std::string(" failed")); }
		auto tm = cw::double_seconds(t_write.elapsed());
		_ct_w.record(tm);
		if ( 1.0 <= tm )
		{
			FLOGM("local {} -> remote {} delta {}): long write ({}) {} sec", _client->write_from_test_local_addr(), _client->write_to_test_remote_addr(), connection_delta_seconds(), _ct_w.count(), tm);
		}
		run_intermittent_sleep();
		run_intermittent_ping_pong(_test_data.post_ping_pong_interval());
	}

	void Connection::write_real_to_test(void *src_, std::size_t sz_, void *desc_)
	{
		using namespace cw;
		timer t_write;
		// auto st = client.read(fabric_fabric::data_size);
		if ( sz_ > _test_data.size()) { throw std::runtime_error(__func__ + std::string(" test dest too small")); }
		auto st = _client->write_to_test(src_, sz_, desc_);
		if ( st != ::S_OK ) { throw std::runtime_error(__func__ + std::string(" failed")); }
		auto tm = double_seconds(t_write.elapsed());
		_ct_wr_real_to_test.record(tm);
		if ( 1.0 <= tm )
		{
			FLOGM("local {} -> remote {}, delta {}): long write ({}) {} sec", src_, _client->write_to_test_remote_addr(), connection_delta_seconds(), _ct_wr_real_to_test.count(), tm);
		}
	}

	void Connection::write_test_to_real(std::size_t sz_, std::uint64_t vaddr_, std::uint64_t key_)
	{
		using namespace cw;
		timer t_write;
		// auto st = client.read(fabric_fabric::data_size);
		if ( sz_ > _test_data.size()) { throw std::runtime_error(__func__ + std::string(" test dest too small")); }
		auto st = _client->write_from_test(sz_, vaddr_, key_);
		if ( st != ::S_OK ) { throw std::runtime_error(__func__ + std::string(" failed")); }
		auto tm = double_seconds(t_write.elapsed());
		_ct_wr_test_to_real.record(tm);
		if ( 1.0 <= tm )
		{
			FLOG("{} remote {} <- local {} (at {} sec, delta {} ): long write ({}) {} sec"
				, __func__, reinterpret_cast<void *>(vaddr_), _client->write_from_test_local_addr()
				, connection_seconds(), connection_delta_seconds(), _ct_wr_test_to_real.count(), tm
			);
		}
	}

	void Connection::read_test_from_real(std::size_t sz_, std::uint64_t vaddr_, std::uint64_t key_)
	{
		using namespace cw;
		timer t_write;
		// auto st = client.read(fabric_fabric::data_size);
		if ( _test_data.size() < sz_ ) { throw std::runtime_error(__func__ + common::to_string(" test dest  size ", _test_data.size(), " less than read size ", sz_)); }
		auto st = _client->read_to_test(sz_, vaddr_, key_);
		if ( st != ::S_OK ) { throw std::runtime_error(__func__ + std::string(" failed")); }
		auto tm = double_seconds(t_write.elapsed());
		_ct_rd_test_from_real.record(tm);
		FLOG("FLOG TEST one {} two {} three {}", 0, 2.0, "3");
		if ( 1.0 <= tm )
		{
			FLOGM("remote {} <- local {} (at {} sec, delta {}): long write ({}) {} sec", reinterpret_cast<void *>(vaddr_), _client->write_from_test_local_addr(), connection_seconds(), connection_delta_seconds(), _ct_rd_test_from_real.count(), tm);
		}
	}
#endif

int Connection::tick()
{
  using namespace mcas::protocol;

  switch (_state) {
  case INITIALIZE: {
    set_state(HANDSHAKE_SEND);
    break;
  }
  case HANDSHAKE_SEND: {

    const auto iobs = make_iob_ptr_send();
    auto       msg = new (iobs->base()) mcas::protocol::Message_handshake(auth_id(), 1
#if CW_TEST
		, cw::test_data::memory_size()
#endif
      );
    msg->set_status(S_OK);

    /* set security options */
    msg->security_tls_auth = _options.tls;
    PNOTICE("TLS is %s", _options.tls ? "ON" : "OFF");
    iobs->set_length(msg->msg_len());
    post_send({iobs->iov, iobs->iov + 1}, iobs->desc, iobs->to_context(), msg, __func__);

    try {
      wait_for_completion(iobs->to_context());
    }
    catch (...) {
      PERR("%s %s handshake send failed", __FILE__, __func__);
      set_state(STOPPED);
    }

    static int sent = 0;
    sent++;
    PMAJOR(">>> Sent handshake (%d)", sent);

    set_state(HANDSHAKE_GET_RESPONSE);
    break;
  }
  case HANDSHAKE_GET_RESPONSE: {
    const auto iobr = make_iob_ptr_recv();
    post_recv({iobr->iov, iobr->iov + 1}, iobr->desc, iobr->to_context());

    try {
      wait_for_completion(iobr->to_context());
      const auto response_msg = msg_recv<const mcas::protocol::Message_handshake_reply>(&*iobr, "handshake");

#if CW_TEST
				{
					using namespace cw;
					_rm_out =
						std::make_shared<registered_memory>(
							_transport
#if 0
							, _test_data.memory_size()
#else
							, std::make_unique<cw::dram_memory>(_test_data.memory_size())
#endif
							, 0
						);
					_rm_in =
						std::make_shared<registered_memory>(
							_transport
#if 0
							, _test_data.memory_size()
#else
							, std::make_unique<cw::dram_memory>(_test_data.memory_size())
#endif
							, 0
						);

					_client =
						std::make_shared<remote_memory_client_connected>(
							_rm_out
							, _rm_in
							, cvk_type{ _transport, response_msg->vaddr, response_msg->key }
							, quit_option::remain
						);

					/* expect that all max messages are greater than 0, and the same */
					assert(0U < _client->max_message_size());

					// Initial count taken from server side, as that is typically local and easier to alter via env variables or parameters
					// count = common:t_cenv_value<std::size_t>("COUNT", 10000); }
					FLOG("TEST CLIENT vaddr {} key {} size {} count {} pre_ping_pong interval {} post_ping_pong interval {} sleep interval {} sleep time {} sec"
						, reinterpret_cast<void *>(response_msg->vaddr)
						, response_msg->key
						, _test_data.size()
						,  _test_data.count()
						, _test_data.pre_ping_pong_interval()
						, _test_data.post_ping_pong_interval()
						, _test_data.sleep_interval()
						, std::chrono::duration<double>(_test_data.sleep_time()).count()
					);
					FLOGM("local {} -> remote {}"
						, _client->write_from_test_local_addr()
						, _client->write_to_test_remote_addr()
					);
					{
						auto t_start = std::chrono::steady_clock::now();
						while ( _ct_w.count() < _test_data.count() )
						{
							run_one_test_element_rdma();
						}
						auto t_duration = std::chrono::steady_clock::now() - t_start;

						auto data_size_total = _test_data.count() * _test_data.size();
						FLOG("Data rate {} bytes in {} seconds {} GB/sec", data_size_total, double_seconds(t_duration), double(data_size_total) / 1e9 / double_seconds(t_duration));
						FLOG("wr ()", _ct_w.out("usec"));
					}
				}
#endif

      /* server is indicating that it wants to start TLS session */
      if(response_msg->start_tls)
        start_tls();
    }
    catch (...) {
      PERR("%s %s handshake response failed", __FILE__, __func__);
      set_state(STOPPED);
    }

    static int recv = 0;
    recv++;

    set_state(READY);

    _max_message_size = max_message_size(); /* from fabric component */
    break;
  }
  case READY: {
    return 0;
    break;
  }
  case SHUTDOWN: {
    const auto iobs = make_iob_ptr_send();
    auto       msg  = new (iobs->base()) mcas::protocol::Message_close_session(reinterpret_cast<uint64_t>(this));

    iobs->set_length(msg->msg_len());
    post_send({iobs->iov, iobs->iov + 1}, iobs->desc, iobs->to_context(), msg, __func__);

    // server-side may have disappeared
    // try {
    //   wait_for_completion(iobs->to_context());
    // }
    // catch (...) {
    // }

    set_state(STOPPED);
    PLOG("Connection::%s: connection %p shutdown.", __func__, common::p_fmt(this));
    return 0;
  }
  case STOPPED: {
    assert(0);
    return 0;
  }
  }  // end switch

  return 1;
}

using TLS_transport = mcas::client::TLS_transport;

unsigned TLS_transport::debug_level()
{
  return TLS_DEBUG_LEVEL;
}

int TLS_transport::gnutls_pull_timeout_func(gnutls_transport_ptr_t
		, unsigned int // ms
	)
{
  return 0;
}


ssize_t TLS_transport::gnutls_pull_func(gnutls_transport_ptr_t connection, void* buffer, size_t buffer_size)
{
  assert(connection);

  auto p_connection = reinterpret_cast<Connection*>(connection);

  if(p_connection->_tls_buffer.remaining() >= buffer_size) {

    if(debug_level() > 2)
      PLOG("TLS pull: taking %lu bytes from remaining (%lu)", buffer_size, p_connection->_tls_buffer.remaining());

    p_connection->_tls_buffer.pull(buffer, buffer_size);
    return buffer_size;
  }

  auto iobr = p_connection->make_iob_ptr_recv();

  p_connection->post_recv(&*iobr);
  p_connection->wait_for_completion(iobr->to_context()); /* await response */

  void * base_v = iobr->base();
  uint64_t * base = reinterpret_cast<uint64_t*>(base_v);
  uint64_t payload_size = base[0];

  if(debug_level() > 2)
    PLOG("TLS received: iob_len=%lu payload-len=%lu", iobr->length(), payload_size);

  p_connection->_tls_buffer.push(reinterpret_cast<void*>(&base[1]), payload_size);
  p_connection->_tls_buffer.pull(buffer, buffer_size);
  return buffer_size;
}

ssize_t TLS_transport::gnutls_vec_push_func(gnutls_transport_ptr_t connection, const giovec_t * iovec, int iovec_cnt )
{
  assert(connection);
  auto p_connection = reinterpret_cast<Connection*>(connection);
  auto iobs = p_connection->make_iob_ptr_send();

  void * base_v = iobs->base();
  uint64_t * base = reinterpret_cast<uint64_t*>(base_v);

  char * ptr = reinterpret_cast<char*>(&base[1]);
  size_t size = 0;

  for(int i=0; i<iovec_cnt; i++) {
    memcpy(ptr, iovec[i].iov_base, iovec[i].iov_len);
    size += iovec[i].iov_len;
    ptr += iovec[i].iov_len;
  }

  base[0] = size; /* prefix with length */
  iobs->set_length(size + sizeof(uint64_t));

  p_connection->sync_send(&*iobs, "TLS packet (client send)", __func__);

  if(debug_level() > 2)
    PLOG("TLS sent: %lu bytes (%p)", size, reinterpret_cast<void*>(&*iobs));

  return size;
}

void Connection::start_tls()
{
  if(_options.tls == false) throw Logic_exception("TLS contradiction");

  if(gnutls_global_init() != GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_global_init() failed");

  if (gnutls_certificate_allocate_credentials(&_xcred) != GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_certificate_allocate_credentials() failed");

  std::string cert_file(::getenv(ENVIRONMENT_VARIABLE_CERT));
  std::string key_file(::getenv(ENVIRONMENT_VARIABLE_KEY));

  PLOG("start_tls: cert=%s key=%s", cert_file.c_str(), key_file.c_str());

  if (gnutls_certificate_set_x509_key_file(_xcred, cert_file.c_str(), key_file.c_str(), GNUTLS_X509_FMT_PEM) !=
      GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_certificate_set_x509_key_file() failed");

  if (gnutls_init(&_session, GNUTLS_CLIENT) != GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_init() failed");

  if (gnutls_priority_init(&_priority, cipher_suite, NULL) != GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_priority_init() failed");

  if (gnutls_priority_set(_session, _priority) != GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_priority_set() failed");

  if (gnutls_credentials_set(_session, GNUTLS_CRD_CERTIFICATE, _xcred) != GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_credentials_set() failed");

  //  gnutls_handshake_set_timeout(_session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

  /* hook in TLS transport to use our RDMA connection */
  gnutls_transport_set_ptr(_session, this);
  gnutls_transport_set_vec_push_function(_session, TLS_transport::gnutls_vec_push_func);
  gnutls_transport_set_pull_function(_session, TLS_transport::gnutls_pull_func);
  gnutls_transport_set_pull_timeout_function(_session, TLS_transport::gnutls_pull_timeout_func);

  /* initiate handshake */
  int rc;
  if ((rc = gnutls_handshake(_session)) < 0) {
    if (rc == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR) {

      PERR("TLS certificate verification error");
      /* check certificate verification status */
      gnutls_datum_t out;
      auto           type   = gnutls_certificate_type_get(_session);
      auto           status = gnutls_session_get_verify_cert_status(_session);
      if (gnutls_certificate_verification_status_print(status, type, &out, 0) != GNUTLS_E_SUCCESS)
        throw General_exception("gnutls_certificate_verification_status_print() failed");

      gnutls_deinit(_session);
      gnutls_free(out.data);
    }
    throw General_exception("Client: handshake failed: %s\n", gnutls_strerror(rc));
  }

  /* get final result */
  gnutls_alert_description_t result;
  gnutls_record_recv(_session, &result, sizeof(result));

  if(result == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR)
    throw API_exception("server rejected certificate because verification failed");
  else if(result > 0)
    throw API_exception("TLS handshake rejected");

  PLOG("TLS handshake complete");
}
