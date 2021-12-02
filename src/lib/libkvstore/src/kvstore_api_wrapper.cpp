#include <unistd.h>
#include <stdlib.h>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <common/logging.h>
#include <common/errors.h>
#include <common/utils.h>
#include <api/components.h>
#include <api/mcas_itf.h>

//#include "kvstore_api_wrapper.h"

/* don't want to include the header */
typedef int       status_t;
typedef void *    kvstore_t;
typedef void *    lock_token_t;
typedef void *    pool_iterator_t;
typedef uint32_t  kvstore_flags_t;
typedef uint64_t  pool_t;
typedef uint64_t  addr_t;

using namespace component;
using namespace rapidjson;

struct pool_reference_t {
  const void*     key;
  size_t          key_len;
  const void*     value;
  size_t          value_len;
  struct timespec timestamp; /* zero if not supported */
  int             time_match;
};

static unsigned log_level = 3;

extern "C" status_t kvstore_open(const unsigned debug_level,
                                 const char * json_config,
                                 kvstore_t * out_handle)
{
  using namespace component;
  
  if(json_config == nullptr) return E_INVAL;

  Document document;
  document.Parse(json_config);

  if(!document.HasMember("store_type")) {
    PWRN("invalid JSON config file: no store_type field");
    return E_INVAL;
  }
  
  if(!document["store_type"].IsString()) {
    PWRN("invalid JSON config file: store_type is not string");
    return E_INVAL;
  }
  std::string type = document["store_type"].GetString();

  IKVStore_factory::map_create params;
  if(document.HasMember("mm_plugin_path")) {
    if(log_level > 1)
      PLOG("mm_plugin_path:%s", document["mm_plugin_path"].GetString());
    params["mm_plugin_path"] = document["mm_plugin_path"].GetString();
  }

  if(document.HasMember("dax_config")) {
    assert(document["dax_config"].IsArray());

    std::stringstream ss;
    ss << "[";

    bool first=true;
    for (auto& v : document["dax_config"].GetArray()) {
      if(first)
        first=false;
      else
        ss << ",";
      
      PLOG("%s",v["addr"].GetString());
      PLOG("%s",v["path"].GetString());
      ss << "{\"path\":\"" << v["path"].GetString() << "\",\"addr\":" << std::stol(v["addr"].GetString(),nullptr,16) << "}";
    }
    ss << "]";

    /* unparse back to string */
    // StringBuffer buffer;
    // Writer<StringBuffer> writer(buffer);
    // document["dax_config"].Accept(writer);
    params["dax_config"] = ss.str();
    
    if(log_level > 1)
      PLOG("dax_config:%s", ss.str().c_str());
  }


  if(type == "mapstore") {
    std::string libname = "libcomponent-";
    libname += type;
    libname += ".so";
    IBase * comp = load_component(libname, mapstore_factory);
    if(!comp) return E_FAIL;
    auto fact = make_itf_ref(static_cast<IKVStore_factory *>(comp->query_interface(IKVStore_factory::iid())));
    auto kvstore = fact->create(debug_level, params);
    *const_cast<kvstore_t*>(out_handle) = kvstore;
  }
  else if(type == "hstore" || type == "hstore-cc" || type == "hstore-mm") {
    std::string libname = "libcomponent-" + type + ".so";
    IBase * comp = load_component(libname, hstore_factory);
    if(!comp) return E_FAIL;
    auto fact = make_itf_ref(static_cast<IKVStore_factory *>(comp->query_interface(IKVStore_factory::iid())));
    auto kvstore = fact->create(debug_level, params);
    *const_cast<kvstore_t*>(out_handle) = kvstore;
  }
  else {
    PWRN("invalid store type (%s)", type.c_str());
    return E_INVAL;
  }

  return S_OK;
}


extern "C" status_t kvstore_close(kvstore_t handle)
{
  IKVStore * store = reinterpret_cast<IKVStore*>(handle);
  store->release_ref();
  return S_OK;
}


extern "C" status_t kvstore_create_pool(const kvstore_t store_handle,
                                        const char * pool_name,
                                        const size_t size,
                                        const kvstore_flags_t flags,
                                        pool_t * out_pool_handle)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  auto pool = kvstore->create_pool(pool_name,
                                   size,
                                   flags);
  if( pool == IKVStore::POOL_ERROR)
    return E_FAIL;
  *out_pool_handle = pool;
  
  return S_OK;
}


extern "C" status_t kvstore_create_pool_ex(const kvstore_t store_handle,
                                           const char * pool_name,
                                           const size_t size,
                                           const kvstore_flags_t flags,
                                           const size_t expected_object_count,
                                           const addr_t base_addr,
                                           pool_t * out_pool_handle)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  auto pool = kvstore->create_pool(pool_name,
                                   size,
                                   flags,
                                   expected_object_count,
                                   IKVStore::Addr{base_addr});
  if( pool == IKVStore::POOL_ERROR)
    return E_FAIL;
  *out_pool_handle = pool;
  
  return S_OK;
}


extern "C" status_t kvstore_open_pool_ex(const kvstore_t store_handle,
                                         const char * pool_name,
                                         const kvstore_flags_t flags,
                                         const addr_t base_addr,
                                         pool_t * out_pool_handle)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  auto pool = kvstore->open_pool(pool_name,
                                 flags,
                                 IKVStore::Addr{base_addr});
  if( pool == IKVStore::POOL_ERROR)
    return E_FAIL;
  *out_pool_handle = pool;
  
  return S_OK;
}


extern "C" status_t kvstore_open_pool(const kvstore_t store_handle,
                                      const char * pool_name,
                                      const kvstore_flags_t flags,
                                      pool_t * out_pool_handle)
{
  return kvstore_open_pool_ex(store_handle,
                              pool_name,
                              flags,
                              0,
                              out_pool_handle);
}


extern "C" status_t kvstore_close_pool(const kvstore_t store_handle,
                                       const pool_t pool_handle)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->close_pool(pool_handle);
}


extern "C" status_t kvstore_delete_pool(const kvstore_t store_handle,
                                        const char * pool_name)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->delete_pool(pool_name);
}


extern "C" status_t kvstore_get_pool_names(const kvstore_t store_handle,
                                           char *** out_names,
                                           size_t * out_names_count)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  std::list<std::string> names;

  auto status = kvstore->get_pool_names(names);
  if(status != S_OK) return status;
  auto count = names.size();
  *out_names_count = count;
  *out_names = reinterpret_cast<char**>(malloc(sizeof(char *) * count));

  auto itr = names.begin();
  for(unsigned i = 0 ; i < count ; i++) {
    (*out_names)[i] = reinterpret_cast<char*>(malloc((*itr).size() + 1));
    strcpy((*out_names)[i], (*itr).c_str());
    itr++;
  }
  return S_OK;
}


extern "C" status_t kvstore_grow_pool(const kvstore_t store_handle,
                                      const pool_t pool_handle,
                                      const size_t increment_size,
                                      size_t * reconfigured_size)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->grow_pool(pool_handle, increment_size, *reconfigured_size);
}


extern "C" status_t kvstore_put(const kvstore_t store_handle,
                                const pool_t pool_handle,
                                const char * key,
                                const void * value,
                                const size_t value_len,
                                const kvstore_flags_t flags)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->put(pool_handle, key, value, value_len, flags);
}


extern "C" status_t kvstore_get(const kvstore_t store_handle,
                                const pool_t pool_handle,
                                const char * key,
                                void ** value,
                                size_t * value_len)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  void * p;
  size_t len_p;
  auto status = kvstore->get(pool_handle, key, p, len_p);
  if(status == S_OK) {
    *value = p;
    *value_len = len_p;
  }
  return status;
}


extern "C" status_t kvstore_swap_keys(const kvstore_t store_handle,
                                      const pool_t pool_handle,
                                      const char * key0,
                                      const char * key1)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->swap_keys(pool_handle, key0, key1);
}


extern "C" status_t kvstore_lock_existing(const kvstore_t store_handle,
                                          const pool_t pool_handle,
                                          const char * key,
                                          const int lock_type,
                                          void ** out_value_ptr,
                                          size_t * out_value_len,
                                          lock_token_t * out_lock_token)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);

  void * value_ptr = nullptr;
  size_t value_len = 0;

  IKVStore::key_t lock_handle;

  KVStore::lock_type_t lt = IKVStore::STORE_LOCK_NONE;
  if(lock_type == 1) lt = IKVStore::STORE_LOCK_READ;
  else if(lock_type == 2) lt = IKVStore::STORE_LOCK_WRITE;

  auto status = kvstore->lock(pool_handle,
                              key,
                              lt,
                              value_ptr,
                              value_len, // value len of 0 means don't implicitly create
                              0, // alignment not specified for existing
                              lock_handle);
  if(status == S_OK ) {
    *out_value_ptr = value_ptr;
    *out_value_len = value_len;
    *out_lock_token = reinterpret_cast<lock_token_t>(lock_handle);
  }
  return status;
}


extern "C" status_t kvstore_lock_existing_or_create(const kvstore_t store_handle,
                                                    const pool_t pool_handle,
                                                    const char * key,
                                                    const int lock_type,
                                                    const size_t alignment,
                                                    const size_t value_len,
                                                    void ** out_value_ptr,
                                                    size_t * out_value_len,
                                                    lock_token_t * out_lock_token)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);

  void * value_ptr = nullptr;
  size_t inout_value_len = value_len;

  IKVStore::key_t lock_handle;

  KVStore::lock_type_t lt = IKVStore::STORE_LOCK_NONE;
  if(lock_type == 1) lt = IKVStore::STORE_LOCK_READ;
  else if(lock_type == 2) lt = IKVStore::STORE_LOCK_WRITE;

  auto status = kvstore->lock(pool_handle,
                              key,
                              lt,
                              value_ptr,
                              inout_value_len,
                              alignment,
                              lock_handle);

  if(status == S_OK || status == S_OK_CREATED ) {
    *out_value_ptr = value_ptr;
    *out_value_len = inout_value_len;
    *out_lock_token = reinterpret_cast<lock_token_t>(lock_handle);
  }
  return status;
}

  
extern "C" status_t kvstore_unlock(const kvstore_t store_handle,
                                   const pool_t pool_handle,
                                   const lock_token_t lock_token,
                                   const int flush)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  auto lock_token_c = reinterpret_cast<KVStore::key_t>(lock_token);
  if(flush > 0)
    return kvstore->unlock(pool_handle, lock_token_c, KVStore::UNLOCK_FLAGS_FLUSH);
  else
    return kvstore->unlock(pool_handle, lock_token_c);
}

extern "C" status_t kvstore_resize_value(const kvstore_t store_handle,
                                         const pool_t pool_handle,
                                         const char * key,
                                         const size_t new_size,
                                         const size_t alignment)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->resize_value(pool_handle, key, new_size, alignment);
}

extern "C" status_t kvstore_erase(const kvstore_t store_handle,
                                  const pool_t pool,
                                  const char * key)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->erase(pool, key);
}

extern "C" size_t kvstore_count(const kvstore_t store_handle,
                                const pool_t pool)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->count(pool);
}

extern "C" status_t kvstore_allocate_pool_memory(const kvstore_t store_handle,
                                                 const pool_t pool,
                                                 const size_t size,
                                                 const size_t alignment,
                                                 void ** out_ptr)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  void * ptr = nullptr;
  auto s = kvstore->allocate_pool_memory(pool, size, alignment, ptr);
  if(s == S_OK) {
    *out_ptr = ptr;
  }
  return s;
}

extern "C" status_t kvstore_free_pool_memory(const kvstore_t store_handle,
                                             const pool_t pool,
                                             const void * ptr,
                                             const size_t size)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  return kvstore->free_pool_memory(pool, ptr, size);
}

extern "C" status_t kvstore_iterator_open(const kvstore_t store_handle,
                                          const pool_t pool,
                                          pool_iterator_t * out_iterator_handle)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  assert(out_iterator_handle);
  *out_iterator_handle = kvstore->open_pool_iterator(pool);
  return S_OK;
}

extern "C" status_t kvstore_iterator_next(const kvstore_t store_handle,
                                          const pool_t pool,
                                          pool_iterator_t iterator_handle,
                                          const struct timespec t_begin,
                                          const struct timespec t_end,
                                          struct pool_reference_t * out_reference)
{
  assert(out_reference);
  
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  IKVStore::pool_reference_t ref;
  bool time_match = 0;
  auto status = kvstore->deref_pool_iterator(pool,
                                             reinterpret_cast<IKVStore::pool_iterator_t>(iterator_handle),
                                             t_begin,
                                             t_end,
                                             ref,
                                             time_match,
                                             true);

  if(status == S_OK) {
    out_reference->key = ref.key;
    out_reference->key_len = ref.key_len;
    out_reference->value = ref.value;
    out_reference->value_len = ref.value_len;
    out_reference->timestamp = ref.timestamp;
    out_reference->time_match = time_match;
  }
  
  return status;
}

extern "C" status_t kvstore_iterator_close(const kvstore_t store_handle,
                                           const pool_t pool,
                                           pool_iterator_t iterator_handle)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);
  assert(iterator_handle);
  return kvstore->close_pool_iterator(pool,
                                      reinterpret_cast<IKVStore::pool_iterator_t>(iterator_handle));
}

extern "C" status_t kvstore_free_memory(const kvstore_t store_handle,
                                        void * ptr)
{
  auto kvstore = reinterpret_cast<IKVStore *>(store_handle);  
  return kvstore->free_memory(ptr);
}
