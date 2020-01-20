#include <common/cpu.h>
#include <common/logging.h>
#include <numa.h>
#include <unistd.h>
#include <boost/tokenizer.hpp>
#include <string>
#include "ado_manager.h"

using namespace std;
using namespace Common;
using namespace Threadipc;

bool add_to_schedule(string &ret, unsigned int c)
{
  if (ret.find(to_string(c)) == string::npos) {
    ret.append(to_string(c)).append(",");
    return true;
  }
  return false;
}

status_t string_to_set(std::string def, set<unsigned int> &cores)
{
  using namespace boost;

  if (def.size() == 0) return S_OK;

  if (def.find(",") == std::string::npos) {
    try {
      cores.insert(stoi(def));
      return S_OK;
    }
    catch (const std::invalid_argument &) {
      return E_INVAL;
    }
  }

  boost::char_separator<char>                   sep(",");
  vector<string>                                v;
  boost::tokenizer<boost::char_separator<char>> tok(def, sep);

  try {
    for_each(tok.begin(), tok.end(), [&](const string &s) {
      try {
        cores.insert(stoi(s));
      }
      catch (const std::invalid_argument &) {
        PWRN("invalid token in cpu mask string version.");
      }
    });
  }
  catch (...) {
    return E_FAIL;
  }

  return S_OK;
}

void ADO_manager::init()
{
  // set up manager cpu pool
  int core = get_ado_manager_core();
  if (core != -1) _manager_cpu_pool.insert(core);
  // setup ado cpu pool
  auto ado_cores = get_ado_cores();
  if (ado_cores.empty()) {
    for (unsigned i = 0; i < thread::hardware_concurrency(); i++)
      _ado_cpu_pool.insert(i);
    for (unsigned i = 0; i < shard_count(); i++)
      _ado_cpu_pool.erase(get_shard_core(i));
  }
  else {
    auto     pos   = ado_cores.find("-");
    unsigned start = stoi(ado_cores.substr(0, pos + 1));
    unsigned end   = stoi(ado_cores.substr(pos + 1));
    for (unsigned i = start; i <= end; i++) _ado_cpu_pool.insert(i);
  }

  if (core == -1) {
    for (auto i : _ado_cpu_pool) _manager_cpu_pool.insert(i);
    for (unsigned i = 0; i < shard_count(); i++) {
      set<unsigned> cores;
      string_to_set(get_shard_ado_core(i), cores);
      for (auto j : cores) _manager_cpu_pool.erase(j);
    }
  }

  // set affinity
  cpu_mask_t mask;
  for (auto &c : _manager_cpu_pool) {
    mask.add_core(c);
  }
  set_cpu_affinity_mask(mask);

  _running = true;
  // main loop (send and receive from the queue)
  main_loop();
}

void ADO_manager::main_loop()
{
  uint64_t                  tick alignas(8)           = 0;
  static constexpr uint64_t CHECK_CONNECTION_INTERVAL = 10000000;

  while (!_exit) {
    struct message *msg = NULL;
    Thread_ipc::instance()->get_next_mgr(msg);
    // PERR("get_next_mgr dequeue failed!");
    if(_exit) continue;
    switch (msg->op) {
      case Operation::schedule:
        schedule(msg->shard_core, msg->cores, msg->core_number, msg->numa_zone);
        break;
      case Operation::register_ado:
        register_ado(msg->shard_core, msg->cores, msg->ado_id);
        break;
      default:
        break;
    }

    if (tick % CHECK_CONNECTION_INTERVAL == 0) resource_check();

    tick++;
  }
}

void ADO_manager::register_ado(unsigned int shard, string &cpus, string &id)
{
  struct ado ado;
  ado.shard_id = shard;
  ado.cpus     = cpus;
  ado.id       = id;
  _ados.push_back(ado);
}

void ADO_manager::kill_ado(const struct ado &ado)
{
  if (!Thread_ipc::instance()->kill_to_ado(ado.shard_id)) {
    PERR("kill_to_ado enqueue failed!");
  }
}

void ADO_manager::schedule(unsigned int shard,
                           string       cpus,
                           float        cpu_num,
                           numa_node_t  numa_zone)
{
  string            ret;
  set<unsigned int> cores;
  string_to_set(cpus, cores);
  if (cpu_num == -1) {
    // just assign core
    for (auto &c : cores) {
      ret.append(to_string(c)).append(",");
      _ado_cpu_pool.erase(c);
    }
  }
  else {
    // assuming share
    for (auto &c : _ado_cpu_pool) {
      add_to_schedule(ret, c);
      /*
      if (cpu_num == 0)
        break;
      if (numa_node_of_cpu(c.first) == numa_zone) {
        if (add_to_schedule(ret, c)) {
          _ado_cpu_pool.erase(c);
          _ado_cpu_pool.insert(make_pair(c.first, c.second + 1));
          cpu_num--;
        }
      }
    }
    if (cpu_num > 0) {
      for (auto &c : _ado_cpu_pool) {
        if (0 == cpu_num)
          break;
        if (add_to_schedule(ret, c)) {
          _ado_cpu_pool.erase(c);
          _ado_cpu_pool.insert(make_pair(c.first, c.second + 1));
          cpu_num--;
        }
      }
    }*/
    }
    if (!Thread_ipc::instance()->schedule_to_ado(
            shard, ret.substr(0, ret.size() - 1), cpu_num, numa_zone)) {
      PERR("schedule_to_ado enqueue failed!");
    }
  }
}
