/*
   Copyright [2017-2021] [IBM Corporation]
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


#ifndef MCAS_HSTORE_HEAP_MR_EPHEMERAL_H
#define MCAS_HSTORE_HEAP_MR_EPHEMERAL_H

#include <common/logging.h> /* log_source */
#include "injectee.h"

#include "hstore_config.h"
#include "histogram_log2.h"
#include "hop_hash_log.h"

#include <boost/icl/interval_set.hpp>
#include <common/byte_span.h>
#include <common/string_view.h>
#include <mm_plugin_itf.h>
#include <nupm/region_descriptor.h>

#include <algorithm> /* min, swap */
#include <cstddef> /* size_t */

struct heap_mr_shim
{
	using string_view = common::string_view;
	using byte_span = common::byte_span;
private:
	MM_plugin_wrapper _mm;
public:
	heap_mr_shim(string_view path);

	status_t allocate(void * & ptr
		, std::size_t bytes
		, std::size_t alignment
	);

	status_t free(void * & ptr
		, std::size_t bytes
	);

	status_t remaining(std::size_t& out_size) const;

	status_t inject_allocation(void *ptr, std::size_t size);

	void add_managed_region(byte_span region);
};

struct heap_mr_ephemeral
	: private common::log_source
	, public injectee
{
	using string_view = common::string_view;
private:
	using byte_span = common::byte_span;
	heap_mr_shim _heap;

	/* Although the mm heap tracks managed regions, its definition of a managed
	 * region probably differs from what hstore needs:
	 *
	 * The hstore managed region must include the hstore metadata area, as that
	 * must be include in the "maanged regions" returned through the kvstore
	 * interface.
	 *
	 * The mm heap managed region cannot include the hstore metadata area, as
	 * that would allow the allocator to use the area for memory allocation.
	 *
	 * Therefore, heap_mr_ephemeral keeps its own copy of the managed regions.
	 */
	nupm::region_descriptor _managed_regions;
	std::size_t _allocated;
	std::size_t _capacity;
	/* The set of reconstituted addresses. Only needed during recovery.
	 * Potentially large, so should be erased after recovery. But there
	 * is no mechanism to erase it yet.
	 */
	using alloc_set_t = boost::icl::interval_set<const char *>; /* std::byte_t in C++17 */
	alloc_set_t _reconstituted; /* std::byte_t in C++17 */
	using hist_type = util::histogram_log2<std::size_t>;
	hist_type _hist_alloc;
	hist_type _hist_inject;
	hist_type _hist_free;

	static constexpr unsigned log_min_alignment = 3U; /* log (sizeof(void *)) */
	static_assert(sizeof(void *) == 1U << log_min_alignment, "log_min_alignment does not match sizeof(void *)");
	/* Rca_LB seems not to allocate at or above about 2GiB. Limit reporting to 16 GiB. */
	static constexpr unsigned hist_report_upper_bound = 34U;

	void add_managed_region(const byte_span &r);
public:
	explicit heap_mr_ephemeral(
		unsigned debug_level
		, string_view plugin_path
		, string_view id
		, string_view backing_file
	);
	virtual ~heap_mr_ephemeral() {}

	void add_managed_region(const byte_span &r_full, const byte_span &r_heap, unsigned numa_node);

	nupm::region_descriptor get_managed_regions() const { return _managed_regions; }
	nupm::region_descriptor set_managed_regions(nupm::region_descriptor n) { using std::swap; swap(n, _managed_regions); return n; }

	template <bool B>
		void write_hist(const byte_span & pool_) const
		{
			static bool suppress = false;
			if ( ! suppress )
			{
				hop_hash_log<B>::write(LOG_LOCATION, "pool ", ::base(pool_), " [range] alloc inject free");
				std::size_t lower_bound = 0;
				auto limit = std::min(std::size_t(hist_report_upper_bound), _hist_alloc.data().size());
				for ( unsigned i = log_min_alignment; i != limit; ++i )
				{
					const std::size_t upper_bound = 1ULL << i;
					if ( _hist_alloc.data()[i] != 0 || _hist_inject.data()[i] != 0 || _hist_free.data()[i] != 0 )
					{
						hop_hash_log<B>::write(LOG_LOCATION
							, "[", lower_bound, "..", upper_bound, "): "
							, _hist_alloc.data()[i], " ", _hist_inject.data()[i], " ", _hist_free.data()[i]
							, " "
						);
					}
					lower_bound = upper_bound;
				}
				suppress = true;
			}
		}

	std::size_t allocated() const {  return _allocated; }
	std::size_t capacity() const { return _capacity; };
	void inject_allocation(void *p, std::size_t sz, unsigned numa_node) override;
	void *allocate(std::size_t sz, unsigned numa_node, std::size_t alignment);
	void *allocate_tracked(std::size_t sz, unsigned numa_node, std::size_t alignment);
	void free(void *p, std::size_t sz, unsigned numa_node);
	bool is_reconstituted(const void *p) const;
	using common::log_source::debug_level;
};

#endif
