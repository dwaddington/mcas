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

#ifndef __MCAS_ADO_MAP_H__
#define __MCAS_ADO_MAP_H__

#include <tuple>
#include <common/exceptions.h>
#include <unordered_map>
#include "connection_handler.h"

namespace mcas
{
/**
 * @brief      Map managing the pool name to ADO proxy (ADO instance) mappings
 */
class Ado_map {
  static constexpr unsigned _debug_level = 3;

  using map_t = std::unordered_map<std::string, component::IADO_proxy *>;

 public:
  Ado_map() : _map() {}

  bool has_ado_for_pool(const std::string &pool_name) const { return _map.find(pool_name) != _map.end(); }

  void add_ado_for_pool(const std::string &pool_name, gsl::not_null<component::IADO_proxy *> proxy)
  {
    if (_map.emplace(pool_name, proxy).second == false)
      throw Logic_exception("Ado_map: pool (%s) already assigned proxy (%p)", pool_name.c_str(), proxy);
  }

  component::IADO_proxy *get_ado_for_pool(const std::string &pool_name)
  {
    auto i = _map.find(pool_name);
    if (i == _map.end()) throw General_exception("get_ado_for_pool: not found");
    i->second->add_ref();
    return i->second;
  }

  map_t::const_iterator begin() const noexcept { return _map.begin(); }
  map_t::iterator       begin() noexcept { return _map.begin(); }

  map_t::const_iterator end() const noexcept { return _map.end(); }
  map_t::iterator       end() noexcept { return _map.end(); }

  void remove(component::IADO_proxy *proxy)
  {
    for (auto i : _map) {
      if (i.second == proxy) {
        _map.erase(i.first);
        return;
      }
    }
    throw General_exception("Shard:: ado_map::remove proxy interface not found");
  }

 private:
  map_t _map;
};

/**
 * @brief      Manages mapping between pool identifiers and ADO proxy
 */
class Ado_pool_map
    : private std::unordered_map<component::IKVStore::pool_t,
                                 std::tuple<component::IADO_proxy *, Connection_handler *, unsigned>> {
  unsigned _debug_level;

  using map_t = std::unordered_map<component::IKVStore::pool_t,
                                   std::tuple<component::IADO_proxy *, Connection_handler *, unsigned>>;

 public:
  explicit Ado_pool_map(unsigned debug_level_) : map_t(), _debug_level(debug_level_) {}
  void release(const component::IKVStore::pool_t pool)
  {
    if (1 < _debug_level) PLOG("Ado_pool_map: removing mapping (%lx)", pool);

    auto entry = find(pool);
    /* if one, destroy, otherwise decrement reference count */
    if (std::get<2>(entry->second) == 1)
      erase(entry);
    else
      std::get<2>(entry->second)--;
  }

  void add(const component::IKVStore::pool_t pool, component::IADO_proxy *ado, Connection_handler *handler)
  {
    if (1 < _debug_level)
      PLOG("Ado_pool_map: adding mapping (%lx->%p,%p)", pool, static_cast<const void *>(ado),
           static_cast<const void *>(handler));

    auto entry = find(pool);
    if (entry == end())
      emplace(pool, std::make_tuple(ado, handler, 1));
    else
      std::get<2>(entry->second)++;
  }

  using map_t::begin;
  using map_t::end;
  using map_t::find;

  component::IADO_proxy *get_proxy(const component::IKVStore::pool_t pool)
  {
    auto i = map_t::find(pool);
    if (i == map_t::end()) return nullptr;
    auto result = std::get<0>(i->second);
    return result;
  }
};

}  // namespace mcas

#endif  // __MCAS_ADO_MAP_H__
