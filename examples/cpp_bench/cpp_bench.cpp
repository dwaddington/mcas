/*
   Copyright [2019-21] [IBM Corporation]
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

#include <iostream>
#include <fstream>
#include <unistd.h>
#include <boost/program_options.hpp>

#include <common/exceptions.h>
#include <common/str_utils.h> /* random_string */
#include <common/utils.h> /* MiB */
#include <common/task.h>
#include <common/perf/tm_actual.h>

#include <api/components.h>
#include <api/mcas_itf.h>

struct {
  std::string addr;
  std::string device;
  std::string log;
  unsigned    debug_level;
  unsigned    patience;
  unsigned    base_core;
  unsigned    cores;
  unsigned    key_size;
  unsigned    value_size;
  unsigned    pairs;
  unsigned    iterations;
  unsigned    pool_size;
} Options;

component::IMCAS_factory * factory = nullptr;

struct record_t {
  std::string key;
};

std::string   _value;
std::mutex    _iops_lock;
static unsigned long _iops = 0;

class IOPS_task : public common::Tasklet {
 public:

  IOPS_task(unsigned arg) {}

  virtual void initialize(unsigned core) override
  {
    _store.reset(factory->create(Options.debug_level, "cpp_bench", Options.addr, Options.device));

    char poolname[64];
    sprintf(poolname, "cpp_bench.pool.%u", core);

    _store->delete_pool(poolname); /* delete any existing pool */
    
    _pool = _store->create_pool(poolname, GiB(Options.pool_size));

    //    _data = (record_t *) malloc(sizeof(record_t) * Options.pairs);
    _data = new record_t [Options.pairs+1];
    assert(_data);
    
    PINF("Setting up data a priori: core %u", core);

    /* set up data */
    _value = common::random_string(Options.value_size);
    for (unsigned long i = 0; i < Options.pairs; i++) {
      _data[i].key = common::random_string(Options.key_size);
    }

    _ready_flag = true;
  }

  virtual bool do_work(unsigned core) override
  {
    if (_iterations == 0) {
      PINF("Starting worker: core %u", core);
      _start_time = std::chrono::high_resolution_clock::now();
    }

TM_INSTANCE
    status_t rc = _store->put(TM_REF _pool,
                              _data[_iterations].key,
                              _value.data(),
                              Options.value_size);
   
    if (rc != S_OK) throw General_exception("put operation failed:rc=%d", rc);

    assert(rc == S_OK);

    _iterations++;
    if (_iterations > Options.pairs) {
      _end_time = std::chrono::high_resolution_clock::now();
      PINF("Worker: %u complete", core);
      return false;
    }
    return true;
  }

  virtual void cleanup(unsigned core) override
  {
    PINF("Cleanup %u", core);
    auto secs = std::chrono::duration<double>(_end_time - _start_time).count();
    _iops_lock.lock();
    auto iops = double(Options.pairs) / secs;
    PINF("%f iops (core=%u)", iops, core);
    _iops += iops;
    _iops_lock.unlock();
    _store->close_pool(_pool);
    delete [] _data;
  }

  virtual bool ready() override { return _ready_flag; }

 private:
  std::chrono::high_resolution_clock::time_point _start_time, _end_time;
  bool                                           _ready_flag = false;
  unsigned long                                  _iterations = 0;
  component::Itf_ref<component::IKVStore>        _store;
  record_t *                                     _data;
  component::IKVStore::pool_t                    _pool;
};


/*-------------------------------------------------------------*/

int main(int argc, char* argv[])
{
  using namespace component;

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()("help", "Show help")
      ("debug", po::value<unsigned>()->default_value(0), "Debug level 0-3")
      ("patience", po::value<unsigned>()->default_value(30), "Patience with server (seconds)")
      ("server", po::value<std::string>()->default_value("10.0.0.101:11911:verbs"), "Server address IP:PORT[:PROVIDER]")
      ("device", po::value<std::string>()->default_value("mlx5_0"), "Network device (e.g., mlx5_0)")
      ("basecore", po::value<unsigned>()->default_value(0), "Starting base worker core")
      ("cores", po::value<unsigned>()->default_value(1), "Core/thread count")
      ("key", po::value<unsigned>()->default_value(8), "Size of key in bytes")
      ("value", po::value<unsigned>()->default_value(16), "Size of value in bytes")
      ("pairs", po::value<unsigned>()->default_value(100000), "Number of key-value pairs")
      ("poolsize", po::value<unsigned>()->default_value(2), "Size of pool in GiB")
      ("log", po::value<std::string>()->default_value("/tmp/cpp-bench-log.txt"), "File to log results")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);

    if (vm.count("help") > 0) {
      std::cout << desc;
      return -1;
    }

    Options.addr        = vm["server"].as<std::string>();
    Options.debug_level = vm["debug"].as<unsigned>();
    Options.patience    = vm["patience"].as<unsigned>();
    Options.base_core   = vm["basecore"].as<unsigned>();
    Options.cores       = vm["cores"].as<unsigned>();
    Options.key_size    = vm["key"].as<unsigned>();
    Options.value_size  = vm["value"].as<unsigned>();
    Options.pairs       = vm["pairs"].as<unsigned>();
    Options.device      = vm["device"].as<std::string>();
    Options.pool_size   = vm["poolsize"].as<unsigned>();
    Options.log         = vm["log"].as<std::string>();
  }
  catch (...) {
    std::cerr << "bad command line option configuration\n";
    return -1;
  }


  /* load component and create factory */
  IBase *comp = load_component("libcomponent-mcasclient.so", mcas_client_factory);
  factory = static_cast<IMCAS_factory *>(comp->query_interface(IMCAS_factory::iid()));
  assert(factory);

  /* create instance of MCAS client session */
  auto mcas = factory->mcas_create(1 /* debug level, 0=off */,
                                   Options.patience,
                                   getlogin(),
                                   Options.addr, /* MCAS server endpoint */
                                   Options.device); /* see mcas_client.h */

  {
    unsigned NUM_CORES = Options.cores;
    cpu_mask_t mask;
    
    for (unsigned c = 0; c < NUM_CORES; c++) mask.add_core(c + Options.base_core);
    {
      common::Per_core_tasking<IOPS_task, unsigned> t(mask, 11911);
      t.wait_for_all();
    }

    {
      std::ofstream tmp(Options.log);
      tmp << "Total IOPS: " << reinterpret_cast<unsigned long>(_iops) << "\n";
    }

    PMAJOR("Aggregate IOPS: %lu", reinterpret_cast<unsigned long>(_iops));
  }

  factory->release_ref();
  return 0;
}


