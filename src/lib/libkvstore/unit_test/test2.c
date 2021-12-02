/*
  Copyright [2021] [IBM Corporation]
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

#define DAX_CONFIG "[{ \"path\": \"/mnt/pmem0\", \"addr\": \"0x900000000\" }]"
//#define DAX_CONFIG "[{ \"path\": \"/dev/dax0.0\", \"addr\": \"0x900000000\" }]"

/** 
 * Use this with testing ADO
 * 
 */
int main() //int argc, char* argv[])
{
  kvstore_t store;
  unsigned debug_level = 3;

  if(getenv("MM_PLUGIN_PATH") == NULL) {
    printf("MM_PLUGIN_PATH environment variable must be defined for this test (libmm-plugin-rcalb.so for mapstore)\n");
    return -1;
  }
  
  char json_spec[4096];
  sprintf(json_spec,
          "{ \"store_type\":\"hstore\", \"mm_plugin_path\" : \"%s\", \"dax_config\" : %s }",
          getenv("MM_PLUGIN_PATH"),
          DAX_CONFIG
          );

  printf("%s\n",json_spec);

  assert(kvstore_open(debug_level, json_spec, &store) == 0);

  /* create pool, add some pairs, close, delete */
  {
    pool_t pool;
    assert(kvstore_create_pool(store, "myPool", MB(8), 0, &pool) == 0);
    
    for(unsigned i=0;i<10;i++) {
      char key[32];
      char value[32];
      sprintf(key, "key-%u", i);
      sprintf(value, "value-of-%u", i);
      assert(kvstore_put(store, pool, key, value, strlen(value), 0) == 0);
    }

    assert(kvstore_close_pool(store, pool) == 0);
    assert(kvstore_delete_pool(store, "myPool") == 0);
  }

  /* create multiple pools and add pairs */
  {
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
      assert(kvstore_put(store, pool, key, value, strlen(value), 0) == S_OK);
    }
    
    /* get them back */
    for(unsigned i=0;i<10;i++) {
      char key[32];
      sprintf(key, "key-%u", i);
      printf("getting .. (%s)\n", key);
      void * value_ptr = NULL;;
      size_t value_len = 0;
      status_t s = kvstore_get(store, pool, key, &value_ptr, &value_len);
      if(s!=S_OK) {
        printf("error result: %d\n", s);
        return 0;
      }
      
      sprintf(key, "key-%u", i);
      assert(value_ptr);
      printf("key:(%s) value:(%.*s:%lu)\n",
             key, (int) value_len, (char*)value_ptr, value_len);
      kvstore_free_memory(store, value_ptr);
    }

    /* iterate over them */
    {
      pool_iterator_t iter;
      assert(kvstore_iterator_open(store, pool, &iter) == 0);
      struct pool_reference_t ref;
      struct timespec ts_zero = {0,0};

      unsigned count = 0;
      while(kvstore_iterator_next(store, pool, iter, ts_zero, ts_zero, &ref) == S_OK) {
        printf("Got pair: %s %s ts=%ld:%lu match=%s\n",
               (const char*) ref.key, (const char *) ref.value,
               ref.timestamp.tv_sec,
               ref.timestamp.tv_nsec, ref.time_match > 0 ? "yes" : "no");
        count++;
      }
      printf("Total count:%u\n", count);
      assert(count == 10);
      
      assert(kvstore_iterator_close(store, pool, iter) == 0);
    }

    /* perform growth */
    {
      size_t new_size = 0;
      assert(kvstore_close_pool(store, pool) == 0);
      assert(kvstore_grow_pool(store, pool, MB(2), &new_size) == -53); /* can't use closed pool */
      assert(kvstore_open_pool(store, "myPool", 0, &pool) == 0);
      assert(kvstore_grow_pool(store, pool, MB(2), &new_size) == 0);
      assert(new_size >= (MB(4)  + MB(2)));
    }

    /* resize value */
    {
      char value[32];
      sprintf(value, "hello world!");
      assert(kvstore_put(store, pool, "myResizeKey", value, strlen(value), 0) == 0);
      {
        void * value_ptr = NULL;
        size_t value_len = 0;
        assert(kvstore_get(store, pool, "myResizeKey", &value_ptr, &value_len) == 0);
        printf("%.*s\n", (int) value_len, (char*) value_ptr);
        assert(strncmp(value_ptr, "hello world!",strlen("hello world!")) == 0); 
      }
      /* now resize */
      assert(kvstore_resize_value(store, pool, "myResizeKey", 5, 0) == 0);

      {
        void * value_ptr = NULL;
        size_t value_len = 0;
        assert(kvstore_get(store, pool, "myResizeKey", &value_ptr, &value_len) == 0);
        printf("%.*s\n", (int) value_len, (char*) value_ptr);
        assert(strncmp(value_ptr, "hello", 5) == 0);
      }
    }

    /* erase pair */
    {
      void * value_ptr = NULL;
      size_t value_len = 0;

      assert(kvstore_erase(store, pool, "madeUpKey") == E_KEY_NOT_FOUND);
      assert(kvstore_erase(store, pool, "myResizeKey") == S_OK);
      assert(kvstore_get(store, pool, "myResizeKey", &value_ptr, &value_len) == E_KEY_NOT_FOUND);
    }

    /* perform locking (on existing) */
    {
      void * p;
      size_t p_len;
      lock_token_t tok;
      assert(kvstore_lock_existing(store, pool, "key-2", KVSTORE_LOCK_WRITE, &p, &p_len, &tok) == 0);
      printf("Locked:key-2 (%s)\n", (char*) p);
      assert(kvstore_unlock(store, pool, tok) == 0);
    }

    /* lock with implicit create */
    {
      lock_token_t tok = NULL;
      void * p;
      size_t p_len = 1024;
      size_t alignment = 8;

      assert(kvstore_lock_existing(store, pool, "key-new", KVSTORE_LOCK_WRITE, &p, &p_len, &tok) != 0);
      assert(kvstore_lock_existing_or_create(store, pool, "key-new", KVSTORE_LOCK_WRITE, alignment, p_len,
                                             &p, &p_len, &tok) == S_OK_CREATED);
      printf("Locked:key-new (%p)(%ld) token=%p\n", (char*) p, p_len, tok);
      assert(tok != NULL);
      assert(kvstore_unlock(store, pool, tok) == 0);
    }

    /* allocate and deallocate un-named memory */
    {
      void * p = NULL;
      assert(kvstore_allocate_pool_memory(store, pool, 1024, 0xFF, &p) == 0);
      assert(p);
      assert((((addr_t)p) & 0xFF) == 0); // check alignment
      memset(p, 0, 1024);
      assert(kvstore_free_pool_memory(store, pool, p, 1024) == 0);
    }
      

    /* clean up */
    assert(kvstore_close_pool(store, pool) == 0);
    assert(kvstore_close_pool(store, pool2) == 0);
    assert(kvstore_delete_pool(store, "myPool") == 0);
    assert(kvstore_delete_pool(store, "badPoolName") == -53);
    assert(kvstore_close(store) == 0);
  }
  
  printf("OK!!!\n");
  return 0;
}
