#ifndef __SHARD_CLUSTER_MESSAGES_H__
#define __SHARD_CLUSTER_MESSAGES_H__

#include <common/memory.h>
#include <common/mpmc_bounded_queue.h>
#include <string>

namespace mcas
{
class Cluster_message {
 public:
  Cluster_message(const std::string& sender, const std::string& type, const std::string& content)
      : _sender(sender),
        _type(type),
        _content(content)
  {
  }

  Cluster_message(const Cluster_message& msg) : _sender(msg._sender), _type(msg._type), _content(msg._content) {}

  ~Cluster_message() {}

  Cluster_message& operator=(const Cluster_message& msg)
  {
    _sender  = msg._sender;
    _type    = msg._type;
    _content = msg._content;
    return *this;
  }

  std::string _sender;
  std::string _type;
  std::string _content;
};

using cluster_signal_queue_t = common::Mpmc_bounded_lfq<Cluster_message*>;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
/**
 * Queue for sending cluster messages up to shard
 *
 */
class Cluster_signal_queue {
  static constexpr size_t   CLUSTER_SIGNAL_QUEUE_SIZE = 2;
  static constexpr unsigned ITERATION_GUARD           = 100000;

 public:
  Cluster_signal_queue()
      : _queue_buffer(cluster_signal_queue_t::allocate_queue_memory(CLUSTER_SIGNAL_QUEUE_SIZE)),
        _queue(CLUSTER_SIGNAL_QUEUE_SIZE, _queue_buffer)
  {
  }

  /* DUMB POINTER */
  ~Cluster_signal_queue() { ::free(_queue_buffer); }

  void send_message(const std::string& sender, const std::string& type, const std::string& content)
  {
    unsigned iter    = 0;
    auto     element = new Cluster_message(sender, type, content);
    while (!_queue.enqueue(element)) {
      iter++;
      if (iter > ITERATION_GUARD) {
        Cluster_message * tmp = nullptr;
        if(_queue.pop(tmp)) {
          if(tmp)
            delete tmp;
          if(!_queue.push(element))
            throw Logic_exception("signal queue full pop/push on max failed"); /* TODO retry */
        }
        else throw Logic_exception("signal queue full handling");
        return;
      }
    }
  }

  bool recv_message(Cluster_message*& msg) { return (_queue.dequeue(msg)); }

 private:
  void*                  _queue_buffer;
  cluster_signal_queue_t _queue;
};

#pragma GCC diagnostic pop

}  // namespace mcas

#endif  // __SHARD_CLUSTER_MESSAGES_H__
