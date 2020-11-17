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

#include "heap_rc.h"

#include "dax_manager.h"
#include "heap_rc_ephemeral.h"
#include "hstore_config.h"
#include "tracked_header.h"
#include "valgrind_memcheck.h"
#include <common/utils.h>
#include <algorithm> /* max */
#include <cinttypes>
#include <memory> /* make_unique */
#include <numeric> /* acccumulate */
#include <stdexcept> /* range_error */
#include <string> /* to_string */

/* When used with ADO, this space apparently needs a 2MiB alignment.
 * 4 KiB alignment sometimes produces a disagreement between server and ADO mappings,
 * which manifests as incorrect key and data values as seen on the ADO side.
 */
heap_rc::heap_rc(
	unsigned debug_level_, ::iovec pool0_full_, ::iovec pool0_heap_, unsigned numa_node_, const std::string & id_, const std::string & backing_file_
)
	: _pool0_full(pool0_full_)
	, _pool0_heap(pool0_heap_)
	, _numa_node(numa_node_)
	, _more_region_uuids_size(0)
	, _more_region_uuids()
	, _tracked_anchor(debug_level_, &_tracked_anchor, &_tracked_anchor, sizeof(_tracked_anchor), sizeof(_tracked_anchor))
	, _eph(std::make_unique<heap_rc_ephemeral>(debug_level_, id_, backing_file_))
{
	void *last = static_cast<char *>(pool0_heap_.iov_base) + pool0_heap_.iov_len;
	if ( 0 < debug_level_ )
	{
		PLOG("%s: split %p .. %p) into segments", __func__, pool0_heap_.iov_base, last);

		PLOG("%s: pool0 full %p: 0x%zx", __func__, _pool0_full.iov_base, _pool0_full.iov_len);
		PLOG("%s: pool0 heap %p: 0x%zx", __func__, _pool0_heap.iov_base, _pool0_heap.iov_len);
	}
	/* cursor now locates the best-aligned region */
	_eph->add_managed_region(_pool0_full, _pool0_heap, _numa_node);
	hop_hash_log<trace_heap_summary>::write(
		LOG_LOCATION
		, " pool ", _pool0_full.iov_base, " .. ", iov_limit(_pool0_full)
		, " size ", _pool0_full.iov_len
		, " new"
	);
	VALGRIND_CREATE_MEMPOOL(_pool0_heap.iov_base, 0, false);
	persister_nupm::persist(this, sizeof(*this));
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winit-self"
#pragma GCC diagnostic ignored "-Wuninitialized"
heap_rc::heap_rc(
	unsigned debug_level_
	, const std::unique_ptr<dax_manager> &dax_manager_
	, const std::string & id_
    , const std::string & backing_file_
	, const ::iovec *iov_addl_first_
	, const ::iovec *iov_addl_last_
)
	: _pool0_full(this->_pool0_full)
	, _pool0_heap(this->_pool0_heap)
	, _numa_node(this->_numa_node)
	, _more_region_uuids_size(this->_more_region_uuids_size)
	, _more_region_uuids(this->_more_region_uuids)
	, _tracked_anchor(this->_tracked_anchor)
	, _eph(std::make_unique<heap_rc_ephemeral>(debug_level_, id_, backing_file_))
{
	_eph->add_managed_region(_pool0_full, _pool0_heap, _numa_node);
	hop_hash_log<trace_heap_summary>::write(
		LOG_LOCATION
		, " pool ", _pool0_full.iov_base, " .. ", iov_limit(_pool0_full)
		, " size ", _pool0_full.iov_len
		, " reconstituting"
	);

	VALGRIND_MAKE_MEM_DEFINED(_pool0_heap.iov_base, _pool0_heap.iov_len);
	VALGRIND_CREATE_MEMPOOL(_pool0_heap.iov_base, 0, true);

	for ( auto r = iov_addl_first_; r != iov_addl_last_; ++r )
	{
		_eph->add_managed_region(*r, *r, _numa_node);
	}

	for ( std::size_t i = 0; i != _more_region_uuids_size; ++i )
	{
		auto r = open_region(dax_manager_, _more_region_uuids[i], _numa_node);
		_eph->add_managed_region(r, r, _numa_node);
		VALGRIND_MAKE_MEM_DEFINED(r.iov_base, r.iov_len);
		VALGRIND_CREATE_MEMPOOL(r.iov_base, 0, true);
	}
	_tracked_anchor.recover(debug_level_, _eph.get(), _numa_node);
}
#pragma GCC diagnostic pop

heap_rc::~heap_rc()
{
	quiesce();
}

::iovec heap_rc::open_region(const std::unique_ptr<dax_manager> &dax_manager_, std::uint64_t uuid_, unsigned numa_node_)
{
	auto iovs = dax_manager_->open_region(std::to_string(uuid_), numa_node_).address_map;
	if ( iovs.size() != 1 )
	{
		throw std::range_error("failed to re-open region " + std::to_string(uuid_));
	}
	return iovs.front();
}

auto heap_rc::regions() const -> nupm::region_descriptor
{
	return _eph->get_managed_regions();
}

void *heap_rc::iov_limit(const ::iovec &r)
{
	return static_cast<char *>(r.iov_base) + r.iov_len;
}

namespace
{
	std::size_t region_size(const std::vector<::iovec> &v)
	{
		return
			std::accumulate(
				v.begin()
				, v.end()
				, std::size_t(0)
				, [] (std::size_t s, const ::iovec &iov) -> std::size_t
					{
						return s + iov.iov_len;
					}
			);
	}
}

auto heap_rc::grow(
	const std::unique_ptr<dax_manager> & dax_manager_
	, std::uint64_t uuid_
	, std::size_t increment_
) -> std::size_t
{
	if ( 0 < increment_ )
	{
		if ( _more_region_uuids_size == _more_region_uuids.size() )
		{
			throw std::bad_alloc(); /* max # of regions used */
		}
		const auto hstore_grain_size = std::size_t(1) << (HSTORE_LOG_GRAIN_SIZE);
		auto size = ( (increment_ - 1) / hstore_grain_size + 1 ) * hstore_grain_size;

		auto grown = false;
		{
			const auto old_regions = regions();
			const auto &old_region_list = old_regions.address_map;
			const auto old_list_size = old_region_list.size();
			const auto old_size = region_size(old_region_list);
			_eph->set_managed_regions(dax_manager_->resize_region(old_regions.id,  _numa_node, old_size + increment_));
			const auto new_region_list = regions().address_map;
			const auto new_size = region_size(new_region_list);
			const auto new_list_size = new_region_list.size();

			if ( old_size <  new_size )
			{
				for ( auto i = old_list_size; i != new_list_size; ++i )
				{
					const auto &r = new_region_list[i];
					_eph->add_managed_region(r, r, _numa_node);
					hop_hash_log<trace_heap_summary>::write(
						LOG_LOCATION
						, " pool ", r.iov_base, " .. ", iov_limit(r)
						, " size ", r.iov_len
						, " grow"
					);
				}
			}
			grown = true;
		}

		if ( ! grown )
		{
			auto uuid = _more_region_uuids_size == 0 ? uuid_ : _more_region_uuids[_more_region_uuids_size-1];
			auto uuid_next = uuid + 1;
			for ( ; uuid_next != uuid; ++uuid_next )
			{
				if ( uuid_next != 0 )
				{
					try
					{
						/* Note: crash between here and "Slot persist done" may cause dax_manager_
						 * to leak the region.
						 */
						auto rv = dax_manager_->create_region(std::to_string(uuid_next), _numa_node, size).address_map;
						{
							auto &slot = _more_region_uuids[_more_region_uuids_size];
							slot = uuid_next;
							persister_nupm::persist(&slot, sizeof slot);
							/* Slot persist done */
						}
						{
							++_more_region_uuids_size;
							persister_nupm::persist(&_more_region_uuids_size, _more_region_uuids_size);
						}
						for ( const auto &r : rv )
						{
							_eph->add_managed_region(r, r, _numa_node);
							hop_hash_log<trace_heap_summary>::write(
								LOG_LOCATION
								, " pool ", r.iov_base, " .. ", iov_limit(r)
								, " size ", r.iov_len
								, " grow"
							);
						}
						break;
					}
					catch ( const std::bad_alloc & )
					{
						/* probably means that the uuid is in use */
					}
					catch ( const General_exception & )
					{
						/* probably means that the space cannot be allocated */
						throw std::bad_alloc();
					}
				}
			}
			if ( uuid_next == uuid )
			{
				throw std::bad_alloc(); /* no more UUIDs */
			}
		}
	}
	return _eph->capacity();
}

void heap_rc::quiesce()
{
	hop_hash_log<trace_heap_summary>::write(LOG_LOCATION, " size ", _pool0_heap.iov_len, " allocated ", _eph->allocated());
	_eph->write_hist<trace_heap_summary>(_pool0_heap);
	VALGRIND_DESTROY_MEMPOOL(_pool0_heap.iov_base);
	VALGRIND_MAKE_MEM_UNDEFINED(_pool0_heap.iov_base, _pool0_heap.iov_len);
	_eph.reset(nullptr);
}

namespace
{
	/* Round up to (ceiling) power of 2, from Hacker's Delight 3-2 */
	std::size_t clp2(std::size_t sz_)
	{
		if ( sz_ != 0 )
		{
			--sz_;
			sz_ |= sz_ >> 1;
			sz_ |= sz_ >> 2;
			sz_ |= sz_ >> 4;
			sz_ |= sz_ >> 8;
			sz_ |= sz_ >> 16;
			sz_ |= sz_ >> 32;
		}
		return sz_ + 1;
	}
}

void *heap_rc::alloc(const std::size_t sz_, const std::size_t alignment_)
{
	auto alignment = std::max(alignment_, sizeof(void *));

	if ( (alignment & (alignment - 1U)) != 0 )
	{
		throw std::invalid_argument("alignment is not a power of 2");
	}

	auto sz = sz_;

	if ( sz < alignment )
	{
		/* round up only to a power of 2, so Rca_LB will find the element
		 * on free.
		 */
		sz = clp2(sz);
		assert( (sz & (sz - 1)) == 0 );
		/* Allocation must be a multiple of alignment. In the case,
		 * adjust alignment. */
		alignment = std::max(sizeof(void *), sz);
	}

	/* In any case, sz must be a multiple of alignment. */
	sz = (sz + alignment - 1U)/alignment * alignment;

	try {
		auto p = _eph->allocate(sz, _numa_node, alignment);
		/* Note: allocation exception from Rca_LB is General_exception, which does not derive
		 * from std::bad_alloc.
		 */

		VALGRIND_MEMPOOL_ALLOC(_pool0_heap.iov_base, p, sz);
		hop_hash_log<trace_heap>::write(LOG_LOCATION, "pool ", _pool0_full.iov_base, " addr ", p, " align ", alignment_, " -> ", alignment, " size ", sz_, " -> ", sz);
		return p;
	}
	catch ( const std::bad_alloc & )
	{
		_eph->write_hist<true>(_pool0_heap);
		/* Sometimes lack of space will cause heap to throw a bad_alloc. */
		throw;
	}
	catch ( const General_exception &e )
	{
		_eph->write_hist<true>(_pool0_heap);
		/* Sometimes lack of space will cause heap to throw a General_exception with this explanation. */
		/* Convert to bad_alloc. */
		if ( e.cause() == std::string("region allocation out-of-space") )
		{
			throw std::bad_alloc();
		}
		throw;
	}
}

void *heap_rc::alloc_tracked(const std::size_t sz_, const std::size_t align_)
{
	if ( align_ != 0 && (align_ & (align_ - 1U)) != 0 )
	{
		throw std::invalid_argument("alignment is not a power of 2");
	}

	/* alignment: enough for tracked_header prefix, and a power of 2 */
	auto align = clp2(std::max(align_, sizeof(tracked_header)));

	/* size: a multiple of alignment */
	auto sz = round_up(sz_ + align, align);

	try {
		auto p = _eph->allocate(sz, _numa_node, align);
		/* Note: allocation exception from Rca_LB is General_exception, which does not derive
		 * from std::bad_alloc.
		 */

		VALGRIND_MEMPOOL_ALLOC(_pool0_heap.iov_base, p, sz);
		hop_hash_log<trace_heap>::write(LOG_LOCATION, "pool ", _pool0_full.iov_base, " addr ", p, " align ", align_, " -> ", align, " size ", sz_, " -> ", sz);
		tracked_header *h = new (static_cast<char *>(p) + align - sizeof(tracked_header))
			tracked_header(_eph->debug_level(), &_tracked_anchor, _tracked_anchor._next, sz, align);
		persister_nupm::persist(h, sizeof *h);

		_tracked_anchor._next->_prev = h; /* _prev, need not flush */
		_tracked_anchor._next = h; /* _next, must flush */
		persister_nupm::persist(&_tracked_anchor._next, sizeof _tracked_anchor._next);

#if 0
		PLOG(
			"%s: TH %p prev %p next %p size %zu align %zu"
			, __func__
			, static_cast<const void *>(h)
			, static_cast<const void *>(h->_prev)
			, static_cast<const void *>(h->_next)
			, h->_size
			, h->_align
		);
#endif
		return h + 1;
	}
	catch ( const std::bad_alloc & )
	{
		_eph->write_hist<true>(_pool0_heap);
		/* Sometimes lack of space will cause heap to throw a bad_alloc. */
		throw;
	}
	catch ( const General_exception &e )
	{
		_eph->write_hist<true>(_pool0_heap);
		/* Sometimes lack of space will cause heap to throw a General_exception with this explanation. */
		/* Convert to bad_alloc. */
		if ( e.cause() == std::string("region allocation out-of-space") )
		{
			throw std::bad_alloc();
		}
		throw;
	}
}

void heap_rc::inject_allocation(const void * p, std::size_t sz_)
{
	auto alignment = sizeof(void *);
	sz_ = std::max(sz_, alignment);
	auto sz = (sz_ + alignment - 1U)/alignment * alignment;
	/* NOTE: inject_allocation should take a const void* */
	_eph->inject_allocation(const_cast<void *>(p), sz, _numa_node);
	VALGRIND_MEMPOOL_ALLOC(_pool0_heap.iov_base, p, sz);
	hop_hash_log<trace_heap>::write(LOG_LOCATION, "pool ", _pool0_heap.iov_base, " addr ", p, " size ", sz);
}

void heap_rc::free(void *p_, std::size_t sz_, std::size_t alignment_)
{
	alignment_ = std::max(alignment_, sizeof(void *));
	sz_ = std::max(sz_, alignment_);
	auto sz = (sz_ + alignment_ - 1U)/alignment_ * alignment_;
	VALGRIND_MEMPOOL_FREE(_pool0_heap.iov_base, p_);
	hop_hash_log<trace_heap>::write(LOG_LOCATION, "pool ", _pool0_heap.iov_base, " addr ", p_, " size ", sz);
	return _eph->free(p_, sz, _numa_node);
}

void heap_rc::free_tracked(
	void *p_
	, std::size_t sz_
	, std::size_t // align_
)
{
	tracked_header *h = static_cast<tracked_header *>(p_)-1;
	auto align = h->_align;
	/* size: a multiple of alignment */
	auto sz = round_up(sz_ + align, align);
	if ( 3 < _eph->debug_level() )
	{
		PLOG(
			"%s: TH %p prev %p next %p size %zu align %zu"
			, __func__
			, static_cast<const void *>(h)
			, static_cast<const void *>(h->_prev)
			, static_cast<const void *>(h->_next)
			, h->_size
			, h->_align
		);
	}
	h->_next->_prev = h->_prev; /* _prev, need not flush */
	h->_prev->_next = h->_next; /* _next, must flush */
	persister_nupm::persist(&h->_prev->_next, sizeof h->_prev->_next);

	auto p = static_cast<char *>(p_) - h->_align;
	assert(sz == h->_size);
	VALGRIND_MEMPOOL_FREE(_pool0_heap.iov_base, p);
	hop_hash_log<trace_heap>::write(LOG_LOCATION, "pool ", _pool0_heap.iov_base, " addr ", p, " size ", sz);
	return _eph->free(p, sz, _numa_node);
}

unsigned heap_rc::percent_used() const
{
    return _eph->capacity() == 0 ? 0xFFFFU : unsigned(_eph->allocated() * 100U / _eph->capacity());
}

bool heap_rc::is_reconstituted(const void * p_) const
{
	return _eph->is_reconstituted(p_);
}
