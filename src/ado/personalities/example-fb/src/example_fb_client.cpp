#include "example_fb_proto_generated.h"
#include "example_fb_client.h"
#include <api/mcas_itf.h>
#include <api/components.h>
#include <common/dump_utils.h>

using namespace flatbuffers;
using namespace Example_fb_protocol;

static unsigned long g_transaction_id = 0;

namespace example_fb
{

Client::Client(const unsigned debug_level,
               const std::string& addr_with_port,
               const std::string& nic_device)
{
  using namespace Component;
  
  auto dll = load_component("libcomponent-mcasclient.so", mcas_client_factory);
  //  auto factory = static_cast<IMCAS_factory*>(dll->query_interface(IMCAS_factory::iid()));
  auto factory = dll->query_interface<IMCAS_factory>();
  _mcas = factory->mcas_create(debug_level, getlogin(), addr_with_port, nic_device);
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
  return _mcas->open_pool(pool_name, read_only ? Component::IKVStore::FLAGS_READ_ONLY : 0);
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

status_t Client::put(const pool_t pool,
                     const std::string& key,
                     const std::string& value)
{
  assert(_mcas);
  status_t s;
  FlatBufferBuilder fbb;
  auto req = CreatePutRequestDirect(fbb, key.c_str(), value.c_str());
  auto msg = CreateMessage(fbb, g_transaction_id++, Element_PutRequest, req.Union());
  fbb.Finish(msg);
  
  std::string response;
  
  s = _mcas->invoke_put_ado(pool,
                            key,
                            fbb.GetBufferPointer(),
                            fbb.GetSize(),
                            value.data(),
                            value.length(),
                            128, //root length TODO
                            Component::IMCAS::ADO_FLAG_DETACHED,
                            response);
  return s;
}

status_t Client::get(const pool_t pool,
                     const std::string& key,
                     const int version_index,
                     std::string& out_value)
{
  assert(_mcas);
  using namespace Example_fb_protocol;
  
  status_t s;
  FlatBufferBuilder fbb;
  auto req = CreateGetRequestDirect(fbb, key.c_str(), version_index);
  auto msg = CreateMessage(fbb, g_transaction_id++, Element_GetRequest, req.Union());
  fbb.Finish(msg);

  std::string response;

  s = _mcas->invoke_ado(pool,
                        key,
                        fbb.GetBufferPointer(),
                        fbb.GetSize(),
                        0,
                        response);
    
  auto response_msg = GetSizePrefixedMessage(response.data());
  uint32_t fb_message_size = *((const uint32_t *)response.data()) + 4;

  auto get_response = response_msg->element_as_GetResponse();
  assert(get_response);
  out_value.assign(response.data() + fb_message_size, get_response->value_len());

  return s;
}

}

