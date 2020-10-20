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


#ifndef MCAS_HSTORE_NUPM_REGION_H
#define MCAS_HSTORE_NUPM_REGION_H

/* requires persist_data_t definition */
#include "hstore_config.h"
#include "persist_data.h"

#include <nupm/region_descriptor.h>
#include <sys/uio.h>
#include <memory>
#include <sstream>
#include <stdexcept>

struct dax_manager;

template <typename PersistData, typename Heap, typename HeapAllocator>
  struct region
  {
  private:
    static constexpr std::uint64_t magic_value = HeapAllocator::magic_value; // 0xc74892d72eed493a;
  public:
    using heap_type = Heap;
    using persist_data_type = PersistData;

  private:
    std::uint64_t magic;
    /* The hashed value of the string which names the region.
     * Preserved only form the basis for new strings generated
     * for grow.
     */
    std::uint64_t _uuid;
    heap_type _heap;
    persist_data_type _persist_data;

  public:

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
    region(
      AK_ACTUAL
      unsigned debug_level
      , std::uint64_t uuid_
      , std::size_t size_
      , std::size_t expected_obj_count
      , unsigned numa_node_
      , const std::string & id_
      , const std::string & backing_file_
    )
      : magic(0)
      , _uuid(uuid_)
      , _heap(
        debug_level
#if USE_CC_HEAP == 4
        , &_persist_data.ase()
        , (&_persist_data.aspd())
        , (&_persist_data.aspk())
        , &_persist_data.asx()
#endif
        , ::iovec{this, size_}
        , ::iovec{this+1, adjust_size(size_)}
        , numa_node_
        , id_
        , backing_file_
      )
      , _persist_data(
        AK_REF
        expected_obj_count
        , typename PersistData::allocator_type(locate_heap())
    )
    {
      magic = magic_value;
      persister_nupm::persist(this, sizeof *this);
    }
#pragma GCC diagnostic pop

    /* The "reanimate" (or "reconsitute") constructor */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winit-self"
#pragma GCC diagnostic ignored "-Wuninitialized"
    region(
      unsigned debug_level
      , const std::unique_ptr<dax_manager> & dax_manager_
      , const std::string & id_
      , const std::string & backing_file_
      , const ::iovec *iov_addl_first_
      , const ::iovec *iov_addl_last_
    )
      : magic(0)
      , _uuid(this->_uuid)
      , _heap(
        debug_level
        , dax_manager_
        , id_
        , backing_file_
        , iov_addl_first_
        , iov_addl_last_
#if USE_CC_HEAP == 4
        , &this->_persist_data.ase()
        , &this->_persist_data.aspd()
        , &this->_persist_data.aspk()
        , &this->_persist_data.asx()
#endif
      )
      , _persist_data(std::move(this->_persist_data))
    {
      magic = magic_value;
      persister_nupm::persist(this, sizeof *this);
#if USE_CC_HEAP == 4
      /* any old values in the allocation states have been queried, as needed, by
       * the crash-consistent allocator. Reset all allocation states.
       */
      this->_persist_data.ase().reset();
      this->_persist_data.aspd().reset();
      this->_persist_data.aspk().reset();
      this->_persist_data.asx().reset();
#endif
    }
#pragma GCC diagnostic pop

    auto adjust_size(std::size_t sz_)
    {
      if ( sz_ < sizeof *this )
      {
        std::ostringstream s;
        s << "Have " << std::hex << std::showbase << sz_ << " bytes. Cannot create a persisted region from less than " << sizeof *this << " bytes";
        throw std::range_error(s.str());
      }
      return sz_ - sizeof *this;
    }

    HeapAllocator locate_heap() { return HeapAllocator(&_heap); }
    persist_data_type &persist_data() { return _persist_data; }
    bool is_initialized() const noexcept { return magic == magic_value; }
    unsigned percent_used() const { return _heap.percent_used(); }
    void quiesce() { _heap.quiesce(); }
    nupm::region_descriptor get_regions() const
    {
      return _heap.regions();
    }
    auto grow(
      const std::unique_ptr<dax_manager> & dax_manager_
      , std::size_t increment_
    ) -> std::size_t
    {
      return _heap.grow(dax_manager_, _uuid, increment_);
    }
    /* region used by heap_cc follows */
  };

#endif
