#include "ado_manager.h"

#include <common/cpu.h>
#include <common/get_vector_from_string.h>
#include <common/logging.h>
#include <common/cycles.h>
#include <numa.h>
#include <unistd.h>
#include <algorithm>
#include <random>

#include <boost/tokenizer.hpp>
#include <cassert>
#include <string>
#include <typeinfo>

using Thread_ipc = threadipc::Thread_ipc;

static const char *env_USE_DOCKER = ::getenv("USE_DOCKER");

bool add_to_schedule(std::string &ret, unsigned int c)
{
  if (ret.find(std::to_string(c)) == std::string::npos) {
    ret.append(std::to_string(c)).append(",");
    return true;
  }
  return false;
}

ADO_manager::ADO_manager(Program_options &options)
  : ADO_manager(options.debug_level, options.config)
{
}

ADO_manager::ADO_manager(bool debug_level, common::string_view config)
  : log_source(0U),
    _config(debug_level, config),
    _sla{nullptr},
    _ados{},
    _ado_cpu_pool{},
    _manager_cpu_pool{},
    _m_running{},
    _cv_running{},
    _running{false},
    _m_exit{},
    _exit{false},
    _thread(std::async(std::launch::async, &ADO_manager::init, this))
{
  std::unique_lock<std::mutex> g{_m_running};
  _cv_running.wait(g, [this] () { return is_running(); });
  _sla = NULL;
}

void ADO_manager::init()
{
  // set up manager cpu pool
  int core = _config.get_ado_manager_core();
	pthread_setname_np(pthread_self(), "ADO_manager");
  if (core != -1) _manager_cpu_pool.insert(unsigned(core));

  // setup ado cpu pool
  auto ado_cores = _config.get_ado_cores();
  if (ado_cores.empty()) {
    for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++) _ado_cpu_pool.insert(i);
    for (unsigned i = 0; i < _config.shard_count(); i++) _ado_cpu_pool.erase(_config.get_shard_core(i));
  }
  else {
    auto cores = get_vector_from_string<unsigned>(ado_cores);
    for (auto i : cores) _ado_cpu_pool.insert(i);
  }

  if (core == -1) {
    for (auto i : _ado_cpu_pool) _manager_cpu_pool.insert(i);
    for (unsigned i = 0; i < _config.shard_count(); i++) {
      auto ado_cores_for_shard = _config.get_shard_ado_cores(i);
      if (ado_cores_for_shard.empty()) continue;
      auto cores = get_vector_from_string<unsigned>(ado_cores_for_shard);
      for (auto j : cores) _manager_cpu_pool.erase(j);
    }
  }

  // set affinity
  cpu_mask_t mask;
  for (auto &c : _manager_cpu_pool) {
    mask.add_core(c);
  }
  if (set_cpu_affinity_mask(mask) == -1) {
    PLOG("%s: bad mask parameter", __FILE__);
  }

  CPLOG(2, "CPU MASK: ADO MANAGER process configured with cpu mask: [%s]", mask.string_form().c_str());

  {
    std::unique_lock<std::mutex> g(_m_running);
    _running = true;
    _cv_running.notify_all();
  }
  // main loop (send and receive from the queue)
  main_loop();
}

void ADO_manager::main_loop()
{
  std::unique_lock<std::mutex> g{_m_exit};
  while (!_exit) {
    g.unlock();

    CPLOG(3, "%lu ADO manager thread", rdtsc());

    struct threadipc::Message *msg;

    do {
      msg = nullptr;

      try {
        Thread_ipc::instance()->get_next_mgr(msg);
      }
      catch(std::exception& e) {
        break; /* timeout is used to get out of this on exit */
      }

      assert(msg);

      switch (msg->op) {
      case threadipc::Operation::schedule:
        schedule(msg->shard_core, msg->cores, msg->core_number, msg->numa_zone);
        break;
      case threadipc::Operation::register_ado:
        register_ado(msg->shard_core, msg->cores, msg->ado_id);
        break;
      default:
        break;
      }
    }
    while(msg);

    resource_check();
    g.lock();
  }
}

void ADO_manager::register_ado(unsigned int shard, common::string_view cpus, common::string_view id)
{
  _ados.emplace_back(shard, std::string(cpus), std::string(id));
}

ado::~ado()
{
  Thread_ipc::instance()->send_kill_to_ado(shard_id);
}

void ADO_manager::schedule(unsigned int shard, common::string_view cpus, float cpu_num, numa_node_t numa_zone)
{
  std::string ret;
  if (cpu_num == -1) {
    auto cores = get_vector_from_string<unsigned>(cpus);
    // just assign core
    for (auto &c : cores) {
      ret.append(std::to_string(c)).append(",");
      _ado_cpu_pool.erase(c);
    }
  }
  else if (env_USE_DOCKER || cpu_num >= static_cast<float>(_ado_cpu_pool.size())) {
    // assuming share
    for (auto &c : _ado_cpu_pool) {
      add_to_schedule(ret, c);
    }
  }
  else {
    std::vector<unsigned> output(_ado_cpu_pool.size());
    std::copy(_ado_cpu_pool.begin(), _ado_cpu_pool.end(), output.begin());
    std::random_device rd;
    std::mt19937       g(rd());
    std::shuffle(output.begin(), output.end(), g);
    for (unsigned i = 0; i < static_cast<unsigned>(cpu_num); i++) {
      add_to_schedule(ret, output[i]);
    }
  }

  Thread_ipc::instance()->schedule_to_ado(shard, ret.substr(0, ret.size() - 1), cpu_num, numa_zone);

}

ADO_manager::~ADO_manager()
{
  {
    std::unique_lock<std::mutex> g(_m_exit);
    _exit = true;
  }

  /* send exit message to each ADO processes */
  _ados.clear();

  Thread_ipc::instance()->cleanup();

  try {
    _thread.get(); /* wait for thread to exit */
  }
  catch ( const std::exception &e )  {
    FERRM("ended with exception: {} {}", type_of(e), e.what());
  }

  CPLOG(1, "Ado_manager: threads joined");
}
