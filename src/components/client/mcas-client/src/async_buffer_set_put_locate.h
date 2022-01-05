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

#ifndef MCAS_ASYNC_BUFFER_SET_PUT_LOCATE_H
#define MCAS_ASYNC_BUFFER_SET_PUT_LOCATE_H

#include "async_buffer_set_t.h"
#include "mr_many.h"

namespace
{
	/* Wanted: a generator which takes a range r and a function f and returns
	 * values of f(r). Probably in boost ...
	 */
	std::vector<mcas::range<char *>> make_rounded_range_vector(gsl::span<const common::const_byte_span> values_, std::size_t round_)
	{
		std::vector<mcas::range<char *>> v;
		v.reserve(values_.size());
		for ( std::size_t i = 0; i != values_.size(); ++i )
		{
			v.emplace_back(
				static_cast<char *>(const_cast<void *>(::base(values_[i])))
				, static_cast<char *>(const_cast<void *>(::end(values_[i])))
			);
			v.back().round_inclusive(round_);
		}
		return v;
	}
}

#include <chrono>
#include "wait_poll.h"
struct async_buffer_set_put_locate
	: public async_buffer_set_t
	, private mr_many
	, private ::fi_context2 /* context for the RDMA write */
{
private:
	iob_ptr                      _iobs2;
	iob_ptr                      _iobr2;
	component::IMCAS::pool_t     _pool;
	std::uint64_t                _auth_id;
	std::vector<void *>          _desc;
	std::vector<::iovec>         _v;
	std::uint64_t                _addr;
	std::uint64_t                _key;
	unsigned _state;
#if CW_TEST
	static constexpr unsigned _pass_count = 1;
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
		auto rmc = static_cast<async_buffer_set_put_locate *>(ctxt_);
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

	void wait_complete(Connection *cnxn)
	{
		::wait_poll(
			cnxn->transport()
			, [this] (::fi_context2 *ctxt_, ::status_t stat_, std::uint64_t, std::size_t len_, void *)
				{
					check_complete_static(this, ctxt_, stat_, len_);
				}
		);
		if ( _last_stat != ::S_OK )
		{
			FLOGM("{}", _last_stat);
		}
	}

public:
	async_buffer_set_put_locate(
		TM_ACTUAL
		unsigned                 debug_level_
		, Registrar_memory_direct *rmd_
		, iob_ptr &&               iobs_
		, iob_ptr &&               iobr_
		, iob_ptr &&               iobs2_
		, iob_ptr &&               iobr2_
		, component::IMCAS::pool_t pool_
		, std::uint64_t            auth_id_
		, gsl::span<const common::const_byte_span> values_
		, gsl::span<const component::IKVStore::memory_handle_t> handles_
	)
		: async_buffer_set_t(debug_level_, std::move(iobs_), std::move(iobr_))
		, mr_many(
			TM_REF rmd_
			, make_rounded_range_vector(values_, 4096)
			, handles_
		)
		, ::fi_context2()
		, _iobs2(std::move(iobs2_))
		, _iobr2(std::move(iobr2_))
		, _pool{pool_}
		, _auth_id{auth_id_}
		, _desc()
		, _v()
		, _addr{}
		, _key{}
		, _state(0)
#if CW_TEST
		, _pass_countdown(_pass_count)
#endif
		, _last_stat(::S_OK)
		, _start{}
	{
		_v.reserve(values_.size());
		_desc.reserve(values_.size());
		CPLOG(2, "%s: iobrd %p iobs2 %p iobr2 %p"
			, __func__
			, common::p_fmt(to_context())
			, common::p_fmt(&*_iobs2)
			, common::p_fmt(&*_iobr2)
		);
		for ( std::size_t i = 0; i != values_.size(); ++i )
		{
			/* There is no intention to modify the source, but libfabric uses spans
			 * of iovec (which do not have a "const ptr" version).
			 */
			_v.emplace_back(::iovec{const_cast<void *>(::base(values_[i])), ::size(values_[i])});
			_desc.emplace_back(this->at(i).desc());
		}
	}
	DELETE_COPY(async_buffer_set_put_locate);

	int move_along(TM_ACTUAL Connection *c) override
	{
		TM_SCOPE(async_buffer_set_put_locate)
		switch ( _state )
		{
		case 0: /* check submission, clear and free on completion */
			if ( c->test_completion(iobs->to_context()) )
			{
				/* What to do when first send completes */
				iobs.reset(nullptr);
				/* End */
				_state = 1;
			}
			break;
		case 1:
			{
				TM_SCOPE(async_buffer_set_put_locate_recv1)
				if ( c->test_completion(iobr->to_context()) )
				{
					/* What to do when first recv completes */
					const auto response_msg = c->msg_recv<const mcas::protocol::Message_IO_response>(&*iobr, "ASYNC PUT_LOCATE");
					auto status = response_msg->get_status();

					_addr = response_msg->addr;
					_key = response_msg->key;
					iobr.reset(nullptr);
					if (status != S_OK) {
						return status;
					}
					/* reply have been received, with credentials for the DMA */

					CPLOG(2,
						"%s post_write %p -> (_addr 0x%zx, key 0x%zx)"
						  , __func__
						  , common::p_fmt(to_context())
						  , _addr, _key
					);
					for ( std::size_t i = 0; i != _v.size(); ++i )
					{
						 CPLOG(2,
							  "%s post_write local (addr %p.%zx desc %p)"
							  , __func__
							  , _v[i].iov_base, _v[i].iov_len
							  , _desc[i]
						  );
					}
#if CW_TEST
					/* Try two parts of the write: real local to test remote, test local to real remote */
					c->write_real_to_test(_v[0].iov_base, _v[0].iov_len, _desc[0]); /* write local real data to remote test buffer */
					c->write_test_to_real(_v[0].iov_len, _addr, _key); /* write local test buffer to remote real destination */
#endif
					c->post_write(_v, _desc.data(), _addr, _key, to_context());
#if CW_TEST
					_start = clock_type::now();
#endif
					wait_complete(c);
					/* End */
					_state = 2;
				}
			}
			break;
		case 2:
			{
				TM_SCOPE(async_buffer_set_put_locate_dma)
				/* What to do when DMA completes */
				//CPLOG(0, "%s dma complete %p %f", __func__, common::p_fmt(this), std::chrono::duration_cast<double>(clock_type::now() - _start).count());
#if CW_TEST
				auto du = std::chrono::duration<double>(clock_type::now() - _start).count();
				if ( 1.0 < du )
				{
					PLOG("%s at %f sec delta %f sec: long RDMA %p %f sec %u/%u", __func__, c->connection_seconds(), c->connection_delta_seconds(), common::p_fmt(this), du, _pass_count - _pass_countdown, _pass_count);
				}
				/* DMA is complete. Issue PUT_RELEASE */
				if ( --_pass_countdown != 0 )
				{
					/* testing: run another RDMA */
					c->post_write(_v, _desc.data(), _addr, _key, to_context());
					_start = clock_type::now();
					wait_complete(c);
					c->run_one_test_element_rdma();
				}
				else
#endif
				{
					/* send release message */
					const auto msg = new (_iobs2->base())
					mcas::protocol::Message_IO_request(_auth_id, c->request_id(), _pool, mcas::protocol::OP_TYPE::PUT_RELEASE, _addr);

					c->post_recv(&*_iobr2);
					c->sync_inject_send(&*_iobs2, msg, __func__);
					_state = 3;
				}
				/* End */
			}
			break;
		case 3:
			{
				TM_SCOPE(async_buffer_set_put_locate_recv2)
				if (!c->test_completion(&*_iobr2->to_context())) {
					return E_BUSY;
				}
				/* What to do when second recv completes */
				const auto response_msg = c->msg_recv<const mcas::protocol::Message_IO_response>(&*_iobr2, "ASYNC PUT_RELEASE");
				auto status = response_msg->get_status();

				_iobr2.reset(nullptr);
				return status;
				/* End */
			}
			break;

		default:
			throw API_exception("invalid async handle, task already completed?");
		}
		return E_BUSY;
	}
};

#endif
