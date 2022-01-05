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

#ifndef MCAS_CLIENT_ASYNC_BUFFER_SET_PUT_DIRECT_OFFSET_H
#define MCAS_CLIENT_ASYNC_BUFFER_SET_PUT_DIRECT_OFFSET_H

#include "async_buffer_set_t.h"

#include "connection.h"
#include "memory_registered.h"
#include "protocol.h"

#include <common/delete_copy.h>
#include <common/logging.h>
#include <common/perf/tm.h>
#include <cstddef>
#include <vector>

struct async_buffer_set_put_direct_offset
	: public async_buffer_set_t
	, public ::memory_registered {
private:
	using Message_IO_request = mcas::protocol::Message_IO_request;
	using Message_IO_response = mcas::protocol::Message_IO_response;
	using OP_TYPE = mcas::protocol::OP_TYPE;
	using locate_element                               = Message_IO_response::locate_element;
	iob_ptr                                     _iobrd;
	iob_ptr                                     _iobs2;
	iob_ptr                                     _iobr2;
	component::IMCAS::pool_t                    _pool;
	std::uint64_t                               _auth_id;
	std::size_t                                 _offset;
	const char *                                _buffer;
	std::size_t &                               _length;
	std::uint64_t                               _key;
	void *                                      _desc[1];
	::iovec                                     _v[1];
	std::vector<locate_element>                 _addr_list;
	std::vector<locate_element>::const_iterator _addr_cursor;

public:
	async_buffer_set_put_direct_offset(TM_ACTUAL unsigned                 debug_level_,
		Registrar_memory_direct *rmd_,
		iob_ptr &&               iobs_,
		iob_ptr &&               iobr_,
		iob_ptr &&               iobrd_,
		iob_ptr &&               iobs2_,
		iob_ptr &&               iobr2_,
		component::IMCAS::pool_t pool_,
		std::uint64_t            auth_id_,
		std::size_t              offset_,
		const void *             buffer_,
		std::size_t &            length_,
		void *                   desc_)
	: async_buffer_set_t(debug_level_, std::move(iobs_), std::move(iobr_))
	, memory_registered(TM_REF rmd_,
			mcas::range<char *>(static_cast<char *>(const_cast<void *>(buffer_)),
						              static_cast<char *>(const_cast<void *>(buffer_)) + length_)
			.round_inclusive(4096),
			desc_),
		_iobrd(std::move(iobrd_)),
		_iobs2(std::move(iobs2_)),
		_iobr2(std::move(iobr2_)),
		_pool(pool_),
		_auth_id{auth_id_},
		_offset{offset_},
		_buffer(static_cast<const char *>(buffer_)),
		_length(length_),
		_key{},
		_desc{this->desc()}  // provided by M
	,
		_v{},
		_addr_list{},
		_addr_cursor{}
	{
		CFLOGM(2, "iobs2 {} iobr2 {}"
			, common::p_fmt(&*_iobs2)
			, common::p_fmt(&*_iobr2)
		);
	}
	DELETE_COPY(async_buffer_set_put_direct_offset);
	int                         move_along(TM_ACTUAL Connection *c) override
	{
		TM_SCOPE(async_buffer_set_put_direct_offset)
		if (iobs) { /* check submission, clear and free on completion */
			if (c->test_completion(iobs->to_context()) == false) {
				return E_BUSY;
			}
			/* What to do when first send completes */
			iobs.reset(nullptr);
			/* End */
		}

		if (iobr) { /* check recv, clear and free on completion */
			if (c->test_completion(iobr->to_context()) == false) {
				return E_BUSY;
			}
			/* What to do when first recv completes */
			const auto response = c->msg_recv<const Message_IO_response>(&*iobr, "ASYNC OP_LOCATE");
			auto status = response->get_status();

			{
				auto cursor = response->edata();
				_addr_list  = std::vector<locate_element>(cursor, cursor + response->element_count());
				CFLOGM(2,
					"edata count {} {} to {}"
					, response->element_count()
					, common::p_fmt(cursor)
					, common::p_fmt(cursor + response->element_count())
				);
				_length = 0;
				for (const auto &e : _addr_list) {
					_length += e.len;
					CFLOGM(2,
						"addr 0x{:x} len 0x{:x}"
						, e.addr, e.len
					);
				}
			}

			_key         = response->key;
			_addr_cursor = _addr_list.begin();

			iobr.reset(nullptr);

			return status == S_OK ? E_BUSY : status;
		}

		if ( _addr_list.empty() )
			{
				return S_OK;
			}

		if (_addr_cursor != _addr_list.end()) {
			if (_iobrd && !c->test_completion(_iobrd->to_context())) {
				return E_BUSY;
			}

			_iobrd = c->make_iob_ptr_write();
			CFLOGM(2, "iobrd {}", common::p_fmt(&*_iobrd));

			/* reply received, with credentials for the DMA */
			_v[0] = ::iovec{const_cast<char *>(_buffer), _addr_cursor->len};

			CFLOGM(2,
				"post_write {} local (addr {}.{:x} desc {}) -> (_addr 0x{:x}, key 0x{:x})"
				, common::p_fmt(&*_iobrd)
				, _v[0].iov_base, _v[0].iov_len
				, _desc[0]
				, _addr_cursor->addr
				, _key
			);

			c->post_write(_v, std::begin(_desc), _addr_cursor->addr, _key, _iobrd->to_context());
			_buffer += _addr_cursor->len;
			++_addr_cursor;
			/* End */
		}

		if (_iobrd) {
			if (!c->test_completion(_iobrd->to_context())) {
				return E_BUSY;
			}
			/* What to do when DMA completes */
			/* DMA done. Might need another DMA */
			CFLOGM(2
				, "dma write complete {}"
				, common::p_fmt(&*_iobrd)
			);

			_iobrd.reset(nullptr);
			/* DMA is complete. Issue OP_RELEASE */

			/* send release message */
			const auto msg =
				new (_iobs2->base())
					Message_IO_request(
						_auth_id, c->request_id(), _pool, OP_TYPE::RELEASE_WITH_FLUSH, _offset, _length
					);

			c->post_recv(&*_iobr2);
			c->sync_inject_send(&*_iobs2, msg, __func__);
			/* End */
		}

		if ( _iobr2 ) {
			if ( ! c->test_completion(&*_iobr2->to_context()) ) {
				return E_BUSY;
			}
			/* What to do when second recv completes */
			const auto response_msg = c->msg_recv<const Message_IO_response>(&*_iobr2, "OP_RELEASE_WITH_FLUSH,");
			return response_msg->get_status();
			/* End */
		}
		else {
			throw API_exception("invalid async handle, task already completed?");
		}
	}
};

#endif
