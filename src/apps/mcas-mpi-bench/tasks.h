#ifndef __TASKS_H__
#define __TASKS_H__

struct {
  std::string addr;
  std::string device;
  std::string log;
  std::string test;
  unsigned    debug_level;
  unsigned    patience;
  unsigned    base_core;
  unsigned    cores;
  unsigned    key_size;
  unsigned    value_size;
  unsigned    pairs;
  unsigned    iterations;
  unsigned    repeats;
  unsigned    pool_size;
  unsigned    port;
  unsigned    cps;
} Options;

struct record_t {
  std::string key;
  void * data;
};

std::string          _value;

component::IMCAS_factory * factory = nullptr;

class IOPS_base {
public:
  
  unsigned long cleanup(unsigned rank)
  {
    PINF("Cleanup %u", rank);
    auto secs = std::chrono::duration<double>(_end_time - _start_time).count();
    auto iops = (double(Options.pairs) * double(Options.repeats)) / secs;
    PINF("%f iops (rank=%u)", iops, rank);
    unsigned long i_iops = boost::numeric_cast<unsigned long>(iops);

    for (unsigned long i = 0; i < Options.pairs; i++)
      _store->free_memory(_data[i].data);
    
    _store->close_pool(_pool);
    delete [] _data;
    return i_iops;
  }

protected:
  std::chrono::high_resolution_clock::time_point _start_time, _end_time;
  unsigned long                                  _iterations = 0;
  component::Itf_ref<component::IKVStore>        _store;
  record_t *                                     _data;
  component::IKVStore::pool_t                    _pool;
  std::vector<void *>                            _get_results;
  unsigned                                       _repeats_remaining = Options.repeats;
};

class Write_IOPS_task : public IOPS_base
{
 public:

  Write_IOPS_task(unsigned rank)
  {
    _store.reset(factory->create(Options.debug_level, "cpp_bench", Options.addr, Options.device));

    char poolname[64];
    sprintf(poolname, "cpp_bench.pool.%u", rank);

    _store->delete_pool(poolname); /* delete any existing pool */
    
    _pool = _store->create_pool(poolname, GiB(Options.pool_size));

    _data = new record_t [Options.pairs];
    assert(_data);
    
    PINF("Setting up data a priori: rank %u", rank);

    /* set up data */
    _value = common::random_string(Options.value_size);
    for (unsigned long i = 0; i < Options.pairs; i++) {
      _data[i].key = common::random_string(Options.key_size);
    }
  }

  bool do_work(unsigned rank)
  {
    if (_iterations == 0) {
      PINF("Starting WRITE worker: rank %u", rank);
      _start_time = std::chrono::high_resolution_clock::now();
    }

    status_t rc = _store->put(_pool,
                              _data[_iterations].key,
                              _value.data(),
                              Options.value_size);
   
    if (rc != S_OK)
      throw General_exception("put operation failed:rc=%d", rc);

    _iterations++;
    if (_iterations >= Options.pairs) {
      _repeats_remaining --;
      if(_repeats_remaining == 0) {
        _end_time = std::chrono::high_resolution_clock::now();
        PINF("Worker: %u complete", rank);
        return false;
      }
      _iterations = 1;
    }
    return true;
  }
};


class Read_IOPS_task : public IOPS_base {
 public:

  Read_IOPS_task(unsigned rank)
  {
    _store.reset(factory->create(Options.debug_level, "cpp_bench", Options.addr, Options.device));

    char poolname[64];
    sprintf(poolname, "cpp_bench.pool.%u", rank);

    _store->delete_pool(poolname); /* delete any existing pool */
    
    _pool = _store->create_pool(poolname, GiB(Options.pool_size));

    _data = new record_t [Options.pairs];
    assert(_data);
    
    PINF("Setting up data prior to reading: rank %u", rank);

    /* set up data */
    _value = common::random_string(Options.value_size);
    for (unsigned long i = 0; i < Options.pairs; i++) {
      
      _data[i].key = common::random_string(Options.key_size);

      /* write data in preparation for read */
      status_t rc = _store->put(_pool,
                                _data[i].key,
                                _value.data(), /* same value */
                                Options.value_size);      
      
      if (rc != S_OK)
        throw General_exception("put operation failed:rc=%d", rc);
    }

  }

  bool do_work(unsigned rank)
  {
    if (_iterations == 0) {
      PINF("Starting READ worker: rank %u", rank);
      _start_time = std::chrono::high_resolution_clock::now();
    }

    size_t out_value_size = 0;
    status_t rc = _store->get(_pool,
                              _data[_iterations].key,
                              _data[_iterations].data,
                              out_value_size);

    if (rc != S_OK)
      throw General_exception("get operation failed: (key=%s) rc=%d", _data[_iterations].key.c_str(),rc);

    _iterations++;
    if (_iterations >= Options.pairs) {
      _repeats_remaining --;
      if(_repeats_remaining == 0) {
        _end_time = std::chrono::high_resolution_clock::now();
        PINF("Worker: %u complete", rank);
        return false;
      }
      _iterations = 1;
    }
    return true;
  }

};


class Mixed_IOPS_task : public IOPS_base {
 public:

  Mixed_IOPS_task(unsigned rank)
  {
    _store.reset(factory->create(Options.debug_level, "cpp_bench", Options.addr, Options.device));

    char poolname[64];
    sprintf(poolname, "cpp_bench.pool.%u", rank);

    _store->delete_pool(poolname); /* delete any existing pool */
    
    _pool = _store->create_pool(poolname, GiB(Options.pool_size));

    _data = new record_t [Options.pairs];
    assert(_data);
    
    PINF("Setting up data prior to rw50: rank %u", rank);

    /* set up data */
    _value = common::random_string(Options.value_size);
    for (unsigned long i = 0; i < Options.pairs; i++) {
      
      _data[i].key = common::random_string(Options.key_size);

      /* write data in preparation for read */
      status_t rc = _store->put(_pool,
                                _data[i].key,
                                _value.data(), /* same value */
                                Options.value_size);      
      
      if (rc != S_OK)
        throw General_exception("put operation failed:rc=%d", rc);
    }

  }

  virtual bool do_work(unsigned rank)
  {
    if (_iterations == 0) {
      PINF("Starting RW50 worker: rank %u", rank);
      _start_time = std::chrono::high_resolution_clock::now();
    }

    if (_iterations % 2 == 0) {
      size_t out_value_size = 0;
      status_t rc = _store->get(_pool,
                                _data[_iterations].key,
                                _data[_iterations].data,
                                out_value_size);

      if (rc != S_OK)
        throw General_exception("get operation failed: (key=%s) rc=%d", _data[_iterations].key.c_str(),rc);
    }
    else {
      status_t rc = _store->put(_pool,
                                _data[_iterations].key,
                                _value.data(),
                                Options.value_size);
      
      if (rc != S_OK)
        throw General_exception("put operation failed:rc=%d", rc);
    }

    _iterations++;
    if (_iterations >= Options.pairs) {
      _repeats_remaining --;
      if(_repeats_remaining == 0) {
        _end_time = std::chrono::high_resolution_clock::now();
        PINF("Worker: %u complete", rank);
        return false;
      }
      _iterations = 1;
    }

    return true;
  }

};

#endif

