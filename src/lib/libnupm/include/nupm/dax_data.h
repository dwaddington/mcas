/*
   Copyright [2017-2019] [IBM Corporation]
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


#ifndef __NUPM_DAX_DATA_H__
#define __NUPM_DAX_DATA_H__

#include <libpmem.h>
#include <common/types.h>
#include <common/utils.h>
#include <boost/icl/split_interval_map.hpp>
#include <algorithm>
#include <stdexcept>

namespace nupm
{
static std::uint32_t constexpr DM_REGION_MAGIC = 0xC0070000;
static unsigned constexpr DM_REGION_NAME_MAX_LEN = 1024;
static std::uint32_t constexpr DM_REGION_VERSION = 3;
static unsigned constexpr dm_region_log_grain_size = DM_REGION_LOG_GRAIN_SIZE; // log2 granularity (CMake default is 25, i.e. 32 MiB)

class DM_undo_log {
  static constexpr unsigned MAX_LOG_COUNT = 4;
  static constexpr unsigned MAX_LOG_SIZE  = 64;
  struct log_entry_t {
    byte   log[MAX_LOG_SIZE];
    void * ptr;
    size_t length; /* zero indicates log freed */
  };

 public:
  void log(void *ptr, size_t length)
  {
    assert(length > 0);
    assert(ptr);

    if (length > MAX_LOG_SIZE)
      throw std::invalid_argument("log length exceeds max. space");

    for (unsigned i = 0; i < MAX_LOG_COUNT; i++) {
      if (_log[i].length == 0) {
        _log[i].length = length;
        _log[i].ptr    = ptr;
        pmem_memcpy_nodrain(_log[i].log, ptr, length);
        // TODO
        //memcpy(_log[i].log, ptr, length);
        //mem_flush(&_log[i], sizeof(log_entry_t));
        return;
      }
    }
    throw API_exception("undo log full");
  }

  void clear_log()
  {
    for (unsigned i = 0; i < MAX_LOG_COUNT; i++) _log[i].length = 0;
  }

  void check_and_undo()
  {
    for (unsigned i = 0; i < MAX_LOG_COUNT; i++) {
      if (_log[i].length > 0) {
        PLOG("undo log being applied (ptr=%p, len=%lu).", _log[i].ptr,
             _log[i].length);
        // TODO
        pmem_memcpy_persist(_log[i].ptr, _log[i].log, _log[i].length);
        //memcpy(_log[i].ptr, _log[i].log, _log[i].length);
        //        mem_flush_nodrain(_log[i].ptr, _log[i].length);
        _log[i].length = 0;
      }
    }
  }

 private:
  log_entry_t _log[MAX_LOG_COUNT];
} __attribute__((packed));

class DM_region {
public:
  using grain_offset_t = uint32_t;
  grain_offset_t offset_grain;
  grain_offset_t length_grain;
  uint64_t region_id;

 public:
  /* re-zeroing constructor */
  DM_region() : offset_grain(0), length_grain(0), region_id(0) { assert(check_aligned(this, 8)); }

  void initialize(size_t space_size, std::size_t grain_size)
  {
    offset_grain = 0;
    length_grain = boost::numeric_cast<uint32_t>(space_size / grain_size);
    region_id = 0; /* zero indicates free */
  }

  friend class DM_region_header;
} __attribute__((packed));

/* Note: "region" has at least two meanings:
 *  1. The space which begins with a DM_region_header
 *  2. The space described by a DM_region
 */
class DM_region_header {
 private:
  static constexpr uint16_t DEFAULT_MAX_REGIONS = 1024;

  uint32_t    _magic;         // 4
  uint32_t    _version;       // 8
  uint64_t    _device_size;   // 16
  uint32_t    _region_count;  // 20
  uint16_t    _log_grain_size; // 22
  uint16_t    _resvd;         // 24
  uint8_t     _padding[40];   // 64
  DM_undo_log _undo_log;

 public:
  auto grain_size() const { return std::size_t(1) << _log_grain_size; }

  /* Rebuilding constructor */
  DM_region_header(size_t device_size)
    : _magic(DM_REGION_MAGIC)
    , _version(DM_REGION_VERSION)
    , _device_size(device_size)
    , _region_count( (::pmem_flush(this, sizeof(DM_region_header)), DEFAULT_MAX_REGIONS) )
    , _log_grain_size(dm_region_log_grain_size)
    , _resvd()
    , _undo_log()
  {
    (void)_resvd; // unused
    (void)_padding; // unused
    DM_region *region_p = region_table_base();
    /* initialize first region with all capacity */
    region_p->initialize(device_size - grain_size(), grain_size());
    _undo_log.clear_log();
    region_p++;

    for (uint16_t r = 1; r < _region_count; r++) {
      new (region_p) DM_region();
      _undo_log.clear_log();
      region_p++;
    }
    major_flush();
  }

  void check_undo_logs()
  {
    _undo_log.check_and_undo();
  }

  void debug_dump()
  {
    PINF("DM_region_header:");
    PINF(
        " magic [0x%8x]\n version [%u]\n device_size [%lu]\n region_count [%u]",
        _magic, _version, _device_size, _region_count);
    PINF(" base [%p]", static_cast<void *>(this));

    for (uint16_t r = 0; r < _region_count; r++) {
      auto reg = region_table_base()[r];
      if (reg.region_id > 0) {
        PINF(" - USED: %lu (%lx-%lx)", reg.region_id,
             grain_to_bytes(reg.offset_grain),
             grain_to_bytes(reg.offset_grain + reg.length_grain) - 1);
        assert(reg.length_grain > 0);
      }
      else if (reg.length_grain > 0) {
        PINF(" - FREE: %lu (%lx-%lx)", reg.region_id,
             grain_to_bytes(reg.offset_grain),
             grain_to_bytes(reg.offset_grain + reg.length_grain) - 1);
      }
    }
  }

  void *get_region(uint64_t region_id, size_t *out_size)
  {
    if (region_id == 0) throw std::invalid_argument("invalid region_id");

    for (uint16_t r = 0; r < _region_count; r++) {
      auto reg = region_table_base()[r];
      if (reg.region_id == region_id) {
        PLOG("found matching region (%lx)", region_id);
        if (out_size) *out_size = grain_to_bytes(reg.length_grain);
        return arena_base() + grain_to_bytes(reg.offset_grain);
      }
    }
    return nullptr; /* not found */
  }

  void erase_region(uint64_t region_id)
  {
    if (region_id == 0) throw std::invalid_argument("invalid region_id");

    for (uint16_t r = 0; r < _region_count; r++) {
      DM_region *reg = &region_table_base()[r];
      if (reg->region_id == region_id) {
        reg->region_id = 0; /* power-fail atomic */
        pmem_flush(&reg->region_id, sizeof(reg->region_id));
        return;
      }
    }
    throw std::domain_error("region not found");
  }

  void *allocate_region(uint64_t region_id, DM_region::grain_offset_t size_in_grain)
  {
    if (region_id == 0) throw std::invalid_argument("invalid region_id");

    for (uint16_t r = 0; r < _region_count; r++) {
      auto reg = region_table_base()[r];
      if (reg.region_id == region_id)
        throw std::bad_alloc();
    }

    uint32_t new_offset;
    bool     found = false;
    for (uint16_t r0 = 0; r0 < _region_count; ++r0) {
      DM_region *reg = &region_table_base()[r0];
      if (reg->region_id == 0 && reg->length_grain >= size_in_grain) {
        if (reg->length_grain == size_in_grain) {
          /* exact match */
          void *rp = arena_base() + grain_to_bytes(reg->offset_grain);
          // zero region
          tx_atomic_write(reg, region_id);
          return rp;
        }
        else {
          /* cut out */
          new_offset = reg->offset_grain;

          auto changed_length = reg->length_grain - size_in_grain;
          auto changed_offset = reg->offset_grain + size_in_grain;

          for (uint16_t r = 0; r < _region_count; ++r) {
            DM_region *reg_n = &region_table_base()[r];
            if (reg_n->region_id == 0 && reg_n->length_grain == 0) {
              void *rp = arena_base() + grain_to_bytes(new_offset);
              tx_atomic_write(reg_n, boost::numeric_cast<uint16_t>(changed_offset), boost::numeric_cast<uint16_t>(changed_length), reg,
                              new_offset, size_in_grain, region_id);
              return rp;
            }
          }
        }
      }
    }
    if (!found)
      throw General_exception("no more regions (size in grain=%u)", size_in_grain);

    throw General_exception("no spare slots");
  }

  size_t get_max_available() const
  {
    auto max_grain_element =
      std::max_element(
        &region_table_base()[0]
			, &region_table_base()[_region_count]
        , [] (const DM_region &a, const DM_region &b) -> bool { return a.length_grain < b.length_grain; }
      );
    return grain_to_bytes( max_grain_element == &region_table_base()[_region_count] ? 0 : max_grain_element->length_grain);
  }

  inline size_t grain_to_bytes(unsigned grain) const { return size_t(grain) << _log_grain_size; }

  inline void major_flush()
  {
    pmem_flush(this, sizeof(DM_region_header) + (sizeof(DM_region) * _region_count));
  }

  bool check_magic() const
  {
    return (_magic == DM_REGION_MAGIC) && (_version == DM_REGION_VERSION);
  }

 private:
  void tx_atomic_write(DM_region *dst, uint64_t region_id)
  {
    _undo_log.log(&dst->region_id, sizeof(region_id));
    dst->region_id = region_id;
    pmem_flush(&dst->region_id, sizeof(region_id));
    _undo_log.clear_log();
  }

  void tx_atomic_write(DM_region *dst0, // offset0, size0, offset1, size1 all expressed in grains
                       uint32_t   offset0,
                       uint32_t   size0,
                       DM_region *dst1,
                       uint32_t   offset1,
                       uint32_t   size1,
                       uint64_t   region_id1)
  {
    _undo_log.log(dst0, sizeof(DM_region));
    _undo_log.log(dst1, sizeof(DM_region));

    dst0->offset_grain = offset0;
    dst0->length_grain = size0;
    pmem_flush(dst0, sizeof(DM_region));

    dst1->region_id = region_id1;
    dst1->offset_grain = offset1;
    dst1->length_grain = size1;
    pmem_flush(dst1, sizeof(DM_region));

    _undo_log.clear_log();
  }

  inline unsigned char *arena_base()
  {
    return static_cast<unsigned char *>(static_cast<void *>(this)) + grain_size();
  }

  inline DM_region *region_table_base() { return static_cast<DM_region *>(static_cast<void *>(this + 1)); }
  inline const DM_region *region_table_base() const { return static_cast<const DM_region *>(static_cast<const void *>(this + 1)); }

  inline DM_region *region(size_t idx)
  {
    if (idx >= _region_count) return nullptr;
    DM_region *p = static_cast<DM_region *>(region_table_base());
    return &p[idx];
  }

  void reset_header(size_t device_size)
  {
    _magic       = DM_REGION_MAGIC;
    _version     = DM_REGION_VERSION;
    _device_size = device_size;
    pmem_flush(this, sizeof(DM_region_header));
  }
} __attribute__((packed));

}  // namespace nupm

#endif  //__NUPM_DAX_DATA_H__
