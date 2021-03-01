#ifndef __TASKS_H__
#define __TASKS_H__

#define MEMORY_TRANSFER_SANITY_CHECK

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
  bool        direct;
} Options;

struct record_t {
  std::string key;
  void * data;
};

component::IMCAS_factory * factory = nullptr;

class IOPS_base {
public:

  IOPS_base() {
    PINF("Value size:%u", Options.value_size);
  }
  
  unsigned long cleanup(unsigned rank)
  {
    PINF("Cleanup %u", rank);
    std::chrono::duration<double, std::milli> elapsed = _end_time - _start_time;
    auto secs = elapsed.count() / 1000.0;
    PINF("%f seconds duration", secs);
    auto iops = double(Options.pairs * Options.repeats) / secs;
    PINF("%f iops (rank=%u)", iops, rank);
    unsigned long i_iops = boost::numeric_cast<unsigned long>(iops);

    if(!Options.direct) {
      for (unsigned long i = 0; i < Options.pairs; i++)
        _store->free_memory(_data[i].data);
    }
    
    _store->close_pool(_pool);

    ::free(_value);
    delete [] _data;
    return i_iops;
  }

protected:
  std::chrono::high_resolution_clock::time_point _start_time, _end_time;
  unsigned long                                  _iterations = 0;
  bool                                           _first = true;
  component::Itf_ref<component::IKVStore>        _store;
  record_t *                                     _data;
  component::IKVStore::pool_t                    _pool;
  std::vector<void *>                            _get_results;
  unsigned                                       _repeats_remaining = Options.repeats;
  component::IMCAS::memory_handle_t              _memhandle;
  char *                                         _value;  
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

    PINF("Setting up data a priori: rank %u", rank);

    /* set up value and key space */
    _value = static_cast<char*>(aligned_alloc(64, Options.value_size));
    auto svalue = common::random_string(Options.value_size);
    memcpy(_value, svalue.data(), Options.value_size);

    if(Options.direct) {
      _memhandle = _store->register_direct_memory(_value, Options.value_size);
      assert(_memhandle != component::IMCAS::MEMORY_HANDLE_NONE);
    }
        
    for (unsigned long i = 0; i < Options.pairs; i++) {
      _data[i].key = common::random_string(Options.key_size);
    }
  }

  bool do_work(unsigned rank)
  {
    if (_first) {
      _first = false;
      PINF("Starting WRITE worker: rank %u", rank);
      _start_time = std::chrono::high_resolution_clock::now();     
    }

    status_t rc;

    if(Options.direct) {
      rc = _store->put_direct(_pool,
                              _data[_iterations].key,
                              _value,
                              Options.value_size,
                              _memhandle);
    }
    else {
      rc = _store->put(_pool,
                       _data[_iterations].key,
                       _value,
                       Options.value_size);
    }
      
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
      _iterations = 0;
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
    
    /* set up value and key space */
    _value = static_cast<char*>(aligned_alloc(64, Options.value_size));
    auto svalue = common::random_string(Options.value_size);
    memcpy(_value, svalue.data(), Options.value_size);

    if(Options.direct) {
      _memhandle = _store->register_direct_memory(_value, Options.value_size);
      assert(_memhandle != component::IMCAS::MEMORY_HANDLE_NONE);
    }

#ifdef MEMORY_TRANSFER_SANITY_CHECK
    memset(_value,0xA,Options.value_size);
#endif
    
    status_t rc;
    for (unsigned long i = 0; i < Options.pairs; i++) {
      
      _data[i].key = common::random_string(Options.key_size);

      /* write data in preparation for read */
      if(Options.direct) {
        rc = _store->put_direct(_pool,
                                _data[i].key,
                                _value, /* same value */
                                Options.value_size,
                                _memhandle);
      }
      else {
        rc = _store->put(_pool,
                         _data[i].key,
                         _value, /* same value */
                         Options.value_size);
      }      
        
      if (rc != S_OK)
        throw General_exception("put operation failed:rc=%d", rc);
    }
  }

  bool do_work(unsigned rank)
  {
    if (_first) {
      _first = false;
      PINF("Starting READ worker: rank %u", rank);
      _start_time = std::chrono::high_resolution_clock::now();
    }

    size_t out_value_size = 0;
    status_t rc;

#ifdef MEMORY_TRANSFER_SANITY_CHECK
    memset(_value,0xF,Options.value_size);
#endif

    if(Options.direct) {
      rc = _store->get_direct(_pool,
                              _data[_iterations].key,
                              _value,
                              out_value_size,
                              _memhandle);
    }
    else {      
      rc = _store->get(_pool,
                       _data[_iterations].key,
                       _data[_iterations].data,
                       out_value_size);      
    }

#ifdef MEMORY_TRANSFER_SANITY_CHECK
      for(unsigned i=0;i<Options.value_size;i++) {
        if(_value[i] != 0xA)
          throw General_exception("memory sanity check failed");
#endif

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
      _iterations = 0;
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

    /* set up value and key space */
    _value = static_cast<char*>(aligned_alloc(64, Options.value_size));
    auto svalue = common::random_string(Options.value_size);
    memcpy(_value, svalue.data(), Options.value_size);

    if(Options.direct) {
      _memhandle = _store->register_direct_memory(_value, Options.value_size);
      assert(_memhandle != component::IMCAS::MEMORY_HANDLE_NONE);
    }

    status_t rc;
    for (unsigned long i = 0; i < Options.pairs; i++) {
      
      _data[i].key = common::random_string(Options.key_size);

      /* write data in preparation for read */
      if(Options.direct) {
        rc = _store->put_direct(_pool,
                                _data[i].key,
                                _value, /* same value */
                                Options.value_size,
                                _memhandle);
      }
      else {
        rc = _store->put(_pool,
                         _data[i].key,
                         _value, /* same value */
                         Options.value_size);
      }
      
      if (rc != S_OK)
        throw General_exception("put operation failed:rc=%d", rc);
    }

  }

  virtual bool do_work(unsigned rank)
  {
    if (_first) {
      _first = false;
      PINF("Starting RW50 worker: rank %u", rank);
      _start_time = std::chrono::high_resolution_clock::now();
    }

    if (_iterations % 2 == 0) {
      size_t out_value_size = 0;
      status_t rc;

      if(Options.direct) {
        rc = _store->get_direct(_pool,
                                _data[_iterations].key,
                                _value,
                                out_value_size,
                                _memhandle);
      }
      else {
        rc = _store->get(_pool,
                         _data[_iterations].key,
                         _data[_iterations].data,
                         out_value_size);
      }

      if (rc != S_OK)
        throw General_exception("get operation failed: (key=%s) rc=%d", _data[_iterations].key.c_str(),rc);
    }
    else {
      status_t rc;

      if(Options.direct) {
        rc = _store->put_direct(_pool,
                                _data[_iterations].key,
                                _value,
                                Options.value_size,
                                _memhandle);        
      }
      else {
        rc = _store->put(_pool,
                         _data[_iterations].key,
                         _value,
                         Options.value_size);
      }
      
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
      _iterations = 0;
    }

    return true;
  }

};

#endif

