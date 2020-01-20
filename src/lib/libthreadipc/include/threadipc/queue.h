#ifndef __QUEUE_H_
#define __QUEUE_H_

#include <common/mpmc_bounded_queue.h>
#include <common/types.h>
#include <mutex>
#include <stdlib.h> /* aligned_alloc */

#define QUEUE_SIZE 256

namespace Threadipc
{
enum class Operation { kill, schedule, reschedule, register_ado, wait };

struct message {
  unsigned    shard_core  = 0;
  Operation   op          = Operation::wait;
  std::string cores;
  float       core_number = -1;
  std::string ado_id;
  numa_node_t numa_zone   = 0;
  message() :cores(std::string()), ado_id(std::string()){}
};

class Thread_ipc {
  using queue_type = Common::Mpmc_bounded_lfq_sleeping<message *>;
 public:
  static Thread_ipc *instance();
  Thread_ipc(Thread_ipc const &) = delete;  // copy constructor deleted
  Thread_ipc &operator=(const Thread_ipc &) = delete;
  bool        schedule_to_mgr(unsigned int shard_core,
                              std::string &cores,
                              float        core_number,
                              numa_node_t  numa_zone);
  bool        register_to_mgr(unsigned int shard_core,
                              std::string &cores,
                              std::string  id);
  bool        schedule_to_ado(unsigned int       shard_core,
                              const std::string &cores,
                              float              core_number,
                              numa_node_t        numa_zone);
  bool        kill_to_ado(unsigned int shard_core);
  bool        get_next_ado(struct message *&msg);
  bool        get_next_mgr(struct message *&msg);
  bool        add_to_ado(struct message *msg);
  bool        add_to_mgr(struct message *msg);
  void        cleanup();
  virtual ~Thread_ipc() { cleanup(); }
  /* pre-C++17, provide help to align instance of Thread_ipc */
#if __cplusplus < 201703L
  void *operator new(std::size_t sz)
  {
    return aligned_alloc(alignof(Thread_ipc), sz);
  }
#endif

 private:
  Thread_ipc();
  queue_type          queue_to_mgr;
  queue_type          queue_to_ado;
  static std::mutex   _ipc_mutex;
  static Thread_ipc * _ipc;
};
}  // namespace Threadipc

#endif
