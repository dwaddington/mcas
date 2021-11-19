/*
  Copyright [2020] [IBM Corporation]
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

#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <kvstore_api_wrapper.h>

#define KB(X) (X << 10)
#define MB(X) (X << 20)
#define GB(X) ((1ULL << 30) * X)
#define TB(X) ((1ULL << 40) * X)

/** 
 * Use this with testing ADO
 * 
 */
int main() //int argc, char* argv[])
{
  kvstore_t store;
  unsigned debug_level = 3;

  assert(kvstore_create(debug_level, "{ \"store_type\":\"mapstore\" }", &store) == 0);

  pool_t pool, pool2, pool3;
  assert(kvstore_create_pool(store, "myPool", MB(4), 0, &pool) == 0);
  assert(kvstore_create_pool(store, "myPool2", MB(4), 0, &pool2) == 0);
  assert(kvstore_create_pool(store, "myPoolThatIsSmall", KB(4), 0, &pool3) == 0);

  {
    char ** names;
    size_t name_count = 0;
    kvstore_get_pool_names(store, &names, &name_count);
    printf("Pool name count:%lu\n", name_count);
    assert(name_count == 3);
    for(unsigned i=0;i<name_count;i++) {
      printf("Pool: %s\n", names[i]);
      free(names[i]);
    }
    free(names);
  }

  /* put kv pairs */
  for(unsigned i=0;i<10;i++) {
    char key[32];
    char value[32];
    sprintf(key, "key-%u", i);
    sprintf(value, "value-of-%u", i);
    assert(kvstore_put(store, pool, key, value, strlen(value), 0) == 0);
  }

  /* get them back */
  for(unsigned i=0;i<10;i++) {
    char key[32];
    void * value_ptr = NULL;;
    size_t value_len = 0;
    sprintf(key, "key-%u", i);
    assert(kvstore_get(store, pool, key, &value_ptr, &value_len) == 0);
    printf("key:(%s) value:(%s:%lu)\n",
           key, (char*)value_ptr, value_len);
    free(value_ptr);
  }

  /*   status_t kvstore_lock_existing(const kvstore_t store_handle, */
  /*                                const pool_t pool_handle, */
  /*                                const char * key, */
  /*                                const int lock_type, */
  /*                                void ** out_value_ptr, */
  /*                                size_t * out_value_len, */
  /*                                lock_token_t * out_lock_token); */
  
  /* status_t kvstore_unlock(const kvstore_t store_handle, */
  /*                         const pool_t pool_handle, */
  /*                         const lock_token_t lock_token); */


  {
    void * p;
    size_t p_len;
    lock_token_t tok;
    assert(kvstore_lock_existing(store, pool, "key-2", 2, &p, &p_len, &tok) == 0);
    printf("Locked:key-2 (%s)\n", (char*) p);
    assert(kvstore_unlock(store, pool, tok) == 0);
  }

  printf("\n\n");
  size_t new_size;
  assert(kvstore_grow_pool(store, pool, MB(2), &new_size) == 0);
  printf("new size = %lu\n", new_size);
  assert(new_size > (MB(4)  + MB(2)));
  
  assert(kvstore_close_pool(store, pool) == 0);
  assert(kvstore_open_pool(store, "myPool", 0, &pool) == 0);
  assert(kvstore_close_pool(store, pool) == 0);
  assert(kvstore_close_pool(store, pool2) == 0);
  assert(kvstore_delete_pool(store, "myPool") == 0);
  assert(kvstore_delete_pool(store, "badPoolName") == -53);
  assert(kvstore_close(store) == 0);
  printf("OK!!!\n");
  //  free(store); - compiler should prevent this
  

  /* /\* open session *\/ */
  /* mcas_session_t session; */

  /* assert(mcas_open_session_ex(argv[1], /\* server *\/ */
  /*                             argv[2], /\* net device *\/ */
  /*                             2, */
  /*                             30, */
  /*                             &session) == 0); */

  /* /\* create a pool *\/ */
  /* mcas_pool_t pool; */
  /* assert(mcas_create_pool(session, "myPool", MB(64), 0, &pool) == 0); */

  /* assert(mcas_configure_pool(pool, "AddIndex::VolatileTree") == 0); */
  
  /* /\* put *\/ */
  /* assert(mcas_put(pool, "someKey", "someValue", 0) == 0); */
  /* assert(mcas_put(pool, "someX", "someValue", 0) == 0); */
  /* assert(mcas_put(pool, "someY", "someValue", 0) == 0); */
  /* assert(mcas_put(pool, "someZ", "someValue", 0) == 0); */
  /* assert(mcas_put(pool, "someOtherKey", "someOtherValue", 0) == 0); */

  /* /\* async put *\/ */
  /* { */
  /*   mcas_async_handle_t handle; */
  /*   assert(mcas_async_put(pool, "fooBar", "fooBarValue", 0, &handle) == 0); */

  /*   while(mcas_check_async_completion(session, handle) != 0) { */
  /*     usleep(1000); */
  /*   } */
  /* } */
    
  /* /\* get *\/ */
  /* { */
  /*   void * v; */
  /*   size_t vlen = 0; */
  /*   assert(mcas_get(pool, "fooBar", &v, &vlen) == 0); */
  /*   assert(vlen > 0); */
  /*   assert(mcas_free_memory(session, v) == 0); */
  /* } */

  /* printf("count: %lu\n", mcas_count(pool)); */

  /* /\* get attribute *\/ */
  /* { */
  /*   uint64_t * p = NULL; */
  /*   size_t p_count = 0; */
  /*   assert(mcas_get_attribute(pool, */
  /*                             NULL, */
  /*                             ATTR_COUNT, */
  /*                             &p, */
  /*                             &p_count) == 0); */
  /*   printf("count-->: %lu\n", p[0]); */
  /*   free(p); */
  /* } */
  

  /* /\* erase key *\/ */
  /* assert(mcas_erase(pool, "someKey") == 0); */
  
  /* /\* allocate some memory *\/ */
  /* void * ptr = aligned_alloc(4096, MB(2)); */
  /* mcas_memory_handle_t mr; */
  /* memset(ptr, 0xf, MB(2)); */
  /* assert(mcas_register_direct_memory(session, ptr, MB(2), &mr) == 0); */


  /* /\* perform direct transfers *\/ */
  /* assert(mcas_put_direct_ex(pool, */
  /*                           "myBigKey", */
  /*                           ptr, */
  /*                           MB(2), */
  /*                           mr, */
  /*                           0) == 0); */

  /* { */
  /*   size_t s = MB(2); */
  /*   assert(mcas_get_direct_ex(pool, */
  /*                             "myBigKey", */
  /*                             ptr, */
  /*                             &s, */
  /*                             mr) == 0); */
  /* } */

  /* /\* async get direct *\/ */
  /* { */
  /*   mcas_async_handle_t async_handle; */
  /*   size_t s = MB(2); */
  /*   memset(ptr, 0x0, MB(2)); */
  /*   assert(mcas_async_get_direct_ex(pool, */
  /*                                   "myBigKey", */
  /*                                   ptr, */
  /*                                   &s, */
  /*                                   mr, */
  /*                                   &async_handle) == 0); */
  /*   while(mcas_check_async_completion(session, async_handle) != 0) */
  /*     usleep(1000); */

  /*   char * p = (char*)ptr; */
  /*   assert(p[0] == 0xf); */
  /* } */

  /* /\* invoke ado *\/ */
  /* { */
  /*   const char * request = "RUN!TEST-BasicAdoResponse"; */
  /*   mcas_response_array_t out_response_vector; */
  /*   size_t out_response_vector_count = 0; */
    
  /*   assert(mcas_invoke_ado(pool, */
  /*                          "adoKey0-will-be-response", */
  /*                          request, */
  /*                          strlen(request), */
  /*                          0, */
  /*                          4096, */
  /*                          &out_response_vector, */
  /*                          &out_response_vector_count) == 0); */
  /*   printf("response count: %lu\n", out_response_vector_count); */
  /*   assert(out_response_vector_count == 1); */
  /*   printf("response[0]: (%.*s)\n", */
  /*          (int) out_response_vector[0].len, */
  /*          (char*) out_response_vector[0].ptr); */

  /*   mcas_free_responses(out_response_vector); */
  /* } */

  /* /\* async invoke put ado *\/ */
  /* { */
  /*   const char * request = "RUN!TEST-BasicAdoResponse"; */
  /*   const char * value = "Test value"; */
  /*   mcas_response_array_t out_response_vector; */
  /*   size_t out_response_vector_count = 0; */
  /*   mcas_async_handle_t task; */
    
  /*   assert(mcas_async_invoke_put_ado(pool, */
  /*                                    "adoKey1-will-be-response", */
  /*                                    request,                        */
  /*                                    strlen(request), */
  /*                                    value, */
  /*                                    strlen(value), */
  /*                                    4096, */
  /*                                    0, */
  /*                                    &task) == 0); */

  /*   while(mcas_check_async_invoke_put_ado(pool, */
  /*                                         task, */
  /*                                         &out_response_vector, */
  /*                                         &out_response_vector_count) != 0) { */
  /*     usleep(100); */
  /*   } */
  /*   printf("response count: %lu\n", out_response_vector_count); */
  /*   assert(out_response_vector_count == 1); */
  /*   printf("response[0]: (%.*s)\n", */
  /*          (int) out_response_vector[0].len, */
  /*          (char*) out_response_vector[0].ptr); */

  /*   mcas_free_responses(out_response_vector); */
  /* } */


  /* /\* find *\/ */
  /* { */
  /*   char * matched_key; */
  /*   offset_t curr_offset = 0; */
  /*   int hr; */
  /*   do { */
  /*     hr = mcas_find(pool, */
  /*                    "next:", */
  /*                    curr_offset, */
  /*                    &curr_offset, */
  /*                    &matched_key); */
  /*     printf("find matched-> (%s)\n", matched_key); */
  /*     curr_offset++; */
  /*   } */
  /*   while(hr >= 0); */
  /* } */


  /* /\* clean up memory *\/ */
  /* assert(mcas_unregister_direct_memory(session, mr) == 0); */
  /* free(ptr); */
  /* assert(mcas_close_pool(pool) == 0); */

  /* /\* delete pool *\/ */
  /* assert(mcas_delete_pool(session, "myPool") == 0); */
  
  /* /\* close session *\/ */
  /* assert(mcas_close_session(session) == 0); */

  return 0;
}
