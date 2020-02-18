#ifndef __mcas_SERVER_TASK_H__
#define __mcas_SERVER_TASK_H__

namespace mcas
{
class Shard_task {
 public:
  Shard_task(Connection_handler* handler) : _handler(handler) {}
  Shard_task(const Shard_task &) = delete;
  Shard_task &operator=(const Shard_task &) = delete;
  virtual ~Shard_task() = default;
  virtual status_t    do_work()                 = 0;
  virtual const void* get_result() const        = 0;
  virtual size_t      get_result_length() const = 0;
  virtual offset_t    matched_position() const  = 0;
  Connection_handler* handler() const { return _handler; }

 protected:
  Connection_handler* _handler;
};

}  // namespace mcas

#endif  // __mcas_SERVER_TASK_H__
