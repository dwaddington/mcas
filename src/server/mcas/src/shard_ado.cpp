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

/*< #included in shard.cpp */
#include <api/ado_itf.h>
#include <common/errors.h>
#include <common/exceptions.h>
#include <nupm/mcas_mod.h>

#include <cstdint> /* PRIu64 */
#include <sstream>

#include "config_file.h"  // includes rapidjson
#include "mcas_config.h"
#include "resource_unavailable.h"

#ifdef PROFILE
#include <gperftools/profiler.h>
#endif

//#define SHORT_CIRCUIT_ADO_HANDLING

status_t Shard::conditional_bootstrap_ado_process(Component::IKVStore*        kvs,
                                                  Connection_handler*         handler,
                                                  Component::IKVStore::pool_t pool_id,
                                                  Component::IADO_proxy*&     ado,
                                                  pool_desc_t&                desc)
{
  assert(pool_id);
  assert(kvs);
  assert(handler);

  /* ADO processes are instantiated on a per-pool basis.  First
     check if an ADO process already exists.
  */
  bool bootstrap = true;

  auto i = _ado_map.find(pool_id);

  if (i == _ado_map.end()) {
    /* need to launch new ADO process */
    std::vector<std::string> args;
    args.push_back("--plugins");

    std::string plugin_str;
    for (auto& plugin : *_ado_plugins) {
      args.push_back(plugin);
      plugin_str += plugin + " ";
    }
    args.push_back(plugin_str);

    PMAJOR("Shard: Launching with ADO path: (%s)", _ado_path.c_str());
    PMAJOR("Shard: ADO plugins: %s", plugin_str.c_str());

    ado = _i_ado_mgr->create(handler->auth_id(), kvs, pool_id,
                             desc.name,                // pool name
                             desc.size,                // pool_size,
                             desc.flags,               // const unsigned int pool_flags,
                             desc.expected_obj_count,  // const uint64_t expected_obj_count,
                             _ado_path, args, 0);

    if (_debug_level > 2) PLOG("ADO process launched OK.");

    assert(ado);
    _ado_map[pool_id] = std::make_pair(ado, handler);
  }
  else {
    ado       = i->second.first;
    bootstrap = false;
  }

  /* conditionally bootstrap ADO */
  if (bootstrap) {
    auto rc = ado->bootstrap_ado(desc.opened_existing);
    if (rc != S_OK) {
      return rc;
    }

    /* exchange memory mapping information */
    if (check_xpmem_module()) {  // nupm::check_mcas_kernel_module()) {

      std::vector<::iovec> regions;
      auto                 rc = _i_kvstore->get_pool_regions(pool_id, regions);
      if (rc != S_OK) {
        PWRN("cannot get pool regions; unable to map to ADO");
        return rc;
      }

      for (auto& r : regions) {
        r.iov_len            = round_up_page(r.iov_len);  // hack
        xpmem_segid_t seg_id = ::xpmem_make(r.iov_base, r.iov_len, XPMEM_PERMIT_MODE, reinterpret_cast<void*>(0666));
        if (seg_id == -1) throw Logic_exception("xpmem_make failed unexpectedly");

        ado->send_memory_map(seg_id, r.iov_len, r.iov_base);

        if (_debug_level > 2) PLOG("Shard_ado: exposed region: %p %lu", r.iov_base, r.iov_len);
      }
    }

#if defined(PROFILE) && defined(PROFILE_POST_ADO)
    PLOG("Starting profiler");
    ProfilerStart("post_ado_launch.prof");
#endif
  } /* end of bootstrap */

  return S_OK;
}

void Shard::process_put_ado_request(Connection_handler* handler, Protocol::Message_put_ado_request* msg)
{
  using namespace Component;

  IADO_proxy*     ado = nullptr;
  status_t        rc;
  IKVStore::key_t key_handle = 0;
  auto            locktype   = IKVStore::STORE_LOCK_WRITE;
  void*           value      = nullptr;
  size_t          value_len  = 0;
  const char*     key_ptr    = nullptr;
  bool            new_root   = false;

  static const auto error_func = [&](const char* message) {
    auto response_iob = handler->allocate();
    auto response     = new (response_iob->base())
        Protocol::Message_ado_response(response_iob->length(), E_FAIL, handler->auth_id(), msg->request_id);

    response->append_response(const_cast<char*>(message), strlen(message), 0 /* layer id */);

    response->set_status(E_INVAL);
    response_iob->set_length(response->message_size());
    handler->post_send_buffer(response_iob);
  };

#ifdef SHORT_CIRCUIT_ADO_HANDLING
  error_func("ADO!SC");
  return;
#endif

  if (!_i_ado_mgr) {
    error_func("ADO!NOT_ENABLED");
    return;
  }

  /* ADO should already be running */
  ado = _ado_map[msg->pool_id].first;
  if (!ado) throw General_exception("ADO is not running");

  if (msg->value_len() == 0) {
    error_func("ADO!ZERO_VALUE_LEN");
    return;
  }

  /* option ADO_FLAG_NO_OVERWRITE means that we don't copy
     value in if the key-value already exists */
  bool value_already_exists = false;
  if ((msg->flags & IMCAS::ADO_FLAG_NO_OVERWRITE) || (msg->flags & IMCAS::ADO_FLAG_DETACHED)) {
    std::vector<uint64_t> answer;
    std::string           key(msg->key());

    if (_i_kvstore->get_attribute(msg->pool_id, IKVStore::Attribute::VALUE_LEN, answer, &key) !=
        IKVStore::E_KEY_NOT_FOUND) {
      /* already exists */
      value_already_exists = true;
    }
  }

  /* if ADO_FLAG_DETACHED and we need to create root value */
  if ((msg->flags & IMCAS::ADO_FLAG_DETACHED) && (msg->root_val_len > 0)) {
    value_len = msg->root_val_len;

    status_t s = _i_kvstore->lock(msg->pool_id, msg->key(), locktype, value, value_len, key_handle, &key_ptr);
    if (s < S_OK) {
      error_func("ADO!ALREADY_LOCKED");
      return;
    }
    if (key_handle == IKVStore::KEY_NONE) throw Logic_exception("lock gave KEY_NONE");

    new_root = (s == S_OK_CREATED) ? true : false;
  }

  void*  detached_val_ptr = nullptr;
  size_t detached_val_len = 0;

  /* NOTE: this logic needs reviewing to ensure appropriate
     semantics for different flag combinations */

  if (msg->flags & IMCAS::ADO_FLAG_DETACHED) {
    auto size_to_allocate = round_up(msg->value_len(), 8);

    /* detached value request */
    rc = _i_kvstore->allocate_pool_memory(msg->pool_id, size_to_allocate, 8, /* alignment */
                                          detached_val_ptr);
    if (rc != S_OK) {
      PWRN("allocate_pool_memory for detached value failed (len=%lu, rc=%d)", size_to_allocate, rc);
      error_func("ADO!OUT_OF_MEMORY");
      return;
    }
    detached_val_len = size_to_allocate;
    memcpy(detached_val_ptr, msg->value(), msg->value_len());

    if (_debug_level > 2) PLOG("Shard_ado: allocated detached memory (%p,%lu)", detached_val_ptr, detached_val_len);
  }
  else if (value_already_exists && (msg->flags & IMCAS::ADO_FLAG_NO_OVERWRITE)) {
    /* do nothing, drop through */
  }
  else {
    /* write value passed with invocation message */
    rc = _i_kvstore->put(msg->pool_id, msg->key(), msg->value(), msg->value_len());
    if (rc != S_OK) throw Logic_exception("put_ado_invoke: put failed");
  }

  /*------------------------------------------------------------------
     Lock kv pair if needed, then create a work request and send to
     the ADO process via UIPC
  */
  if (!value) { /* now take the lock if not already locked */
    if (_i_kvstore->lock(msg->pool_id, msg->key(), locktype, value, value_len, key_handle, &key_ptr) != S_OK) {
      error_func("ADO!ALREADY_LOCKED");
      return;
    }
    if (key_handle == IKVStore::KEY_NONE) throw Logic_exception("lock gave KEY_NONE");
  }

  if (_debug_level > 2) PLOG("Shard_ado: locked KV pair (value=%p, value_len=%lu)", value, value_len);

  /* register outstanding work */
  auto wr = _wr_allocator.allocate();
  *wr     = {msg->pool_id, key_handle, key_ptr, msg->get_key_len(), locktype, msg->request_id, msg->flags};

  auto wr_key = reinterpret_cast<work_request_key_t>(wr); /* pointer to uint64_t */
  _outstanding_work.insert(wr_key);

  wmb();

  /* now send the work request */
  ado->send_work_request(wr_key, key_ptr, msg->get_key_len(), value, value_len, detached_val_ptr, detached_val_len,
                         msg->request(), msg->request_len(), new_root);

  if (_debug_level > 2) PLOG("Shard_ado: sent work request (len=%lu, key=%lx)", msg->request_len(), wr_key);
}

void Shard::process_ado_request(Connection_handler* handler, Protocol::Message_ado_request* msg)
{
  using namespace Component;

  IADO_proxy* ado;

  static const auto error_func = [&](status_t status, const char* message) {
    auto response_iob = handler->allocate();
    auto response     = new (response_iob->base())
        Protocol::Message_ado_response(response_iob->length(), status, handler->auth_id(), msg->request_id);
    response->append_response(const_cast<char*>(message), strlen(message), 0);
    response_iob->set_length(response->message_size());
    handler->post_send_buffer(response_iob);
  };

  if (_debug_level > 2) PLOG("Shard_ado: process_ado_request");

#ifdef SHORT_CIRCUIT_ADO_HANDLING
  error_func(E_INVAL, "ADO!SC");
  return;
#endif

  if (!ado_enabled()) {
    error_func(E_INVAL, "ADO!NOT_ENABLED");
    return;
  }

  if (msg->flags & IMCAS::ADO_FLAG_DETACHED) { /* not valid for plain invoke_ado */
    error_func(E_INVAL, "ADO!INVALID_ARGS");
    return;
  }

  void*  value     = nullptr;
  size_t value_len = msg->ondemand_val_len;

  /* handle ADO_FLAG_CREATE_ONLY - no invocation to ADO is made */
  if (msg->flags & IMCAS::ADO_FLAG_CREATE_ONLY) {
    std::vector<uint64_t> answer;
    std::string           key(msg->key());
    if (_i_kvstore->get_attribute(msg->pool_id, IKVStore::Attribute::VALUE_LEN, answer, &key) !=
        IKVStore::E_KEY_NOT_FOUND) {
      error_func(E_ALREADY_EXISTS, "ADO!ALREADY_EXISTS");
      if (_debug_level > 1) PWRN("process_ado_request: ADO_FLAG_CREATE_ONLY, key already exists");
      return;
    }

    IKVStore::key_t key_handle;
    status_t s = _i_kvstore->lock(msg->pool_id, msg->key(), IKVStore::STORE_LOCK_READ, value, value_len, key_handle);
    if (s < S_OK) {
      error_func(E_LOCKED, "ADO!ALREADY_LOCKED");
      if (_debug_level > 1) PWRN("process_ado_request: key already locked (ADO_FLAG_CREATE_ONLY)");
      return;
    }

    /* zero memory */
    pmem_memset(value, 0, value_len, 0);

    if (_i_kvstore->unlock(msg->pool_id, key_handle) != S_OK)
      throw Logic_exception("unable to unlock after lock");

    /* copy value address into response */
    auto response_iob = handler->allocate();
    auto response     = new (response_iob->base())
        Protocol::Message_ado_response(response_iob->length(), S_OK, handler->auth_id(), msg->request_id);
    response->append_response(&value, sizeof(value), 0 /* layer id */);
    response->set_status(S_OK);
    response_iob->set_length(response->message_size());
    handler->post_send_buffer(response_iob);

    // /* unlock key-value pair because we are not invoking ADO */
    // _i_kvstore->unlock(msg->pool_id, key_handle);

    return;  // end of ADO_FLAG_CREATE_ONLY condition
  }

  /* check for ADO_FLAG_CREATE_ON_DEMAND */
  // if(!(msg->flags & IMCAS::ADO_FLAG_CREATE_ON_DEMAND)) {
  //   std::vector<uint64_t> answer;
  //   std::string key(msg->key());
  //   if(_i_kvstore->get_attribute(msg->pool_id,
  //                                IKVStore::Attribute::VALUE_LEN,
  //                                answer,
  //                                &key) != IKVStore::E_KEY_NOT_FOUND) {
  //     error_func("ADO!ALREADY_EXISTS");
  //     if(option_DEBUG > 1)
  //       PWRN("process_ado_request: key already exists with ADO_FLAG_CREATE_ON_DEMAND");
  //     return;
  //   }
  // }

  /*  ADO should already be running */
  ado = _ado_map[msg->pool_id].first;
  assert(ado);

  /* get key-value pair */
  IKVStore::key_t key_handle;
  const char*     key_ptr = nullptr;

  auto     locktype = IKVStore::STORE_LOCK_WRITE;
  status_t s        = _i_kvstore->lock(msg->pool_id, msg->key(), locktype, value, value_len, key_handle, &key_ptr);
  if (s < S_OK) {
    error_func(E_LOCKED, "ADO!ALREADY_LOCKED");
    if (_debug_level > 1) PWRN("process_ado_request: key already locked");
    return;
  }

  if (key_handle == IKVStore::KEY_NONE) throw Logic_exception("lock gave KEY_NONE");

  if (_debug_level > 2) PLOG("Shard_ado: locked KV pair (value=%p, value_len=%lu)", value, value_len);

  /* register outstanding work */
  auto wr = _wr_allocator.allocate();
  *wr     = {msg->pool_id, key_handle, key_ptr, msg->get_key_len(), locktype, msg->request_id, msg->flags};

  auto wr_key = reinterpret_cast<work_request_key_t>(wr); /* pointer to uint64_t */
  _outstanding_work.insert(wr_key); /* save request by index on key-handle */

  wmb();

  /* now send the work request */
  ado->send_work_request(wr_key, key_ptr, msg->get_key_len(), value, value_len, nullptr, /* no payload */
                         0, msg->request(), msg->request_len(), (s == S_OK_CREATED));

  if (_debug_level > 2)
    PLOG("Shard_ado: sent work request (len=%lu, key=%lx, key_ptr=%p)", msg->request_len(), wr_key, key_ptr);

  /* for "asynchronous" calls we don't send a message
     for "synchronous call" we don't send a response to the client
     until the work completion has been picked up.  Of course this
     gives synchronous semantics on the client side.  We may need to
     extend this to asynchronous semantics for longer ADO
     operations */
}

/**
 * Handle messages coming back from the ADO process.
 *
 */
void Shard::process_messages_from_ado()
{
  using namespace Component;

  for (auto record : _ado_map) { /* for each ADO process */

    IADO_proxy*         ado     = record.second.first;
    Connection_handler* handler = record.second.second;

    assert(ado);
    assert(handler);

    {
      work_request_key_t request_key     = 0;
      status_t           response_status = E_FAIL;

      IADO_plugin::response_buffer_vector_t response_buffers;
      /* ADO work completion */
      while (ado->check_work_completions(request_key,
                                         response_status,
                                         response_buffers)) {

        if (response_status > S_USER0 || response_status < E_ERROR_BASE) response_status = E_FAIL;

        if (_debug_level > 2)
          PLOG("Shard_ado: check_work_completions(response_status=%d, response_count=%lu", response_status,
               response_buffers.size());

        auto work_item = _outstanding_work.find(request_key);
        if (work_item == _outstanding_work.end())
          throw General_exception("Shard_ado: bad work request key from ADO (0x%" PRIx64 ")", request_key);

        auto request_record = request_key_to_record(request_key);

        if (_debug_level > 2) {
          for (auto r : response_buffers) {
            PLOG("Shard_ado: returning response (%p,%lu,%d)", r.ptr, r.len, r.pool_ref);
          }
        }

        _outstanding_work.erase(work_item);

        /* unlock the KV pair */
        if (_i_kvstore->unlock(request_record->pool, request_record->key_handle) != S_OK)
          throw Logic_exception("Shard_ado: unlock for KV after ADO work completion failed");

        if (_debug_level > 2)
          PLOG("Shard_ado: unlocked KV pair (pool=%lx, key_handle=%p)", request_record->pool,
               static_cast<const void *>(request_record->key_handle));

        /* unlock deferred locks, e.g., resulting from table operation create */
        {
          std::vector<IKVStore::key_t> keys_to_unlock;
          ado->get_deferred_unlocks(request_key, keys_to_unlock);
          for (auto k : keys_to_unlock) {
            if (_i_kvstore->unlock(request_record->pool, k) != S_OK) throw Logic_exception("deferred unlock failed");

            if (_debug_level > 2) PLOG("Shard_ado: deferred unlock (%p)", static_cast<void*>(k));
          }
        }

        /* handle erasing target */
        if(response_status == IADO_plugin::S_ERASE_TARGET) {

          status_t s = _i_kvstore->erase(request_record->pool,
                                         std::string(request_record->key_ptr, request_record->key_len));
          if(s != S_OK)
            PWRN("Shard_ado: request to erase target failed unexpectedly (key=%s,rc=%d)", request_record->key_ptr, s);
          response_status = s;
        }
        
        /* for async, save failed requests */
        if (request_record->is_async()) {
          /* if the ADO operation response is bad, save it for
             later, otherwise don't do anything */
          if (response_status < S_OK) {
            if (_debug_level > 2) PWRN("Shard_ado: saving ADO completion failure");
            _failed_async_requests.push_back(request_record);
          }
          else {
            if (_debug_level > 2) PWRN("Shard_ado: async ADO completion OK!");
          }
        }
        else /* for sync, give response */
        {
          auto iob = handler->allocate();

          auto response_msg = new (iob->base()) Protocol::Message_ado_response(
              iob->length(), response_status, handler->auth_id(), request_record->request_id);

          /* TODO: for the moment copy pool buffers in, we should
             be able to do zero copy though.
           */
          size_t appended_buffer_size = 0;

          for (auto& rb : response_buffers) {
            assert(rb.ptr);
            response_msg->append_response(rb.ptr, boost::numeric_cast<uint32_t>(rb.len), rb.layer_id);
            appended_buffer_size += rb.len;
          }

          iob->set_length(response_msg->message_size());

          handler->post_send_buffer(iob);
        }
        _wr_allocator.free_wr(request_record);
      }
    }

    uint64_t     work_id = 0; /* maps to record of pool, key handle, lock type, request id etc. */
    ADO_op       op      = ADO_op::UNDEFINED;
    std::string  key, key_expression;
    size_t       value_len      = 0;
    size_t       align_or_flags = 0;
    void*        addr           = nullptr;
    offset_t     begin_pos      = 0;
    int          find_type      = 0;
    uint32_t     max_comp       = 0;
    epoch_time_t t_begin = 0, t_end = 0;
    Component::IKVStore::pool_iterator_t iterator   = nullptr;
    Component::IKVStore::key_t           key_handle = nullptr;
    Buffer_header*                       buffer;

    /* process callbacks from ADO */
    while (ado->recv_callback_buffer(buffer) == S_OK) {
      /* handle TABLE OPERATIONS */
      if (ado->check_table_ops(buffer, work_id, op, key, value_len, align_or_flags, addr)) {
        switch (op) {
          case ADO_op::CREATE: {
            std::vector<uint64_t> val;
            if (_i_kvstore->get_attribute(ado->pool_id(), IKVStore::VALUE_LEN, val, &key) !=
                IKVStore::E_KEY_NOT_FOUND) {
              if (_debug_level > 3) PWRN("Shard_ado: table op CREATE, key-value pair already exists");

              if (align_or_flags & IKVStore::FLAGS_CREATE_ONLY) {
                ado->send_table_op_response(E_ALREADY_EXISTS, nullptr, 0, nullptr);
                break;
              }
            }
            goto open; /* stop compiler complaining about flow through*/
          }
          case ADO_op::OPEN:
          open : {
            if (_debug_level > 2) PLOG("Shard_ado: received table op create/open (%s)", key.c_str());

            IKVStore::key_t key_handle;
            void*           value = nullptr;
            const char*     key_ptr;

            bool invoke_completion_unlock = !(align_or_flags & IADO_plugin::FLAGS_ADO_LIFETIME_UNLOCK);

            status_t rc = _i_kvstore->lock(ado->pool_id(), key, IKVStore::STORE_LOCK_WRITE, value, value_len,
                                           key_handle, &key_ptr);

            if (rc < S_OK || key_handle == nullptr) { /* to fix, store should return error code */
              if (_debug_level > 2) PLOG("Shard_ado: locked failed");
              ado->send_table_op_response(rc);
            }
            else {
              if (_debug_level > 2)
                PLOG("Shard_ado: locked KV pair (keyhandle=%p, value=%p,len=%lu) invoke_completion_unlock=%d",
                     static_cast<void*>(key_handle), value, value_len, invoke_completion_unlock);

              add_index_key(ado->pool_id(), key);

              /* auto-unlock means we add a deferred unlock that happens after
                 the ado invocation (identified by work_id) has completed. */
              if (align_or_flags & IADO_plugin::FLAGS_NO_IMPLICIT_UNLOCK) {
                if (_debug_level > 2) PLOG("Shard_ado: locked (%s) without implicit unlock", key.c_str());
              }
              else if (invoke_completion_unlock) { /* unlock on ADO invoke completion */
                if (work_id == 0) {
                  ado->send_table_op_response(E_INVAL);
                }
                else {
                  try {
                    ado->add_deferred_unlock(work_id, key_handle);
                  }
                  catch (std::range_error &) {
                    ado->send_table_op_response(E_MAX_REACHED);
                  }
                }
              }
              else { /* unlock at ADO process shutdown */
                ado->add_life_unlock(key_handle);
              }

              assert(reinterpret_cast<uint64_t>(addr) <= 1);

              ado->send_table_op_response(S_OK, static_cast<void*>(value), value_len, key_ptr, key_handle);
            }
          } break;
          case ADO_op::ERASE: {
            if (_debug_level > 2) PLOG("Shard_ado: received table op erase");
            ado->send_table_op_response(_i_kvstore->erase(ado->pool_id(), key));
            break;
          } 
          case ADO_op::VALUE_RESIZE: /* resize only allowed on current work invocation target */
          {
            if (_debug_level > 2)
              PLOG("Shard_ado: received table op resize value (work_id=%p)",
                   reinterpret_cast<const void*>(work_id));

            /* for resize, we need unlock, resize, and then relock */
            auto work_item = _outstanding_work.find(work_id);

            if (work_item == _outstanding_work.end()) {
              ado->send_table_op_response(E_INVAL);
              break;
            }

            /* use the work id to get the key handle */
            work_request_t* wr      = request_key_to_record(work_id);
            const char*     key_ptr = nullptr;
            status_t        rc;

            if (!wr) throw Logic_exception("unable to get request from work_id");

            if ((rc = _i_kvstore->unlock(ado->pool_id(), wr->key_handle)) != S_OK) {
              ado->send_table_op_response(rc);
              break;
            }

            if (_debug_level > 2) PLOG("Shard_ado: table op resize, unlocked");

            /* perform resize */
            void*  new_value      = nullptr;
            size_t new_value_len  = 0;
            auto   old_key_handle = wr->key_handle;
            rc                    = _i_kvstore->resize_value(ado->pool_id(), key, value_len, align_or_flags);

            if (_i_kvstore->lock(ado->pool_id(), key, IKVStore::STORE_LOCK_WRITE, new_value, new_value_len,
                                 wr->key_handle /* update key handle in record */, &key_ptr) != S_OK)
              throw Logic_exception("ADO OP_RESIZE request failed to relock");

            /* update deferred locks */
            if (ado->update_deferred_unlock(work_id, wr->key_handle) != S_OK) {
              if (ado->remove_life_unlock(old_key_handle) == S_OK) ado->add_life_unlock(wr->key_handle);
            }

            ado->send_table_op_response(rc, new_value, new_value_len, key_ptr);
            break;
          }
          case ADO_op::ALLOCATE_POOL_MEMORY: {
            status_t rc;
            assert(work_id == 0);
            /* work request is not needed */
            void* out_addr = nullptr;
            rc             = _i_kvstore->allocate_pool_memory(ado->pool_id(), value_len, align_or_flags, out_addr);

            if (_debug_level > 2)
              PLOG("Shard_ado: allocate_pool_memory align_or_flags=%lu rc=%d addr=%p", align_or_flags, rc, out_addr);
            ado->send_table_op_response(rc, out_addr);
            break;
          } 
          case ADO_op::FREE_POOL_MEMORY: {
            assert(work_id == 0);
            /* work request is not needed */
            if (value_len == 0) {
              ado->send_table_op_response(E_INVAL);
              break;
            }

            status_t rc = _i_kvstore->free_pool_memory(ado->pool_id(), addr, value_len);
            if (_debug_level > 2) PLOG("Shard_ado : allocate_pool_memory free rc=%d", rc);

            if (rc != S_OK) PWRN("Shard_ado: Table operation OP_FREE failed");

            ado->send_table_op_response(rc);
            break;
          } 
          default:
            throw Logic_exception("unknown table op code");
        }
        // end of if(check_table_ops..
      }
      else if (ado->check_pool_info_op(buffer)) {
        using namespace rapidjson;

        uint64_t     expected_obj_count = 0;
        size_t       pool_size          = 0;
        unsigned int pool_flags         = 0;
        handler->pool_manager().get_pool_info(ado->pool_id(), expected_obj_count, pool_size, pool_flags);

        try {
          Document doc;
          doc.SetObject();

          Value pool_size_v(pool_size);
          doc.AddMember("pool_size", pool_size_v, doc.GetAllocator());
          Value expected_obj_count_v(expected_obj_count);
          doc.AddMember("expected_obj_count", expected_obj_count_v, doc.GetAllocator());
          Value pool_flags_v(pool_flags);
          doc.AddMember("pool_flags", pool_flags_v, doc.GetAllocator());
          std::vector<uint64_t> v64;
          if (_i_kvstore->get_attribute(ado->pool_id(), IKVStore::Attribute::COUNT, v64) == S_OK) {
            Value obj_count_v(v64[0]);
            doc.AddMember("current_object_count", obj_count_v, doc.GetAllocator());
          }
          std::stringstream      ss;
          OStreamWrapper         osw(ss);
          Writer<OStreamWrapper> writer(osw);
          doc.Accept(writer);
          ado->send_pool_info_response(S_OK, ss.str());
        }
        catch (...) {
          throw Logic_exception("pool info JSON creation failed");
        }
      }
      else if (ado->check_op_event_response(buffer, op)) {
        switch (op) {
          case ADO_op::POOL_DELETE: {
            /* close pool, then delete */
            if ((_i_kvstore->close_pool(ado->pool_id()) != S_OK) || (_i_kvstore->delete_pool(ado->pool_name()) != S_OK))
              throw Logic_exception("unable to delete pool after POOL DELETE op event");

            if (_debug_level > 2) PLOG("POOL DELETE op event completion");

            break;
          }
          default:
            throw Logic_exception("unknown op event");
        }
      }
      else if (ado->check_iterate(buffer, t_begin, t_end, iterator)) {
        Component::IKVStore::pool_reference_t ref;
        if (!iterator) {
          iterator = _i_kvstore->open_pool_iterator(ado->pool_id());
        }

        if (!iterator) { /* still no iterator, component doesn't support */
          ado->send_iterate_response(E_NOT_IMPL, iterator, ref);
        }
        else {
          status_t rc;
          bool     time_match = false;
          do {
            rc = _i_kvstore->deref_pool_iterator(ado->pool_id(), iterator, t_begin, /* time constraints */
                                                 t_end, ref, time_match, true);

            if (rc == E_OUT_OF_BOUNDS) {
              _i_kvstore->close_pool_iterator(ado->pool_id(), iterator);
              break;
            }
          } while (!time_match); /* TODO: limit number of iterations */

          if (_debug_level > 2) PLOG("Shard_ado: iterator timestamp (%lu)", ref.timestamp);

          ado->send_iterate_response(rc, iterator, ref);
        }
      }
      else if (ado->check_vector_ops(buffer, t_begin, t_end)) {
        /* WARNING: this could block the shard thread. we may
           neeed to make it a "task" - but we can't do this
           without a map iterator that can be restarted.
        */
        /* vector operation, collect all key-value pointers */
        status_t                      rc;
        size_t                        count  = 0;
        void*                         buffer = nullptr;
        IADO_plugin::Reference_vector v;

        /* allocate memory from pool for the vector */
        if (t_begin > 0 || t_end > 0) {
          /* we have to map first to get count */
          rc = _i_kvstore->map(
              ado->pool_id(),
              [&count](const void*, const size_t, const void*, const size_t, const tsc_time_t) -> int {
                count++;
                return 0;
              },
              t_begin, t_end);
          if (_debug_level > 2) PLOG("map time constraints: count=%lu", count);
        }
        else {
          count = _i_kvstore->count(ado->pool_id());
        }

        auto buffer_size = IADO_plugin::Reference_vector::size_required(count);
        rc               = _i_kvstore->allocate_pool_memory(ado->pool_id(), buffer_size, 0, buffer);

        if (rc != S_OK) {
          ado->send_vector_response(rc, IADO_plugin::Reference_vector());
        }
        else {
          /* populate vector */
          IADO_plugin::kv_reference_t* ptr   = static_cast<IADO_plugin::kv_reference_t*>(buffer);
          size_t                       check = 0;

          if (t_begin == 0 && t_end == 0) {
            rc = _i_kvstore->map(ado->pool_id(),
                                 [count, &check, &ptr](const void* key, const size_t key_len, const void* value,
                                                       const size_t value_len) -> int {
                                   assert(key);
                                   assert(key_len);
                                   assert(value);
                                   assert(value_len);
                                   if (check > count) return -1;
                                   ptr->key       = const_cast<void*>(key);
                                   ptr->key_len   = key_len;
                                   ptr->value     = const_cast<void*>(value);
                                   ptr->value_len = value_len;
                                   ptr++;
                                   check++;
                                   return 0;
                                 });
          }
          else {
            rc = _i_kvstore->map(
                ado->pool_id(),
                [count, &check, &ptr](const void* key, const size_t key_len, const void* value, const size_t value_len,
                                      const tsc_time_t timestamp) -> int {
                  assert(key);
                  assert(key_len);
                  assert(value);
                  assert(value_len);
                  if (check > count) return -1;
                  ptr->key       = const_cast<void*>(key);
                  ptr->key_len   = key_len;
                  ptr->value     = const_cast<void*>(value);
                  ptr->value_len = value_len;
                  ptr++;
                  check++;
                  return 0;
                },
                t_begin, t_end);
          }

          ado->send_vector_response(rc, IADO_plugin::Reference_vector(count, buffer, buffer_size));
        }
      }
      else if (ado->check_index_ops(buffer, key_expression, begin_pos, find_type, max_comp)) {
        status_t rc;
        auto     i_kvindex = lookup_index(ado->pool_id());

        if (!i_kvindex) {
          PWRN("ADO index operation: no index enabled");
          ado->send_find_index_response(E_NO_INDEX, 0, "noindex");
        }
        else {
          std::string matched_key;
          offset_t    matched_pos = -1;

          rc = i_kvindex->find(key_expression, begin_pos, IKVIndex::convert_find_type(find_type), matched_pos,
                               matched_key, MAX_INDEX_COMPARISONS);

          ado->send_find_index_response(rc, matched_pos, matched_key);
        }
      }
      else if (ado->check_unlock_request(buffer, work_id, key_handle)) {
        if (_debug_level > 2) PLOG("ADO callback: unlock request (work_id=%lx, handle=%p", work_id, static_cast<const void *>(key_handle));

        /* unlock should fail if implicit unlock exists, i.e.  it
           should only be performed on locks taken via
           FLAGS_NO_IMPLICIT_UNLOCK */
        if (key_handle == nullptr || ado->check_for_implicit_unlock(work_id, key_handle)) {
          ado->send_unlock_response(E_INVAL);
        }
        else {
          ado->send_unlock_response(_i_kvstore->unlock(ado->pool_id(), key_handle));
        }
      }
      else {
        throw Logic_exception("Shard_ado: bad op request from ADO plugin");
      }

      /* release buffer */
      ado->free_callback_buffer(buffer);
    }
  }
}
