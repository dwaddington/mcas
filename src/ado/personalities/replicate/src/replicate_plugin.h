#ifndef __REPLICATE_PLUG_COMPONENT_H_
#define __REPLICATE_PLUG_COMPONENT_H_

#include <api/ado_itf.h>
#include <api/mcas_itf.h>
#include <conhash/conhash.h>
#include <ctime>
#include <libnuraft/nuraft.hxx>
#include <list>
#include <map>
#include <unordered_map>
#include "in_memory_log_store.hxx"

using pool_t = uint64_t;

class ADO_replicate_plugin : public Component::IADO_plugin {
 private:
  static constexpr bool option_DEBUG = true;
  struct Slave {
    Component::IMCAS *mcas_client;
    time_t            last_timestamp;
    Slave(Component::IMCAS *client, time_t timestamp)
        : mcas_client(client), last_timestamp(timestamp)
    {
    }
  };
  // record key->node ip
  std::unordered_map<std::string, std::list<std::string>> _chains;
  // ip-> node, maybe we need to consider capacity of the slave node
  std::map<std::string, Slave *> _slaves;
  unsigned                       _role = 0;  // 0 is master, 1 is slave

 public:
  /**
   * Constructor
   *
   *
   */
  ADO_replicate_plugin() : _chains(), _slaves(), _conhash() {}

  /**
   * Destructor
   *
   */
  virtual ~ADO_replicate_plugin() { cleanup(); }
  /**
   * Component/interface management
   *
   */
  DECLARE_VERSION(0.1f);
  // clang-format off
  DECLARE_COMPONENT_UUID(0x59564581, 0x9e1b, 0x4811, 0xbdb2, 0x19, 0x57,0xa0, 0xa6, 0x84, 0x69);
  // clang-format on

  void *query_interface(Component::uuid_t &itf_uuid) override
  {
    if (itf_uuid == Component::IADO_plugin::iid()) {
      return static_cast<Component::IADO_plugin *>(this);
    }
    else
      return NULL;  // we don't support this interface
  }

  void unload() override { delete this; }

 public:
  size_t _replica_size = 3;
  /* IADO_plugin */
  status_t register_mapped_memory(void * shard_vaddr,
                                  void * local_vaddr,
                                  size_t len) override;

  status_t do_work(
      const uint64_t     work_key,
      const std::string &key,
      void *             value,
      size_t             value_len,
      void *             detached_value,
      size_t             detached_value_len,
      const void * in_work_request, /* don't use iovec because of non-const */
      const size_t in_work_request_len,
      bool         new_root,
      response_buffer_vector_t &response_buffers) override;

  status_t shutdown() override;

 private:
  ConHash               _conhash;
  nuraft::raft_launcher _launcher;
  status_t              handle_heartbeat(std::string &ip);
  pool_t                handle_create_pool(const std::string &name,
                                           const size_t       size,
                                           uint32_t           flags = 0,
                                           uint64_t           expected_obj_count = 0)
  {  // TODO: how to handle the pool? consistent hashing on pool???
    return E_NOT_SUPPORTED;
  }
  status_t handle_delete_pool(const std::string &name)
  {
    return E_NOT_SUPPORTED;
  }
  status_t handle_get(const pool_t       pool,
                      const std::string &key,
                      void *& out_value, /* release with free_memory() API */
                      size_t &out_value_len);
  status_t handle_put(const pool_t       pool,
                      const std::string &key,
                      const void *       value,
                      const size_t       value_len,
                      uint32_t           flags);
  void     cleanup();
};

#endif