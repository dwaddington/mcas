
#ifndef __ADO_MANAGER_H__
#define __ADO_MANAGER_H__

#include <common/logging.h> /* log_source */
#include <common/string_view.h>
#include <common/types.h>
#include <threadipc/queue.h> /* Threadipc */

#include <set>
#include <future>
#include <string>
#include <utility>
#include <vector>

#include "config_file.h"
#include "program_options.h"

class SLA;

struct ado {
  unsigned int shard_id;
  std::string  cpus;
  // can be container id
  std::string id;
  ado(unsigned int shard_id_, common::string_view cpus_, common::string_view id_)
    : shard_id(shard_id_)
    , cpus(cpus_)
    , id(id_)
  {}
  ~ado();
};

struct compare {
  bool operator()(const std::pair<unsigned int, unsigned int> &lhs,
                  const std::pair<unsigned int, unsigned int> &rhs) const
  {
    if (lhs.second == rhs.second) return (lhs.first < rhs.first);
    return (lhs.second < rhs.second);
  }
};

class ADO_manager : private common::log_source {
private:
  mcas::Config_file         _config;

public:
  ADO_manager(Program_options &options);
  ADO_manager(bool debug_level, common::string_view config);
  ADO_manager(const ADO_manager &) = delete;
  ADO_manager &operator=(const ADO_manager &) = delete;
  ~ADO_manager();
  void setSLA();

private:
  SLA *                   _sla;
  std::vector<struct ado> _ados;
  // std::set<std::pair<unsigned int, unsigned int>, compare> _ado_cpu_pool;
  std::set<unsigned int> _ado_cpu_pool;
  std::set<unsigned int> _manager_cpu_pool;
  std::mutex             _m_running;
  std::condition_variable _cv_running;
  bool                   _running;
  std::mutex             _m_exit;
  bool                   _exit;
  std::future<void>      _thread;

  void init();
  void main_loop();
  bool is_running() const { return _running; }
  void resource_check()
  {
    // TODO
  }
  void        register_ado(unsigned int, common::string_view, common::string_view);
  void        kill_ado(const struct ado &ado);
  void        schedule(unsigned int shard_core, common::string_view cpus, float cpu_num, numa_node_t numa_zone);
  std::string reschedule(const struct ado &ado)
  {
    // TODO
    return ado.cpus;
  }
};

#endif
