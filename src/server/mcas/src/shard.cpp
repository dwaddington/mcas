/*
  Copyright [2017-2019] [IBM Corporation]
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
//#define PROFILE_MAIN_LOOP
#define PROFILE_POST_ADO


#include "shard.h"

#include "resource_unavailable.h"
#include <api/components.h>
#include <api/kvindex_itf.h>
#include <boost/numeric/conversion/cast.hpp>
#include <common/dump_utils.h>
#include <common/utils.h>
#include <libpmem.h>
#include <nupm/mcas_mod.h>
#include <zlib.h>

#ifdef PROFILE
#include <gperftools/profiler.h>
#endif

#include <algorithm> /* remove */
#include <sstream>

using namespace mcas;

namespace mcas {
/* global parameters */
namespace Global {
unsigned debug_level = 0;

}

void Shard::thread_entry(const std::string& backend,
                         const std::string& index,
                         const std::string& pci_addr,
                         const std::string& dax_config,
                         const std::string& pm_path, unsigned debug_level,
                         const std::string ado_cores,
                         float ado_core_num) {
  if(_debug_level > 2)
    PLOG("shard:%u worker thread entered.", _core);

  /* pin thread */
  cpu_mask_t mask;
  mask.add_core(_core);
  set_cpu_affinity_mask(mask);

  try {
    initialize_components(backend, index, pci_addr, dax_config, pm_path,
                          debug_level, ado_cores, ado_core_num);

    main_loop();
  } catch (General_exception e) {
    PERR("Shard component initialization failed.");
  }

  if(_debug_level > 2)
    PLOG("Shard:%u worker thread exited.", _core);
}

void Shard::initialize_components(const std::string& backend,
                                  const std::string& index,
                                  const std::string& pci_addr,
                                  const std::string& dax_config,
                                  const std::string& pm_path,
                                  unsigned debug_level,
                                  const std::string ado_cores,
                                  float ado_core_num) {
  using namespace Component;

  /* STORE */
  {
    IBase *comp;

    if(backend == "mapstore")
      comp = load_component("libcomponent-mapstore.so", mapstore_factory);
    else if(backend == "hstore")
      comp = load_component("libcomponent-hstore.so", hstore_factory);
    else if(backend == "filestore")
      comp = load_component("libcomponent-filestore.so", filestore_factory);
    else if(backend == "dummystore")
      comp = load_component("libcomponent-dummystore.so", dummystore_factory);
    else
      throw General_exception("invalid backend (%s)", backend.c_str());

    if(_debug_level > 2)
      PLOG("Shard: using store backend (%s)", backend.c_str());

    if(!comp)
      throw General_exception("unable to initialize mcas backend component");

    IKVStore_factory *fact = static_cast<IKVStore_factory *>(
                                                             comp->query_interface(IKVStore_factory::iid()));
    assert(fact);

    if(backend == "hstore") {
      if(dax_config.empty())
        throw General_exception("hstore backend requires dax configuration");

      _i_kvstore = fact->create("owner", "name", dax_config);
      _store_requires_flush = true;
    } else if(backend == "nvmestore") {
      if(pci_addr.empty())
        throw General_exception(
                                "nvmestore backend needs pci device configuration");
      std::map<std::string, std::string> params;
      params["owner"] = "unknown-owner";
      params["name"] = "unknown-name";
      params["pci"] = pci_addr;
      params["pm_path"] = pm_path;
      params["persist_type"] = "hstore";
      _i_kvstore = fact->create(debug_level, params);
    } else if(backend == "pmstore") { /* components that support debug level */
      _i_kvstore = fact->create(debug_level, "owner", "name", "");
    } else if(backend == "filestore") {
      std::map<std::string, std::string> params;
      params["pm_path"] = pm_path;

      _i_kvstore = fact->create(debug_level, params);
    } else {
      _i_kvstore = fact->create("owner", "name");
    }
    fact->release_ref();
  }

  /* optional ADO components */
  {
    /* check XPMEM kernel module */    
    if(!check_xpmem_module())  {
      PMAJOR("XPMEM kernel module not found. Disabling ADO.");
      return;
    }

    IBase *comp = load_component("libcomponent-adomgrproxy.so",
                                 ado_manager_proxy_factory);
    if(comp) {
      IADO_manager_proxy_factory *fact =
        static_cast<IADO_manager_proxy_factory *>(
                                                  comp->query_interface(IADO_manager_proxy_factory::iid()));
      assert(fact);

      _i_ado_mgr = fact->create(_debug_level, _core, ado_cores, ado_core_num);
      if(_i_ado_mgr == nullptr)
        throw General_exception(
                                "Instantiation of ADO manager failed unexpectedly.");
      PMAJOR("ADO manager created.");

      fact->release_ref();
    }
    else {
      PMAJOR("ADO not found and thus not enabled.");
    }
  }
}

void Shard::main_loop() {
  using namespace mcas::Protocol;

  std::ostringstream ss;
  ss << "shard-" << _core;
  pthread_setname_np(pthread_self(),ss.str().c_str());
  assert(_i_kvstore);

#ifdef PROFILE_MAIN_LOOP
  ProfilerStart("shard_main_loop");
#endif

  static constexpr uint64_t CHECK_CONNECTION_INTERVAL = 10000000;

  Connection_handler::action_t action;

  unsigned idle = 0;
  uint64_t tick alignas(8) = 0;

  for (; _thread_exit == false; ++idle, ++tick) {
    /* check for new connections - but not too often */
    if(tick % CHECK_CONNECTION_INTERVAL == 0)
      check_for_new_connections();

#ifdef IDLE_CHECK
    if(idle > 1000)
      usleep(100000);
#endif

    /* if there are no sessions, sleep thread */
    if(_handlers.empty()) {
      usleep(500000);
      check_for_new_connections();
    }
    else {
      std::vector<Connection_handler *> pending_close;

      _stats.client_count = boost::numeric_cast<uint16_t>(
                                                          _handlers.size()); /* update stats client count */

      /* iterate connection handlers (each connection is a client session) */
      for (const auto handler : _handlers) {

        /* issue tick, unless we are stalling */
        auto tick_response = handler->tick();

        /* close session */
        if(tick_response == mcas::Connection_handler::TICK_RESPONSE_CLOSE) {
          idle = 0;

          /* close all open pools belonging to session  */
          if(_debug_level > 1)
            PLOG("Shard: forcing pool closures");

          if(_forced_exit) {
            PLOG("Shard: forcing exit..");
            _thread_exit = true;
          }

          for (auto &p : handler->pool_manager().open_pool_set()) {
            auto pool_id = p.first;
            /* close ADO process on pool close */
            if(ado_enabled()) {
              auto ado_itf = get_ado_interface(pool_id);
              ado_itf->shutdown();
              ado_itf->release_ref();
              _ado_map.erase(pool_id);
            }

            _i_kvstore->close_pool(pool_id);
            /* for debugging we force exit after pool closure */
          }

          if(_debug_level > 1)
            PMAJOR("Shard: closing connection %p", handler);
          pending_close.push_back(handler);
        }

        /* process ALL deferred actions */
        while (handler->get_pending_action(action)) {
          idle = 0;
          switch (action.op) {
          case Connection_handler::ACTION_RELEASE_VALUE_LOCK:
            if(_debug_level > 2)
              PLOG("releasing value lock (%p)", action.parm);
            release_locked_value(action.parm);
            break;
          default:
            throw Logic_exception("unknown action type");
          }
        }

        /* A process which cannot handle the top queue message due to
         * lack of resource may throw resource_unavailable, which will
         * leave the Protocol::Message on the queue for later
         * handling.
         */
        try
          {
            /* collect ALL available messages */
            while (Protocol::Message *p_msg = handler->peek_pending_msg()) {
              idle = 0;
              assert(p_msg);
              switch (p_msg->type_id) {
              case MSG_TYPE_IO_REQUEST:
                process_message_IO_request(handler, static_cast<Protocol::Message_IO_request *>(p_msg));
                break;
              case MSG_TYPE_ADO_REQUEST:
                process_ado_request(handler, static_cast<Protocol::Message_ado_request *>(p_msg));
                break;
              case MSG_TYPE_PUT_ADO_REQUEST:
                process_put_ado_request(handler, static_cast<Protocol::Message_put_ado_request *>(p_msg));
                break;
              case MSG_TYPE_POOL_REQUEST:
                process_message_pool_request(handler, static_cast<Protocol::Message_pool_request *>(p_msg));
                break;
              case MSG_TYPE_INFO_REQUEST:
                process_info_request(handler, static_cast<Protocol::Message_INFO_request *>(p_msg));
                break;
              default:
                throw General_exception("unrecognizable message type");
              }
              handler->free_buffer(handler->pop_pending_msg()); //recv_buffer();
              /* send_buffer may have been consumed. Refresh it. */
            }
          }
        catch ( const resource_unavailable &e )
          {
            PLOG("short of buffers in 'handler' processing: %s", e.what());
          }
      }

      /* handle messages send back from ADO */
      try {
        process_messages_from_ado();
      }
      catch ( const resource_unavailable &e ) {
        PLOG("short of buffers in 'ADO' processing: %s", e.what());
      }

      /* handle tasks */
      process_tasks(idle);

      /* handle pending close sessions */
      for (auto &h : pending_close) {

        _handlers.erase(std::remove(_handlers.begin(), _handlers.end(), h),
                        _handlers.end());

        if(_debug_level > 1) {
          PLOG("Deleting handler (%p)", h);
        }
        assert(h);
        delete h;

        if(_debug_level > 1)
          PLOG("# remaining handlers (%lu)", _handlers.size());
      }
    }
  }

  if(_debug_level > 1)
    PMAJOR("Shard (%p) exited", this);

#ifdef PROFILE
  ProfilerStop();
  ProfilerFlush();
#endif
}

void Shard::process_message_pool_request(Connection_handler *handler,
                                         Protocol::Message_pool_request *msg)
{
  using namespace Component;
  // validate auth id
  assert(msg->op);

  /* allocate response buffer */
  auto response_iob = handler->allocate();
  assert(response_iob);
  assert(response_iob->base());
  memset(response_iob->iov->iov_base, 0, response_iob->iov->iov_len);

  Pool_manager& pool_mgr = handler->pool_manager();
  
  Protocol::Message_pool_response *response = new (response_iob->base())
    Protocol::Message_pool_response(handler->auth_id());

  assert(response->version == Protocol::PROTOCOL_VERSION);
  response->set_status(S_OK);

  /* handle operation */
  if(msg->op == mcas::Protocol::OP_CREATE) {
    if(_debug_level > 1)
      PMAJOR("POOL CREATE: op=%u name=%s size=%lu obj-count=%lu", msg->op,
             msg->pool_name(), msg->pool_size, msg->expected_object_count);

    const std::string pool_name = msg->pool_name();

    IKVStore::pool_t pool;
    if(pool_mgr.check_for_open_pool(pool_name, pool)) {
      if(msg->flags & IMCAS::ADO_FLAG_CREATE_ONLY) {
        if(_debug_level)
          PWRN("request to create pool denied, create only specified on existing pool");
        response->pool_id = IKVStore::POOL_ERROR;
        response->set_status(E_FAIL);
      }
      else {
        pool_mgr.add_reference(pool);
      }
    }
    else {

      pool = _i_kvstore->create_pool(msg->pool_name(), msg->pool_size,
                                     msg->flags, msg->expected_object_count);

      if(pool == IKVStore::POOL_ERROR) {
        response->pool_id = 0;
        response->set_status(IKVStore::POOL_ERROR);
        PWRN("unable to create pool (%s)", pool_name.c_str());
      }
      else {
        /* register pool handle */
        pool_mgr.register_pool(pool_name,
                               pool,
                               msg->expected_object_count,
                               msg->pool_size,
                               msg->flags);

        response->pool_id = pool;
        response->set_status(S_OK);
      }

      if(_debug_level > 2)
        PLOG("OP_CREATE: new pool id: %lx", pool);

      /* check for ability to pre-register memory with RDMA stack */
      std::vector<::iovec> regions;
      status_t hr;
      if((hr = _i_kvstore->get_pool_regions(pool, regions)) == S_OK) {
        for (auto &r : regions) {
          if(_debug_level > 1)
            PLOG("region: %p %lu MiB", r.iov_base, REDUCE_MB(r.iov_len));
          /* pre-register memory region with RDMA */
          handler->ondemand_register(r.iov_base, r.iov_len);
        }
      }
      else {
        PLOG("pool region query NOT supported, using on-demand");
      }
    }

    if(_debug_level > 1)
      PMAJOR("POOL CREATE: pool_id=%lx", pool);

    if(pool && ado_enabled()) { /* if ADO is enabled start ADO process */
      IADO_proxy * ado = nullptr;
      pool_desc_t desc = {pool_name, msg->pool_size, msg->flags, msg->expected_object_count, false};
      conditional_bootstrap_ado_process(_i_kvstore, handler, pool, ado, desc);
    }
  }
  else if(msg->op == mcas::Protocol::OP_OPEN) {
    if(_debug_level > 1)
      PMAJOR("POOL OPEN: name=%s", msg->pool_name());

    IKVStore::pool_t pool;
    const std::string pool_name(msg->pool_name());

    /* check that pool is not already open */
    if(pool_mgr.check_for_open_pool(pool_name, pool)) {
      PLOG("reusing existing open pool (%p)", reinterpret_cast<void *>(pool));
      /* pool exists, increment reference */
      pool_mgr.add_reference(pool);
      response->pool_id = pool;
    }
    else {
      /* pool does not exist yet */
      pool = _i_kvstore->open_pool(msg->pool_name());

      if(pool == IKVStore::POOL_ERROR) {
        response->pool_id = 0;
        response->set_status(E_INVAL);
      }
      else {
        /* register pool handle */
        pool_mgr.register_pool(pool_name, pool, 0, 0, msg->flags);
        response->pool_id = pool;
      }
    }
    if(_debug_level > 1)
      PMAJOR("POOL OPEN: pool id: %lx", pool);

    if(pool != IKVStore::POOL_ERROR && ado_enabled()) { /* if ADO is enabled start ADO process */
      IADO_proxy * ado = nullptr;
      pool_desc_t desc = {pool_name, msg->pool_size, msg->flags, msg->expected_object_count, true};
      conditional_bootstrap_ado_process(_i_kvstore, handler, pool, ado, desc);
    }

  }
  else if(msg->op == mcas::Protocol::OP_CLOSE) {
    if(_debug_level > 1)
      PMAJOR("POOL CLOSE: pool_id=%lx", msg->pool_id);

    if(!pool_mgr.is_pool_open(msg->pool_id)) {
      response->set_status(E_INVAL);
    }
    else {
      /* release reference, if its zero, we can close pool for real */
      if(pool_mgr.release_pool_reference(msg->pool_id)) {

        if(_debug_level > 1)
          PLOG("Shard: pool reference now zero. pool_id=%lx", msg->pool_id);
      
        /* close ADO process on pool close */
        if(ado_enabled()) {
          auto ado_itf = get_ado_interface(msg->pool_id);
          ado_itf->shutdown();
          ado_itf->release_ref();
          _ado_map.erase(msg->pool_id);
        }

        auto rc = _i_kvstore->close_pool(msg->pool_id);
        if(_debug_level)
          PLOG("Shard: close_pool result:%d", rc);
        response->set_status(rc);

      }
      else {
        response->set_status(S_OK);
      }
    }
  }
  else if(msg->op == mcas::Protocol::OP_DELETE) {
    /* msg->pool_id make be invalid */
    if(msg->pool_id > 0 && pool_mgr.is_pool_open(msg->pool_id)) {

      if(_debug_level > 1)
        PMAJOR("POOL DELETE by handle: pool_id=%lx", msg->pool_id);

      try {
        if(pool_mgr.pool_reference_count(msg->pool_id) == 1) {
          auto pool_name = pool_mgr.pool_name(msg->pool_id);

          if(!pool_mgr.release_pool_reference(msg->pool_id))
            throw Logic_exception("unexpected pool reference count");

          /* notify ADO if needed */
          if(ado_enabled()) {
            auto ado_itf = get_ado_interface(msg->pool_id);

            /* send message to ADO, but perform closure only
               when a response is given back from the ADO.
               we can't block here though - the shard 
               thread must keep going to avoid cross-client
               degradation */
            ado_itf->send_op_event(ADO_op::POOL_DELETE);
          }
          else {
            /* close and delete pool */
            _i_kvstore->close_pool(msg->pool_id);
            response->set_status(_i_kvstore->delete_pool(pool_name));
          }
        }
        else {
          response->set_status(E_BUSY);
        }
      }
      catch(std::invalid_argument e) {
        throw e;
      }
    }
    /* try delete by pool name */
    else {
      if(_debug_level > 2)
        PMAJOR("POOL DELETE by name: name=%s", msg->pool_name());

      IKVStore::pool_t pool;
      const auto pool_name = msg->pool_name();

      response->pool_id = 0;
      /* check if pool is still open; return error if it is */
      if(pool_mgr.check_for_open_pool(pool_name, pool)) {
        if(_debug_level > 2)
          PWRN("Shard: pool delete on pool that is still open");
      
        response->set_status(IKVStore::E_ALREADY_OPEN);
      }
      else {
        response->set_status(_i_kvstore->delete_pool(msg->pool_name()));
      }
    }
  }
  else {
    throw Protocol_exception("process_message_pool_request - bad operation (msg->op = %d)", msg->op);
  }

  /* trim response length */
  response_iob->set_length(response->msg_len);

  /* finally, send response */
  handler->post_response(response_iob);
}

void Shard::add_locked_value(const pool_t pool_id,
                             Component::IKVStore::key_t key, void *target,
                             size_t target_len) {
  auto i = _locked_values.find(target);
  if(i == _locked_values.end()) {
    _locked_values[target] = {pool_id, key, 1, target_len};
  } else {
    _locked_values[target].count++;
  }
}

void Shard::release_locked_value(const void *target) {
  auto i = _locked_values.find(target); /* search by target address */
  if(i == _locked_values.end())
    throw Logic_exception(
                          "bad target to unlock value; value never locked? (%p)", target);

  if(i->second.count == 1) {

    /* we may need to flush store */
    if(_store_requires_flush) {
      /* do this by hand to bypass pmem_is_pmem check */
      pmem_flush(target, i->second.value_size);
      pmem_drain();
    }

    _i_kvstore->unlock(i->second.pool, i->second.key);
    _locked_values.erase(i);
  } else {
    i->second.count--;
  }
}

void Shard::process_message_IO_request(Connection_handler *handler,
                                       Protocol::Message_IO_request *msg) {
  using namespace Component;
  int status = S_OK;

  const auto iob = handler->allocate();
  assert(iob);

  _stats.op_request_count++;

  /////////////////////////////////////////////////////////////////////////////
  //   PUT ADVANCE   //
  /////////////////////
  if(msg->op == Protocol::OP_PUT_ADVANCE) {
    if(_debug_level > 2)
      PLOG("PUT_ADVANCE: (%p) key=(%.*s) value_len=%lu request_id=%lu", this,
           static_cast<int>(msg->key_len), msg->key(), msg->val_len,
           msg->request_id);

    /* open memory */
    void *target = nullptr;
    size_t target_len = msg->val_len;
    assert(target_len > 0);
    assert(msg->pool_id > 0);

    /* can't support dont stomp flag */
    if(msg->flags & IKVStore::FLAGS_DONT_STOMP) {
      status = E_INVAL;
      PWRN("PUT_ADVANCE failed IKVStore::FLAGS_DONT_STOMP not viable");
      _stats.op_failed_request_count++;
      goto send_response;
    }

    std::string k(msg->key(), msg->key_len);
    /* create (if needed) and lock value */
    Component::IKVStore::key_t key_handle;
    status_t rc = _i_kvstore->lock(msg->pool_id, k, IKVStore::STORE_LOCK_WRITE,
                                   target, target_len, key_handle);

    if(rc == E_FAIL || key_handle == Component::IKVStore::KEY_NONE) {
      PWRN("PUT_ADVANCE failed to lock value");
      status = E_INVAL;
      _stats.op_failed_request_count++;
      goto send_response;
    }

    if(target_len != msg->val_len) {
      PWRN("existing entry length does NOT equal request length");
      status = E_INVAL;
      _stats.op_failed_request_count++;
      goto send_response;
    }

    auto pool_id = msg->pool_id;

    /* register clean up task for value */
    add_locked_value(pool_id, key_handle, target, target_len);

    /* register memory unless pre-registered */
    Connection_base::memory_region_t region =
      handler->ondemand_register(target, target_len);

    /* set up value memory to receive value from network */
    handler->set_pending_value(target, target_len, region);

    /* update index ; position OK? */
    add_index_key(msg->pool_id, k);

    Protocol::Message_IO_response *response = new (iob->base())
      Protocol::Message_IO_response(iob->length(), handler->auth_id());
    response->request_id = msg->request_id;
    response->set_status(S_OK);

    iob->set_length(response->msg_len);

    handler->post_send_buffer(iob);

    /* update stats */
    _stats.op_put_direct_count++;

    return;
  }

 send_response:

  Protocol::Message_IO_response *response = new (iob->base())
    Protocol::Message_IO_response(iob->length(), handler->auth_id());

  /////////////////////////////////////////////////////////////////////////////
  //   PUT           //
  /////////////////////
  if(msg->op == Protocol::OP_PUT) {
    /* for basic 'puts' we have to do a memcpy - to support "in-place"
       puts for larger data, we use a two-stage operation
    */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

    if(_debug_level > 2)
      PMAJOR("PUT: (%p) key=(%.*s) value=(%.*s ...) len=(%lu)", this,
             static_cast<int>(msg->key_len), msg->key(),
             (min(msg->val_len, 20)), msg->value(), msg->val_len);

#pragma GCC diagnostic pop

    if(UNLIKELY(msg->resvd & mcas::Protocol::MSG_RESVD_SCBE)) {
      status = S_OK; // short-circuit backend
      if(_debug_level > 2)
        PLOG("PUT: short-circuited backend");
    }
    else {
      const std::string k(msg->key(), msg->key_len);

      status = _i_kvstore->put(msg->pool_id, k, msg->value(), msg->val_len,
                               msg->flags);

      if(_debug_level > 2) {
        if(status == E_ALREADY_EXISTS) {
          PLOG("kvstore->put returned E_ALREADY_EXISTS");
          _stats.op_failed_request_count++;
        } else {
          PLOG("kvstore->put returned %d", status);
        }
      }

      add_index_key(msg->pool_id, k);
    }
    /* update stats */
    _stats.op_put_count++;
  }
  /////////////////////////////////////////////////////////////////////////////
  //   GET           //
  /////////////////////
  else if(msg->op == Protocol::OP_GET) {
    if(_debug_level > 2)
      PMAJOR("GET: (%p) (request=%lu,buffer_size=%lu) key=(%.*s) ", this,
             msg->request_id, msg->val_len, int(msg->key_len), msg->key());

    if(msg->resvd & mcas::Protocol::MSG_RESVD_SCBE) {
      if(_debug_level > 2)
        PLOG("GET: short-circuited backend");
      response->data_len = 0;
      response->request_id = msg->request_id;
      response->set_status(S_OK);
      iob->set_length(response->base_message_size());
      handler->post_response(iob);
    } else {
      void *value_out = nullptr;
      size_t value_out_len = 0;
      size_t client_side_value_len = msg->val_len;
      bool is_direct = msg->resvd & Protocol::MSG_RESVD_DIRECT;
      std::string k(msg->key(), msg->key_len);

      Component::IKVStore::key_t key_handle;
      status_t rc = _i_kvstore->lock(msg->pool_id, k, IKVStore::STORE_LOCK_READ,
                                     value_out, value_out_len, key_handle);

      if(rc == E_FAIL ||
          key_handle == Component::IKVStore::KEY_NONE) { /* key not found */
        if(_debug_level > 2)
          PLOG("Shard: locking value failed");

        response->set_status(Component::IKVStore::E_KEY_NOT_FOUND);
        iob->set_length(response->base_message_size());
        handler->post_response(iob, nullptr);
        _stats.op_failed_request_count++;
        return;
      }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"

      if(_debug_level > 2)
        PLOG("Shard: locked OK: value_out=%p (%.*s ...) value_out_len=%lu",
             value_out, static_cast<int>(min(value_out_len, 20)),
             static_cast<char *>(value_out), value_out_len);

#pragma GCC diagnostic pop

      assert(value_out_len);
      assert(value_out);

      /* optimize based on size */
      if(!is_direct && (value_out_len < TWO_STAGE_THRESHOLD)) {
        /* value can fit in message buffer, let's copy instead of
           performing two-part DMA */
        if(_debug_level > 2)
          PLOG("Shard: performing memcpy for small get");

        iob->set_length(response->base_message_size() + value_out_len);
        memcpy(response->data, value_out, value_out_len);

        _i_kvstore->unlock(msg->pool_id, key_handle);

        response->data_len = value_out_len;
        response->request_id = msg->request_id;
        response->set_status(S_OK);

        assert(iob);
        handler->post_response(iob);

        _stats.op_get_count++;
      } else {

        if(_debug_level > 2)
          PLOG("Shard: get using two stage get response (value_out_len=%lu)",
               value_out_len);

        /* check if client has allocated sufficient space */
        if(client_side_value_len < value_out_len) {
          _i_kvstore->unlock(msg->pool_id, key_handle);
          response->set_status(E_INSUFFICIENT_SPACE);
          iob->set_length(response->base_message_size());
          handler->post_response(iob, nullptr);
          PWRN("Shard: responding with Client posted insufficient space.");
          _stats.op_failed_request_count++;
          return;
        }

        iob->set_length(response->base_message_size());

        /* register memory on-demand */
        auto region = handler->ondemand_register(value_out, value_out_len);
        assert(region);

        response->data_len = value_out_len;
        response->request_id = msg->request_id;
        response->set_status(S_OK);

        /* create value iob */
        buffer_t *value_buffer;

        assert(response->get_status() == S_OK);

        value_buffer = new buffer_t(value_out_len);
        value_buffer->iov =
          new iovec{static_cast<void *>(value_out), value_out_len};
        value_buffer->region = region;
        value_buffer->desc = handler->get_memory_descriptor(region);

        /* register clean up task for value */
        add_locked_value(msg->pool_id, key_handle, value_out, value_out_len);

        if(!is_direct && (value_out_len <= (handler->IO_buffer_size() -
                                             response->base_message_size()))) {
          if(_debug_level > 2)
            PLOG("posting response header and value together");

          /* post both buffers together in same response packet */
          handler->post_response(iob, value_buffer);
        } else {
          if(_debug_level > 2)
            PLOG("posting response with separate value following");

          /* for large gets we use a two-stage protocol sending
             response message and value separately
          */
          response->set_twostage_bit();

          /* send two separate packets for response and value */
          handler->post_response(iob);
          handler->post_send_value_buffer(value_buffer);
          handler->set_state_wait_send_value();
        }
        _stats.op_get_twostage_count++;
      }
    }
    return;
  }
  /////////////////////////////////////////////////////////////////////////////
  //   ERASE         //
  /////////////////////
  else if(msg->op == Protocol::OP_ERASE) {
    std::string k(msg->key(), msg->key_len);

    status = _i_kvstore->erase(msg->pool_id, k);

    if(status == S_OK)
      remove_index_key(msg->pool_id, k);
    else
      _stats.op_failed_request_count++;

    _stats.op_erase_count++;
  }
  /////////////////////////////////////////////////////////////////////////////
  //   CONFIGURE     //
  /////////////////////
  else if(msg->op == Protocol::OP_CONFIGURE) {
    if(_debug_level > 1)
      PMAJOR("Shard: pool CONFIGURE (%s)", msg->cmd());
    status = process_configure(msg);
  } else {
    throw Protocol_exception("operation not implemented");
  }

  response->request_id = msg->request_id;
  response->set_status(status);

  iob->set_length(response->msg_len);
  handler->post_response(iob); // issue IO request response
}

void Shard::process_info_request(Connection_handler *handler,
                                 Protocol::Message_INFO_request *msg) {
  if(msg->type == Protocol::INFO_TYPE_FIND_KEY) {
    if(_debug_level > 1)
      PLOG("Shard: INFO request INFO_TYPE_FIND_KEY (%s)", msg->c_str());

    if(_index_map == nullptr) { /* index does not exist */
      PLOG("Shard: cannot perform regex request, no index!! use "
           "configure('AddIndex::VolatileTree') ");
      const auto iob = handler->allocate();
      Protocol::Message_INFO_response *response =
        new (iob->base()) Protocol::Message_INFO_response(handler->auth_id());

      response->set_status(E_INVAL);
      handler->post_send_buffer(iob);
      return;
    }

    try {
      add_task_list(new Key_find_task(msg->c_str(), msg->offset, handler,
                                      _index_map->at(msg->pool_id)));
    } catch (...) {
      const auto iob = handler->allocate();
      Protocol::Message_INFO_response *response =
        new (iob->base()) Protocol::Message_INFO_response(handler->auth_id());

      response->set_status(E_INVAL);
      handler->post_send_buffer(iob);
      return;
    }

    return; /* response is not issued straight away */
  }

  const auto iob = handler->allocate();
  assert(iob);

  if(_debug_level > 1)
    PLOG("Shard: INFO request type:0x%X", msg->type);

  /* stats request handler */
  if(msg->type == Protocol::INFO_TYPE_GET_STATS) {

    Protocol::Message_stats *response =
      new (iob->base()) Protocol::Message_stats(handler->auth_id(), _stats);
    response->set_status(S_OK);
    iob->set_length(sizeof(Protocol::Message_stats));

    if(_debug_level > 1)
      dump_stats();

    handler->post_send_buffer(iob);
    return;
  }

  /* info requests */
  Protocol::Message_INFO_response *response =
    new (iob->base()) Protocol::Message_INFO_response(handler->auth_id());

  if(msg->type == Component::IKVStore::Attribute::COUNT) {
    response->value = _i_kvstore->count(msg->pool_id);
  } else if(msg->type == Component::IKVStore::Attribute::VALUE_LEN) {
    std::vector<uint64_t> v;
    std::string key = msg->key();
    auto hr = _i_kvstore->get_attribute(msg->pool_id, Component::IKVStore::Attribute::VALUE_LEN, v, &key);
    response->set_status(hr);

    if(hr == S_OK && v.size() == 1) {
      response->value = v[0];
    } else {
      PWRN("_i_kvstore->get_attribute failed");
      response->value = 0;
    }
    if(_debug_level > 1)
      PLOG("Shard: INFO reqeust INFO_TYPE_VALUE_LEN rc=%d val=%lu", hr,
           response->value);
  } else {
    std::vector<uint64_t> v;
    std::string key = msg->key();
    auto hr = _i_kvstore->get_attribute(msg->pool_id, static_cast<Component::IKVStore::Attribute>(msg->type), v,
                                        &key);
    response->set_status(hr);

    if(hr == S_OK && v.size() == 1) {
      response->value = v[0];
    } else {
      /* crc32 we can do here also */
      if(msg->type == Component::IKVStore::Attribute::CRC32) {
        response->set_status(S_OK);
        void *p = nullptr;
        size_t p_len = 0;
        Component::IKVStore::key_t key_handle;
        status_t rc = _i_kvstore->lock(msg->pool_id, key,
                                       Component::IKVStore::STORE_LOCK_READ, p,
                                       p_len, key_handle);

        if(rc < S_OK || key_handle == Component::IKVStore::KEY_NONE) {
          response->set_status(E_FAIL);
          response->value = 0;
        } else {
          /* do CRC */
          uint32_t crc = uint32_t(crc32(0, static_cast<const Bytef *>(p), uInt(p_len)));
          response->set_status(S_OK);
          response->value = crc;

          _i_kvstore->unlock(msg->pool_id, key_handle);
        }
      } else {
        PWRN("_i_kvstore->get_attribute failed");
        response->set_status(E_FAIL);
        response->value = 0;
      }
    }
    if(_debug_level > 1)
      PLOG("Shard: INFO reqeust INFO_TYPE_VALUE_LEN rc=%d val=%lu", hr,
           response->value);
  }

  iob->set_length(response->base_message_size());
  handler->post_send_buffer(iob);
  return;
}

void Shard::process_tasks(unsigned &idle) {
 retry:
  for (task_list_t::iterator i = _tasks.begin(); i != _tasks.end(); i++) {
    auto t = *i;
    assert(t);
    idle = 0;

    status_t s = t->do_work();
    if(s != Component::IKVStore::S_MORE) {

      auto handler = t->handler();
      auto response_iob = handler->allocate();
      assert(response_iob);
      Protocol::Message_INFO_response *response = new (response_iob->base())
        Protocol::Message_INFO_response(handler->auth_id());

      if(s == S_OK) {
        response->set_value(response_iob->length(), t->get_result(),
                            t->get_result_length());
        response->offset = t->matched_position();

        response->set_status(S_OK);
        response_iob->set_length(response->message_size());
      }
      else if(s == E_FAIL) {
        response->set_status(E_FAIL);
        response_iob->set_length(response->base_message_size());
      }
      else {
        throw Logic_exception("unexpected task condition");
      }

      handler->post_send_buffer(response_iob);
      _tasks.erase(i);

      goto retry;
    }
  }
}

void Shard::check_for_new_connections() {
  /* new connections are transferred from the connection handler
     to the shard thread */
  Connection_handler *handler;

  while ((handler = get_new_connection()) != nullptr) {
    if(_debug_level > 1)
      PMAJOR("Shard: processing new connection (%p)", handler);
    _handlers.push_back(handler);
  }
}

status_t Shard::process_configure(Protocol::Message_IO_request *msg) {
  using namespace Component;

  std::string command(msg->cmd());

  if(command.substr(0, 10) == "AddIndex::") {
    std::string index_str = command.substr(10);

    /* TODO: use shard configuration */
    if(index_str == "VolatileTree") {

      if(_index_map == nullptr)
        _index_map = new index_map_t();

      /* create index component and put into shard index map */
      IBase *comp =
        load_component("libcomponent-indexrbtree.so", rbtreeindex_factory);
      if(!comp)
        throw General_exception("unable to load libcomponent-indexrbtree.so");
      auto factory = static_cast<IKVIndex_factory *>(
                                                     comp->query_interface(IKVIndex_factory::iid()));
      assert(factory);

      std::ostringstream ss;
      ss << "auth_id:" << msg->auth_id;
      auto index = factory->create(ss.str(), "");
      assert(index);

      _index_map->insert(std::make_pair(reinterpret_cast<IKVStore::pool_t>(msg->pool_id), index));

      factory->release_ref();

      if(_debug_level > 1)
        PLOG("Shard: rebuilding volatile index ...");

      status_t hr;
      if((hr = _i_kvstore->map_keys(msg->pool_id,
                                     [&index](const std::string &key) {
                                       index->insert(key);
                                       return 0;
                                     })) != S_OK) {

        hr = _i_kvstore->map(msg->pool_id,
                             [&index](const void * key,
                                      const size_t key_len,
                                      const void * value,
                                      const size_t value_len) {
                               std::string k(reinterpret_cast<const char*>(key), key_len);
                               index->insert(k);
                               return 0;
                             });
      }

      return hr;
    } else {
      PWRN("unknown index (%s)", index_str.c_str());
      return E_BAD_PARAM;
    }
  } else if(command == "RemoveIndex::") {
    try {
      auto index = _index_map->at(msg->pool_id);
      _index_map->erase(msg->pool_id);
      delete index;
      if(_debug_level > 1)
        PLOG("Shard: removed index on pool (%lx)", msg->pool_id);
    } catch (...) {
      return E_BAD_PARAM;
    }

    return S_OK;
  } else {
    PWRN("unknown configure command (%s)", command.c_str());
    return E_BAD_PARAM;
  }

  return S_OK;
}

#include "shard_ado.cpp"

} // namespace mcas
