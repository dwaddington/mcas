
#ifndef __ADO_MANAGER_H__
#define __ADO_MANAGER_H__

#include <common/types.h>
#include <threadipc/queue.h>
#include <set>
#include <thread>
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
};

struct compare {
  bool operator()(const std::pair<unsigned int, unsigned int> &lhs,
                  const std::pair<unsigned int, unsigned int> &rhs) const
  {
    if (lhs.second == rhs.second) return (lhs.first < rhs.first);
    return (lhs.second < rhs.second);
  }
};

class ADO_manager : public mcas::Config_file {
 public:
  ADO_manager(Program_options &options)
      : mcas::Config_file(options.config_file),
        _thread(&ADO_manager::init, this)
  {
    while (!_running) usleep(1000);
    _sla = NULL;
  }
  ~ADO_manager() { exit(); }
  void setSLA();

 private:
  SLA *                   _sla = NULL;
  std::vector<struct ado> _ados;
  // std::set<std::pair<unsigned int, unsigned int>, compare> _ado_cpu_pool;
  std::set<unsigned int> _ado_cpu_pool;
  std::set<unsigned int> _manager_cpu_pool;
  bool                   _running = false;
  bool                   _exit    = false;
  std::thread            _thread;

  void init();
  void main_loop();

  void exit()
  {
    _exit = true;
    /* kill ADO processes */
    for (auto &ado : _ados) kill_ado(ado);

    Threadipc::Thread_ipc::instance()->cleanup();
    _thread.join();
    PLOG("Ado_manager: threads joined");
  }

  void resource_check()
  {
    // TODO
  }
  void        register_ado(unsigned int, std::string &, std::string &);
  void        kill_ado(const struct ado &ado);
  void        schedule(unsigned int shard_core,
                       std::string  cpus,
                       float        cpu_num,
                       numa_node_t  numa_zone);
  std::string reschedule(const struct ado &ado)
  {
    // TODO
    return ado.cpus;
  }
};

#endif
