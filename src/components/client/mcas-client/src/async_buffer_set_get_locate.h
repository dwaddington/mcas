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

#ifndef MCAS_ASYNC_BUFFER_SET_GET_LOCATE_H
#define MCAS_ASYNC_BUFFER_SET_GET_LOCATE_H

#include "async_buffer_set_t.h"
#include "memory_registered.h"
#include "protocol.h"
#include "range.h"

#include <chrono>
#include <common/type_name.h>
#include "wait_poll.h"
struct async_buffer_set_get_locate
	: public async_buffer_set_t
	, public memory_registered
	, private ::fi_context2 /* context for the RDMA read */
{
private:
	component::IMCAS::pool_t     _pool;
	std::uint64_t                _auth_id;
	void *                       _value;
	std::size_t &                _value_len;
	void *                       _desc[1];
	::iovec                      _v[1];
	std::uint64_t                _addr;
#if 1 /* needed only for testing (multiple RDMAs per request) */
	std::uint64_t                _key;
#endif
	unsigned _state;
#if CW_TEST
	static constexpr unsigned _pass_count = 100;
	unsigned _pass_countdown;
#endif
	status_t _last_stat;
	using clock_type = std::chrono::high_resolution_clock;
	clock_type::time_point _start;
	::fi_context2 *to_context() { return this; }

	static void check_complete_static(void *t_, ::fi_context2 *ctxt_, ::status_t stat_, std::size_t len_)
	try
	{
		/* The callback context must be the object which was polling. */
		auto rmc = static_cast<async_buffer_set_get_locate *>(ctxt_);
		assert(t_ == rmc);
		assert(rmc);
		rmc->check_complete(stat_, len_);
	}
	catch ( std::exception &e )
	{
		FLOG("{}", e.what());
	}

	void check_complete(::status_t stat_, std::size_t)
	{
		_last_stat = stat_;
	}

#if 0
	void wait_complete(Connection *cnxn)
	{
PLOG("%s enter", __func__);
		::wait_poll(
			cnxn->transport()
			, [this] (::fi_context2 *ctxt_, ::status_t stat_, std::uint64_t, std::size_t len_, void *)
				{
					check_complete_static(this, ctxt_, stat_, len_);
				}
		);
PLOG("%s done", __func__);
		if ( _last_stat != ::S_OK )
		{
			FLOGM("{}", _last_stat);
		}
	}
#endif
public:
	async_buffer_set_get_locate(TM_ACTUAL unsigned debug_level_,
		Registrar_memory_direct *rmd_,
		iob_ptr &&               iobs_,
		iob_ptr &&               iobr_,
		component::IMCAS::pool_t pool_,
		std::uint64_t            auth_id_,
		void *                   value_,
		std::size_t &            value_len_, // In: buffer size of value_. Out: actual KV store value size
		Connection       *c,
		void *                   desc_,
		std::uint64_t            addr_,
		std::uint64_t            key_
	)
		: async_buffer_set_t(debug_level_, std::move(iobs_), std::move(iobr_))
		, memory_registered(TM_REF rmd_,
			mcas::range<char *>(static_cast<char *>(value_), static_cast<char *>(value_) + value_len_)
				.round_inclusive(4096),
			desc_)
		, ::fi_context2()
		, _pool(pool_)
		, _auth_id(auth_id_)
		, _value(value_)
		, _value_len(value_len_)
		, _desc{this->desc()}  // provided by M
		, _v{::iovec{_value, _value_len}}
		, _addr(addr_)
		, _key(key_)
		, _state(0)
#if CW_TEST
		, _pass_countdown(_pass_count)
#endif
		, _last_stat(S_OK)
		, _start{}
	{
		CPLOG(3, "%s state %d", __func__, _state);
		TM_SCOPE()
		CPLOG(2, "%s: this %p iobs2 %p iobr2 %p"
			, __func__
			, common::p_fmt(this)
			, common::p_fmt(&*iobs)
			, common::p_fmt(&*iobr)
		);
		/* reply have been received, with credentials for the DMA */

		CFLOGM(2,
			"post_read {} local (addr {}.{:x} desc {}) <- (_addr 0x{:x}, key 0x{:x})"
			, common::p_fmt(this)
			, _v[0].iov_base, _v[0].iov_len
			, _desc[0]
			, _addr, key_
		);
		c->post_read(_v, std::begin(_desc), _addr, key_, this->to_context());
		_start = clock_type::now();
#if 0
		wait_complete(c);
#endif
		/* End */
	}

	DELETE_COPY(async_buffer_set_get_locate);

	int move_along(TM_ACTUAL Connection *c) override
	{
		TM_SCOPE(async_buffer_set_get_locate)
		switch ( _state )
		{
		case 0:
			if ( c->test_completion(this->to_context()) )
			{
#if CW_TEST
				auto du = std::chrono::duration<double>(clock_type::now() - _start).count();
				if ( 2.0 < du )
				{
					PLOG("%s dma complete %p %f %u/%u", __func__, common::p_fmt(this), du, _pass_count - _pass_countdown, _pass_count);
				}
				if ( --_pass_countdown != 0 )
					/* DMA is complete. Issue PUT_RELEASE */
				{
					CPLOG(3, "%s state %d count %d", __func__, _state, _pass_countdown);
					/* testing: run another RDMA */
					c->post_read(_v, std::begin(_desc), _addr, _key, to_context());
					_start = clock_type::now();
#if 0
					wait_complete(c);
#endif
				}
				else
#endif
				{
					/* DMA is complete. Issue PUT_RELEASE */

					/* send release message */
					const auto msg = new (iobs->base())
					mcas::protocol::Message_IO_request(_auth_id, c->request_id(), _pool, mcas::protocol::OP_TYPE::GET_RELEASE, _addr);

					c->post_recv(&*iobr);
					c->sync_inject_send(&*iobs, msg, __func__);
					_state = 1;
					CPLOG(3, "%s state %d", __func__, _state);
					/* End */
				}
			}
			break;
		case 1:
			if ( c->test_completion(iobr->to_context()) )
			{
				/* What to do when second recv completes */
				const auto response_msg = c->msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, "ASYNC GET_RELEASE");
				auto status = response_msg->get_status();

				iobr.reset(nullptr);
				return status;
				/* End */
			}
			break;
		default:
			throw API_exception("invalid state, task already completed?");
		}
		return E_BUSY;
	}
};

#endif
