/***see definitions at:
https://github.com/eBay/NuRaft/blob/master/include/libnuraft/state_machine.hxx
 *
  **/
#ifndef __REPLICATE_STATE_H_
#define __REPLICATE_STATE_H_

#include <atomic>
#include <libnuraft/nuraft.hxx>
#include <mutex>

namespace nuraft
{
using chains_t = std::unordered_map<std::string, std::list<std::string>>;
using chain_t  = std::pair<std::string, std::list<std::string>>;
class Replicate_state_machine : public state_machine {
 private:
  struct snapshot_ctx {
    snapshot_ctx(ptr<snapshot>& s, chains_t v) : snapshot(s), chains(v) {}
    ptr<snapshot> snapshot;
    chains_t      chains;
  };
  // record key->node ip
  chains_t _chains;
  // Last committed Raft log number.
  std::atomic<uint64_t> _last_committed_idx;

  // Keeps the last 3 snapshots, by their Raft log numbers.
  ptr<snapshot_ctx> _snapshot;

  // Mutex for `snapshots_`.
  std::mutex _snapshot_lock;
  std::mutex _chains_lock;

  static ptr<buffer> enc_log(chain_t& chain);

  static void dec_log(buffer& log, chain_t& chain);

 public:
  virtual ptr<buffer>   commit(const ulong log_idx, buffer& data) override;
  virtual ptr<snapshot> last_snapshot()
  {
    std::lock_guard<std::mutex> ll(_snapshot_lock);
    return _snapshot->snapshot;
  }
  virtual ulong last_commit_index() { return _last_committed_idx; }
  virtual void  save_logical_snp_obj(snapshot& s,
                                     ulong&    obj_id,
                                     buffer&   data,
                                     bool      is_first_obj,
                                     bool      is_last_obj) override;

  virtual bool apply_snapshot(snapshot& s) override;

  virtual int read_logical_snp_obj(snapshot&    s,
                                   void*&       user_snp_ctx,
                                   ulong        obj_id,
                                   ptr<buffer>& data_out,
                                   bool&        is_last_obj) override;

  virtual void create_snapshot(
      snapshot&                         s,
      async_result<bool>::handler_type& when_done) override;
  std::unordered_map<std::string, std::list<std::string>> get_current_chain()
  {
    std::lock_guard<std::mutex> ll(_chains_lock);
    return _chains;
  }
};
}  // namespace nuraft

#endif