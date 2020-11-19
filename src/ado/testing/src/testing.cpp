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

#include "testing.h"

#include <api/interfaces.h>
#include <ccpm/immutable_allocator.h>
#include <common/cycles.h>
#include <common/logging.h>
#include <common/utils.h>
#include <gsl/gsl_byte>
#include <gsl/span>
#include <libpmem.h>
#include <boost/numeric/conversion/cast.hpp>
#include <experimental/string_view>
#include <algorithm> /* copy, fill, find */
#include <cstring> /* strncmp, strcpy, memset */
#include <map>
#include <string>

using std::experimental::string_view;

status_t ADO_testing_plugin::register_mapped_memory(void *shard_vaddr, void *local_vaddr, size_t len)
{
  PLOG("ADO_testing_plugin: register_mapped_memory (%p, %p, %lu)", shard_vaddr, local_vaddr, len);
  /* we would need a mapping if we are not using the same virtual
     addresses as the Shard process */
  return S_OK;
}

#define ASSERT_OK(X, ...) \
  if (X != S_OK) {      \
    PERR(__VA_ARGS__);  \
    return E_FAIL;      \
  }

#define ASSERT_TRUE(X, ...) \
  if (!(X)) {             \
    PERR(__VA_ARGS__);    \
    return E_FAIL;        \
  }

#define ASSERT_FALSE(X, ...) \
  if (X) {                 \
    PERR(__VA_ARGS__);     \
    return E_FAIL;         \
  }

void ADO_testing_plugin::launch_event(const uint64_t     auth_id,
                                      const std::string &pool_name,
                                      const size_t       pool_size,
                                      const unsigned int pool_flags,
                                      const unsigned int memory_type,
                                      const size_t       expected_obj_count,
                                      const std::vector<std::string>& params)
{
  PMAJOR("ADO_testing_plugin: launch event (auth_id=%lu, %s, %lu, %u, %u, %lu)",
         auth_id, pool_name.c_str(), pool_size,
         pool_flags, memory_type, expected_obj_count);

  for(auto& p : params)
    PMAJOR("param: %s", p.c_str());
}

void ADO_testing_plugin::notify_op_event(component::ADO_op op)
{
  PMAJOR("ADO_testing_plugin: op event (%s)", to_str(op).c_str());
}

namespace
{
  using IADO_plugin = component::IADO_plugin;
  using response_buffer_vector_t = IADO_plugin::response_buffer_vector_t;
  using value_space_t = IADO_plugin::value_space_t;

  status_t basicInvokeAdo(
    IADO_plugin *ap_
    , uint64_t work_key_
    /* work_key_ is required for these callbacks only: create_key, open_key, resize_value, unlock */
    , const std::vector<string_view> & args_
    , const string_view key_
    , value_space_t & values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    const gsl::span<gsl::byte> value(static_cast<gsl::byte *>(values_[0].ptr), values_[0].len);

    PLOG("ADO_testing_plugin: running (key=%.*s)", int(key_.size()), key_.begin());
    PLOG("ADO_testing_plugin: cmd=(%.*s)", int(args_[0].size()), args_[0].begin());
    ASSERT_TRUE(args_[0] == "RUN!TEST-BasicInvokeAdo", "ADO_testing_plugin: unexpected message");

    status_t rc = S_OK;

    std::fill(value.begin(), value.end(), gsl::byte(0xe));
    PLOG("value=%p value_len=%zu", static_cast<void *>(&*value.begin()), value.size());
    ASSERT_TRUE(value.size() == 4096, "ADO_testing_plugin: value bad length");

    /* resize value */
    void *new_addr = nullptr;
    rc             = ap_->cb_resize_value(work_key_, std::string(key_), KB(8), new_addr);
    ASSERT_TRUE(rc == S_OK, "ADO_testing_plugin: resize_value callback failed");
    PLOG("value after resize=%p", new_addr);

    /* check old content is still there */
    void *tmp = malloc(value.size());
    if ( tmp == nullptr )
    {
      throw std::bad_alloc();
    }
    std::memset(tmp, 0xe, value.size());
    /* no actual check of tmp vs new_addr?? */
    /* tmp never freed?? */

    PLOG("ADO_testing_plugin: content after resize OK.");
    std::memset(new_addr, 0xf, KB(8));

    /* create new key-value pair */
    void *                     new_value_addr = nullptr;
    const char *               new_key_addr   = nullptr;
    component::IKVStore::key_t key_handle;
    rc = ap_->cb_create_key(work_key_, "newKey", 12, true, new_value_addr, &new_key_addr, &key_handle);
    std::memset(new_value_addr, 'N', 12);
    PLOG("new key created at %p", new_value_addr);
    ASSERT_TRUE(new_value_addr && rc == S_OK, "ADO_testing_plugin: create_key callback failed");
    ASSERT_TRUE(new_key_addr != nullptr, "ADO_testing_plugin: key ptr is null");
    ASSERT_TRUE(std::strncmp(new_key_addr, "newKey", 6) == 0, "ADO_testing_plugin: key ptr invalid");
    PLOG("key ptr verified OK!");

    /* allocate value memory */
    PLOG("calling cb_allocate_pool_memory");
    void * new_value_memory = nullptr;
    size_t s                = 1200;
    rc                      = ap_->cb_allocate_pool_memory(s, 8 /* alignment */, new_value_memory);
    ASSERT_TRUE(new_value_memory != nullptr, "cb_allocate_pool_memory return nullptr");
    PLOG("new value: %p rc=%d", new_value_memory, rc);
    rc = ap_->cb_free_pool_memory(s, new_value_memory);
    ASSERT_TRUE(rc == S_OK, "free_pool_memory callback failed");
    PLOG("free OK");

    /* get pool info */
    std::string info;
    rc = ap_->cb_get_pool_info(info);
    ASSERT_TRUE(rc == S_OK, "get_pool_info callback failed");
    PINF("pool info:(%s)", info.c_str());
    return rc;
  }

  status_t adoKeyReference(
    IADO_plugin *ap_
    , uint64_t work_key_ /* a key to be passed to some, but not all, callbacks. Why only some? */
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    void *                     new_value_addr = nullptr;
    const char *               new_key_addr   = nullptr;
    component::IKVStore::key_t key_handle, key_handle2;
    ASSERT_OK(
        ap_->cb_create_key(work_key_, "akrKey", 256, ADO_testing_plugin::FLAGS_NO_IMPLICIT_UNLOCK, new_value_addr, &new_key_addr, &key_handle),
        "ADO_testing_plugin: create key failed");

    ASSERT_TRUE(new_value_addr, "ADO_testing_plugin: bad result from create key");
    ASSERT_TRUE(new_key_addr, "ADO_testing_plugin: bad result from create key");

    std::memset(new_value_addr, 'N', 256);
    ASSERT_TRUE(ap_->cb_create_key(work_key_, "akrKey", 256, true, new_value_addr, &new_key_addr, &key_handle2) == E_LOCKED,
                "ADO_testing_plugin: create key should be locked already");

    ASSERT_TRUE(key_handle != nullptr, "ADO_testing_plugin: invalid key_handle");
    ASSERT_OK(ap_->cb_unlock(work_key_, key_handle), "ADO_testing_plugin: unlock callback failed");
    return S_OK;
  }

  status_t basicInvokePutAdo(
    IADO_plugin * // ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & args_
    , const string_view key_
    , value_space_t & values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    const gsl::span<gsl::byte> value(static_cast<gsl::byte *>(values_[0].ptr), values_[0].len);
    string_view val(static_cast<const char *>(static_cast<const void *>(&*value.begin())), value.size());
    PLOG("ADO_testing_plugin: (key=%.*s,value=%.*s)", int(key_.size()), key_.begin(), int(val.size()), val.begin());

    PLOG("ADO_testing_plugin: (request=%.*s)", int(args_[0].size()), args_[0].begin());

    ASSERT_TRUE("RUN!TEST-BasicInvokePutAdo" == args_[0], "ADO_testing_plugin: invoke_put_ado failed");
    ASSERT_TRUE("VALUE_TO_PUT" == val, "ADO_testing_plugin: invoke_put_ado failed");
    return S_OK;
  }

  status_t invokeAdoCreateOnDemand(
    IADO_plugin *ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    void * src  = nullptr;
    void * dst  = nullptr;
    size_t size = KB(64);
    status_t rc = ap_->cb_allocate_pool_memory(size, 4096 /* alignment */, src);
    rc |= ap_->cb_allocate_pool_memory(size, 4096 /* alignment */, dst);
    ASSERT_TRUE(rc == S_OK, "InvokeAdoCreateOnDemand: failed");
    std::memset(src, 0, size);
    std::memset(dst, 0xA, size);
    auto start = rdtsc();
    std::memcpy(dst, src, size);
    pmem_persist(dst, size);
    auto stop = rdtsc();
    PLOG("%lu cycles for %lu (%f usec)", stop - start, size, common::cycles_to_usec(stop - start));
    return rc;
  }

  status_t findKeyCallback(
    IADO_plugin *ap_
    , uint64_t work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    std::string matched_key;
    offset_t    matched_pos = 0;

    void *new_value_addr = nullptr;
    ASSERT_OK(ap_->cb_create_key(work_key_, "aKey", 12, true, new_value_addr, nullptr, nullptr),
              "ADO_testing_plugin: create key failed");
    ASSERT_OK(ap_->cb_create_key(work_key_, "bKey", 13, true, new_value_addr, nullptr, nullptr),
              "ADO_testing_plugin: create key failed");
    ASSERT_OK(ap_->cb_create_key(work_key_, "cKey", 14, true, new_value_addr, nullptr, nullptr),
              "ADO_testing_plugin: create key failed");

    PLOG("issuing find key (pos=%lu)..", matched_pos);
    status_t rc;
    while ((rc = ap_->cb_find_key(".*",
                             matched_pos,                          // begin pos
                             component::IKVIndex::FIND_TYPE_NEXT,  // find type
                             matched_pos, matched_key)) == S_OK) {
      PLOG("match: (%s) @ %lu", matched_key.c_str(), matched_pos);
      matched_pos++;
      ASSERT_OK(rc, "ADO_testing_plugin: find_key failed");
    }
    return S_OK; // ignore rc; always OK
  }

  status_t basicAllocatePoolMemory(
    IADO_plugin *ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    void * result = nullptr;
    void * result2 = nullptr;
    size_t size   = KB(4);

    status_t rc = ap_->cb_allocate_pool_memory(size, 4096 /* alignment */, result);
    ASSERT_TRUE(rc == S_OK, "BasicAllocatePoolMemory::allocate_pool_memory failed");
    PLOG("allocated %p", result);

    rc = ap_->cb_allocate_pool_memory(size, 4096 /* alignment */, result2);
    ASSERT_TRUE(rc == S_OK, "BasicAllocatePoolMemory::allocate_pool_memory failed");
    PLOG("allocated %p", result2);

    ASSERT_TRUE(result != result2, "BasicAllocatePoolMemory::allocate_pool_memory repeat allocation failed");

    ASSERT_OK(ap_->cb_free_pool_memory(size, result), "BasicAllocatePoolMemory::allocate_pool_memory freed failed");
    ASSERT_OK(ap_->cb_free_pool_memory(size, result2), "BasicAllocatePoolMemory::allocate_pool_memory freed failed");
    return rc;
  }

  status_t basicDetachedMemory(
    IADO_plugin *ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    ASSERT_TRUE(values_[1].ptr != nullptr, "BasicDetachedMemory::invalid detached ptr");
    const gsl::span<gsl::byte> detached_value(static_cast<gsl::byte *>(values_[1].ptr), values_[1].len);

    /* free detached memory */
    status_t rc = ap_->cb_free_pool_memory(detached_value.size(), &*detached_value.begin());
    ASSERT_TRUE(rc == S_OK, "BasicDetachedMemory::free_pool_memory failed");
    return rc;
  }

  /* RUN!TEST-AddDetachedMemory shall allocate 17 areas of deatched memory, and replace the 17*8 bytes written by value1 with 17 pointers to those areas */
  status_t addDetachedMemory(
    IADO_plugin *ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    ASSERT_TRUE(values_[0].ptr != nullptr, "addDetachedMemory::invalid space");
    std::array<void *, 17> p;
    ASSERT_TRUE(values_[0].len == p.size() * sizeof(void *), "addDetachedMemory::invalid space");
    constexpr size_t align = 8;
    for ( auto i = 0U; i != p.size(); ++i )
    {
      std::size_t sz = 1 << i;
      status_t rc = ap_->cb_allocate_pool_memory(sz, align, p[i]);
      ASSERT_OK(rc, "%s: rc %d", __func__, rc);
      /* fill the allocated space with a checkable value */
      std::fill_n(static_cast<char *>(p[i]), sz, char('0' + i));
    }
    ASSERT_TRUE(values_[0].len == p.size() * sizeof(&p[0]), "%s: mismatch values_[0].len %zu vs p.size %zu, sizeof(&p[0]) %zu", __func__, values_[0].len, p.size(), sizeof(&p[0]));
    /* preserve the pointers in the value location */
    memcpy(values_[0].ptr, &p[0], p.size() * sizeof(&p[0]));
    return S_OK;
  }

  /* RUN!TEST-CompareDetachedMemory shall iverify that the 17 areas of deatched memory written by RUN!TEST-AddDetachedMemory are intact */
  status_t compareDetachedMemory(
    IADO_plugin *ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    ASSERT_TRUE(values_[1].ptr != nullptr, "compareDetachedMemory::invalid detached ptr");
    std::array<void *, 17> p;
    ASSERT_TRUE(values_[0].len == p.size() * sizeof(&p[0]), "%s: mismatch values_[0].len %zu vs p.size %zu, sizeof(&p[0]) %zu", __func__, values_[0].len, p.size(), sizeof(&p[0]));
    /* Recover the pointers from the value location */
    memcpy(&p[0], values_[0].ptr, p.size() * sizeof(&p[0]));
    for ( auto i = 0U; i != p.size(); ++i )
    {
      std::size_t sz = 1 << i;
      auto ev = char('0' + i);
      auto c = static_cast<char *>(p[i]);
      auto e = std::find_if(c, c+sz, [ev] (const char c_) { return c_ != ev; });
      ASSERT_TRUE(e == c+sz, "%s: detached element %d byte at %zu bad", __func__, i, std::size_t(e-c));
      status_t rc = ap_->cb_free_pool_memory(sz, p[i]);
      ASSERT_OK(rc, "%s: rc %d", __func__, rc);
    }
    return S_OK;
  }

  status_t getReferenceVector(
    IADO_plugin *ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    IADO_plugin::Reference_vector v;
    status_t rc = ap_->cb_get_reference_vector(0, 0, v);
    PMAJOR("rc (count=%lu, val=%p, val_len=%lu", v.count(), static_cast<const void *>(v.ref_array()),
           v.value_memory_size());

    ASSERT_TRUE(rc == S_OK, "get_reference_vector failed rc %d", rc);
    ASSERT_TRUE(v.count() == 4, "GetReferenceVector:: unexpected vector size %zu", v.count());

    auto ref = v.ref_array();
    for (size_t i = 0; i < v.count(); i++) {
      PLOG("kv: key=(%p, %lu) val=(%p, %lu)", ref[i].key, ref[i].key_len, ref[i].value, ref[i].value_len);
      ASSERT_FALSE(ref[i].key == nullptr, "GetReferenceVector::bad reference vector");
      ASSERT_TRUE(ref[i].key_len > 0, "GetReferenceVector::bad reference vector");
      ASSERT_FALSE(ref[i].value == nullptr, "GetReferenceVector::bad reference vector");
      ASSERT_TRUE(ref[i].value_len > 0, "GetReferenceVector::bad reference vector");
    }

    for (size_t i = 0; i < v.count(); i++) {
      PLOG("kv: key=(%s) val=(%s)", reinterpret_cast<char *>(ref[i].key), reinterpret_cast<char *>(ref[i].value));
    }

    rc = ap_->cb_free_pool_memory(v.value_memory_size(), v.value_memory());

    ASSERT_TRUE(rc == S_OK, "GetReferenceVector::free_pool_memory failed");
    return rc;
  }

  status_t iterator(
    IADO_plugin *ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    component::IKVStore::pool_iterator_t  iterator = nullptr;
    component::IKVStore::pool_reference_t r;
    status_t rc = S_OK;
    while ((rc = ap_->cb_iterate(0, 0, iterator, r)) == S_OK) {
      PLOG("Iterator: ref (%.*s,%.*s,%s)", int(r.key_len), static_cast<const char*>(r.key),
           int(r.value_len), static_cast<const char *>(r.value), r.timestamp.str().c_str());
    }
    if (rc == E_NOT_IMPL) PLOG("Component does not support iterator.");

    ASSERT_TRUE(rc == E_NOT_IMPL || rc == E_OUT_OF_BOUNDS, "iterator failed");
    return S_OK;
  }

  status_t iteratorTS(
    IADO_plugin *ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & args_ // arg_[1] is string representation of the time point over which to iterate
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & response_buffers_
  )
  {
    ASSERT_TRUE(1 < args_.size(), "%s: missing time point argument", __func__);
    common::epoch_time_t ts(std::strtoull(args_[1].begin(), nullptr, 0), 0);

    PLOG("IteratorTS: requested begin timestamp: %lu seconds", ts.seconds());
    status_t rc = S_OK;
    unsigned cnt = 0;
    {
      component::IKVStore::pool_iterator_t iterator = nullptr;
      component::IKVStore::pool_reference_t r;
      while ((rc = ap_->cb_iterate(ts, 0, iterator, r)) == S_OK) {
        PLOG("Iterator: ref (%.*s,%.*s,%s)", int(r.key_len), static_cast<const char*>(r.key),
             int(r.value_len), static_cast<const char *>(r.value), r.timestamp.str().c_str());
        cnt++;
      }
      if (rc == E_NOT_IMPL) PLOG("Component does not support iterator.");

      /* return count */
      uint64_t * rb = reinterpret_cast<uint64_t *>(malloc(sizeof(uint64_t)));
      if ( rb == nullptr )
      {
        throw std::bad_alloc();
      }
      *rb = cnt;
      //      *rb memcpy(rb, text.data(), rb_len);
      response_buffers_.emplace_back(rb, sizeof(uint64_t), IADO_plugin::response_buffer_t::alloc_type_malloc{});
    }

    return S_OK;
  }

  status_t erase(
    IADO_plugin * // ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    PLOG("performing self erase");
    return IADO_plugin::S_ERASE_TARGET;
  }

  status_t basicAdoResponse(
    IADO_plugin * // ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view key_
    , value_space_t & // values_
    , response_buffer_vector_t & response_buffers_
  )
  {
    /* return the key in the ADO response */
    auto rb_len = key_.size();
    void * rb = malloc(rb_len);
    if ( rb == nullptr )
    {
      throw std::bad_alloc();
    }
    std::copy(key_.begin(), key_.end(), static_cast<char *>(rb));
    response_buffers_.emplace_back(rb, rb_len, IADO_plugin::response_buffer_t::alloc_type_malloc{});
    return S_OK;
  }

  status_t getReferenceVectorByTime(
    IADO_plugin *ap_
    , uint64_t work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & // values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
    IADO_plugin::Reference_vector v;
    status_t rc = ap_->cb_get_reference_vector(0, 0, v);
    PMAJOR("rc (count=%lu, val=%p, val_len=%lu", v.count(), static_cast<const void *>(v.ref_array()),
           v.value_memory_size());

    ASSERT_TRUE(rc == S_OK, "get_reference_vector failed");
    ASSERT_TRUE(v.count() == 21, "get_reference_vector failed");

    {
      auto ref = v.ref_array();
      for (size_t i = 0; i < v.count(); i++) {
        PLOG("kv: key=(%p, %lu) val=(%p, %lu)", ref[i].key, ref[i].key_len, ref[i].value, ref[i].value_len);
        ASSERT_FALSE(ref[i].key == nullptr, "GetReferenceVector::bad reference vector");
        ASSERT_TRUE(ref[i].key_len > 0, "GetReferenceVector::bad reference vector");
        ASSERT_FALSE(ref[i].value == nullptr, "GetReferenceVector::bad reference vector");
        ASSERT_TRUE(ref[i].value_len > 0, "GetReferenceVector::bad reference vector");
      }
    }

    rc = ap_->cb_free_pool_memory(v.value_memory_size(), v.value_memory());
    ASSERT_TRUE(rc == S_OK, "GetReferenceVector::free_pool_memory failed");

    wmb();
    /* now do by time retrieval */
    {
      sleep(3);
      auto now = common::epoch_now();
      PLOG("epoch now: %s", now.str().c_str());
      wmb();
      sleep(1);
      {
        void *new_value_addr = nullptr;
        ASSERT_OK(ap_->cb_create_key(work_key_, "aKey", 7, true, new_value_addr, nullptr, nullptr),
                  "create key failed GetReferenceVectorByTime");
        std::strcpy(static_cast<char *>(new_value_addr), "Hello!");
      }

      wmb();
      {
        void *new_value_addr = nullptr;
        ASSERT_OK(ap_->cb_create_key(work_key_, "anotherKey", 7, true, new_value_addr, nullptr, nullptr),
                  "create key failed GetReferenceVectorByTime");
        std::strcpy(static_cast<char *>(new_value_addr), "World!");
      }

      wmb();
      rc = ap_->cb_get_reference_vector(now, 0, v);

      PMAJOR("by-time rc (count=%lu, val=%p, val_len=%lu", v.count(),
             static_cast<const void *>(v.ref_array()), v.value_memory_size());

      ASSERT_TRUE(v.count() == 2, "bad vector count GetReferenceVectorByTime");

      {
        auto ref = v.ref_array();
        for (size_t i = 0; i < v.count(); i++) {
          PLOG("kv: key=(%.*s) val=(%.*s)", int(ref[i].key_len),
               static_cast<const char *>(ref[i].key), int(ref[i].value_len),
               static_cast<const char *>(ref[i].value));
          ASSERT_FALSE(ref[i].key == nullptr,
                       "GetReferenceVector::bad reference vector");
          ASSERT_TRUE(ref[i].key_len > 0,
                      "GetReferenceVector::bad reference vector");
          ASSERT_FALSE(ref[i].value == nullptr,
                       "GetReferenceVector::bad reference vector");
          ASSERT_TRUE(ref[i].value_len > 0,
                      "GetReferenceVector::bad reference vector");
        }
      }

      rc = ap_->cb_free_pool_memory(v.value_memory_size(), v.value_memory());
      ASSERT_TRUE(rc == S_OK, "GetReferenceVector::free_pool_memory failed");
    }
    return rc;
  }

  /* this gets run for ado-perf performance test */
  status_t other(
    IADO_plugin * // ap_
    , uint64_t // work_key_
    , const std::vector<string_view> & // args_
    , const string_view // key_
    , value_space_t & values_
    , response_buffer_vector_t & // response_buffers_
  )
  {
#if 0
    const gsl::span<gsl::byte> value(static_cast<gsl::byte *>(values_[0].ptr), values_[0].len);
    pmem_memset(&*value.begin(), 0x1, value.size(), 0);
#else
    (void)values_;
#endif
    return S_OK;
  }
}

namespace
{
  std::vector<string_view> split(string_view sv)
  {
    std::vector<string_view> v;
    auto cursor = sv.begin();
    while ( cursor != sv.end() )
    {
      /* find to first space */
      auto e = std::find(cursor, sv.end(), ' ');
      v.emplace_back(cursor, e-cursor);
      /* skip over spacesi (one or more) */
      cursor = std::find_if(e, sv.end(), [] ( const char &c ) { return c != ' '; });
    }
    return v;
  }
}

status_t ADO_testing_plugin::do_work(uint64_t                     work_key,
                                     const char *                 key_addr,
                                     size_t                       key_len,
                                     IADO_plugin::value_space_t & values,
                                     const void *                 in_work_request, /* don't use iovec because of non-const */
                                     const size_t                 in_work_request_len,
                                     bool                         new_root,
                                     response_buffer_vector_t &   response_buffers)
{
  (void)new_root; // unused

  ASSERT_TRUE(values[0].ptr != nullptr, "ADO_testing_plugin:%s: bad parameter", __func__);
  ASSERT_TRUE(values[0].len != 0, "ADO_testing_plugin:%s: value_len is 0", __func__);
  ASSERT_TRUE(key_addr != nullptr, "ADO_testing_plugin:%s: bad parameter", __func__);
  ASSERT_TRUE(key_len != 0, "ADO_testing_plugin:%s: bad parameter", __func__);

  const string_view key(key_addr, key_len);
  const string_view cmd(static_cast<const char *>(in_work_request), in_work_request_len);

  using fn_t =
    status_t (*)(
      IADO_plugin * // ap_
      , uint64_t // work_key_
      , const std::vector<string_view> & // args_: the work request. Element 0 is "RUN!TEST-....". Ignored except for basicAdoResponse, where it is checkied against expected value "RUN!TEST-BasicInvokeAdo"
      , const string_view // key_: iA unique key, usually some variant of the command name Ignored except in basicInvokeAdo (used as the key to value in cb_resize_value), basicInvokePutAdo (printed in the log) and basicAdoResponse (returned as the response string)
      , value_space_t & // values_ : used in basicInvokeAdo (value[0] filled with 0xe prior to resize), basicInvokePutAdo (printed in the log), basicDetachedMemory (value[1] is freed) and other (possibly persists value[0])
      , response_buffer_vector_t & // response_buffers_
    );

  const static std::map<string_view, fn_t> ops = {
    { "RUN!TEST-BasicInvokeAdo", basicInvokeAdo },
    { "RUN!TEST-BasicInvokePutAdo", basicInvokePutAdo },
    { "RUN!TEST-InvokeAdoCreateOnDemand", invokeAdoCreateOnDemand },
    { "RUN!TEST-AdoKeyReference", adoKeyReference },
    { "RUN!TEST-FindKeyCallback", findKeyCallback },
    { "RUN!TEST-BasicAllocatePoolMemory", basicAllocatePoolMemory },
    { "RUN!TEST-BasicDetachedMemory", basicDetachedMemory },
    { "RUN!TEST-AddDetachedMemory", addDetachedMemory },
    { "RUN!TEST-CompareDetachedMemory", compareDetachedMemory },
    { "RUN!TEST-GetReferenceVector", getReferenceVector },
    { "RUN!TEST-GetReferenceVectorByTime", getReferenceVectorByTime },
    { "RUN!TEST-Iterate", iterator },
    { "RUN!TEST-IteratorTS", iteratorTS },
    { "RUN!TEST-Erase", erase },
    { "RUN!TEST-BasicAdoResponse", basicAdoResponse },
    { "BLAST ME!", other }, // used by ado-perf
    { "put", other }, // used by ado-perf
    { "erase", erase }, // used by ado-perf
  };

  std::vector<string_view> args = split(cmd);

  assert(0 != args.size());
  const auto it = ops.find(args[0]);
  ASSERT_TRUE(it != ops.end(), "%s command %.*s not found", __func__, int(args[0].size()), args[0].begin());
  status_t rc = (it->second)(this, work_key, args, key, values, response_buffers);

  if(option_DEBUG)
    PMAJOR("%s %.*s length %zu return %d", __FILE__, int(key.size()), key.begin(), key.size(), rc);

  return rc;
}

status_t ADO_testing_plugin::shutdown()
{
  /* here you would put graceful shutdown code if any */
  return S_OK;
}

/**
 * Factory-less entry point
 *
 */
extern "C" void *factory_createInstance(component::uuid_t interface_iid)
{
  PLOG("instantiating ADO_testing_plugin");
  if (interface_iid == interface::ado_plugin)
    return static_cast<void *>(new ADO_testing_plugin());
  else
    return NULL;
}

#undef RESET_STATE
