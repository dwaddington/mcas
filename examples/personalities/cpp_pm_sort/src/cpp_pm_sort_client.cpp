/*
 * Description  : PM sorting ADO client
 * Authors      : Omri Arad, Yoav Ben Shimon, Ron Zadicario
 * Authors email: omriarad3@gmail.com, yoavbenshimon@gmail.com, ronzadi@gmail.com
 * License      : Apache License, Version 2.0
 * Done as part of the Workshop on Search Data Structures for High-Performance Databases
 * at Tel-Aviv University.
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"

#include "cpp_pm_sort_proto_generated.h"
#include "cpp_pm_sort_client.h"
#include "cpp_pm_sort_plugin.h"
#include <api/mcas_itf.h>
#include <api/components.h>
#include <common/dump_utils.h>

using namespace flatbuffers;
using namespace cpp_pm_sort_protocol;

static unsigned long g_transaction_id = 0;

namespace cpp_pm_sort
{

Client::Client(const unsigned debug_level,
	       unsigned patience,
	       const std::string& addr_with_port,
	       const std::string& nic_device)
{
	using namespace component;

	auto dll = load_component("libcomponent-mcasclient.so", mcas_client_factory);
	auto factory = dll->query_interface<IMCAS_factory>();
	_mcas = factory->mcas_create(debug_level, patience, getlogin(), addr_with_port, nic_device);
	factory->release_ref();
}

pool_t Client::create_pool(const std::string& pool_name,
			   const size_t size,
			   const size_t expected_obj_count)
{
	assert(_mcas);
	return _mcas->create_pool(pool_name, size, 0, expected_obj_count);
}

pool_t Client::open_pool(const std::string& pool_name,
			 bool read_only)
{
	assert(_mcas);
	return _mcas->open_pool(pool_name, read_only ? component::IKVStore::FLAGS_READ_ONLY : 0);
}

status_t Client::close_pool(const pool_t pool)
{
	assert(_mcas);
	return _mcas->close_pool(pool);
}

status_t Client::delete_pool(const std::string& pool_name)
{
	assert(_mcas);
	return _mcas->delete_pool(pool_name);
}

status_t Client::remove_chunk(const pool_t pool, const std::string& key)
{
	assert(_mcas);
	using namespace cpp_pm_sort_protocol;

	return _mcas->erase(pool, key);
}

status_t Client::sort(const pool_t pool, unsigned type)
{
        assert(_mcas);
	using namespace cpp_pm_sort_protocol;

	status_t s;
	FlatBufferBuilder fbb;
	auto req = CreateSortRequest(fbb, type);
	auto msg = CreateMessage(fbb, g_transaction_id++, Element_SortRequest, req.Union());
	fbb.Finish(msg);

	std::vector<component::IMCAS::ADO_response> response;
	s = _mcas->invoke_ado(pool,
			      "",
			      fbb.GetBufferPointer(),
			      fbb.GetSize(),
			      0,
			      response);

	return s;
}

status_t Client::init(const pool_t pool)
{
        assert(_mcas);
	using namespace cpp_pm_sort_protocol;

	status_t s;
	FlatBufferBuilder fbb;
	auto req = CreateInitRequest(fbb, 0);
	auto msg = CreateMessage(fbb, g_transaction_id++, Element_InitRequest, req.Union());
	fbb.Finish(msg);

	std::vector<component::IMCAS::ADO_response> response;
	s = _mcas->invoke_ado(pool,
		              "",
		              fbb.GetBufferPointer(),
		              fbb.GetSize(),
		              0,
		              response);

	return s;
}

status_t Client::verify(const pool_t pool)
{
	assert(_mcas);
	using namespace cpp_pm_sort_protocol;

	status_t s;
	FlatBufferBuilder fbb;
	auto req = CreateVerifyRequest(fbb);
	auto msg = CreateMessage(fbb, g_transaction_id++, Element_VerifyRequest, req.Union());
	fbb.Finish(msg);

	std::vector<component::IMCAS::ADO_response> response;
	s = _mcas->invoke_ado(pool,
		              "",
		              fbb.GetBufferPointer(),
		              fbb.GetSize(),
		              0,
		              response);

	return s;
}

}
#pragma GCC diagnostic pop
