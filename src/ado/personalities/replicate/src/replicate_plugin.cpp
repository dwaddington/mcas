#include <api/interfaces.h>
#include <algorithm>
#include <cstdlib>
#include "replicate_plugin.h"

using namespace std;
using namespace nuraft;

status_t ADO_replicate_plugin::register_mapped_memory(void * shard_vaddr,
                                                      void * local_vaddr,
                                                      size_t len)
{
  PLOG("ADO_replicate_plugin: register_mapped_memory (%p, %p, %lu)",
       shard_vaddr, local_vaddr, len);

  return S_OK;
}

status_t ADO_replicate_plugin::shutdown()
{
  _launcher.shutdown();
  return S_OK;
}

status_t ADO_replicate_plugin::do_work(
    const uint64_t     work_key,
    const std::string &key,
    void *             shard_value_vaddr,
    size_t             value_len,
    void *             detached_value,
    size_t             detached_value_len,
    const void *       in_work_request, /* don't use iovec because non-const */
    const size_t,                       // in_work_request_len
    bool,                               // new root
    response_buffer_vector_t &response_buffers)
{
#if 0
  PLOG("ADO_kite_plugin: work_id (%lu)", work_key);
  PLOG("ADO_kite_plugin: do_work (%s, value_addr=%p, valuen_len=%lu)",
       key.c_str(), shard_value_vaddr, value_len);
  PLOG("Current value: %.*s", int(value_len),
       static_cast<char *>(shard_value_vaddr));
#endif
  assert(key != "");

  if (detached_value != nullptr || detached_value_len > 0)
    throw Logic_exception("kite_plugin did not expect detached value");

  // TODO:
  // if(in_work_request=="heartbeat")
  // handle_heartbeat(hostip+port);
  // if(in_work_request=="put")

  response_buffers.push_back({::malloc(3), 3, false});
  memcpy(static_cast<char *>(response_buffers[0].ptr), "OK!", 3);

  return S_OK;
}

status_t ADO_replicate_plugin::handle_heartbeat(string &ip)
{
  time_t current = time(NULL);
  if (_slaves.find(ip) == _slaves.end()) {
    using namespace Component;

    IBase *comp =
        load_component("libcomponent-mcasclient.so", mcas_client_factory);

    auto fact = static_cast<IMCAS_factory *>(
        comp->query_interface(IMCAS_factory::iid()));
    if (!fact) throw Logic_exception("unable to create MCAS factory");
    // TODO: how to pass device
    IMCAS *mcas =
        fact->mcas_create(option_DEBUG, to_string(_role), ip, "device");

    if (!mcas) throw Logic_exception("unable to create MCAS client instance");
    fact->release_ref();

    Node n(ip, 1);
    _conhash.AddNode(n);

    _slaves.emplace(ip, new Slave(mcas, current));
  }
  else {
    _slaves[ip]->last_timestamp = current;
  }
  return S_OK;
}

status_t ADO_replicate_plugin::handle_put(const uint64_t     pool,
                                          const std::string &key,
                                          const void *       value,
                                          const size_t       value_len,
                                          uint32_t           flags)
{
  if (_chains.find(key) == _chains.end()) {
    // new key, v
    srand(static_cast<unsigned>(time(NULL)));
    Node n;
    _conhash.Lookup(key, &n);
    list<string> nodes(_replica_size);
    nodes.emplace_back(n.identify);
    // randomly select other replicas
    while (nodes.size() <= _replica_size) {
      auto index = rand() % _slaves.size();
      auto it    = _slaves.begin();
      advance(it, index);
      // remove duplication
      if (std::find(nodes.begin(), nodes.end(), it->first) == nodes.cend())
        nodes.emplace_back(it->first);
    }
    _chains.emplace(key, nodes);
  }

  for (auto ip : _chains[key]) {
    _slaves[ip]->mcas_client->put(pool, key, value, value_len, flags);
  }

  return S_OK;
}

status_t ADO_replicate_plugin::handle_get(const pool_t       pool,
                                          const std::string &key,
                                          void *&            out_value,
                                          size_t &           out_value_len)
{
  if (_chains.find(key) == _chains.end()) {
    return E_INVAL;
  }

  auto ip = *(_chains.at(key).cend());
  return _slaves[ip]->mcas_client->get(pool, key, out_value, out_value_len);
}

/**
 * Factory-less entry point
 *
 */
extern "C" void *factory_createInstance(Component::uuid_t &interface_iid)
{
  PLOG("instantiating ADO_replciate_plugin");
  if (interface_iid == Interface::ado_plugin)
    return static_cast<void *>(new ADO_replicate_plugin());
  else
    return NULL;
}
