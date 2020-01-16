#include <sstream>
#include "replicate_state_machine.h"

#include <boost/serialization/serialization.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/utility.hpp>

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

using namespace nuraft;
using namespace std;

ptr<buffer> Replicate_state_machine::commit(const ulong log_idx, buffer& data)
{
}

void Replicate_state_machine::save_logical_snp_obj(snapshot& s,
                                                   ulong&    obj_id,
                                                   buffer&   data,
                                                   bool      is_first_obj,
                                                   bool      is_last_obj)
{
  lock_guard<mutex> ll(_snapshot_lock);
  ptr<buffer>       buffer = s.serialize();
  ptr<snapshot>     snap   = snapshot::deserialize(*buffer);
  lock_guard<mutex> lc(_chains_lock);
  _snapshot = cs_new<snapshot_ctx>(snap, _chains);
  buffer_serializer             bs(data);
  stringstream                  ss(bs.get_str());
  boost::archive::text_iarchive iarch(ss);
  iarch >> _snapshot->chains;
  obj_id++;
}

bool Replicate_state_machine::apply_snapshot(snapshot& s)
{
  lock_guard<mutex> ll(_snapshot_lock);
  lock_guard<mutex> lc(_chains_lock);
  _chains = _snapshot->chains;
  return true;
}

int Replicate_state_machine::read_logical_snp_obj(snapshot&    s,
                                                  void*&       user_snp_ctx,
                                                  ulong        obj_id,
                                                  ptr<buffer>& data_out,
                                                  bool&        is_last_obj)
{
  lock_guard<mutex> ll(_snapshot_lock);
  if (_snapshot == nullptr) {
    return 0;
  }
  data_out = buffer::alloc(sizeof(chains_t));
  buffer_serializer             bs(data_out);
  stringstream                  ss;
  boost::archive::text_oarchive oarch(ss);
  lock_guard<mutex>             lc(_chains_lock);
  oarch << _chains;
  bs.put_str(ss.str());
  is_last_obj = true;
}

void Replicate_state_machine::create_snapshot(
    snapshot&                         s,
    async_result<bool>::handler_type& when_done)
{
  lock_guard<mutex> ll(_snapshot_lock);
  ptr<buffer>       buffer = s.serialize();
  ptr<snapshot>     ss     = snapshot::deserialize(*buffer);
  lock_guard<mutex> lc(_chains_lock);
  _snapshot = cs_new<snapshot_ctx>(ss, _chains);
  ptr<exception> except(nullptr);
  bool           ret = true;
  when_done(ret, except);
}