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
#include <libpmem.h>
#include <common/logging.h>
#include <common/utils.h>
#include <common/dump_utils.h>
#include <common/cycles.h>
#include <api/interfaces.h>
#include <sstream>
#include <string.h>
#include <ccpm/value.h>
#include <ccpm/record.h>
#include <ccpm/immutable_string_table.h>
#include <EASTL/list.h>

#include "finex_plugin.h"
#include "finex_types.h"

//#include <flatbuffers/flatbuffers.h>

status_t Finex_plugin::register_mapped_memory(void * shard_vaddr,
                                                 void * local_vaddr,
                                                 size_t len)
{
  PLOG("Finex_plugin: register_mapped_memory (%p, %p, %lu)", shard_vaddr, local_vaddr, len);
  /* we would need a mapping if we are not using the same virtual
     addresses as the Shard process */

  return S_OK;
}

#include <libpmem.h>



status_t Finex_plugin::do_work(const uint64_t work_key,
                               const char * key,
                               size_t key_len,
                               IADO_plugin::value_space_t& values,
                               const void * in_work_request,
                               const size_t in_work_request_len,
                               bool new_root,
                               response_buffer_vector_t& response_buffers)
{
  using namespace ccpm;
  using namespace Graph_ADO_protocol;
  using namespace flatbuffers;

  auto value = values[0].ptr;
  auto value_len = values[0].len;

  PLOG("key:%s value:%p value_len:%lu", key, value, value_len);
  PLOG("work_request: %p len=%lu", in_work_request, in_work_request_len);

  Verifier verifier(static_cast<const uint8_t*>(in_work_request), in_work_request_len);
  
  if(VerifyMessageBuffer(verifier)) {
    auto msg = GetMessage(in_work_request);
    auto transaction = msg->element_as_Transaction();
    if(transaction)
      return handle_transaction(transaction, work_key, value, value_len);    
  }

  throw General_exception("unhandled data");
  return E_FAIL;
}

inline void * ptrinc(void * ptr, size_t n) {
  return static_cast<void*>(static_cast<byte*>(ptr) + n);
}


using namespace ccpm;


status_t Finex_plugin::handle_transaction(const Graph_ADO_protocol::Transaction * transaction,
                                          uint64_t work_key,
                                          void * value,
                                          size_t value_len)
{
  PMAJOR("transaction: %s %s", transaction->target()->c_str(), transaction->source()->c_str());
  assert(value_len >= sizeof(Cow_value_pointer<int>));

  assert(value_len >= sizeof(finex::Transaction));

  status_t rc;
  void * src_value = nullptr, * dst_value = nullptr;
  size_t src_value_len = 0, dst_value_len = 0;

  rc =_cb.open_key(work_key,
                   transaction->source()->c_str(),
                   0, //Component::IADO_plugin::FLAGS_PERMANENT_LOCK,
                   src_value,
                   src_value_len,
                   nullptr,
                   nullptr);

  
  rc |=_cb.open_key(work_key,
                    transaction->target()->c_str(),
                    0, //Component::IADO_plugin::FLAGS_PERMANENT_LOCK,
                    dst_value,
                    dst_value_len,
                    nullptr,
                    nullptr);

  if(rc != S_OK) {
    PWRN("Bad data!");
    return rc;
  }

  PINF("Dest node  : dst_value=%p dst_len=%lu", dst_value, dst_value_len);
  PINF("Source node: src_value=%p src_len=%lu", src_value, src_value_len);

  // using Allocator = EASTL_keyed_memory_allocator;
    
  // /* memory allocator attached to a key */
  // static Keyed_memory_allocator mm("my-allocator", work_key, _cb, first);
  // Allocator heap(&mm);
  // //  heap.set_inner_allocator(&mm);
  
  // using Element = uint64_t;
  // using List = eastl::list<Element, Allocator>;
  // eastl::list<Element, Allocator> * edges;
  // if(first) {
  //   edges = new (mm.allocate(sizeof(List))) List(heap);
  //   *(reinterpret_cast<List **>(value)) = edges;
  //   first = false;
  // }
  // else {
  //   edges = *(reinterpret_cast<List **>(value));
  //   edges->set_allocator(heap);
  // }

  // PLOG("edges @%p", edges);
  
  // edges->push_front(count++);
  // mm.persist();
  
  // for(auto& i : *edges) {
  //   PLOG("edge: %lu", i);
  // }
  // PLOG("---");

  // /* get hold of root node or create a new one */
  // List_node<Edge> * root_node;  
  // if(first) {
  //   first = false;
  //   root_node = new (mm.allocate()) List_node<Edge>;
  //   *(reinterpret_cast<List_node<Edge> **>(value)) = root_node;
  // }
  // else {
  //   root_node = *(reinterpret_cast<List_node<Edge> **>(value));
  // }
#if 0
  /* open nodes */
  status_t rc;
  void * src_value = nullptr, * dst_value = nullptr;
  size_t src_len = 0, dst_len = 0;

  PLOG("opening key (%s)", transaction->source()->c_str());
  rc =_cb.open_key(work_key,
                        transaction->source()->c_str(),
                        0, //Component::IADO_plugin::FLAGS_PERMANENT_LOCK,
                        src_value,
                        src_len);
  PINF("Source node: rc=%d src_value=%p src_len=%lu", rc, src_value, src_len);
                        

  PLOG("opening key (%s)", transaction->target()->c_str());
  rc =_cb.open_key(work_key,
                        transaction->target()->c_str(),
                        0, //Component::IADO_plugin::FLAGS_PERMANENT_LOCK,
                        dst_value,
                        dst_len);
  PINF("Dest node: rc=%d dst_value=%p dst_len=%lu", rc, dst_value, dst_len);

  /* create new node and add to list */
  auto new_node = new (mm.allocate()) List_node<Edge>();
  new_node->assign(dst_value, src_value, transaction->amount());
  root_node->push_front(new_node);

  root_node->print();
#endif
  return S_OK;
}
                                  
status_t Finex_plugin::shutdown()
{
  /* here you would put graceful shutdown code if any */
  return S_OK;
}

/** 
 * Factory-less entry point 
 * 
 */
extern "C" void * factory_createInstance(Component::uuid_t& interface_iid)
{
  PLOG("instantiating Finex_plugin");
  if(interface_iid == Interface::ado_plugin) 
    return static_cast<void*>(new Finex_plugin());
  else return NULL;
}



  
  // if(VerifyPropertySetBuffer(verifier)==false)
  //   throw General_exception("bad data");
  
  //  Versioned_record_type record(shard_value_vaddr, value_len);
  
  // ccpm::Immutable_string_table<> * symtab;
  
  // if(strncmp("st-init",(const char *)in_work_request,7)==0) {
  //   symtab = new ccpm::Immutable_string_table<>(shard_value_vaddr, value_len);
  //   return S_OK;
  // }

  // return S_OK;
  // symtab = new ccpm::Immutable_string_table<>(shard_value_vaddr);
  //  auto new_str_ptr = symtab->add_string((const char *) in_work_request,  in_work_request_len);
  //  PLOG("added string: %p (%s)", new_str_ptr, (const char *) new_str_ptr);
  //  
  //  PLOG("Finex_plugin: work_id (%lu)", work_key);
  //  PLOG("Finex_plugin: do_work (%s, value_addr=%p, valuen_len=%lu)", key.c_str(), shard_value_vaddr, value_len);
  // PLOG("Current value: %.*s", (int) value_len, (char *) shard_value_vaddr); 
  // memset(shard_value_vaddr, 'X', value_len);  
  // nupm::mem_flush(shard_value_vaddr, value_len);
  // out_work_response_len = 3;
  // out_work_response = ::malloc(out_work_response_len);
  // strncpy((char*) out_work_response, "OK!", 3);
  
  // /* test callback */
  // void * new_value_addr = nullptr;
  // _func_create_key(work_key, "newKey", 12, new_value_addr);
  // memset(new_value_addr,'N', 12);
  // PLOG("new key created at %p", new_value_addr);

