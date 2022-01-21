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

#include "make_kvstore.h"
#include "pool_instance.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Weffc++"
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-compare"
#include <gtest/gtest.h>
#pragma GCC diagnostic pop

#include <common/json.h>
#include <common/str_utils.h>
#include <common/utils.h>
#include <api/kvstore_itf.h>
#include <boost/program_options.hpp>
#include <cstddef> /* size_t */
#include <iostream> /* cerr, cout */
#include <string>

#define ASSERT_OK(X) ASSERT_TRUE(S_OK == X)

static std::string store;
static custom_store *cs;
static component::IKVStore_factory::map_create mc;

namespace {

// The fixture for testing class Foo.
class KVStore_test
	: public ::testing::Test
{
	static bool numa_notified;
public:
	using IKVStore = component::IKVStore;
	using IKVStore_factory = component::IKVStore_factory;
	using string_view = common::string_view;
protected:
	static const std::string test1_pool;
	static const std::string iterator_test_pool;
	static const std::string timestamp_test_pool;

	// Objects declared here can be used by all tests in the test case
	std::shared_ptr<IKVStore> _kvstore;

	KVStore_test()
		: _kvstore(make_kvstore(store, cs, mc))
	{
		if ( ! mc.count(+IKVStore_factory::k_numa_nodes) == 0 && ! numa_notified )
		{
			std::cerr << "If using mmap memory. Run with --numa-nodes==<numa_nodes> to allocate from numa nodes\n";
			numa_notified = true;
		}

	}
	// If the constructor and destructor are not enough for setting up
	// and cleaning up each test, you can define the following methods:

	auto open_pool(string_view pool_name) -> IKVStore::pool_t
	{
		return _kvstore->open_pool(std::string(pool_name));
	}

	auto create_pool(string_view pool_name, std::size_t sz = MB(32))
	{
		return pool_instance(_kvstore, pool_name, sz);
	}
};

bool KVStore_test::numa_notified = false;
const std::string KVStore_test::test1_pool = "test1.pool";
const std::string KVStore_test::timestamp_test_pool = "timestamp-test.pool";
const std::string KVStore_test::iterator_test_pool = "iterator-test.pool";

TEST_F(KVStore_test, OpenPool)
{
	ASSERT_TRUE(_kvstore);
	auto pool = create_pool(test1_pool);
	ASSERT_TRUE(pool.handle() != IKVStore::POOL_ERROR);
}

TEST_F(KVStore_test, BasicPut)
{
	auto pool = create_pool(test1_pool);
	ASSERT_TRUE(pool.handle());
	pool.put2();
}

TEST_F(KVStore_test, BasicGet)
{
	auto pool = create_pool(test1_pool);
	pool.put2();
	std::string key = "MyKey";

	void * value = nullptr;
	size_t value_len = 0;
	pool.get(key, value, value_len);
	PINF("Value=(%.50s) %lu", static_cast<const char *>(value), value_len);

	ASSERT_TRUE(value);
	ASSERT_TRUE(value_len == KB(8));
	pool.free_memory(value);

	value = nullptr;
	value_len = 0;
	pool.get(key, value, value_len);
	PINF("Repeat Value=(%.50s) %lu", static_cast<const char *>(value), value_len);
	auto count = pool.count();
	PINF("Count = %ld", count);
	ASSERT_TRUE(count == 2);
	ASSERT_TRUE(value);
	ASSERT_TRUE(value_len == KB(8));
	pool.free_memory(value);
}

TEST_F(KVStore_test, BasicMap)
{
	auto pool = create_pool(test1_pool);
	pool.put2();
	pool.map(
		[](const void * key,
	                 const size_t key_len,
	                 const void * value,
	                 const size_t value_len) -> int
		{
			FINF("key:({}) value({})"
				, string_view(static_cast<const char *>(key), key_len)
				, string_view(static_cast<const char *>(value), value_len)
			);
			return 0;
		}
	);
}

TEST_F(KVStore_test, ValueResize)
{
	auto pool = create_pool(test1_pool);
	pool.put2();
	pool.map(
		[](
			const void * key,
			size_t key_len,
			const void * value,
			size_t value_len) -> int
		{
			FINF("key:({}) value({}-{})"
				, string_view(static_cast<const char *>(key), key_len)
				, string_view(static_cast<const char *>(value), value_len)
				, value_len
			);
			return 0;
		}
	);

	ASSERT_TRUE(pool.resize_value(
	                                   "MyKey",
	                                   KB(16),
	                                   8) == S_OK);

	pool.map(
		[](
			const void * key,
			size_t key_len,
			const void * value,
			size_t value_len) -> int
		{
			FINF("key:({}) value({}-{})"
				, string_view(static_cast<const char *>(key), key_len)
				, string_view(static_cast<const char *>(value), value_len)
				, value_len
			);
	                return 0;
		}
	);

}

TEST_F(KVStore_test, BasicRemove)
{
	auto pool = create_pool(test1_pool);
	pool.put2();
	pool.erase("MyKey");
}

TEST_F(KVStore_test, ClosePool)
{
	auto pool = create_pool(test1_pool);
	ASSERT_TRUE(pool.close() == S_OK);
}

TEST_F(KVStore_test, ReopenPool)
{
	auto pool = create_pool(test1_pool);
	ASSERT_TRUE(pool.handle() != IKVStore::POOL_ERROR);
	FLOG("re-opened pool: {}", reinterpret_cast<const void *>(pool.handle()));
}

TEST_F(KVStore_test, ReClosePool)
{
	auto pool = create_pool(test1_pool);
	pool.close();
}

TEST_F(KVStore_test, Timestamps)
{
	auto pool = create_pool(timestamp_test_pool, MB(32));
	/* if timestamping is enabled */
	if(_kvstore->get_capability(IKVStore::Capability::WRITE_TIMESTAMPS)) {

		auto now = common::epoch_now();

		for(unsigned i=0;i<10;i++) {
			auto value = common::random_string(16);
			auto key = common::random_string(8);
			FLOG("adding key-value pair ({})", key);
			pool.put(key, value.c_str(), value.size());
			sleep(2);
		}

		pool.map(
			[](const void* key,
	                         const size_t key_len,
	                         const void* value,
	                         const size_t value_len,
	                         const common::tsc_time_t timestamp) -> bool
			{
				(void)value; // unused
				(void)value_len; // unused
				FLOG("Timestamped record: {} @ {}"
					, string_view(static_cast<const char *>(key), key_len)
					, timestamp.raw()
				);
				return true;
			}
			, 0, 0
		);

		PLOG("After 5 seconds");
		pool.map(
			[](const void* key,
	                         size_t key_len,
	                         const void* value,
	                         size_t value_len,
	                         common::tsc_time_t timestamp) -> bool
			{
				(void)value; // unused
				(void)value_len; // unused
				FLOG("After 5 Timestamped record: {} @ {}"
					, string_view(static_cast<const char *>(key), key_len)
					, timestamp.raw()
				);
				return true;
			}
			, now.add_seconds(5)
			, common::epoch_time_t{0,0}
		);
	}

	PLOG("Closing pool.");
	ASSERT_TRUE(pool.handle() != IKVStore::POOL_ERROR);
	ASSERT_TRUE(pool.close() == S_OK);
}

TEST_F(KVStore_test, Iterator)
{
	ASSERT_TRUE(_kvstore);
	auto pool = create_pool(iterator_test_pool, MB(32));

	common::epoch_time_t now = 0;

	for(unsigned i=0;i<10;i++) {
	  auto value = common::random_string(16);
	  auto key = common::random_string(8);

	  if(i==5) { sleep(2); now = common::epoch_now(); }

	  FLOG("({}) adding key-value pair key({}) value({})", i, key, value);
	  pool.put(key, value.c_str(), value.size());
	}

	pool.map(
	              [](const void * key,
	                 const size_t key_len,
	                 const void * value,
	                 const size_t value_len) -> int
	              {
	                FINF("key:({} {}) value({})"
						, key
						, string_view(static_cast<const char *>(key), key_len)
						, string_view(static_cast<const char *>(value), value_len)
	                    , static_cast<const char *>(value));
	                return 0;
	              }
	              );

	PLOG("Iterating...");
	status_t rc;
	IKVStore::pool_reference_t ref;
	bool time_match;

	{
		auto iter = pool.open_iterator();
		while((rc = iter.deref(0, 0, ref, time_match, true)) == S_OK) {
			FLOG("iterator: key({}) value({}) {}"
				, string_view(static_cast<const char *>(ref.key), ref.key_len)
				, string_view(static_cast<const char *>(ref.value), ref.value_len)
				, ref.timestamp.seconds()
			);
		}
	}
	ASSERT_TRUE(rc == E_OUT_OF_BOUNDS);

	{
		auto iter = pool.open_iterator();
		ASSERT_TRUE(now.seconds() > 0);
		while((rc = iter.deref(0, now, ref, time_match, true)) == S_OK) {
			FLOG("(time-constrained) iterator: key({}) value({}) {} (match={})"
				, string_view(static_cast<const char *>(ref.key), ref.key_len)
				, string_view(static_cast<const char *>(ref.value), ref.value_len)
				, ref.timestamp.seconds()
				, time_match ? "y":"n"
			);
		}

	}
	ASSERT_TRUE(rc == E_OUT_OF_BOUNDS);

	PLOG("Disturbed iteration...");
	unsigned i=0;
	{
		auto iter = pool.open_iterator();
			while((rc = iter.deref(0, 0, ref, time_match, true)) == S_OK) {
				FLOG("iterator: key({}) value({}) {}"
					, string_view(static_cast<const char *>(ref.key), ref.key_len)
					, string_view(static_cast<const char *>(ref.value), ref.value_len)
					, ref.timestamp.seconds()
				);
				i++;
				if(i == 5) {
				/* disturb iteration */
				auto value = common::random_string(16);
				auto key = common::random_string(8);
				FLOG("adding key-value pair key({}) value({})", key, value);
				pool.put(key, value.c_str(), value.size());
			}
		}
		ASSERT_TRUE(rc == E_ITERATOR_DISTURBED);
	}

	PLOG("Closing pool.");
	ASSERT_TRUE(pool.handle() != IKVStore::POOL_ERROR);
	ASSERT_TRUE(pool.close() == S_OK);
}


TEST_F(KVStore_test, KeySwap)
{
	ASSERT_TRUE(_kvstore);
	auto pool = create_pool("keyswap", MB(32));
	ASSERT_TRUE(pool.handle() != IKVStore::POOL_ERROR);

	std::string left_key = "LeftKey";
	std::string right_key = "RightKey";
	std::string left_value = "This is left";
	std::string right_value = "This is right";

	ASSERT_OK(pool.put(left_key, left_value.c_str(), left_value.length()));
	ASSERT_OK(pool.put(right_key, right_value.c_str(), right_value.length()));

	ASSERT_OK(pool.swap_keys(left_key, right_key));

	iovec new_left{}, new_right{};
	pool.get(left_key, new_left.iov_base, new_left.iov_len);
	pool.get(right_key, new_right.iov_base, new_right.iov_len);

	FLOG("left: {}", string_view(static_cast<const char *>(new_left.iov_base), new_left.iov_len));
	FLOG("right: {}", string_view(static_cast<const char *>(new_right.iov_base), new_right.iov_len));
	ASSERT_TRUE(strncmp(static_cast<const char *>(new_left.iov_base), right_value.c_str(), new_left.iov_len) == 0);
	ASSERT_TRUE(strncmp(static_cast<const char *>(new_right.iov_base), left_value.c_str(), new_right.iov_len) == 0);
	pool.free_memory(new_right.iov_base);
	pool.free_memory(new_left.iov_base);

	ASSERT_TRUE(pool.close() == S_OK);
}

TEST_F(KVStore_test, AlignedLock)
{
	ASSERT_TRUE(_kvstore);
	auto pool = create_pool("alignedlock", MB(32));
	ASSERT_TRUE(pool.handle() != IKVStore::POOL_ERROR);

	std::vector<size_t> alignments = {8,16,32,128,512,2048,4096};

	for(size_t alignment: alignments)
	{
	  void * addr = nullptr;
	  size_t value_len = 4096;
	  IKVStore::key_t handle, handle2;

	  ASSERT_TRUE(pool.lock("key", IKVStore::STORE_LOCK_WRITE, addr, value_len, alignment, handle) == S_OK_CREATED);
	  ASSERT_TRUE(check_aligned(addr, alignment));
	  ASSERT_TRUE(pool.lock("key", IKVStore::STORE_LOCK_WRITE, addr, value_len, alignment, handle2) == E_LOCKED);
	  ASSERT_OK(pool.unlock(handle));
	  ASSERT_OK(pool.erase("key"));
	}

	ASSERT_TRUE(pool.close() == S_OK);
}

#if 0
// mapstore-specific
TEST_F(KVStore_test, NumaMask)
{
	ASSERT_TRUE(_kvstore);
	auto pool = create_pool("numaMask", MB(1));
	ASSERT_TRUE(pool != IKVStore::POOL_ERROR);

	if ( numa_nodes && strlen(numa_nodes) )
	{
		std::vector<uint64_t> numa_mask_result;
		pool.get_attribute(IKVStore::NUMA_MASK, numa_mask_result);
		ASSERT_EQ(1, numa_mask_result.size());
		auto numa_mask = numa_mask_result[0];
		/* a trivial mask (1 node, less than 3 chars) should yield exactly one 1 bit */
		if ( strlen(numa_nodes) < 3 )
	      {
			ASSERT_NE(0, numa_mask);
			ASSERT_EQ(0, (numa_mask & (numa_mask-1)));
		}
		std::cout << "Numa node mask is " << std::hex << numa_mask << "\n";
	}
	pool.close();
}
#endif
} // namespace

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);

	namespace po = boost::program_options;

	po::options_description            desc("Options");
	po::positional_options_description g_pos; /* no positional options */

	namespace c_json = common::json;
	using json = c_json::serializer<c_json::dummy_writer>;

	desc.add_options()
		("help,h", "Show help")
		("numa-nodes", po::value<std::string>(), "Numa node specification (mapstore only)")
		("store", po::value<std::string>()->default_value("mapstore"), "Store type to test: e.g., hstore, hstore-cc, mapstore")
		("daxconfig"
			, po::value<std::string>()->default_value(
				json::array(
					json::object(
						json::member("path", "/dev/dax0.0")
						, json::member("addr", 0x9000000000)
					)
		        ).str()
			)
			, "dax configuration (hstore* only), in JSON. default [{\"path\": \"/dev/dax0.0\", \"addr\": \"0x9000000000\"}]"
		)
		;

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).options(desc).positional(g_pos).run(), vm);

	if (vm.count("help") > 0)
	{
	    std::cout << desc;
	  	return -1;
	}

	store = vm["store"].as<std::string>();
	using IKVStore_factory = component::IKVStore_factory;
	mc = IKVStore_factory::map_create
		{
			{+IKVStore_factory::k_name, "numa0"},
			{+IKVStore_factory::k_dax_config, vm["daxconfig"].as<std::string>()}
		};
	if ( vm.count("numa-nodes") )
	{
		mc.insert( {+IKVStore_factory::k_numa_nodes, vm["numa-nodes"].as<std::string>()} );
	}

	cs = locate_custom_store(store);
	auto r = RUN_ALL_TESTS();

	return r;
}
