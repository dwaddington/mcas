/*
  Copyright [2019-2020] [IBM Corporation]
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

/*
 * Authors:
 *
 * Luna Xu (xuluna@ibm.com)
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */

#include "ado_proto.h"
#include "ado_proxy.h"

#include <boost/tokenizer.hpp>
#include <api/ado_itf.h>
#include <common/logging.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>  // Non-docker only
#include <csignal>
#include <sched.h>
#include <unistd.h>
#include <values.h>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <cstring>
#include <iostream>
#include <memory>
#include <numeric> /* accumulate */
#include <vector>

using namespace rapidjson;
using namespace component;
using namespace std;

sig_atomic_t ADO_proxy::_exited;

void std::default_delete<DOCKER>::operator()(DOCKER *d) { docker_destroy(d); }

namespace
{
/* create unique channel id prefix */
auto make_channel_name(void *t)
{
  std::stringstream ss;
  /* needs to be unique to the process and the shard */
  ss << "channel-" << ::getpid() << "-" << std::hex << reinterpret_cast<unsigned long>(t);
  return ss.str();
}
}  // namespace

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"  // uninitalized or default initialized: container_id, _pid, docker

static const char *env_USE_DOCKER = ::getenv("USE_DOCKER");

ADO_proxy::ADO_proxy(const uint64_t              auth_id,
                     const unsigned              debug_level,
                     component::IKVStore *       kvs,
                     component::IKVStore::pool_t pool_id,
                     const std::string &         pool_name,
                     const size_t                pool_size,
                     const unsigned int          pool_flags,
                     const uint64_t              expected_obj_count,
                     const std::string &         filename,
                     std::vector<std::string> &  args,
                     std::string                 cores,
                     int                         memory,
                     float                       cpu_num,
                     numa_node_t                 numa_zone)
: _debug_level(debug_level),
  _auth_id(auth_id),
  _kvs(kvs),
  _pool_id(pool_id),
  _pool_name(pool_name),
  _pool_size(pool_size),
  _pool_flags(pool_flags),
  _expected_obj_count(expected_obj_count),
  _cores(cores),
  _filename(filename),
  _args(args),
  _channel_name(make_channel_name(this)),
  _ipc(std::make_unique<ADO_protocol_builder>(debug_level, _channel_name, ADO_protocol_builder::Role::CONNECT)),
  _core_number(cpu_num), _memory(memory), _numa(numa_zone), _deferred_unlocks(), _life_unlocks()
{
  (void) _core_number;  // unused
  (void) _memory;       // unused
  (void) _numa;         // unused
  assert(pool_id);
  // launch ado process

  this->launch(debug_level);
}
#pragma GCC diagnostic pop

ADO_proxy::~ADO_proxy()
{
}

status_t ADO_proxy::bootstrap_ado(bool opened_existing)
{
  std::vector<uint64_t> attrs;
  assert(_kvs);
  if (_kvs->get_attribute(_pool_id, IKVStore::Attribute::MEMORY_TYPE, attrs) != S_OK)
    throw Logic_exception("get_attributes failed on storage engine");

  auto hr = _ipc->send_bootstrap(_auth_id, _pool_name, _pool_size, _pool_flags, boost::numeric_cast<unsigned int>(attrs[0]),
                                 _expected_obj_count, opened_existing);

  if (hr != S_OK)
    throw General_exception("send_bootstrap failed");
  
  hr = _ipc->recv_bootstrap_response();
  if (hr == S_OK)
    PMAJOR("ADO_proxy::bootstrap response OK.");
  else
    PWRN("ADO_proxy::invalid bootstrap response to ADO_proxy");

  return hr;
}

status_t ADO_proxy::send_op_event(component::ADO_op op)
{
  return _ipc->send_op_event(op);
}

status_t ADO_proxy::send_cluster_event(const std::string& sender,
                                       const std::string& type,
                                       const std::string& content)
{
  return _ipc->send_cluster_event(sender, type, content);
}


status_t ADO_proxy::send_memory_map(uint64_t token, size_t size, void *value_vaddr)
{
  PLOG("ADO_proxy: sending memory map request");
  return _ipc->send_memory_map(token, size, value_vaddr);
}

status_t ADO_proxy::send_memory_map_named(unsigned region_id, string_view pool_name, std::size_t offset, ::iovec iov)
{
  PLOG("ADO_proxy:%s sending", __func__);
  return _ipc->send_memory_map_named(region_id, pool_name, offset, iov);
}

status_t ADO_proxy::send_work_request(const uint64_t work_request_key,
                                      const char *   key,
                                      const size_t   key_len,
                                      const void *   value,
                                      const size_t   value_len,
                                      const void *   detached_value,
                                      const size_t   detached_value_len,
                                      const void *   invocation_data,
                                      const size_t   invocation_data_len,
                                      const bool     new_root)
{
  _outstanding_wr++;

  return _ipc->send_work_request(work_request_key, key, key_len, value, value_len, detached_value, detached_value_len,
                                 invocation_data, invocation_data_len, new_root);
}

status_t ADO_proxy::send_table_op_response(const status_t             s,
                                           const void *               value_addr,
                                           size_t                     value_len,
                                           const char *               key_ptr,
                                           component::IKVStore::key_t key_handle)
{
  return _ipc->send_table_op_response(s, value_addr, value_len, key_ptr, key_handle);
}

status_t ADO_proxy::send_find_index_response(const status_t     status,
                                             const offset_t     matched_position,
                                             const std::string &matched_key)
{
  return _ipc->send_find_index_response(status, matched_position, matched_key);
}

status_t ADO_proxy::send_vector_response(const status_t status, const component::IADO_plugin::Reference_vector &rv)
{
  return _ipc->send_vector_response(status, rv);
}

status_t ADO_proxy::send_iterate_response(const status_t                              status,
                                          const component::IKVStore::pool_iterator_t  iterator,
                                          const component::IKVStore::pool_reference_t reference)
{
  return _ipc->send_iterate_response(status, iterator, reference);
}

status_t ADO_proxy::send_pool_info_response(const status_t status, const std::string &info)
{
  return _ipc->send_pool_info_response(status, info);
}

status_t ADO_proxy::send_unlock_response(const status_t status) 
{ 
  return _ipc->send_unlock_response(status); 
}

bool ADO_proxy::check_work_completions(uint64_t &                                        request_key,
                                       status_t &                                        out_status,
                                       component::IADO_plugin::response_buffer_vector_t &response_buffers)
{
  if (_outstanding_wr == 0) return false;

  auto result = _ipc->recv_from_ado_work_completion(request_key, out_status, response_buffers);
  if (result) _outstanding_wr--;

  return result;
}

bool ADO_proxy::check_table_ops(const void *       buffer,
                                uint64_t&          work_key,
                                component::ADO_op& op,
                                std::string &      key,
                                size_t&            value_len,
                                size_t&            align_or_flags,
                                void *&            addr)
{
  return _ipc->recv_table_op_request(static_cast<const Buffer_header *>(buffer), work_key, op, key, value_len,
                                     align_or_flags, addr);
}

bool ADO_proxy::check_index_ops(const void * buffer,
                                std::string &key_expression,
                                offset_t &   begin_pos,
                                int &        find_type,
                                uint32_t     max_comp)
{
  (void) max_comp;  // unused
  return _ipc->recv_index_op_request(static_cast<const Buffer_header *>(buffer), key_expression, begin_pos, find_type);
}

bool ADO_proxy::check_vector_ops(const void *buffer, common::epoch_time_t &t_begin, common::epoch_time_t &t_end)
{
  return _ipc->recv_vector_request(static_cast<const Buffer_header *>(buffer), t_begin, t_end);
}

bool ADO_proxy::check_pool_info_op(const void *buffer)
{
  return _ipc->recv_pool_info_request(static_cast<const Buffer_header *>(buffer));
}

bool ADO_proxy::check_iterate(const void *                          buffer,
                              common::epoch_time_t &                t_begin,
                              common::epoch_time_t &                t_end,
                              component::IKVStore::pool_iterator_t &iterator)
{
  return _ipc->recv_iterate_request(static_cast<const Buffer_header *>(buffer), t_begin, t_end, iterator);
}

bool ADO_proxy::check_op_event_response(const void *buffer, component::ADO_op &op)
{
  return _ipc->recv_op_event_response(static_cast<const Buffer_header *>(buffer), op);
}

bool ADO_proxy::check_unlock_request(const void *buffer, uint64_t &work_id, component::IKVStore::key_t &key_handle)
{
  return _ipc->recv_unlock_request(static_cast<const Buffer_header *>(buffer), work_id, key_handle);
}

bool ADO_proxy::check_configure_request(const void* buffer, uint64_t& options)
{
  return _ipc->recv_configure_request(static_cast<const Buffer_header *>(buffer), options);
}

status_t ADO_proxy::send_configure_response(const status_t status)
{
  return _ipc->send_configure_response(status); 
}

status_t ADO_proxy::recv_callback_buffer(Buffer_header *&out_buffer)
{
  return _ipc->recv_callback(out_buffer);
}

void ADO_proxy::free_callback_buffer(void *buffer)
{
  _ipc->free_ipc_buffer(buffer);
}

void ADO_proxy::child_exit(int, siginfo_t *, void *)
{
  ADO_proxy::_exited = 1;
}

struct clone_params_t {
  const char * arg;
  char * const* argv;
};
  
static int clone_entry(void * arg)
{
  clone_params_t * p = reinterpret_cast<clone_params_t *>(arg);
  if(::execv(p->arg, p->argv) == -1) {
    perror("execv");
    throw General_exception("%s:%s execv failed unexpectedly (%s)", __FILE__, __func__, p->argv[0]);
  }
  
  return 0;
}

void ADO_proxy::launch(unsigned debug_level)
{
  if (env_USE_DOCKER) {
    _exited = 0;
    _docker.reset(docker_init(const_cast<char *>(std::string("v1.39").c_str())));
    if (!_docker) {
      perror(__FILE__ " Cannot initiate docker");
      return;
    }
    // create container
    Document req;
    req.SetObject();
    auto &allocator = req.GetAllocator();
    req.AddMember("Image", "res-mcas-docker-local.artifactory.swg-devops.com/ado:latest", allocator);
    Value config(kObjectType);
    config.AddMember("IpcMode", "host", allocator);
    config.AddMember("Privileged", true, allocator);
    Value vol(kArrayType);
    vol.PushBack("/tmp:/tmp", allocator);
    vol.PushBack("/dev:/dev", allocator);
    config.AddMember("Binds", vol.Move(), allocator);
    Value cap(kArrayType);
    cap.PushBack("ALL", allocator);
    config.AddMember("CapAdd", cap.Move(), allocator);
    config.AddMember("CpusetCpus", Value(_cores.c_str(), allocator).Move(), allocator);
    config.AddMember("CpusetMems", Value(to_string(_numa).c_str(), allocator).Move(), allocator);
    config.AddMember("CpuPeriod", 100000, allocator);
    config.AddMember("CpuQuota", static_cast<int>(100000 * _core_number), allocator);
    //  Value env(kArrayType);
    // env.PushBack("LD_LIBRARY_PATH=/mcas/build/dist", allocator);
    // config.AddMember("Env", env.Move(), allocator);
    req.AddMember("HostConfig", config, allocator);
    Value cmd(kArrayType);
    _args.push_back("--channel_id");
    _args.push_back(_channel_name);
    for (auto &arg : _args) {
      cmd.PushBack(Value{}.SetString(arg.c_str(), static_cast<rapidjson::SizeType>(arg.length()), allocator),
                   allocator);
    }
    req.AddMember("Cmd", cmd.Move(), allocator);

    StringBuffer         sb;
    Writer<StringBuffer> writer(sb);
    req.Accept(writer);

    cout << sb.GetString() << endl;

    {
      std::string url("http://v1.39/containers/create");
      std::string opt(sb.GetString());
      CURLcode    response = docker_post(_docker.get(), &url[0], &opt[0]);
      assert(response == CURLE_OK);
    }
    req.SetObject();
    req.Parse(docker_buffer(_docker.get()));
    _container_id = req["Id"].GetString();

    // launch container
    {
      std::string url("http://v1.39/containers/" + _container_id + "/start");
      CURLcode    response = docker_post(_docker.get(), &url[0], NULL);
      assert(response == CURLE_OK);
    }
  }
  else {
    /* run ADO process in GDB in an xterm window */
    std::vector<std::string> args;

    /* set USE_XTERM to optionally launch in XTERM */
    char *env_USE_XTERM = ::getenv("USE_XTERM");
    char *env_USE_XTERM_OPTIONS = ::getenv("USE_XTERM_OPTIONS");    
    /* set USE_GDB to optionally start in GDB */
    char *env_USE_GDB = ::getenv("USE_GDB");
    char *env_USE_LOG = ::getenv("USE_LOG");    

    if (env_USE_XTERM) {
      args.push_back("/usr/bin/xterm");

      /* options, e.g. -fa VeraMono -bg wheat -fs 14 can be added using USE_XTERM_OPTIONS */
      if (env_USE_XTERM_OPTIONS) {
        typedef boost::tokenizer<boost::char_separator<char>> tokenizer;
        std::string s(env_USE_XTERM_OPTIONS);
        boost::char_separator<char> sep{" "};
        tokenizer tokens{s,sep};
        for (const auto& t : tokens)
          args.push_back(t);
      }

      args.push_back("-e");
    }
    
    static const std::vector<const char *> gdb = {
      "/usr/bin/gdb",
#if 0
      /* might want to "catch throw" if using gdb ... */
      "-ex", "catch throw General_exception"
#endif
      "-q",
      "-ex",
      "r",
      "--args"
    };

    /* set USE_GDB to optionally start in GDB */
    if (env_USE_GDB) {
      args.insert(args.end(), gdb.begin(), gdb.end());
    }

    args.push_back(_filename);
    args.push_back("--debug");
    args.push_back(std::to_string(debug_level));
    args.push_back("--channel");
    args.push_back(_channel_name);
    args.push_back("--cpumask");
    args.push_back(_cores);

    args.insert(args.end(), _args.begin(), _args.end());

    if (env_USE_LOG) args.push_back("--log");

    std::vector<char *> c;
    std::string         conc_cmd;

    for (auto &a : args) {
      c.push_back(&a[0]);
    }

    PLOG("cmd:%s", std::accumulate(args.begin(), args.end(), std::string(),
                                   [](const std::string &a, const std::string &b) { return a + " " + b; })
         .c_str());

    c.push_back(nullptr);

    /* The creator is surprising incurious about the state of the child after creation.
     * It does not notice when the child exits.
     */
    {
      struct sigaction n;
      n.sa_sigaction = child_exit;
      sigemptyset(&n.sa_mask);
      n.sa_flags = 0 | SA_NOCLDSTOP | SA_SIGINFO;
      ::sigaction(SIGCHLD, &n, nullptr);
    }

    constexpr size_t STACK_SIZE = KiB(256);
    char * stack = static_cast<char*>(mmap(NULL, STACK_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|MAP_STACK, -1,0));
    assert(stack != MAP_FAILED);
    auto stack_top = stack + STACK_SIZE;

    /* clone is safer than fork, apparently */
    clone_params_t p{args[0].data(), c.data()};
    _child_pid = clone(clone_entry, stack_top, SIGCHLD, &p);
    if(_child_pid == -1) {      
      throw General_exception("%s:%s clone failed", __FILE__, __func__);
    }
  }

  _ipc->create_uipc_channels();

  return;
}

status_t ADO_proxy::kill()
{
  _ipc->send_shutdown();

  if (env_USE_DOCKER) {
    {
      string   url("http://v1.39/containers/" + _container_id + "/wait");
      CURLcode response = docker_post(_docker.get(), &url[0], NULL);
      assert(response == CURLE_OK);
      std::cout << docker_buffer(_docker.get()) << std::endl;
    }

    //  msgctl(_msqid, IPC_RMID, 0);
    // remove container
    {
      string   url("http://v1.39/containers/" + _container_id);
      CURLcode response = docker_delete(_docker.get(), &url[0], NULL);
      assert(response == CURLE_OK);
    }
  }
  return S_OK;
}

bool ADO_proxy::has_exited()
{
  if (_child_pid != 0 && _exited) {
    int status;
    ::waitpid(_child_pid, &status, 0);
    return true;
  }
  return false;
}

status_t ADO_proxy::shutdown()
{
  PLOG("ADO_proxy: shutting down ADO process");
  /* clean up life time locks */
  release_life_locks();
  PLOG("ADO_proxy: about to kill ADO");
  return kill();
}

void ADO_proxy::add_deferred_unlock(const uint64_t work_request_id, const component::IKVStore::key_t key)
{
  CPLOG(2, "ADO_proxy: adding deferred unlock (work_id=%lx, key_handle=%p)",
        work_request_id, reinterpret_cast<void*>(key));

  /* check for _deferred_unlocks being too large
     it may be an attack from ADO code */
  if (_deferred_unlocks[work_request_id].size() > MAX_ALLOWED_DEFERRED_LOCKS)
    throw std::range_error("too many deferred locks");

  _deferred_unlocks[work_request_id].insert(key);

  CPLOG(2, "ADO_proxy: deferred unlock count %lu", _deferred_unlocks[work_request_id].size());
}

status_t ADO_proxy::remove_deferred_unlock(const uint64_t work_request_id, const component::IKVStore::key_t key)
{
  if (_deferred_unlocks.find(work_request_id) == _deferred_unlocks.end()) return E_NOT_FOUND;
  auto &key_v    = _deferred_unlocks[work_request_id];
  auto iter_pos = key_v.find(key);
  if (iter_pos == key_v.end()) return E_NOT_FOUND;
  key_v.erase(iter_pos);
  return S_OK;
}

void ADO_proxy::get_deferred_unlocks(const uint64_t work_key, std::vector<component::IKVStore::key_t> &keys)
{
  auto &v = _deferred_unlocks[work_key];
  keys.assign(v.begin(), v.end());
  v.clear();
}

bool ADO_proxy::check_for_implicit_unlock(const uint64_t work_request_id,
                                          const component::IKVStore::key_t key)
{
  if (_deferred_unlocks.find(work_request_id) != _deferred_unlocks.end()) {
    auto &key_v = _deferred_unlocks[work_request_id];
    if (key_v.find(key) != key_v.end()) return true;
  }

  return _life_unlocks.find(key) != _life_unlocks.end();
}

void ADO_proxy::add_life_unlock(const component::IKVStore::key_t key)
{
  //  PNOTICE("Adding life unlock (%p, %p)", this, key);
  _life_unlocks.insert(key);
}

status_t ADO_proxy::remove_life_unlock(const component::IKVStore::key_t key)
{
  auto pos = _life_unlocks.find(key);
  if (pos == _life_unlocks.end()) return E_NOT_FOUND;
  _life_unlocks.erase(pos);
  return S_OK;
}

void ADO_proxy::release_life_locks()
{
  assert(_kvs);
  auto lock_count = _life_unlocks.size();
  for (auto &lock : _life_unlocks) {
    PLOG("ADO_proxy: releasing lock pool_id=%lx lock=%p", _pool_id, static_cast<const void *>(lock));
    status_t rc = _kvs->unlock(_pool_id, lock);
    if (rc != S_OK) throw Logic_exception("release_life_locks: pool unlock failed (%d)", rc);
  }
  PLOG("ADO_proxy: %zu life locks released.", lock_count);
}

/**
 * Factory entry point
 *
 */
extern "C" void *factory_createInstance(component::uuid_t component_id)
{
  if (component_id == ADO_proxy_factory::component_id()) {
    return static_cast<void *>(new ADO_proxy_factory());
  }
  else
    return NULL;
}
