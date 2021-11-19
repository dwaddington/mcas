#include <unistd.h>
#include <stdlib.h>

#define RAPIDJSON_HAS_STDSTRING 1
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>

#include <common/logging.h>
#include <common/errors.h>
#include <common/utils.h>
#include <api/components.h>
#include <api/mcas_itf.h>

//#include "kvstore_api_wrapper.h"

/* don't want to include the header */
typedef int       status_t;
typedef void *    kvstore_t; 
typedef uint32_t  pool_flags_t;
typedef uint64_t  pool_t;
typedef uint64_t  addr_t;

using namespace component;
using namespace rapidjson;

namespace globals
{
}

extern "C" status_t kvstore_create(const unsigned debug_level,
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

  if(type == "mapstore") {
    std::string libname = "libcomponent-";
    libname += type;
    libname += ".so";
    IBase * comp = load_component(libname, mapstore_factory);
    if(!comp) return E_FAIL;
    auto fact = make_itf_ref(static_cast<IKVStore_factory *>(comp->query_interface(IKVStore_factory::iid())));

    auto kvstore = fact->create(debug_level, {});
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
                                        const pool_flags_t flags,
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
                                           const pool_flags_t flags,
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
                                         const pool_flags_t flags,
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
                                      const pool_flags_t flags,
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

  


//extern "C" status_t mcas_open_session_ex(const char * server_addr,
//                                          const char * net_device,
//                                          unsigned debug_level,
//                                          unsigned patience,
//                                          mcas_session_t * out_session)
// {
//   if(out_session == nullptr)
//     return E_INVAL;
  
//   auto comp = load_component("libcomponent-mcasclient.so", mcas_client_factory);
//   auto factory = static_cast<IMCAS_factory *>(comp->query_interface(IMCAS_factory::iid()));
//   assert(factory);

//   PLOG("%s : server_addr(%s), net_device(%s), debug_level(%u), patience(%u)",
//        __func__, server_addr, net_device, debug_level, patience);

//   try {
//     /* create instance of MCAS client session */
//     mcas_session_t mcas = factory->mcas_create(debug_level /* debug level, 0=off */,
//                                                patience,
//                                                getlogin(),
//                                                server_addr, /* MCAS server endpoint, e.g. 10.0.0.101::11911 */
//                                                net_device); /* e.g., mlx5_0, eth0 */

    
//     factory->release_ref();
//     assert(mcas);
//     *out_session = mcas;
//   }
//   catch(...) {
//     return E_FAIL;
//   }
  
//   return S_OK;
// }


// extern "C" status_t mcas_close_session(const mcas_session_t session)
// {
//   auto mcas = static_cast<IMCAS*>(session);
//   if(!session) return E_INVAL;
  
//   mcas->release_ref();
  
//   return S_OK;
// }

// extern "C" status_t mcas_allocate_direct_memory(const mcas_session_t session,
//                                                 const size_t size,
//                                                 void ** out_ptr,
//                                                 void ** out_handle)
// {
//   auto mcas = static_cast<IMCAS*>(session);
//   if(!session) return E_INVAL;
//   if(out_ptr == nullptr || out_handle == nullptr) return E_INVAL;

//   void * ptr = nullptr;
//   int rc = ::posix_memalign(&ptr, 64, size);

//   //  component::IKVStore::memory_handle_t;
//   auto handle = mcas->register_direct_memory(ptr, size);
//   if(rc || handle == component::IMCAS::MEMORY_HANDLE_NONE)
//     return E_FAIL;

//   *out_ptr = ptr;
//   *out_handle = handle;
//   return S_OK;
// }

// extern "C" void mcas_free_direct_memory(void * ptr)
// {
//   ::free(ptr);
// }


// extern "C" status_t mcas_create_pool_ex(const mcas_session_t session,
//                                         const char * pool_name,
//                                         const size_t size,
//                                         const unsigned int flags,
//                                         const uint64_t expected_obj_count,
//                                         const addr_t base_addr,
//                                         mcas_pool_t * out_pool_handle)                                           
// {
//   try {
//     auto mcas = static_cast<IMCAS*>(session);
//     auto pool = mcas->create_pool(std::string(pool_name),
//                                   size,
//                                   flags,
//                                   expected_obj_count,
//                                   IKVStore::Addr(base_addr));
//     if(pool == IMCAS::POOL_ERROR) return E_FAIL;
//     *out_pool_handle = {mcas, pool};
//   }
//   catch(...) {
//     return E_FAIL;
//   }
//   return S_OK;
// }

// extern "C" status_t mcas_create_pool(const mcas_session_t session,
//                                      const char * pool_name,
//                                      const size_t size,
//                                      const mcas_flags_t flags,
//                                      mcas_pool_t * out_pool_handle)
// {
//   return mcas_create_pool_ex(session, pool_name, size, flags, 1000, 0, out_pool_handle);
// }


// extern "C" status_t mcas_open_pool_ex(const mcas_session_t session,
//                                       const char * pool_name,
//                                       const mcas_flags_t flags,
//                                       const addr_t base_addr,
//                                       mcas_pool_t * out_pool_handle)
// {
//   auto mcas = static_cast<IMCAS*>(session);
//   auto pool = mcas->open_pool(std::string(pool_name),
//                               flags,
//                               IKVStore::Addr(base_addr));

//   if(pool == IMCAS::POOL_ERROR) return E_INVAL;
//   *out_pool_handle = {mcas, pool};  
//   return S_OK;
// }

// extern "C" status_t mcas_open_pool(const mcas_session_t session,
//                                    const char * pool_name,
//                                    const mcas_flags_t flags,
//                                    mcas_pool_t * out_pool_handle)
// {
//   return mcas_open_pool_ex(session, pool_name, flags, 0, out_pool_handle);
// }


// extern "C" status_t mcas_close_pool(const mcas_pool_t pool)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   return mcas->close_pool(pool.handle) == S_OK ? 0 : E_FAIL;
// }

// extern "C" status_t mcas_delete_pool(const mcas_session_t session,
//                                      const char * pool_name)
// {
//   auto mcas = static_cast<IMCAS*>(session);
//   return mcas->delete_pool(pool_name);
// }

// extern "C" status_t mcas_close_delete_pool(const mcas_pool_t pool)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->delete_pool(poolh);
// }

// extern "C" status_t mcas_configure_pool(const mcas_pool_t pool,
//                                         const char * setting)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->configure_pool(poolh, std::string(setting));
// }

// extern "C" status_t mcas_put_ex(const mcas_pool_t pool,
//                                 const char * key,
//                                 const void * value,
//                                 const size_t value_len,
//                                 const unsigned int flags)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->put(poolh, std::string(key), value, value_len, flags);
// }

// extern "C" status_t mcas_put(const mcas_pool_t pool,
//                              const char * key,
//                              const char * value,
//                              const unsigned int flags)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->put(poolh, std::string(key), value, strlen(value), flags);

// }

// extern "C" status_t mcas_register_direct_memory(const mcas_session_t session,
//                                                 const void * addr,
//                                                 const size_t len,
//                                                 mcas_memory_handle_t* out_handle)
// {
//   auto mcas = static_cast<IMCAS*>(session);
//   auto handle = mcas->register_direct_memory(const_cast<void*>(addr), len);
//   if(handle == 0) return E_FAIL;
//   *out_handle = handle;
//   return 0;
// }

// extern "C" status_t mcas_unregister_direct_memory(const mcas_session_t session,
//                                                   const mcas_memory_handle_t handle)
// {
//   auto mcas = static_cast<IMCAS*>(session);
//   return mcas->unregister_direct_memory(static_cast<IMCAS::memory_handle_t>(handle));
// }

// extern "C" status_t mcas_put_direct_ex(const mcas_pool_t pool,
//                                        const char * key,
//                                        const void * value,
//                                        const size_t value_len,
//                                        const mcas_memory_handle_t handle,
//                                        const unsigned int flags)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->put_direct(poolh, key, value, value_len,
//                           static_cast<IMCAS::memory_handle_t>(handle),
//                           flags);
// }

// extern "C" status_t mcas_put_direct(const mcas_pool_t pool,
//                                     const char * key,
//                                     const void * value,
//                                     const size_t value_len)
// {
//   return mcas_put_direct_ex(pool, key, value, value_len, IMCAS::MEMORY_HANDLE_NONE, IMCAS::FLAGS_NONE);
// }



// extern "C" status_t mcas_async_put_ex(const mcas_pool_t pool,
//                                       const char * key,
//                                       const void * value,
//                                       const size_t value_len,
//                                       const unsigned int flags,
//                                       mcas_async_handle_t * out_async_handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   IMCAS::async_handle_t handle;
//   auto result = mcas->async_put(poolh, key, value, value_len, handle, flags);
//   if(result == S_OK)
//     *out_async_handle = {static_cast<void*>(handle), nullptr};
//   return result;
// }

// extern "C" status_t mcas_async_put(const mcas_pool_t pool,
//                                    const char * key,
//                                    const char * value,
//                                    const unsigned int flags,
//                                    mcas_async_handle_t * out_async_handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   IMCAS::async_handle_t ahandle;
//   auto result = mcas->async_put(poolh, key, value, strlen(value), ahandle, flags);
//   if(result == S_OK)
//     *out_async_handle = {static_cast<void*>(ahandle), nullptr};
//   return result;
// }

// extern "C" status_t mcas_async_put_direct_ex(const mcas_pool_t pool,
//                                              const char * key,
//                                              const void * value,
//                                              const size_t value_len,
//                                              const mcas_memory_handle_t handle,
//                                              const unsigned int flags,
//                                              mcas_async_handle_t * out_async_handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   IMCAS::async_handle_t ahandle;
//   auto result = mcas->async_put_direct(poolh, key, value, value_len,
//                                        ahandle,
//                                        static_cast<IMCAS::memory_handle_t>(handle),
//                                        flags);

//   if(result == S_OK)
//     *out_async_handle = {static_cast<void*>(ahandle), nullptr};
//   return result;
// }

// extern "C" status_t mcas_free_memory(const mcas_session_t session,
//                                      void * p)
// {
//   auto mcas = static_cast<IMCAS*>(session);
//   return mcas->free_memory(p);
// }
    


// extern "C" status_t mcas_get(const mcas_pool_t pool,
//                              const char * key,
//                              void** out_value,
//                              size_t* out_value_len)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->get(poolh, key, *out_value, *out_value_len);
// }

// extern "C" status_t mcas_get_direct_ex(const mcas_pool_t pool,
//                                        const char * key,
//                                        void * value,
//                                        size_t * inout_size_value,
//                                        mcas_memory_handle_t handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->get_direct(poolh, key, value, *inout_size_value,
//                           static_cast<IMCAS::memory_handle_t>(handle));
// }

// extern "C" status_t mcas_get_direct(const mcas_pool_t pool,
//                                     const char * key,
//                                     void * value,
//                                     size_t * inout_size_value)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->get_direct(poolh, key, value, *inout_size_value, IMCAS::MEMORY_HANDLE_NONE);
// }


// extern "C" status_t mcas_async_get_direct_ex(const mcas_pool_t pool,
//                                              const char * key,
//                                              void * out_value,
//                                              size_t * inout_size_value,
//                                              mcas_memory_handle_t handle,
//                                              mcas_async_handle_t * out_async_handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);

//   IMCAS::async_handle_t ahandle;
//   auto result = mcas->async_get_direct(poolh, key, out_value, *inout_size_value,
//                                        ahandle, static_cast<IMCAS::memory_handle_t>(handle));
//   if(result == S_OK)
//     *out_async_handle = {ahandle, nullptr};
//   return result;
// }


// extern "C" status_t mcas_async_get_direct(const mcas_pool_t pool,
//                                           const char * key,
//                                           void * out_value,
//                                           size_t * inout_size_value,
//                                           mcas_async_handle_t * out_async_handle)
// {
//   return mcas_async_get_direct_ex(pool, key, out_value, inout_size_value,
//                                   IMCAS::MEMORY_HANDLE_NONE, out_async_handle);
// }


// extern "C" status_t mcas_get_direct_offset_ex(const mcas_pool_t pool,
//                                               const offset_t offset,
//                                               void * out_buffer,
//                                               size_t * inout_size,
//                                               mcas_memory_handle_t handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->get_direct_offset(poolh, offset, *inout_size, out_buffer,
//                                  static_cast<IMCAS::memory_handle_t>(handle));
// }

// extern "C" status_t mcas_get_direct_offset(const mcas_pool_t pool,
//                                            const offset_t offset,
//                                            void * out_buffer,
//                                            size_t * inout_size)
// {
//   return mcas_get_direct_offset_ex(pool, offset,out_buffer, inout_size, IMCAS::MEMORY_HANDLE_NONE);
// }


// extern "C" status_t mcas_async_get_direct_offset_ex(const mcas_pool_t pool,
//                                                     const offset_t offset,
//                                                     void * out_buffer,
//                                                     size_t * inout_size,
//                                                     mcas_memory_handle_t handle,
//                                                     mcas_async_handle_t * out_async_handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   IMCAS::async_handle_t ahandle;
//   auto result = mcas->async_get_direct_offset(poolh, offset, *inout_size, out_buffer,
//                                               ahandle,
//                                               static_cast<IMCAS::memory_handle_t>(handle));
  
//   if(result == S_OK)
//     *out_async_handle = {ahandle, nullptr};
  
//   return result;
// }

// extern "C" status_t mcas_async_get_direct_offset(const mcas_pool_t pool,
//                                                  const offset_t offset,
//                                                  void * out_buffer,
//                                                  size_t * inout_size,
//                                                  mcas_async_handle_t * out_async_handle)
// {
//   return mcas_async_get_direct_offset_ex(pool, offset,
//                                          out_buffer, inout_size,
//                                          IMCAS::MEMORY_HANDLE_NONE,
//                                          out_async_handle);
// }

// extern "C" status_t mcas_put_direct_offset_ex(const mcas_pool_t pool,
//                                               const offset_t offset,
//                                               const void *const buffer,
//                                               size_t * inout_size,
//                                               mcas_memory_handle_t handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);  
  
//   return mcas->put_direct_offset(poolh, offset, *inout_size, buffer,
//                                  static_cast<IMCAS::memory_handle_t>(handle));
// }

// extern "C" status_t mcas_put_direct_offset(const mcas_pool_t pool,
//                                            const offset_t offset,
//                                            const void *const buffer,
//                                            size_t * inout_size)
// {
//   return mcas_put_direct_offset_ex(pool, offset, buffer, inout_size,
//                                    IMCAS::MEMORY_HANDLE_NONE);
// }


// extern "C" status_t mcas_async_put_direct_offset_ex(const mcas_pool_t pool,
//                                                     const offset_t offset,
//                                                     const void *const buffer,
//                                                     size_t * inout_size,
//                                                     mcas_memory_handle_t handle,
//                                                     mcas_async_handle_t * out_async_handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   IMCAS::async_handle_t ahandle;
  
//   auto result = mcas->async_put_direct_offset(poolh, offset, *inout_size, buffer,
//                                               ahandle,
//                                               static_cast<IMCAS::memory_handle_t>(handle));
//   if(result == S_OK)
//     *out_async_handle = {ahandle, nullptr};
  
//   return result;
// }

// extern "C" status_t mcas_async_put_direct_offset(const mcas_pool_t pool,
//                                                  const offset_t offset,
//                                                  const void *const buffer,
//                                                  size_t * inout_size,
//                                                  mcas_async_handle_t * out_async_handle)
// {
//   return mcas_async_put_direct_offset_ex(pool, offset, buffer, inout_size,
//                                          IMCAS::MEMORY_HANDLE_NONE, out_async_handle);
// }

// extern "C" status_t mcas_check_async_completion(const mcas_session_t session,
//                                                 mcas_async_handle_t handle)
// {
//   auto mcas = static_cast<IMCAS*>(session);
  
//   return mcas->check_async_completion(reinterpret_cast<IMCAS::async_handle_t&>(handle.internal));
// }

// extern "C" status_t mcas_find(const mcas_pool_t pool,
//                               const char * key_expression,
//                               const offset_t offset,
//                               offset_t* out_matched_offset,
//                               char** out_matched_key)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   std::string result_key;
//   offset_t result_offset = 0;

//   auto result = mcas->find(poolh, key_expression, offset, result_offset, result_key);

//   if(result == S_MORE || result == S_OK) {
//     *out_matched_offset = result_offset;
//     *out_matched_key = static_cast<char*>(::malloc(result_key.size()));
//     memcpy(*out_matched_key, result_key.c_str(), result_key.size());
//     return 0;
//   }    
//   return E_FAIL;
// }

// extern "C" status_t mcas_erase(const mcas_pool_t pool,
//                                const char * key)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->erase(poolh, key);
// }

// extern "C" status_t mcas_async_erase(const mcas_pool_t pool,
//                                      const char * key,
//                                      mcas_async_handle_t * handle)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   IMCAS::async_handle_t ahandle;
//   auto result = mcas->async_erase(poolh, key, ahandle);
//   if(result == S_OK)
//     *handle = {ahandle, nullptr};
//   return result;
// }
  
// extern "C" size_t mcas_count(const mcas_pool_t pool)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   return mcas->count(poolh);
// }

// extern "C" status_t mcas_get_attribute(const mcas_pool_t pool,
//                                        const char * key, 
//                                        mcas_attribute attr,
//                                        uint64_t** out_value,
//                                        size_t* out_value_count)
// {
//   assert(out_value);
//   assert(out_value_count);
  
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   std::vector<uint64_t> out_attrs;

//   status_t result;
//   if(key) {
//     std::string k(key);
//     result = mcas->get_attribute(poolh, static_cast<IMCAS::Attribute>(attr), out_attrs, &k);
//   }
//   else {
//     result = mcas->get_attribute(poolh, static_cast<IMCAS::Attribute>(attr), out_attrs, nullptr);
//   }

//   auto n_elems = out_attrs.size();

//   if(result == S_OK) {
//     *out_value_count = n_elems;
//     if(n_elems > 0) {
//       *out_value = static_cast<uint64_t*>(::malloc(n_elems * sizeof(uint64_t)));
//       unsigned i=0;
//       for(auto x: out_attrs) {
//         *out_value[i] = x;
//         i++;
//       }
//     }
//     else {
//       *out_value = nullptr;
//     }
//   }      

//   return result;
// }

// extern "C" void mcas_free_responses(mcas_response_array_t out_response_vector)
// {
//   assert(out_response_vector);

//   auto response_ptr = static_cast<layer_response_t*>(out_response_vector);
//   while(response_ptr->len > 0) {
//     ::free(response_ptr->ptr);
//     response_ptr++;
//   }
  
//   ::free(out_response_vector);
// }


// extern "C" status_t mcas_invoke_ado(const mcas_pool_t pool,
//                                     const char * key,
//                                     const void * request,
//                                     const size_t request_len,
//                                     const mcas_ado_flags_t flags,
//                                     const size_t value_size,
//                                     mcas_response_array_t * out_response_vector,
//                                     size_t * out_response_vector_count)
// {
//   using namespace std;
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);

//   std::vector<IMCAS::ADO_response> responses;
  
//   auto result = mcas->invoke_ado(poolh,
//                                  key,
//                                  request,
//                                  request_len,
//                                  flags,
//                                  responses,
//                                  value_size);

//   auto n_responses = responses.size();
//   if(result == S_OK) {
//     *out_response_vector_count = n_responses;
//     if(n_responses > 0) {
//       /* allocate array of pointers */
//       auto rv = static_cast<layer_response_t*>(::malloc((n_responses + 1) * sizeof(layer_response_t)));
//       *out_response_vector = rv;
//       unsigned i = 0;
//       for(auto& r : responses) {
//         auto len = r.data_len();
//         rv[i].len = len;
//         /* allocate memory individual response */
//         rv[i].ptr = ::malloc(len);
//         rv[i].layer_id = r.layer_id();
//         memcpy(rv[i].ptr, r.data(), len); /* may be we can eliminate this? */
//         i++;
//       }
//       rv[i].ptr = nullptr; /* mark end of array */
//       rv[i].len = 0; 
//     }
//     else {
//       *out_response_vector = NULL;
//     }
//   }
//   else {
//     *out_response_vector_count = 0;
//     *out_response_vector = nullptr;
//   }
//   return result;
// }


// extern "C" status_t mcas_async_invoke_ado(const mcas_pool_t pool,
//                                           const char * key,
//                                           const void * request,
//                                           const size_t request_len,
//                                           const mcas_ado_flags_t flags,
//                                           const size_t value_size,
//                                           mcas_async_handle_t * handle)
// {
//   using namespace std;
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);

//   using rv_t = std::vector<IMCAS::ADO_response>;
//   handle->response_data = new rv_t;

//   IMCAS::async_handle_t ahandle;
//   auto result = mcas->async_invoke_ado(poolh,
//                                        key,
//                                        request,
//                                        request_len,
//                                        flags,
//                                        *(reinterpret_cast<rv_t*>(handle->response_data)),
//                                        ahandle,
//                                        value_size);
//   handle->internal = static_cast<void*>(ahandle);

//   return result;
// }

// extern "C" status_t mcas_check_async_invoke_ado(const mcas_pool_t pool,
//                                                 mcas_async_handle_t handle,
//                                                 mcas_response_array_t * out_response_vector,
//                                                 size_t * out_response_vector_count)
// {
//   using namespace component;
//   auto mcas = static_cast<IMCAS*>(pool.session);

//   if(mcas->check_async_completion(reinterpret_cast<IMCAS::async_handle_t&>(handle.internal)) == S_OK) {
//     if(handle.response_data == nullptr)
//       throw General_exception("unexpected null handle data");
//     auto& responses = *(reinterpret_cast<std::vector<IMCAS::ADO_response>*>(handle.response_data));

//     auto n_responses = responses.size();
//     *out_response_vector_count = n_responses;
//     if(n_responses > 0) {
//       auto rv = static_cast<layer_response_t*>(::malloc((n_responses + 1) * sizeof(iovec)));
//       *out_response_vector = rv;
//       unsigned i = 0;
//       for(auto& r : responses) {
//         auto len = r.data_len();
//         rv[i].len = len;
//         rv[i].ptr = ::malloc(len);
//         rv[i].layer_id = r.layer_id();
//         memcpy(rv[i].ptr, r.data(), len); /* may be we can eliminate this? */
//         i++;
//       }
//       rv[i].ptr = nullptr; /* mark end of array */
//       rv[i].len = 0; 
//     }
//     else {
//       *out_response_vector = NULL;
//     }
//     return 0;
//   }

//   return E_FAIL;
// }


// extern "C" status_t mcas_invoke_put_ado(const mcas_pool_t pool,
//                                         const char * key,
//                                         const void * request,
//                                         const size_t request_len,
//                                         const void * value,
//                                         const size_t value_len,
//                                         const size_t root_len,
//                                         const mcas_ado_flags_t flags,
//                                         mcas_response_array_t * out_response_vector,
//                                         size_t * out_response_vector_count)
// {
//   using namespace std;
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);

//   std::vector<IMCAS::ADO_response> responses;

//   auto result = mcas->invoke_put_ado(poolh,
//                                      key,
//                                      request,
//                                      request_len,
//                                      value,
//                                      value_len,
//                                      root_len,
//                                      flags,
//                                      responses);

//   auto n_responses = responses.size();
//   if(result == S_OK) {
//     *out_response_vector_count = n_responses;
//     if(n_responses > 0) {
//       auto rv = static_cast<layer_response_t*>(::malloc((n_responses + 1) * sizeof(iovec)));
//       *out_response_vector = rv;
//       unsigned i = 0;
//       for(auto& r : responses) {
//         auto len = r.data_len();
//         rv[i].len = len;
//         rv[i].ptr = ::malloc(len);
//         rv[i].layer_id = r.layer_id();
//         memcpy(rv[i].ptr, r.data(), len); /* may be we can eliminate this? */
//         i++;
//       }
//       rv[i].ptr = nullptr; /* mark end of array */
//       rv[i].len = 0; 
//     }
//     else {
//       *out_response_vector = NULL;
//     }
//   }
//   else {
//     *out_response_vector_count = 0;
//     *out_response_vector = nullptr;
//   }
//   return result;
// }


// extern "C" status_t mcas_async_invoke_put_ado(const mcas_pool_t pool,
//                                               const char * key,
//                                               const void * request,
//                                               const size_t request_len,
//                                               const void * value,
//                                               const size_t value_len,
//                                               const size_t root_len,
//                                               const mcas_ado_flags_t flags,
//                                               mcas_async_handle_t * handle)
// {
//   using namespace std;
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);

//   using rv_t = std::vector<IMCAS::ADO_response>;
//   handle->response_data = new rv_t;

//   IMCAS::async_handle_t ahandle;
//   auto result = mcas->async_invoke_put_ado(poolh,
//                                            key,
//                                            request,
//                                            request_len,
//                                            value,
//                                            value_len,
//                                            root_len,
//                                            flags,
//                                            *(reinterpret_cast<rv_t*>(handle->response_data)),
//                                            ahandle);

//   handle->internal = static_cast<void*>(ahandle);

//   return result;
// }


// extern "C" status_t mcas_check_async_invoke_put_ado(const mcas_pool_t pool,
//                                                     mcas_async_handle_t handle,
//                                                     mcas_response_array_t * out_response_vector,
//                                                     size_t * out_response_vector_count)
// {
//   return mcas_check_async_invoke_ado(pool, handle, out_response_vector, out_response_vector_count);
// }

// extern "C" status_t mcas_free_response(mcas_response_array_t out_response_vector,
//                                        size_t out_response_vector_count)
// {
//   if(out_response_vector_count == 0 || out_response_vector == nullptr) return E_INVAL;

//   mcas_response_array_t response = out_response_vector;

//   // TODO fix:
//   for(unsigned i=0;i<out_response_vector_count;i++)
//     ::free(response[i].ptr);
  
//   return S_OK;
// }


// extern "C" status_t mcas_get_response(mcas_response_array_t response_vector,
//                                       size_t index,
//                                       void ** out_ptr,
//                                       size_t * out_len)
// {

//   if(out_ptr == nullptr || out_len == nullptr) return E_INVAL;
//   mcas_response_array_t r = response_vector;
    
//   for(size_t i=0;i<index;i++) {
//     if(r == nullptr) return E_INVAL; /* check we didn't go off end of array */
//     r++;
//   }
//   *out_ptr = r->ptr;
//   *out_len = r->len;

//   return S_OK;
// }


// extern "C" void mcas_debug(const mcas_pool_t pool,
//                            const unsigned cmd,
//                            const uint64_t arg)
// {
//   auto mcas = static_cast<IMCAS*>(pool.session);
//   auto poolh = static_cast<IMCAS::pool_t>(pool.handle);
//   mcas->debug(poolh, cmd, arg);
// }

