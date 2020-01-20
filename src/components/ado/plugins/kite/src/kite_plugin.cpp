#include <api/interfaces.h>
#include <assert.h>
#include <common/logging.h>
#include <emmintrin.h>
#include <libpmem.h>
#include <stdlib.h>
#include <string>
#include "kite_plugin.h"

using namespace std;

status_t ADO_kite_plugin::register_mapped_memory(void * shard_vaddr,
                                                 void * local_vaddr,
                                                 size_t len)
{
  PLOG("ADO_kite_plugin: register_mapped_memory (%p, %p, %lu)", shard_vaddr,
       local_vaddr, len);
  /* we would need a mapping if we are not using the same virtual
     addresses as the Shard process */
  return S_OK;
}

status_t ADO_kite_plugin::shutdown()
{
  /* here you would put graceful shutdown code if any */
  return S_OK;
}

status_t ADO_kite_plugin::do_work(
    const uint64_t     work_key,
    const std::string &key,
    void *             shard_value_vaddr,
    size_t             value_len,
    void *             detached_value,
    size_t             detached_value_len,
    const void *       in_work_request, /* don't use iovec because non-const */
    const size_t,                       // in_work_request_len
    bool,                               // new root
    response_buffer_vector_t &response_buffers)
{
  if(work_key == 0){} // something 
#if 0
  PLOG("ADO_kite_plugin: work_id (%lu)", work_key);
  PLOG("ADO_kite_plugin: do_work (%s, value_addr=%p, valuen_len=%lu)",
       key.c_str(), shard_value_vaddr, value_len);
  PLOG("Current value: %.*s", int(value_len),
       static_cast<char *>(shard_value_vaddr));
#endif
  assert(key != "");
  uint64_t id = strtoull(static_cast<const char *>(in_work_request), NULL, 16);

  if (detached_value != nullptr || detached_value_len > 0)
    throw Logic_exception("kite_plugin did not expect detached value");

  response_buffers.push_back({::malloc(3), 3, false});
  memcpy(static_cast<char *>(response_buffers[0].ptr), "OK!", 3);

  void *new_value_addr = nullptr;

  if (strncmp(static_cast<char *>(shard_value_vaddr), "EMPTYPTR", 8) == 0) {
    _cb.allocate_pool_memory(sizeof(uint64_t) * 3, sizeof(uint64_t),
                             new_value_addr);
    uint64_t *arr = static_cast<uint64_t *>(new_value_addr);
    // layout: size, freq, gnome id
    arr[0] = 3;
    arr[1] = 1;
    arr[2] = id;
    //  PMAJOR("created new kmer! now freq is %lu", arr[1]);
    // this should be atomic, not sure if the order is correct TODO
    *(static_cast<void **>(shard_value_vaddr)) = new_value_addr;
    _mm_mfence();
    pmem_flush(shard_value_vaddr, 8);
    _mm_mfence();
    return S_OK;
  }

  auto                    p    = *(static_cast<void **>(shard_value_vaddr));
  uint64_t *              arr  = static_cast<uint64_t *>(p);
  int                     size = static_cast<int>(arr[0]);
  unordered_set<uint64_t> ids(arr + 2, arr + size);
  auto                    ret = ids.insert(id);
  if (ret.second) {
    _cb.allocate_pool_memory((size + 1) * sizeof(uint64_t),
                             sizeof(uint64_t), new_value_addr);
    memcpy(new_value_addr, p, size * sizeof(uint64_t));
    arr = static_cast<uint64_t *>(new_value_addr);
    arr[0]++;
    arr[1]++;
    arr[size] = id;
    PMAJOR("updated existing kmer! now freq is %lu", arr[1]);
    *(static_cast<void **>(shard_value_vaddr)) = new_value_addr;
    _mm_mfence();
    pmem_flush(shard_value_vaddr, 8);
    _mm_mfence();
    _cb.free_pool_memory(value_len, p);
  }

  return S_OK;
}

/**
 * Factory-less entry point
 *
 */
extern "C" void *factory_createInstance(Component::uuid_t &interface_iid)
{
  PLOG("instantiating ADO_kite_plugin");
  if (interface_iid == Interface::ado_plugin)
    return static_cast<void *>(new ADO_kite_plugin());
  else
    return NULL;
}
