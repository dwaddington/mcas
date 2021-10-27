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

#include <cw/cw_fabric_test.h>
#include <cw/cw_common.h>
#include <cw/test_data.h>

#include <api/components.h>
#include <api/fabric_itf.h> /* IFabric, IFabric_server_factory, IFabric_server, IFabric_endpoint_unconnected_server */

#include <common/fd_locked.h>
#include <common/logging.h>
#include <common/moveable_ptr.h>

#include <boost/core/noncopyable.hpp>
#include <nupm/range_manager_impl.h>
#include <nupm/space_opened.h>
#include <gsl/pointers>

#include <fcntl.h> /* O_RDWR */
#include <sys/mman.h> /* MAP_LOCKED */
#include <sys/uio.h> /* iovec */

#include <cstddef> /* size_t */
#include <cstdint> /* uint16_t, uint64_t */
#include <exception>
#include <functional> /* ref */
#include <future>
#include <memory> /* make_shared, shared_ptr */
#include <string>
#include <vector>

struct uc_server_dtor
{
	/* an unconnected server cannot be default-deleted, as the fabric listener,
	 * on another thread, may issue virtual function calls against it.
	 * Provide a deleter with a guard.
	 */
	void operator()(component::IFabric_endpoint_unconnected_server *s)
	{
		s->remove();
	}
};

struct server_connection
{
private:
	gsl::not_null<component::IFabric_server_factory *> _f;
	std::unique_ptr<component::IFabric_endpoint_unconnected_server, uc_server_dtor> _ep;
	common::moveable_ptr<component::IFabric_server> _cnxn;
public:
	component::IFabric_server &cnxn() const { return *_cnxn; }
	explicit server_connection(component::IFabric_server_factory &ep);
	server_connection(const server_connection &) = delete;
	server_connection& operator=(const server_connection &) = delete;
	server_connection(server_connection &&) noexcept = default;
	server_connection& operator=(server_connection &&) noexcept = default;
	/* The presence of a destructor and a pointer member causes -Weffc++ to warn
	 *
	 * warning: ‘class d’ has pointer data members [-Weffc++]
	 * warning:   but does not override ‘d(const d&)’ [-Weffc++]
	 * warning:   or ‘operator=(const d&)’ [-Weffc++]
	 *
	 * g++ should not warn in this case, because the declarataion of a move constructor suppresses
	 * default copy constructor and operator=.
	 */
	~server_connection();
};

namespace
{
	gsl::not_null<component::IFabric_endpoint_unconnected_server *> get_ep(gsl::not_null<component::IFabric_server_factory *> f_)
	{
		FLOG("");
		component::IFabric_endpoint_unconnected_server *ep = nullptr;
		while ( ! ( ep = f_->get_new_endpoint_unconnected() ) ) {}
		return gsl::not_null<component::IFabric_endpoint_unconnected_server *>(ep);
	}
}

server_connection::server_connection(component::IFabric_server_factory &f_)
	: _f(&f_)
	, _ep(get_ep(_f))
	, _cnxn(_f->open_connection(_ep.get()))
{
	FLOG("");
}

server_connection::~server_connection()
{
	if ( _cnxn )
	{
		try
		{
			_f->close_connection(_cnxn);
		}
		catch ( std::exception &e )
		{
			FLOGM("exception: {}", e.what());
		}
	}
	FLOG("");
}

/*
 * A component::IFabric_server_factory, which will support clients until one
 * of them closes with the "quit" flag set.
 */
struct cw_remote_memory_server
	: private boost::noncopyable
{
private:
	std::shared_ptr<component::IFabric_server_factory> _fa;
	std::future<void> _th;

	void listener(
		component::IFabric_server_factory &ep
		, std::function<bool(component::IFabric_server &)> run
	);

public:
	cw_remote_memory_server(
		component::IFabric &fabric
		, const std::string &fabric_spec
		, std::uint16_t control_port
		, const char *
		, std::function<bool(component::IFabric_server &)> run
	);

	~cw_remote_memory_server();
	std::size_t max_message_size() const;
};

void cw_remote_memory_server::listener(
	component::IFabric_server_factory &fa_
	, std::function<bool(component::IFabric_server &)> run_
)
{
	auto quit = false;
	while ( ! quit )
	{
		/* Block until a client requests a connection */
		server_connection sc(fa_);
		assert(sc.cnxn().max_message_size() == this->max_message_size());
		/* run the server code */
		quit = run_(sc.cnxn());
	}
}

cw_remote_memory_server::cw_remote_memory_server(
	component::IFabric &fabric_
	, const std::string &fabric_spec_
	, std::uint16_t control_port_
	, const char *
	, std::function<bool(component::IFabric_server &)> run_
)
	: _fa(fabric_.open_server_factory(fabric_spec_, control_port_))
	, _th(
			std::async(
				std::launch::async
				, &cw_remote_memory_server::listener
				, this
				, std::ref(*_fa)
				, run_
			)
		)
{
}

cw_remote_memory_server::~cw_remote_memory_server()
{
	try
	{
		_th.get();
	}
	catch ( std::exception &e )
	{
		FLOGM("exception: {}", e.what());
	}
}

std::size_t cw_remote_memory_server::max_message_size() const
{
	return _fa->max_message_size();
}

struct simple_range_manager final
	: private common::log_source
	, public nupm::range_manager
{
private:
	std::unique_ptr<range_manager::byte_interval_set> _address_coverage;
	std::unique_ptr<range_manager::byte_interval_set> _address_fs_available;
public:
	explicit simple_range_manager()
		: common::log_source(3)
		, _address_coverage(std::make_unique<byte_interval_set>())
		, _address_fs_available(std::make_unique<byte_interval_set>())
	{}
	bool interferes(byte_interval coverage) const override;
	void add_coverage(byte_interval_set coverage) override;
	void remove_coverage(byte_interval coverage) override;
	void *locate_free_address_range(std::size_t size) const override;
	common::log_source &ls() { return *this; }
};

namespace
{
	bool init_have_odp()
	{
		/* env variable USE_ODP to indicate On Demand Paging */
		char* p = ::getenv("USE_ODP");
		bool odp = true;
		if ( p != nullptr )
		{
			errno = 0;
			odp = bool(std::strtoul(p,nullptr,0));

			auto e = errno;
			if ( e == 0 )
			{
				FLOG("USE_ODP={} ({} on-demand paging)", odp, odp ? "using" : "not using");
			}
			else
			{
				FLOG("USE_ODP specification {} failed to parse: {}", p, ::strerror(e));
			}
		}
		return odp;
	}

	int init_map_lock_mask(const bool have_odp_)
	{
		/* On Demand Paging implies that mapped memory need not be pinned */
		return have_odp_ ? 0 : MAP_LOCKED;
	}
}

int main(int, const char *argv[])
{
	/* server invocation:
	 *   fabric-test1
	 * client in invocation:
	 *   fabric-test1 10.0.0.91
	 */

	fabric_component fc;
	fabric_factory fa(fc);
	fabric_fabric ff(fa);

	(void)argv;
	assert( ! argv[1] );

	FLOG("SERVER begin port {}", fabric_fabric::control_port);

	auto remote_key_base = 0U;

#if 0
	struct log_source_simple
		: common::log_source
	{
		log_source_simple()
			: common::log_source(2)
		{
		}
	};
	struct range_manager_simple
		: log_source_simple
		, nupm::range_manager_impl
	{
		range_manager_simple()
			: log_source_simple()
			, nupm::range_manager_impl(*this)
		{}
	};
	log_source_simple ls;
#endif

	auto base = 0x9000000000;
	common::byte_span sp = common::make_byte_span(reinterpret_cast<void *>(base), 0x1000000000);
	nupm::range_manager_impl rem(common::log_source(2), sp);

	const bool have_odp = init_have_odp();
	const int effective_map_locked = init_map_lock_mask(have_odp);

	cw_remote_memory_server server(
		*ff.fabric()
		, empty_object_json.str()
		, fabric_fabric::control_port
		, ""
		, [&remote_key_base, &rem, base, effective_map_locked] (component::IFabric_server &srv_) -> bool
		{
			try {
#if 0
#else
				std::string p("/dev/dax0.0");
#endif
				bool quit = false;
				/* register a local memory region for RDMA */
				cw::registered_memory rm{
					static_cast<component::IFabric_memory_control *>(&srv_)
#if 0
					, std::make_unique<cw::dram_memory>(cw::test_data::memory_size())
#else
					, std::make_unique<cw::pmem_memory>(
						nupm::space_opened(
							common::log_source(2)
							, &rem
							, common::fd_locked(p.c_str(), O_RDWR, 0666)
							, base
							, effective_map_locked
						)
					)
#endif

					, remote_key_base
				};

				/* Set a receive buffer. We will get one message, when the client is done with rm */
				::iovec v[1] = { ::iovec{&rm[0], 1} };
				cw::remote_memory_accessor ctxt0{};
				srv_.post_recv(v, &ctxt0);
				/* The server sends the first message, containing the client address and key to memory */
				cw::remote_memory_accessor ctxt1{};
				ctxt1.send_memory_info(srv_, rm);
				/* Wait for client indicate exit (by sending one byte to us) */
				cw::wait_poll(
					srv_
					, [&quit, &rm, ctxt0] (void *ctxt_, ::status_t stat_, std::uint64_t, std::size_t len_, void *) -> void
						{
							(void)ctxt_;
							(void)stat_;
							(void)len_;
#if 0
							assert(ctxt_ == ctxt0);
#endif
							assert(stat_ == S_OK);
							assert(len_ == 1);
							/* did client leave with the "quit byte" set to 'q'? */
							quit = char(rm[0]) == char(cw::quit_option::do_quit);
						}
				);
				++remote_key_base;
				return quit;
			}
			catch ( std::exception &e )
			{
				FLOG("exception: {}", e.what());
				throw;
			}
		}
	);

	FLOG("{}", "SERVER end");

	return 0;
}
