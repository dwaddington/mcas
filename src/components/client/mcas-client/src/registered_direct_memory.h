/*
   Copyright [2020] [IBM Corporation]
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
#ifndef MCAS_CLIENT_REGISTRED_DIRCCT_MEMORY_H
#define MCAS_CLIENT_REGISTRED_DIRCCT_MEMORY_H

#include "memory_registered.h"

#include <api/kvstore_itf.h> /* Component::IKVStore::memory_handle_t */
#include <cstddef>           /* size_t */

namespace component
{
class IFabric_client;
}

using memory_registered_fabric = mcas::memory_registered<component::IFabric_client>;

template <typename B>
struct registered_direct_memory_buffer : public memory_registered_fabric {
private:
  B *_buffer;

 public:
  using buffer_t = B;
  // registered_direct_memory_buffer(unsigned                   debug_level_,
  //                                 Component::IFabric_client *transport_,
  //                                 void *                     base_,
  //                                 std::size_t                len_)
  //     : memory_registered(debug_level_, transport_, base_, len_),
  //       _buffer(new buffer_t(base_, len_, mr(), get_memory_descriptor()))
  // {
  // }
  registered_direct_memory_buffer(const registered_direct_memory_buffer &) = delete;
  registered_direct_memory_buffer(registered_direct_memory_buffer &&)      = default;
  registered_direct_memory_buffer &operator=(const registered_direct_memory_buffer &) = delete;

  component::IKVStore::memory_handle_t value() const { return _buffer; }
  ~registered_direct_memory_buffer()
  {
    auto buffer = static_cast<buffer_t *>(value());
    assert(buffer->check_magic());
  }
};

#endif  //__CLIENT_FABRIC_TRANSPORT_H__
