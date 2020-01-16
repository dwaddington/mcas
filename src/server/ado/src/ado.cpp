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
 * Main entry point
 *
 * @param argc
 * @param argv
 *
 * @return
 */
int main(int argc, char* argv[])
{
  std::string plugin, channel_id;
  unsigned debug_level;
  std::string cpu_mask;

  try {
    namespace po = boost::program_options;
    po::options_description desc("Options");
    desc.add_options()
      ("help", "Print help")
      ("plugin", po::value<std::string>(&plugin)->required(), "ADO plugin")
      ("channel_id", po::value<std::string>(&channel_id)->required(), "Channel (prefix) identifier")
      ("debug", po::value<unsigned>(&debug_level)->default_value(0), "Debug level")
      ("cpumask", po::value<std::string>(&cpu_mask), "Cores to restrict threads to; in string form")
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
            void*& out_value_addr) -> status_t
    {
      status_t rc;
      ipc.send_table_op_create(work_request_id, key_name, value_size, flags);
      ipc.recv_table_op_response(rc, out_value_addr);
      return rc;
    };

  auto ipc_open_key =
    [&ipc] (const uint64_t work_request_id,
            const std::string& key_name,
            const int flags,
            void*& out_value_addr,
            size_t& out_value_len) -> status_t
    {
      status_t rc;
      ipc.send_table_op_open(work_request_id, key_name, flags);
      ipc.recv_table_op_response(rc, out_value_addr, &out_value_len);
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


  /* load plugin and register callbacks */
  auto i_plugin = reinterpret_cast<IADO_plugin*>(load_component(plugin.c_str(),
                                                                Interface::ado_plugin));
  if(!i_plugin)
    throw General_exception("unable to load ADO plugin (%s)", plugin.c_str());

  i_plugin->register_callbacks(IADO_plugin::
                               Callback_table{
                                 ipc_create_key,
                                   ipc_open_key,
                                   ipc_erase_key,
                                   ipc_resize_value,
                                   ipc_allocate_pool_memory,
                                   ipc_free_pool_memory,
                                   ipc_get_reference_vector,
                                   ipc_find_key,
                                   ipc_get_pool_info,
                                   ipc_iterate});

  PLOG("ADO: plugin loaded OK! (%s)", plugin.c_str());

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
              /* notify plugin */
              i_plugin->shutdown();
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

          /* register with plugin */
          if(i_plugin->register_mapped_memory(mm->shard_addr, mm_addr, mm->size) != S_OK)
            throw General_exception("calling register_mapped_memory on ADO plugin failed");

          break;
        }
        case(mcas::ipc::MSG_TYPE_WORK_REQUEST):  {
        
          Component::IADO_plugin::response_buffer_vector_t response_buffers;
          auto * wr = reinterpret_cast<Work_request*>(buffer);

          if(debug_level > 1)
            PLOG("ADO process: RECEIVED Work_request: key=(%s) value=%p value_len=%lu invocation_len=%lu detached_value=%p (%.*s) len=%lu new=%d",
                 wr->get_key().c_str(),
                 wr->get_value_addr(),
                 wr->value_len,
                 wr->invocation_data_len,
                 wr->get_detached_value_addr(),
                 (int) wr->detached_value_len,
                 (char*) wr->get_detached_value_addr(),
                 wr->detached_value_len,
                 wr->new_root);

          auto work_request = wr->work_key;

          /* forward to plugin */
          status_t rc =
            i_plugin->do_work(work_request,
                              wr->get_key(),
                              wr->get_value_addr(),
                              wr->value_len,
                              wr->get_detached_value_addr(),
                              wr->detached_value_len,
                              wr->get_invocation_data(),
                              wr->invocation_data_len,
                              wr->new_root,
                              response_buffers);


          /* pass back response data */
          ipc.send_work_response(rc,
                                 work_request,
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
          i_plugin->launch_event(boot_req->auth_id,
                                 pool_name,
                                 boot_req->pool_size,
                                 boot_req->pool_flags,
                                 boot_req->expected_obj_count);
          
          ipc.send_bootstrap_response();
          break;
        }
        case(mcas::ipc::MSG_TYPE_OP_EVENT): {
          auto event = reinterpret_cast<Op_event*>(buffer);
          /* invoke plugin then return completion */
          i_plugin->notify_op_event(event->op);          
          ipc.send_op_event_response(event->op);
          /* now exit */
          exit = true;
          break;
        }
        default: {
          throw Logic_exception("unknown mcas::ipc message type");
        }
        }

      ipc.free_ipc_buffer(buffer);
      count++;
      continue;
    }
  }
}


