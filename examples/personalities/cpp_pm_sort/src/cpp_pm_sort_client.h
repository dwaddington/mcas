/*
 * Description  : PM sorting ADO client
 * Authors      : Omri Arad, Yoav Ben Shimon, Ron Zadicario
 * Authors email: omriarad3@gmail.com, yoavbenshimon@gmail.com, ronzadi@gmail.com
 * License      : Apache License, Version 2.0
 * Done as part of the Workshop on Search Data Structures for High-Performance Databases
 * at Tel-Aviv University.
 */

#ifndef __EXAMPLE_FB_CLIENT_H__
#define __EXAMPLE_FB_CLIENT_H__

#include <string>
#include <api/mcas_itf.h>
#include "cpp_pm_sort_plugin.h"

namespace cpp_pm_sort
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

	status_t remove_chunk(const pool_t pool,
			      const std::string& key);

	status_t sort(const pool_t pool, unsigned type);

	status_t init(const pool_t pool);

	status_t verify(const pool_t pool);
private:
	component::IMCAS * _mcas;
};

}

#endif // __EXAMPLE_FB_CLIENT_H__
