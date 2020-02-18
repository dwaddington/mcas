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

#include "ado.h"
#include "ado_proto.h"
#include "ado_ipc_proto.h"
#include "ado_proto_buffer.h"
#include "resource_unavailable.h"

#include <common/logging.h>
#include <common/exceptions.h>
#include <common/utils.h>
#include <common/dump_utils.h>
#include <api/interfaces.h>
#include <nupm/mcas_mod.h>
#include <atomic>
#include <boost/program_options.hpp>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <queue>
#include <sched.h>
#include <cstdio>
#include <cstdlib>
#include <sys/resource.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <xpmem.h>

using namespace Component;

// helpers

static bool check_xpmem_kernel_module()
{
  int fd = open("/dev/xpmem", O_RDWR, 0666);
  close(fd);
  return (fd != -1);
}

/**
 * Class to manage plugins
 */
class ADO_plugin_mgr
{
public:
  explicit ADO_plugin_mgr(const std::vector<std::string>& plugin_vector,
                          IADO_plugin::Callback_table cb_table)
    : _i_plugins{}
  {
    for(auto& ppath : plugin_vector) {
      auto i_plugin = static_cast<IADO_plugin*>(load_component(ppath.c_str(),
                                                               Interface::ado_plugin));
      if(!i_plugin)
        throw General_exception("unable to load ADO plugin (%s)", ppath.c_str());

      i_plugin->register_callbacks(cb_table);
      PLOG("ADO: plugin loaded OK! (%s)", ppath.c_str());
      _i_plugins.push_back(i_plugin);
    }
  }

  virtual ~ADO_plugin_mgr() {
  }

  void shutdown() {
    for(auto i: _i_plugins) i->shutdown();
  }

  status_t register_mapped_memory(void *shard_vaddr,
                                  void *local_vaddr,
                                  size_t len) {
    status_t s = S_OK;
    for(auto i: _i_plugins) {
      s |= i->register_mapped_memory(shard_vaddr,
                                     local_vaddr,
                                     len);
    }
    return s;
  }

  status_t do_work(const uint64_t work_key,
                   const char * key,
                   size_t key_len,
                   IADO_plugin::value_space_t& values,
                   const void *in_work_request,
                   const size_t in_work_request_len,
                   const bool new_root,
                   IADO_plugin::response_buffer_vector_t& response_buffers) {
    status_t s = S_OK;
    for(auto i: _i_plugins) {
      s |= i->do_work(work_key, key, key_len, values,
                      in_work_request,
                      in_work_request_len,
                      new_root,
                      response_buffers);
    }
    return s;
  }

  void launch_event(const uint64_t auth_id,
                    const std::string& pool_name,
                    const size_t pool_size,
                    const unsigned int pool_flags,
                    const size_t expected_obj_count) {

    for(auto i: _i_plugins)
      i->launch_event(auth_id,
                      pool_name,
                      pool_size,
                      pool_flags,
                      expected_obj_count);
  }

  void notify_op_event(ADO_op op) {
    for(auto i: _i_plugins)
      i->notify_op_event(op);
  }
  
private:
  std::vector<IADO_plugin*> _i_plugins;
};



/**
 * Main entry point
 *
 * @param argc
 * @param argv
 *
 * @return
 */
int main(int argc, char* argv[])
{
  std::string plugins, channel_id;
  unsigned debug_level;
  std::string cpu_mask;

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Print help")
      ("plugins", po::value<std::string>(&plugins)->required(), "ADO plugins")
      ("channel_id", po::value<std::string>(&channel_id)->required(), "Channel (prefix) identifier")
      ("debug", po::value<unsigned>(&debug_level)->default_value(0), "Debug level")
      ("cpumask", po::value<std::string>(&cpu_mask), "Cores to restrict threads to (string form)")
      ;

    po::variables_map vm;
    try {
      po::store(po::parse_command_line(argc, argv, desc), vm);
      if (vm.count("help")) {
        std::cout << desc << std::endl;
        return 0;
      }
      po::notify(vm);
    }
    catch (po::error &e) {
      std::cerr << e.what() << std::endl;
      std::cerr << desc << std::endl;
      return -1;
    }
  }
  catch (std::exception &e) {
    std::cerr << e.what() << std::endl;
    return -1;
  }

  ADO_protocol_builder ipc(channel_id, ADO_protocol_builder::Role::ACCEPT);

  /* Callback functions */

  auto ipc_create_key =
    [&ipc] (const uint64_t work_request_id,
            const std::string& key_name,
            const size_t value_size,
            const int flags,
            void*& out_value_addr,
            const char ** out_key_ptr,
            Component::IKVStore::key_t * out_key_handle) -> status_t
    {
      status_t rc;
      ipc.send_table_op_create(work_request_id, key_name, value_size, flags);
      ipc.recv_table_op_response(rc, out_value_addr, nullptr /* value len */, out_key_ptr, out_key_handle);
      return rc;
    };

  auto ipc_open_key =
    [&ipc] (const uint64_t work_request_id,
            const std::string& key_name,
            const int flags,
            void*& out_value_addr,
            size_t& out_value_len,
            const char** out_key_ptr,
            Component::IKVStore::key_t * out_key_handle) -> status_t
    {
      status_t rc;
      ipc.send_table_op_open(work_request_id, key_name, flags);
      ipc.recv_table_op_response(rc, out_value_addr, &out_value_len, out_key_ptr, out_key_handle);
      return rc;
    };

  auto ipc_erase_key =
    [&ipc] (const std::string& key_name) -> status_t
    {
      status_t rc;
      void* na;
      ipc.send_table_op_erase(key_name);
      ipc.recv_table_op_response(rc, na);
      return rc;
    };

  auto ipc_resize_value =
    [&ipc] (const uint64_t work_request_id,
            const std::string& key_name,
            const size_t new_value_size,
            void*& out_new_value_addr) -> status_t
    {
      status_t rc;
      ipc.send_table_op_resize(work_request_id, key_name, new_value_size);
      ipc.recv_table_op_response(rc, out_new_value_addr);
      return rc;
    };


  auto ipc_allocate_pool_memory =
    [&ipc] (const size_t size,
            const size_t alignment,
            void *&out_new_addr) -> status_t
    {
      status_t rc;
      ipc.send_table_op_allocate_pool_memory(size, alignment);
      ipc.recv_table_op_response(rc, out_new_addr);
      return rc;
    };

  auto ipc_free_pool_memory =
    [&ipc] (const size_t size,
            const void * addr) -> status_t
    {
      status_t rc;
      void * na;
      ipc.send_table_op_free_pool_memory(addr, size);
      ipc.recv_table_op_response(rc, na);
      return rc;
    };

  auto ipc_find_key =
    [&ipc] (const std::string& key_expression,
            const offset_t begin_position,
            const Component::IKVIndex::find_t find_type,
            offset_t& out_matched_position,
            std::string& out_matched_key) -> status_t
    {
      status_t rc;
      ipc.send_find_index_request(key_expression,
                                  begin_position,
                                  find_type);

      ipc.recv_find_index_response(rc,
                                   out_matched_position,
                                   out_matched_key);

      return rc;
    };

  auto ipc_get_reference_vector =
    [&ipc] (const epoch_time_t t_begin,
            const epoch_time_t t_end,
            IADO_plugin::Reference_vector& out_vector) -> status_t
    {
      status_t rc;
      ipc.send_vector_request(t_begin, t_end);
      ipc.recv_vector_response(rc, out_vector);
      return rc;
    };

  auto ipc_get_pool_info =
    [&ipc] (std::string& out_response) -> status_t
    {
      status_t rc;
      ipc.send_pool_info_request();
      ipc.recv_pool_info_response(rc, out_response);
      return rc;
    };

  auto ipc_iterate =
    [&ipc] (const epoch_time_t t_begin,
            const epoch_time_t t_end,
            Component::IKVStore::pool_iterator_t& iterator,
            Component::IKVStore::pool_reference_t& reference) -> status_t
    {
      status_t rc;
      ipc.send_iterate_request(t_begin, t_end, iterator);
      ipc.recv_iterate_response(rc, iterator, reference);
      return rc;
    };

  auto ipc_unlock =
    [&ipc] (const uint64_t work_id,
            Component::IKVStore::key_t key_handle) -> status_t
    {
      status_t rc;
      if(work_id == 0 || key_handle == nullptr) return E_INVAL;
      ipc.send_unlock_request(work_id, key_handle);
      ipc.recv_unlock_response(rc);
      return rc;
    };



  /* load plugin and register callbacks */
  std::vector<std::string> plugin_vector;
  char *token = std::strtok(const_cast<char*>(plugins.c_str()), " ");
  while (token) {
    plugin_vector.push_back(token);
    token = std::strtok(NULL, " ");
  }

  /* load plugins */
  ADO_plugin_mgr plugin_mgr(plugin_vector,
                            IADO_plugin::Callback_table{
                              ipc_create_key,
                                ipc_open_key,
                                ipc_erase_key,
                                ipc_resize_value,
                                ipc_allocate_pool_memory,
                                ipc_free_pool_memory,
                                ipc_get_reference_vector,
                                ipc_find_key,
                                ipc_get_pool_info,
                                ipc_iterate,
                                ipc_unlock});
  
  /* main loop */
  unsigned long count = 0;
  bool exit = false;

  if(cpu_mask.empty() == false) {
    cpu_mask_t m;
    if(string_to_mask(cpu_mask, m) == S_OK) {
      if(set_cpu_affinity_mask(m) == -1)
        throw Logic_exception("bad mask parameter");
      PLOG("ADO process configured with cpu mask: %s", cpu_mask.c_str());
    }
  }

  PLOG("ADO process: main thread (%lu)", pthread_self());

  while (!exit) {

    /* main loop servicing incoming IPC requests */

    if(debug_level > 2)
      PLOG("ADO process: waiting for message (%lu)", count);

    Buffer_header * buffer = nullptr; /* recv will dequeue this */

    /* poll until there is a request, sleep on too much polling  */
    auto st = ipc.poll_recv_sleep(buffer);
    assert(st == S_OK);
    assert(buffer);

    if(debug_level > 2)
      PMAJOR("ADO: got new IPC message");
    
    /*---------------------------------------*/
    /* custom IPC message protocol - staging */
    /*---------------------------------------*/
    using namespace mcas::ipc;
    
    if(mcas::ipc::Message::is_valid(buffer)) {
      
      switch(mcas::ipc::Message::type(buffer))
        {
        case(mcas::ipc::MSG_TYPE_CHIRP): {
          auto chirp = reinterpret_cast<mcas::ipc::Chirp*>(buffer);

          switch(chirp->type)
            {
            case chirp_t::SHUTDOWN:
              PMAJOR("ADO: received Shutdown chirp in %p",
                     static_cast<void *>(buffer));
              plugin_mgr.shutdown();
              exit = true;
              break;
            default:
              throw Protocol_exception("unknown chirp");
            }
          
          break;
        }
        case(mcas::ipc::MSG_TYPE_MAP_MEMORY): {

          auto * mm = reinterpret_cast<Map_memory*>(buffer);
          
          /* use same as shard virtual address for the moment */
          if(!check_xpmem_kernel_module())
            throw General_exception("inaccessible XPMEM kernel module");

          xpmem_addr seg = {0,0};
          seg.apid = xpmem_get(mm->token,
                               XPMEM_RDWR,
                               XPMEM_PERMIT_MODE,
                               reinterpret_cast<void *>(0666));
          if(seg.apid == -1)
            throw General_exception("xpmem_get: failed unexpectedly.");

          auto mm_addr = xpmem_attach(seg,
                                      mm->size,
                                      reinterpret_cast<void*>(mm->shard_addr));

          if(mm_addr == reinterpret_cast<void*>(-1))
            throw General_exception("xpmem_attached: failed unexpectly.");

          //            touch_pages(mm_addr, mm_size);
          PMAJOR("ADO: mapped memory %lx size:%lu", mm->token, mm->size);

          /* register memory with plugins */
          if(plugin_mgr.register_mapped_memory(mm->shard_addr, mm_addr, mm->size) != S_OK)
            throw General_exception("calling register_mapped_memory on ADO plugin failed");

          break;
        }
        case(mcas::ipc::MSG_TYPE_WORK_REQUEST):  {
        
          Component::IADO_plugin::response_buffer_vector_t response_buffers;
          auto * wr = reinterpret_cast<Work_request*>(buffer);

          if(debug_level > 1)
            PLOG("ADO process: RECEIVED Work_request: key=(%s) value=%p value_len=%lu invocation_len=%lu detached_value=%p (%.*s) len=%lu new=%d",
                 wr->get_key(),
                 wr->get_value_addr(),
                 wr->value_len,
                 wr->invocation_data_len,
                 wr->get_detached_value_addr(),
                 int(wr->detached_value_len),
                 static_cast<char *>(wr->get_detached_value_addr()),
                 wr->detached_value_len,
                 wr->new_root);

          auto work_request_id = wr->work_key;

          IADO_plugin::value_space_t values;
          values.append(wr->get_value_addr(),wr->value_len);
          values.append(wr->get_detached_value_addr(), wr->detached_value_len);
          
          /* forward to plugins */
          status_t rc =
            plugin_mgr.do_work(work_request_id,
                               wr->get_key(),
                               wr->get_key_len(),
                               values,
                               wr->get_invocation_data(),
                               wr->invocation_data_len,
                               wr->new_root,
                               response_buffers);

          /* pass back response data */
          ipc.send_work_response(rc,
                                 work_request_id,
                                 response_buffers);

          break;
        }
        case(mcas::ipc::MSG_TYPE_BOOTSTRAP_REQUEST):  {

          auto boot_req = reinterpret_cast<Bootstrap_request*>(buffer);
          std::string pool_name(boot_req->pool_name, boot_req->pool_name_len);

          if(debug_level > 2)
            PLOG("ADO process: bootstrap_request: (%s, %lu, %u, %lu)",
                 pool_name.c_str(), boot_req->pool_size,
                 boot_req->pool_flags, boot_req->expected_obj_count);

          /* call the plugin */
          plugin_mgr.launch_event(boot_req->auth_id,
                                  pool_name,
                                  boot_req->pool_size,
                                  boot_req->pool_flags,
                                  boot_req->expected_obj_count);
          
          ipc.send_bootstrap_response();
          break;
        }
        case(mcas::ipc::MSG_TYPE_OP_EVENT): {
          auto event = reinterpret_cast<Op_event*>(buffer);
          if(debug_level > 1)
            PLOG("ADO_process: received op event (%s)", to_str(event->op).c_str());
          
          /* invoke plugin then return completion */
          plugin_mgr.notify_op_event(event->op);          
          ipc.send_op_event_response(event->op);
          /* now exit */
          exit = true;
          break;
        }
        default: {
          throw Logic_exception("ADO_process: unknown mcas::ipc message type");
        }
        }

      ipc.free_ipc_buffer(buffer);
      count++;
      continue;
    }
  }
}


