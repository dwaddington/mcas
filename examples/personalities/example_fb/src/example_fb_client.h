#ifndef __EXAMPLE_FB_CLIENT_H__
#define __EXAMPLE_FB_CLIENT_H__

#include <string>
#include <api/mcas_itf.h>

namespace example_fb
{
using pool_t = component::IKVStore::pool_t;

class Client {
public:
  Client(const unsigned debug_level,
         unsigned patience,
         const std::string& addr_with_port,
         const std::string& nic_device);

  pool_t create_pool(const std::string& pool_name,
                     const size_t size,
                     const size_t expected_obj_count = 1000);

  pool_t open_pool(const std::string& pool_name,
                   bool read_only = false);

  status_t close_pool(const pool_t pool);

  status_t delete_pool(const std::string& pool_name);

  status_t put(const pool_t pool,
               const std::string& key,
               const std::string& value);

  status_t get(const pool_t pool,
               const std::string& key,
               const int version_index,
               std::string& out_value);

private:
  component::IMCAS * _mcas;
};

}

#endif // __EXAMPLE_FB_CLIENT_H__
