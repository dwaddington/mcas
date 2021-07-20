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

#ifndef MCAS_HSTORE_HEAP_CC_EPHEMERAL_H
#define MCAS_HSTORE_HEAP_CC_EPHEMERAL_H

#include <common/logging.h> /* log_source */

#include "hstore_config.h"
#include "histogram_log2.h"
#include "hop_hash_log.h"
#include "persistent.h"

#include <ccpm/interfaces.h> /* ownership_callback, (IHeap_expandable, region_vector_t) */
#include <common/byte_span.h>
#include <common/string_view.h>
#include <nupm/region_descriptor.h>

#include <algorithm> /* min, swap */
#include <cstddef> /* size_t */
#include <memory> /* unique_ptr */
#include <vector>

namespace impl
{
	struct allocation_state_pin;
	struct allocation_state_emplace;
	struct allocation_state_extend;
}

struct heap_cc_ephemeral
  : private common::log_source
{
private:
	using byte_span = common::byte_span;
	using string_view = common::string_view;
	std::unique_ptr<ccpm::IHeap_expandable> _heap;
	nupm::region_descriptor _managed_regions;
	std::size_t _capacity;
	std::size_t _allocated;
	impl::allocation_state_emplace *_ase;
	impl::allocation_state_pin *_aspd;
	impl::allocation_state_pin *_aspk;
	impl::allocation_state_extend *_asx;

	using hist_type = util::histogram_log2<std::size_t>;
	hist_type _hist_alloc;
	hist_type _hist_inject;
	hist_type _hist_free;

	static constexpr unsigned log_min_alignment = 3U; /* log (sizeof(void *)) */
	static_assert(sizeof(void *) == 1U << log_min_alignment, "log_min_alignment does not match sizeof(void *)");
	/* Rca_LB seems not to allocate at or above about 2GiB. Limit reporting to 16 GiB. */
	static constexpr unsigned hist_report_upper_bound = 34U;
	explicit heap_cc_ephemeral(
		unsigned debug_level_
		, impl::allocation_state_emplace *ase
		, impl::allocation_state_pin *aspd
		, impl::allocation_state_pin *aspk
		, impl::allocation_state_extend *asx
		, std::unique_ptr<ccpm::IHeap_expandable> p
		, string_view id
		, string_view backing_file
		, const std::vector<byte_span> &rv_full
		, const byte_span &pool0_heap
	);
	nupm::region_descriptor get_managed_regions() const { return _managed_regions; }
	nupm::region_descriptor set_managed_regions(nupm::region_descriptor n)
	{
		using std::swap;
		swap(n, _managed_regions);
		return n;
	}

	template <bool B>
		void write_hist(const byte_span & pool_) const
		{
			static bool suppress = false;
			if ( ! suppress )
			{
				hop_hash_log<B>::write(LOG_LOCATION, "pool ", ::base(pool_));
				std::size_t lower_bound = 0;
				auto limit = std::min(std::size_t(hist_report_upper_bound), _hist_alloc.data().size());
				for ( unsigned i = log_min_alignment; i != limit; ++i )
				{
					const std::size_t upper_bound = 1ULL << i;
					hop_hash_log<B>::write(LOG_LOCATION
						, "[", lower_bound, "..", upper_bound, "): "
						, _hist_alloc.data()[i], " ", _hist_inject.data()[i], " ", _hist_free.data()[i]
						, " "
					);
					lower_bound = upper_bound;
				}
				suppress = true;
			}
		}
public:
	friend struct heap_cc;

	using common::log_source::debug_level;

	/* initial construction */
	explicit heap_cc_ephemeral(
		unsigned debug_level
		, impl::allocation_state_emplace *ase
		, impl::allocation_state_pin *aspd
		, impl::allocation_state_pin *aspk
		, impl::allocation_state_extend *asx
		, string_view id
		, string_view backing_file
		, const std::vector<byte_span> &rv_full
		, const byte_span &pool0_heap_
	);

	/* crash-consistent recovery */
	explicit heap_cc_ephemeral(
		unsigned debug_level
		, impl::allocation_state_emplace *ase
		, impl::allocation_state_pin *aspd
		, impl::allocation_state_pin *aspk
		, impl::allocation_state_extend *asx
		, string_view id
		, string_view backing_file
		, const std::vector<byte_span> &rv_full
		, const byte_span &pool0_heap
		, ccpm::ownership_callback_t f
	);
	heap_cc_ephemeral(const heap_cc_ephemeral &) = delete;
	heap_cc_ephemeral& operator=(const heap_cc_ephemeral &) = delete;

	void add_managed_region(const byte_span &r_full, const byte_span &r_heap, unsigned numa_node);
	std::size_t free(persistent_t<void *> *p_, std::size_t sz_);
};

#endif
