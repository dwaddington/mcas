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

#ifndef __mcas_SHARD_H__
#define __mcas_SHARD_H__

#ifdef __cplusplus

#include <api/ado_itf.h>
#include <api/components.h>
#include <api/fabric_itf.h>
#include <api/kvindex_itf.h>
#include <api/kvstore_itf.h>
#include <common/cpu.h>
#include <common/exceptions.h>
#include <common/logging.h>
#include <xpmem.h> /* XPMEM kernel module */

#include <list>
#include <memory>
#include <set>
#include <thread>
#include <unordered_map>

#include "config_file.h"
#include "connection_handler.h"
#include "fabric_transport.h"
#include "mcas_config.h"
#include "pool_manager.h"
#include "security.h"
#include "task_key_find.h"
#include "types.h"

namespace mcas
{
class Connection_handler;

/* Adapter point */
using Shard_transport = Fabric_transport;

class Shard : public Shard_transport {
 private:
  static constexpr size_t TWO_STAGE_THRESHOLD = KiB(8); /* above this two stage protocol is used */
  static constexpr size_t ADO_MAP_RESERVE     = 2048;

 private:
  struct lock_info_t {
    Component::IKVStore::pool_t pool;
    Component::IKVStore::key_t  key;
    int                         count;
    size_t                      value_size;
  };

  struct pool_desc_t {
    std::string  name;
    size_t       size;
    unsigned int flags;
    size_t       expected_obj_count;
    bool         opened_existing;
  };

  using pool_t             = Component::IKVStore::pool_t;
  using buffer_t           = Shard_transport::buffer_t;
  using index_map_t        = std::unordered_map<pool_t, Component::IKVIndex *>;
  using locked_value_map_t = std::unordered_map<const void *, lock_info_t>;
  using task_list_t        = std::list<Shard_task *>;

  unsigned _debug_level;

 public:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++" // many uninitialized/default initialized elements
  Shard(const Config_file &config_file,
        const unsigned     shard_index,
        const std::string  dax_config,
        const unsigned     debug_level,
        const bool         forced_exit)
      : Shard_transport(config_file.get_net_providers(),
                        config_file.get_shard("net", shard_index),
                        config_file.get_shard_port(shard_index)),
        _debug_level(debug_level), _forced_exit(forced_exit), _core(config_file.get_shard_core(shard_index)),
        _ado_map(ADO_MAP_RESERVE), _ado_path(config_file.get_ado_path()),
        _ado_plugins(config_file.get_shard_ado_plugins(shard_index)), _security(config_file.get_cert_path()),
        _thread(&Shard::thread_entry,
                this,
                config_file.get_shard("default_backend", shard_index),
                config_file.get_shard("index", shard_index),
                config_file.get_shard("nvme_device", shard_index),
                dax_config,
                config_file.get_shard("pm_path", shard_index),
                debug_level,
                config_file.get_shard_ado_core(shard_index),
                config_file.get_shard_ado_core_nu(shard_index))
  {
    mcas::Global::debug_level = debug_level;
  }
#pragma GCC diagnostic pop

  Shard(const Shard &) = delete;
  Shard &operator=(const Shard &) = delete;

  ~Shard()
  {
    _thread_exit = true;
    /* TODO: unblock */
    _thread.join();

    assert(_i_kvstore);
    _i_kvstore->release_ref();

    if (_i_ado_mgr) _i_ado_mgr->release_ref();

    if (_index_map) {
      for (auto i : *_index_map) {
        assert(i.second);
        i.second->release_ref();
      }
      delete _index_map;
    }
  }

  bool exited() const { return _thread_exit; }

 private:
  void thread_entry(const std::string &backend,
                    const std::string &index,
                    const std::string &pci_addr,
                    const std::string &dax_config,
                    const std::string &pm_path,
                    unsigned           debug_level,
                    const std::string  ado_cores,
                    float              ado_core_num);

  void add_locked_value(const pool_t pool_id, Component::IKVStore::key_t key, void *target, size_t target_len);

  void release_locked_value(const void *target);

  void initialize_components(const std::string &backend,
                             const std::string &index,
                             const std::string &pci_addr,
                             const std::string &dax_config,
                             const std::string &pm_path,
                             unsigned           debug_level,
                             const std::string  ado_cores,
                             float              ado_core_number);

  void check_for_new_connections();

  void main_loop();

  void process_message_pool_request(Connection_handler *handler, Protocol::Message_pool_request *msg);

  void process_message_IO_request(Connection_handler *handler, Protocol::Message_IO_request *msg);

  void process_info_request(Connection_handler *handler, Protocol::Message_INFO_request *msg);

  void process_ado_request(Connection_handler *handler, Protocol::Message_ado_request *msg);

  void process_put_ado_request(Connection_handler *handler, Protocol::Message_put_ado_request *msg);

  void process_messages_from_ado();

  status_t process_configure(Protocol::Message_IO_request *msg);

  void process_tasks(unsigned &idle);

  Component::IKVIndex *lookup_index(const pool_t pool_id)
  {
    if (_index_map) {
      auto search = _index_map->find(pool_id);
      if (search == _index_map->end()) return nullptr;
      return search->second;
    }
    else
      return nullptr;
  }

  void add_index_key(const pool_t pool_id, const std::string &k)
  {
    auto index = lookup_index(pool_id);
    if (index) index->insert(k);
  }

  void remove_index_key(const pool_t pool_id, const std::string &k)
  {
    auto index = lookup_index(pool_id);
    if (index) index->erase(k);
  }

  inline void add_task_list(Shard_task *task) { _tasks.push_back(task); }

  inline size_t session_count() const { return _handlers.size(); }

 private:
  bool ado_enabled() const { return (_i_ado_mgr && _ado_plugins->size() > 0); }

  auto get_ado_interface(pool_t pool_id)
  {
    auto i = _ado_map.find(pool_id);
    if (i != _ado_map.end())
      return (*i).second.first;
    else
      throw Logic_exception("get_ado_interface failed");
  }

  status_t conditional_bootstrap_ado_process(Component::IKVStore *       kvs,
                                             Connection_handler *        handler,
                                             Component::IKVStore::pool_t pool_id,
                                             Component::IADO_proxy *&    ado,
                                             pool_desc_t &               desc);

  /* per-shard statistics */
  Component::IMCAS::Shard_stats _stats alignas(8);

  void dump_stats()
  {
    PINF("------------------------------------------------");
    PINF("| Shard Statistics                             |");
    PINF("------------------------------------------------");
    PINF("PUT count          : %lu", _stats.op_put_count);
    PINF("GET count          : %lu", _stats.op_get_count);
    PINF("PUT_DIRECT count   : %lu", _stats.op_put_direct_count);
    PINF("GET 2-stage count  : %lu", _stats.op_get_twostage_count);
    PINF("ERASE count        : %lu", _stats.op_erase_count);
    PINF("ADO count          : %lu (enabled=%s)", _stats.op_ado_count, ado_enabled() ? "yes" : "no");
    PINF("Failed count       : %lu", _stats.op_failed_request_count);
    PINF("Session count      : %lu", session_count());
    PINF("------------------------------------------------");
  }

 private:
  struct work_request_t {
    Component::IKVStore::pool_t      pool;
    Component::IKVStore::key_t       key_handle;
    const char *                     key_ptr;
    size_t                           key_len;
    Component::IKVStore::lock_type_t lock_type;
    uint64_t                         request_id; /* original client request */
    uint32_t                         flags;

    inline bool is_async() const { return flags & Component::IMCAS::ADO_FLAG_ASYNC; }
  };

  class Work_request_allocator {
   private:
    static constexpr size_t       NUM_ELEMENTS = WORK_REQUEST_ALLOCATOR_COUNT;
    std::vector<work_request_t *> _free;
    /* "_all" apparently exists only to own the work_request_t elements */
    std::vector<std::unique_ptr<work_request_t>> _all;

   public:
    Work_request_allocator()
      : _free{}, _all{}
    {
      for (size_t i = 0; i < NUM_ELEMENTS; i++) {
        auto wr = std::unique_ptr<work_request_t>(static_cast<work_request_t *>(aligned_alloc(64, sizeof(work_request_t))));
        _free.push_back(wr.get());
        _all.push_back(std::move(wr));
      }
    }

    virtual ~Work_request_allocator()
    {
    }

    inline work_request_t *allocate()
    {
      if (_free.size() == 0) throw General_exception("Work_request_allocator exhausted");
      auto wr = _free.back();
      _free.pop_back();
      return wr;
    }

    inline void free_wr(work_request_t *wr) { _free.push_back(wr); }

  } _wr_allocator;

  using ado_map_t =
      std::unordered_map<Component::IKVStore::pool_t, std::pair<Component::IADO_proxy *, Connection_handler *>>;

  using work_request_key_t = uint64_t;

  static inline work_request_t *request_key_to_record(work_request_key_t key)
  {
    return reinterpret_cast<work_request_t *>(key);
  }

  /* Shard class members */
  index_map_t *                             _index_map            = nullptr;
  bool                                      _thread_exit          = false;
  bool                                      _store_requires_flush = false;
  bool                                      _forced_exit;
  unsigned                                  _core;
  size_t                                    _max_message_size;
  Component::IKVStore *                     _i_kvstore;
  Component::IADO_manager_proxy *           _i_ado_mgr = nullptr; /*< null indicate non-ADO mode */
  ado_map_t                                 _ado_map;
  std::vector<Connection_handler *>         _handlers;
  locked_value_map_t                        _locked_values;
  task_list_t                               _tasks;
  std::set<work_request_key_t>              _outstanding_work;
  std::vector<work_request_t *>             _failed_async_requests;
  const std::string                         _ado_path;
  std::unique_ptr<std::vector<std::string>> _ado_plugins;
  Shard_security                            _security;
  std::thread                               _thread;
};

inline bool check_xpmem_module()
{
  int fd = open("/dev/xpmem", O_RDWR, 0666);
  close(fd);
  return (fd != -1);
}

}  // namespace mcas

#endif

#endif  // __SHARD_HPP__
