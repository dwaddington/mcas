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
#include "store_map.h"

#include <gtest/gtest.h>
#include <api/components.h>
#include <ccpm/cca.h>
#include <ccpm/log.h>
#include <ccpm/interfaces.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Weffc++"
#include <EASTL/iterator.h>
#include <EASTL/list.h>
#include <EASTL/vector.h>
#pragma GCC diagnostic pop
/* note: we do not include component source, only the API definition */
#include <api/kvstore_itf.h>

#include <algorithm> // equal, reverse
#include <cstddef> // size_t
#include <cstdlib> // getenv
#include <iostream> // cerr
#include <string> // string

using namespace Component;

struct log_tracker
{
	ccpm::log *_log; // not owned

	inline void
		track_pre(
			const void *v
			, std::size_t s
			, char = '0'
		) const noexcept
	{
		/* tracker does not excpect *v to be modified, but rollback
		 * will do just that. Perhaps track_pre and track_post parameters
		 * should be void *, not const void *.
		 */
		if ( _log->includes(v) )
		{
			_log->add(const_cast<void *>(v), s);
		}
	}

	inline void
		track_post(
			const void *
			, std::size_t
			, char = '1'
		) const noexcept
	{
	}

	/* after a successful allocate, log the allocate */
	inline void track_allocate(void *p, std::size_t n) const noexcept
	{
		_log->allocated(p, n);
	}

	inline void track_free(void *p, std::size_t n) const noexcept
	{
		_log->freed(p, n);
	}
protected:
	explicit log_tracker(ccpm::log *log_)
		: _log(log_)
	{}
public:
	log_tracker() : _log(nullptr) {} // requied by ListNodeBase
	log_tracker(const log_tracker &) = default;
	log_tracker &operator=(const log_tracker &) = default;
	~log_tracker() = default;
};

class lt_allocator
	: public log_tracker
{
	ccpm::cca *_cca; // not owned
public:
	using tracker_type = log_tracker;
	explicit lt_allocator(ccpm::cca *cca_, ccpm::log *log_)
		: log_tracker(log_)
		, _cca(cca_)
	{}
	lt_allocator(const lt_allocator& x) = default;

	lt_allocator& operator=(const lt_allocator& x) = default;

	void* allocate(size_t n, int flags = 0)
	{
		return allocate(n, sizeof(void *), 0, flags);
	}
	void* allocate(std::size_t n, std::size_t alignment, std::size_t offset, int flags = 0)
	{
		(void)flags;
		(void)offset;
		void *p = nullptr;
		_cca->allocate(p, n, alignment);
		track_allocate(p, n);
		return p;
	}
	void deallocate(void *p, size_t n)
	{
		/* Do not free until log is cleared (i.e, committed) */
		void *pl = p;
		track_free(pl, n);
	}

	const char* get_name() const { return "bob"; }
	void        set_name(const char *) {}
};

bool operator==(const lt_allocator& a, const lt_allocator& b); // unused
bool operator!=(const lt_allocator& a, const lt_allocator& b); // unused

template<typename T, typename Tracker>
	class tracked_value
	{
	public:
		using value_type = T;
		using tracker_type = Tracker;
		value_type _v;
		tracker_type *_t; // not owned

		tracked_value(int v_, tracker_type *t_)
			: _v(v_)
			, _t(t_)
		{
			_t->track_post(this, sizeof *this);
		}
		tracked_value(int v_ = int())
			: _v(v_)
			, _t(nullptr)
		{}
		tracked_value(const tracked_value &) = default;
		tracked_value &operator=(const tracked_value &o_)
		{
			if ( ! _t )
			{
				_t = o_._t;
			}

			_t->track_pre(this, sizeof *this);
			_v = o_._v;
			_t->track_post(this, sizeof *this);
			return *this;
		}
		~tracked_value()
		{
			if ( _t )
			{
				_t->track_pre(this, sizeof *this);
			}
		}

		/* an implicit output conversion from wrapper to wrapped type is
		 * not generally a good idea, but in this case it avoids complexity
		 * when using algorithms such as std::equal
		 */
		operator value_type() const { return _v; }
	};

/* Stuff needed for a crash-consistent container */
template <typename Container>
	struct cc_container
	{
		::iovec initial_region;
		/* a memory resource (in C++17 parlance), for more memory */
		ccpm::cca mr;
		/* a log, which uses the memory resource, for rollback */
		ccpm::log log;
		/* an allocator + tracker, to translate memory resource callbacks to the log */
		lt_allocator allocator;
		/* A container, which uses the memory resource and the log */
public:
		/* The container could be included here directly.
		 * But to simplify the log "includes" filter, which determines which
		 * changes to record and roll back, the container space is instead allocated
		 * in the same memory region as its elements.
		 */
		Container *container;
public:
		cc_container(void *ptr, std::size_t size)
			: initial_region{ptr, size}
			/* Note: ccpm::region_vector_t has a std::vector as a public base class.
			 * Best to avoid it.
			 */
			, mr(ccpm::region_vector_t(initial_region.iov_base, initial_region.iov_len))
			, log(&mr)
			, allocator(&mr, &log)
			, container(new (allocator.allocate(sizeof *container)) Container(allocator))
		{
		}
	};

using logged_int = tracked_value<int, log_tracker>;

// The fixture for testing class Foo.
class Log_test : public ::testing::Test
{
protected:

	Component::IKVStore *instantiate()
	{
		/* create object instance through factory */
		auto link_library = "libcomponent-" + store_map::impl->name + ".so";
		auto comp =
			Component::load_component(
				link_library,
				store_map::impl->factory_id
			);

		if ( comp )
		{
			auto fact = static_cast<IKVStore_factory *>(comp->query_interface(IKVStore_factory::iid()));
			/* numa node 0 */
			auto kvstore = fact->create("owner", "numa0", store_map::location);

			fact->release_ref();
			return kvstore;
		}
		else
		{
			return nullptr;
		}
	}

	Component::IKVStore::pool_t create_pool(
		Component::IKVStore *kvstore
		, const std::string &name
		, std::size_t size
	)
	{
		/* remove any old pool */
		try
		{
			kvstore->delete_pool(name);
		}
		catch ( const Exception & ) {}

		auto pool = kvstore->create_pool(name, size, 0, 0);
		if ( 0 == int64_t(pool) )
		{
			std::cerr
				<< "Pool not created, USE_DRAM was "
				<< ( std::getenv("USE_DRAM") ? std::getenv("USE_DRAM") : " not set" )
				<< "\n";
		}
		return pool;
	}

	void close_pool(
		Component::IKVStore *kvstore
		, Component::IKVStore::pool_t pool
	)
	{
		if ( pmem_effective )
		{
				kvstore->close_pool(pool);
		}
	}

	/* persistent memory if enabled at all, is simulated and not real */
	static bool pmem_simulated;
	/* persistent memory is effective (either real, indicated by no PMEM_IS_PMEM_FORCE or simulated by PMEM_IS_PMEM_FORCE 0 not 1 */
	static bool pmem_effective;

	static std::string pool_name()
	{
		return "/mnt/pmem0/pool/0/test-" + store_map::impl->name + store_map::numa_zone() + ".pool";
	}

};

bool Log_test::pmem_simulated = getenv("PMEM_IS_PMEM_FORCE");
bool Log_test::pmem_effective = ! getenv("PMEM_IS_PMEM_FORCE") || getenv("PMEM_IS_PMEM_FORCE") == std::string("0");

/* broken, possibly because
 * (1) tracker does not record allocations and deallocations. Even worse, deallocations may be reused by the logger.
 *     Consider avoiding deallocations during a transaction. What other side effects can a transaction have?
 * (2) stack items are among those tracked, and they should not be. The logger should know the bounds of the data
 *     persistent data areas, and log only write to those areas.
 */
TEST_F(Log_test, CCVector)
{
/* operations to test
 *   emplace()
 *   insert() - 5 versions
 *   erase()
 *   clear()
 *   assign() - 3 versions
 */
	auto kvstore = std::unique_ptr<Component::IKVStore>(instantiate());
	ASSERT_TRUE(kvstore.get());
	auto pool = create_pool(kvstore.get(), pool_name(), MB(10));
	ASSERT_LT(0, int64_t(pool));

	void *heap_area;
	std::size_t heap_size = MB(5);
	Component::IKVStore::key_t heap_lock{};
	{
		/* An odd "insert or locate" interface. */
		auto r = kvstore->lock(pool, "heap", Component::IKVStore::STORE_LOCK_WRITE, heap_area, heap_size, heap_lock);
		ASSERT_EQ(S_OK_CREATED, r);
	}

	void *vector_area;

	using cc_vector = cc_container<eastl::vector<logged_int, lt_allocator>>;

	std::size_t vector_size = sizeof(cc_vector);
	Component::IKVStore::key_t container_lock{};
	{
		/* An odd "insert or locate" interface. */
		auto r = kvstore->lock(pool, "vector", Component::IKVStore::STORE_LOCK_WRITE, vector_area, vector_size, container_lock);
		ASSERT_EQ(S_OK_CREATED, r);
	}

	auto ccv = new (vector_area) cc_vector(heap_area, heap_size);

	auto ada = &ccv->allocator;
	std::vector<logged_int> original{{3, ada}, {4, ada}, {5, ada}};

	std::copy(original.begin(), original.end(), std::back_inserter(*ccv->container));

	ASSERT_EQ(3, ccv->container->size());
	ASSERT_TRUE(std::equal(ccv->container->begin(), ccv->container->end(), original.begin()));
	/* now 3 4 5 */
	ccv->log.commit();
	ccv->container->push_back({7, ada});
	ASSERT_EQ(4, ccv->container->size());
	ASSERT_TRUE(std::equal(ccv->container->begin(), ccv->container->end(), std::vector<int>{3,4,5,7}.begin()));
	/* 3 4 5 7 */
	{
		auto it = ccv->container->begin(); // -> 3
		++it; // -> 4
		auto jt = ccv->container->erase(it); // -> 5
		ASSERT_EQ(5, *jt);
		ASSERT_EQ(3, ccv->container->size());
		ASSERT_TRUE(std::equal(ccv->container->begin(), ccv->container->end(), std::vector<int>{3,5,7}.begin()));
		/* 3 5 7 */
		++jt;
		ccv->container->insert(jt, {6, ada});
	}
	ASSERT_EQ(4, ccv->container->size());
	ASSERT_TRUE(std::equal(ccv->container->begin(), ccv->container->end(), std::vector<int>{3,5,6,7}.begin()));
	/* 3 5 6 7 */
	ccv->container->insert(ccv->container->begin(), {2, ada});
	/* 2 3 5 6 7 */
	/* Is the container as expected? */
	{
		std::vector<int> mod_expected{2,3,5,6,7};
		ASSERT_EQ(mod_expected.size(), ccv->container->size());
		ASSERT_TRUE(std::equal(ccv->container->begin(), ccv->container->end(), mod_expected.begin()));
	}

	ccv->log.rollback();

	{
		/* Is the rolled back container as expected? */
		ASSERT_EQ(original.size(), ccv->container->size());
		bool is_same = std::equal(ccv->container->begin(), ccv->container->end(), original.begin());

		if ( ! is_same )
		{
			for ( auto it = ccv->container->begin(); it != ccv->container->end(); ++it )
			{
				auto i = *it;
				std::cerr << "value: " << i << "\n";
			}
		}

		ASSERT_TRUE(is_same);
	}

	/* Reverse the elements, to exercise backward iterator (probably). */
	ccv->log.commit();

/* can use std::reverse only if EASTL uses standard iterator categories */
#if EASTL_STD_ITERATOR_CATEGORY_ENABLED
	std::reverse(ccv->container->begin(), ccv->container->end());
	{
		/* Is the reversal also as expected? */
		ASSERT_EQ(original.size(), ccv->container->size());
		ASSERT_TRUE(std::equal(ccv->container->rbegin(), ccv->container->rend(), original.begin()));
	}
#endif

	/* Undo the reversal */
	ccv->log.rollback();

	{
		/* Is the rolled back container as expected? */
		ASSERT_EQ(original.size(), ccv->container->size());
		auto is_same = std::equal(ccv->container->begin(), ccv->container->end(), original.begin());
		if ( ! is_same )
		{
			for ( auto it = ccv->container->begin(); it != ccv->container->end(); ++it )
			{
				auto i = *it;
				std::cerr << "value: " << i << "\n";
			}
		}
		ASSERT_TRUE(is_same);
	}

	{
		auto r = kvstore->unlock(pool, container_lock);
		ASSERT_EQ(S_OK, r);
	}

	{
		auto r = kvstore->unlock(pool, heap_lock);
		ASSERT_EQ(S_OK, r);
	}
	close_pool(kvstore.get(), pool);
}

TEST_F(Log_test, CCList)
{
	auto kvstore = std::unique_ptr<Component::IKVStore>(instantiate());
	ASSERT_TRUE(kvstore.get());
	auto pool = create_pool(kvstore.get(), pool_name(), MB(10));
	ASSERT_LT(0, int64_t(pool));

	void *heap_area;
	std::size_t heap_size = MB(5);
	Component::IKVStore::key_t heap_lock{};
	{
		/* An odd "insert or locate" interface. */
		auto r = kvstore->lock(pool, "heap", Component::IKVStore::STORE_LOCK_WRITE, heap_area, heap_size, heap_lock);
		ASSERT_EQ(S_OK_CREATED, r);
	}

	void *list_area;

	using cc_list = cc_container<eastl::list<logged_int, lt_allocator>>;

	std::size_t list_size = sizeof(cc_list);
	Component::IKVStore::key_t list_lock{};
	{
		/* An odd "insert or locate" interface. */
		auto r = kvstore->lock(pool, "list", Component::IKVStore::STORE_LOCK_WRITE, list_area, list_size, list_lock);
		ASSERT_EQ(S_OK_CREATED, r);
	}

	auto ccl = new (list_area) cc_list(heap_area, heap_size);

	auto ada = &ccl->allocator;
	std::vector<logged_int> original{{3, ada}, {4, ada}, {5, ada}};
	std::copy(original.begin(), original.end(), std::back_inserter(*ccl->container));

	ASSERT_EQ(3, ccl->container->size());
	ASSERT_TRUE(std::equal(ccl->container->begin(), ccl->container->end(), original.begin()));
	/* now 3 4 5 */
	ccl->log.commit();
	ccl->container->push_back(7);
	ASSERT_EQ(4, ccl->container->size());
	ASSERT_TRUE(std::equal(ccl->container->begin(), ccl->container->end(), std::vector<int>{3,4,5,7}.begin()));
	/* 3 4 5 7 */
	{
		auto it = ccl->container->begin(); // -> 3
		++it; // -> 4
		auto jt = ccl->container->erase(it); // -> 5
		ASSERT_EQ(5, *jt);
		ASSERT_EQ(3, ccl->container->size());
		ASSERT_TRUE(std::equal(ccl->container->begin(), ccl->container->end(), std::vector<int>{3,5,7}.begin()));
		/* 3 5 7 */
		++jt;
		ccl->container->insert(jt, 6);
	}
	ASSERT_EQ(4, ccl->container->size());
	ASSERT_TRUE(std::equal(ccl->container->begin(), ccl->container->end(), std::vector<int>{3,5,6,7}.begin()));
	/* 3 5 6 7 */
	ccl->container->push_front(2);
	/* 2 3 5 6 7 */
	/* Is the container as expected? */
	{
		std::vector<int> mod_expected{2,3,5,6,7};
		ASSERT_EQ(mod_expected.size(), ccl->container->size());
		ASSERT_TRUE(std::equal(ccl->container->begin(), ccl->container->end(), mod_expected.begin()));
	}

	ccl->log.rollback();

	{
		/* Is the rolled back container as expected? */
		ASSERT_EQ(original.size(), ccl->container->size());
		ASSERT_TRUE(std::equal(ccl->container->begin(), ccl->container->end(), original.begin()));
	}

	/* Reverse the elements, to exercise backward iterator (probably). */
	ccl->log.commit();

	/* As reversal is performed by an algorithm (not by a container method),
	 * logging does not work unless the container elements (not just the container)
	 * are tracked.
	 */

/* can use std::reverse only of EASTL uses standard iterator categories */
#if EASTL_STD_ITERATOR_CATEGORY_ENABLED
	std::reverse(ccl->container->begin(), ccl->container->end());
	{
		/* Is the reversal also as expected? */
		ASSERT_EQ(original.size(), ccl->container->size());
		ASSERT_TRUE(std::equal(ccl->container->rbegin(), ccl->container->rend(), original.begin()));
	}
#endif

	/* Undo the reversal */
	ccl->log.rollback();

	{
		/* Is the rolled back container as expected? */
		ASSERT_EQ(original.size(), ccl->container->size());
		bool is_same = std::equal(ccl->container->begin(), ccl->container->end(), original.begin());
		if ( ! is_same )
		{
			for ( auto it = ccl->container->begin(); it != ccl->container->end(); ++it )
			{
				auto i = *it;
				std::cerr << "value: " << i << "\n";
			}
		}
		ASSERT_TRUE(is_same);
	}

	{
		auto r = kvstore->unlock(pool, list_lock);
		ASSERT_EQ(S_OK, r);
	}

	{
		auto r = kvstore->unlock(pool, heap_lock);
		ASSERT_EQ(S_OK, r);
	}
	close_pool(kvstore.get(), pool);
}

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	auto r = RUN_ALL_TESTS();

	return r;
}
