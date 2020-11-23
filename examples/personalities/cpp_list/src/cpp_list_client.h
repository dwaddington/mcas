#ifndef __PERSONALITY_CPP_LIST_CLIENT_H__
#define __PERSONALITY_CPP_LIST_CLIENT_H__

#include <string>
#include <common/logging.h>
#include <common/exceptions.h>
#include <api/components.h>
#include <api/mcas_itf.h>
#include <ccpm/immutable_list.h>
#include <cpp_list_proto_generated.h>
#include <nop/utility/stream_writer.h>
#include <flatbuffers/flatbuffers.h>

namespace cpp_list_personality
{
using namespace flatbuffers;
using namespace component;
using namespace structured_ADO_protocol;


// forward decls
status_t execute_invoke_noargs(component::IMCAS * i_mcas,
                               const component::IMCAS::pool_t pool,
                               const std::string& key_name,
                               const std::string& method_name);


/* Allocate memory at specific virtual address
 *
 * @param size Size of memory in bytes
 * @param hint Address to use
 *
 * 
 * @return : pointer to allocated memory
 **/
void * allocate_at(size_t size, const addr_t hint);

/* Free memory allocated with allocate_at
 * 
 * @param p Pointer to memory
 * @param length Size of allocation in bytes
 *
 * @return Code from munmap
 */
int free_at(void * p, size_t length);

/**
 * Durable_object_memory manages local and remote memory for a variable instance
 * 
 */
class Durable_object_memory
{
public:
  Durable_object_memory(component::IMCAS * mcas,
                        const component::IMCAS::pool_t pool,
                        std::string name,
                        size_t size)
    : _name(name), _mcas(mcas), _pool(pool), _size(size)
  {
    if(mcas == nullptr) throw std::invalid_argument("bad parameter");

    /* reserve space for data structure on MCAS server */
    std::vector<component::IMCAS::ADO_response> response;

    status_t rc = mcas->invoke_ado(pool,
                                   name,
                                   nullptr,
                                   0,
                                   component::IMCAS::ADO_FLAG_CREATE_ONLY,
                                   response,
                                   size);
    if(rc != S_OK)
      throw General_exception("invoke_ado failed %d",rc);

    assert(response.size() == 1);

    /* get virtual address from response */
    uint64_t value_vaddr = *(reinterpret_cast<const uint64_t*>(response[0].data()));
    PLOG("returned address: %lx", value_vaddr);

    /* allocate matching memory locally and populate if needed */
    void * ptr = allocate_at(size, value_vaddr);
    memset(ptr, 0, size);

    _regions.push_back({ptr,size});

    /* register for RDMA */
    _memory_handle = mcas->register_direct_memory(ptr, size);
    PLOG("registered direct memory (%p, %lu)", ptr, size);
    assert(_memory_handle != IMCAS::MEMORY_HANDLE_NONE);
  }
  
  virtual ~Durable_object_memory()
  {
  }

  ccpm::region_vector_t& regions() { return _regions; }

  inline component::IMCAS::pool_t pool() const { return _pool; }
  inline component::IMCAS * mcas() const { return _mcas; }
  inline const std::string& name() const { return _name; }
  
protected:
  const std::string                    _name;
  component::IMCAS *                   _mcas;
  const component::IMCAS::pool_t       _pool;
  size_t                               _size;
  ccpm::region_vector_t                _regions;
  component::IKVStore::memory_handle_t _memory_handle;
};


/**
 * Example durable immutable list type
 */
template<typename T>
class Durable_list : private Durable_object_memory,
                     public ccpm::Immutable_list<T>
{
public:
  Durable_list(component::IMCAS * mcas,
               const component::IMCAS::pool_t pool,
               const std::string name,
               const size_t size);

  status_t copy_to_remote();
  status_t copy_from_remote();
  
  status_t remote_push_front(const T& element);
  status_t remote_sort();  
};

template <typename T>
Durable_list<T>::Durable_list(component::IMCAS * mcas,
                              const component::IMCAS::pool_t pool,
                              const std::string name,
                              const size_t size)
  : Durable_object_memory(mcas, pool, name, size),
    ccpm::Immutable_list<T>(regions(), true /* force initialization */)
{
}

template <typename T>
status_t Durable_list<T>::copy_to_remote()
{
  /* copies complete memory - could be optimized to copy only used memory */
  return _mcas->put_direct(_pool,
                           _name,
                           _regions[0].iov_base,
                           _regions[0].iov_len,
                           _memory_handle);
}

template <typename T>
status_t Durable_list<T>::copy_from_remote()
{
  /* copies complete memory - could be optimized to copy only used memory */
  return _mcas->get_direct(_pool,
                           _name,
                           _regions[0].iov_base,
                           _regions[0].iov_len,
                           _memory_handle);
}


template <typename T>
status_t Durable_list<T>::remote_push_front(const T& element)
{
  using namespace flatbuffers;
  using Writer = nop::StreamWriter<std::stringstream>;
  
  nop::Serializer<Writer> serializer;
  serializer.Write(element);

  flatbuffers::FlatBufferBuilder fbb;
  auto params = fbb.CreateString(serializer.writer().take().str());
  auto method = fbb.CreateString("push_front");  
  auto cmd = CreateInvoke(fbb, method, params);
  auto msg = CreateMessage(fbb, Command_Invoke, cmd.Union());
  fbb.Finish(msg);

  /* invoke */
  status_t rc;
  std::vector<component::IMCAS::ADO_response> response;
  
  rc = _mcas->invoke_ado(_pool,
                         _name,
                         fbb.GetBufferPointer(),
                         fbb.GetSize(),
                         0,
                         response);
  return rc;
}


template <typename T>
void push_front(Durable_object_memory& obj,
                const T element)
{
  using Writer = nop::StreamWriter<std::stringstream>;
  nop::Serializer<Writer> serializer;
  serializer.Write(element);

  FlatBufferBuilder fbb;
  auto params = fbb.CreateString(serializer.writer().take().str());
  auto method = fbb.CreateString("push_front");  
  auto cmd = CreateInvoke(fbb, method, params);
  auto msg = CreateMessage(fbb, Command_Invoke, cmd.Union());
  fbb.Finish(msg);

  /* invoke */
  status_t rc;
  std::vector<component::IMCAS::ADO_response> response;
  
  assert(response.size() == 1);
  
  rc = obj.mcas()->invoke_ado(obj.pool(),
                              obj.name(),
                              fbb.GetBufferPointer(),
                              fbb.GetSize(),
                              0,
                              response);
  PLOG("execute_insert_list invoke response: %d (%s)", rc, response[0].data());
}

template <typename T>
status_t Durable_list<T>::remote_sort()
{
  return execute_invoke_noargs(_mcas, _pool, _name, "sort");
}



// template <class T>
// void create_list(Component::IMCAS * i_mcas,
//                  const Component::IMCAS::pool_t pool,
//                  const std::string& name,
//                  std::function<void(ccpm::Immutable_list<T>&)> lambda)
// {
//   using namespace Component;

//   status_t rc;
//   std::string response;
  
//   /* delete anything prior */
//   i_mcas->erase(pool, name);

//   size_t size = MB(1);
//   addr_t value_vaddr = 0;

//   assert(rc == S_OK);
//   /* get virtual address from response */
//   value_vaddr = *(reinterpret_cast<const uint64_t*>(response.data()));
//   PLOG("returned address: %lx", value_vaddr);

//   /* allocate matching memory locally and populate if needed */
//   void * ptr = allocate_at(size, value_vaddr);
//   memset(ptr, 0, size);

//   ccpm::region_vector_t regions{ptr, size};
//   ccpm::Immutable_list<T> myList(regions, true);

//   unsigned count = 1000;
//   for(unsigned i=0;i<count;i++)
//     myList.push_front((rdtsc() * i) % 10000); /* add something to list */
  
//   /* push data structure into mcas */
//   /* If small, use put */
//   //  rc = i_mcas->put(pool, name, regions[0].iov_base, regions[0].iov_len);
//   /* If large and we want to pay the cost of registering, we can use put_direct */
//   auto handle = i_mcas->register_direct_memory(regions[0].iov_base, regions[0].iov_len);
//   rc = i_mcas->put_direct(pool, name, regions[0].iov_base, regions[0].iov_len, handle);
//   assert(rc == S_OK);
  
//   PLOG("create invocation response: %d", rc);
// }

}

#endif // __PERSONALITY_CPP_LIST_CLIENT_H__
