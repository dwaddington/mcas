#ifndef __EXAMPLE_CPP_SYMTAB_TYPES_H__
#define __EXAMPLE_CPP_SYMTAB_TYPES_H__

#include <ccpm/fixed_array.h>
#include <ccpm/basic_types.h>
#include <common/byte_span.h>

namespace cpp_symtab_personality
{

struct Value_root
{
  using byte_span = common::byte_span;
public:
  static constexpr size_t MAX_REGIONS = 16;

  Value_root() {}

  void initialize() {
    regions.initialize();
    index_size = 0;
  }

  void add_region(void * buffer, size_t buffer_len) {
    for(unsigned i=0;i<MAX_REGIONS;i++) {
      if(::base(regions[i].ref()) == nullptr) {
        regions[i] = common::make_byte_span(buffer, buffer_len);
        ++num_regions;
        return;
      }
    }
  }

  ccpm::region_vector_t get_regions() {
    ccpm::region_vector_t result;
    for(unsigned i=0;i<MAX_REGIONS;i++) {
      if(::data(regions[i].ref()) != nullptr) {
        result.push_back(regions[i].ref());
      }
    }
    return result;
  }

  static constexpr size_t memory_footprint() { return sizeof(Value_root); }

  /* fixed array of region information */
  ccpm::Fixed_array<byte_span, MAX_REGIONS> regions;
  ccpm::Uint64                            num_regions;
  size_t                                  index_size;
  std::vector<const char *> *             index;
};

}

#endif // __EXAMPLE_CPP_SYMTAB_TYPES_H__
