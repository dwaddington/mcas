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

/*
 * Authors:
 *
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */

#ifndef __MCAS_CLIENT_COMPONENT_H__
#define __MCAS_CLIENT_COMPONENT_H__

#include "connection.h"
#include "mcas_client_config.h"

#include "buffer_manager.h" /* Buffer_manager */

#include <api/components.h>
#include <api/fabric_itf.h>
#include <api/kvindex_itf.h>
#include <api/kvstore_itf.h>
#include <api/mcas_itf.h>
#include <api/itf_ref.h>
#include <common/logging.h>
#include <common/moveable_ptr.h>

#include <boost/optional.hpp>

#include <cstdint> /* uint16_t */
#include <memory>  /* unique_ptr */
#include <string>

class Open_connection {
  common::moveable_ptr<mcas::client::Connection_handler> _open_cnxn;

 public:
  Open_connection() : _open_cnxn(nullptr) {}
  Open_connection(mcas::client::Connection_handler &_connection);
  Open_connection(Open_connection &&) noexcept = default;
  ~Open_connection();
};

class MCAS_client
    : public virtual component::IKVStore
    , public virtual component::IMCAS
    , private common::log_source
{
  friend class MCAS_client_factory;

 private:
  //  static constexpr bool option_DEBUG = true;
#if 0
  using IKVStore = component::IKVStore;
#endif

 protected:
  /**
   * Constructor
   *
   * @param debug_level Debug level (e.g., 0-3)
   * @param owner Owner information (not used)
   * @param addr_port_str Address and port info (e.g. 10.0.0.22:11911)
   * @param device NIC device (e.g., mlx5_0)
   * @param provider fabric provider ("verbs" or "sockets")
   *
   */
 public:
  MCAS_client(unsigned                            debug_level,
              const common::string_view           src_device,
              const common::string_view           src_addr,
              const common::string_view           provider,
              const common::string_view           dest_addr,
              std::uint16_t                       port,
              unsigned                            patience,
              const common::string_view           other = common::string_view() );

  MCAS_client(const MCAS_client &) = delete;
  MCAS_client &operator=(const MCAS_client &) = delete;

  using pool_t = component::IKVStore::pool_t;

  /**
   * Component/interface management
   *
   */
  DECLARE_VERSION(0.1f);

  // clang-format off
  DECLARE_COMPONENT_UUID(0x2f666078, 0xcb8a, 0x4724, 0xa454, 0xd1, 0xd8, 0x8d, 0xe2, 0xdb, 0x87);
  // clang-format on

  void *query_interface(component::uuid_t &itf_uuid) override
  {
    if (itf_uuid == component::IKVStore::iid()) {
      return static_cast<component::IKVStore *>(this);
    }
    else if (itf_uuid == component::IMCAS::iid()) {
      return static_cast<component::IMCAS *>(this);
    }
    else {
      return NULL;  // we don't support this interface
    }
  }

  void unload() override { delete this; }

  /* IMCAS and IKVStore both derive from Registrar_memory_direct.
   * Pick one and use it for registrations.
   */
  Registrar_memory_direct *registrar() { return static_cast<component::IKVStore *>(this); }
 public:
  /* IKVStore (as remote proxy) */
  virtual int thread_safety() const override;

  virtual int get_capability(Capability cap) const override;

  virtual pool_t create_pool(const std::string &  name,
                             const size_t         size,
                             const unsigned int   flags              = 0,
                             const uint64_t       expected_obj_count = 0,
                             const IKVStore::Addr base = IKVStore::Addr{0}) override;

  virtual pool_t open_pool(const std::string &  name,
                           const unsigned int   flags = 0,
                           const IKVStore::Addr base = IKVStore::Addr{0}) override;

  virtual status_t close_pool(const pool_t pool) override;

  virtual status_t delete_pool(const std::string &name) override;

  virtual status_t delete_pool(const IKVStore::pool_t pool) override;

  virtual status_t configure_pool(const component::IKVStore::pool_t pool, const std::string &json) override;

  virtual status_t put(const pool_t       pool,
                       const std::string &key,
                       const void *       value,
                       const size_t       value_len,
                       const unsigned int flags = IMCAS::FLAGS_NONE) override;

  virtual status_t put_direct(const pool_t                 pool,
                              const std::string &          key,
                              const void *                 value,
                              const size_t                 value_len,
                              const IMCAS::memory_handle_t handle = IMCAS::MEMORY_HANDLE_NONE,
                              const unsigned int           flags  = IMCAS::FLAGS_NONE) override;

  virtual status_t async_put(const IKVStore::pool_t pool,
                             const std::string &    key,
                             const void *           value,
                             const size_t           value_len,
                             async_handle_t &       out_handle,
                             const unsigned int     flags = IMCAS::FLAGS_NONE) override;

  virtual status_t async_put_direct(const IKVStore::pool_t          pool,
                                    const std::string &             key,
                                    const void *                    value,
                                    const size_t                    value_len,
                                    async_handle_t &                out_handle,
                                    const IKVStore::memory_handle_t handle = IMCAS::MEMORY_HANDLE_NONE,
                                    const unsigned int              flags  = IMCAS::FLAGS_NONE) override;

  virtual status_t check_async_completion(async_handle_t &handle) override;

  virtual status_t get(const pool_t       pool,
                       const std::string &key,
                       void *&            out_value, /* release with free() */
                       size_t &           out_value_len) override;

  virtual status_t async_get_direct(const pool_t                 pool,
                              const std::string &          key,
                              void *                       out_value,
                              size_t &                     out_value_len,
                              async_handle_t &          out_handle,
                              const IMCAS::memory_handle_t handle = IMCAS::MEMORY_HANDLE_NONE) override;

  virtual status_t get_direct(const pool_t                 pool,
                              const std::string &          key,
                              void *                       out_value,
                              size_t &                     out_value_len,
                              const IMCAS::memory_handle_t handle = IMCAS::MEMORY_HANDLE_NONE) override;

  virtual status_t get_direct_offset(const IMCAS::pool_t          pool,
                                     const offset_t               offset,
                                     size_t &                     length,
                                     void *                       out_buffer,
                                     const IMCAS::memory_handle_t handle) override;

  virtual status_t async_get_direct_offset(const IMCAS::pool_t          pool,
                                           const offset_t               offset,
                                           size_t &                     length,
                                           void *                       out_buffer,
                                           async_handle_t &             out_handle,
                                           const IMCAS::memory_handle_t handle = IMCAS::MEMORY_HANDLE_NONE) override;

  virtual status_t put_direct_offset(const IMCAS::pool_t          pool,
                                     const offset_t               offset,
                                     size_t &                     length,
                                     const void *                 out_buffer,
                                     const IMCAS::memory_handle_t handle) override;

  virtual status_t async_put_direct_offset(const IMCAS::pool_t          pool,
                                           const offset_t               offset,
                                           size_t &                     length,
                                           const void *                 out_buffer,
                                           async_handle_t &             out_handle,
                                           const IMCAS::memory_handle_t handle = IMCAS::MEMORY_HANDLE_NONE) override;

  virtual status_t erase(const pool_t pool, const std::string &key) override;

  virtual status_t async_erase(const IMCAS::pool_t pool, const std::string &key, async_handle_t &out_handle) override;

  virtual size_t count(const pool_t pool) override;

  virtual status_t get_attribute(const IKVStore::pool_t    pool,
                                 const IKVStore::Attribute attr,
                                 std::vector<uint64_t> &   out_attr,
                                 const std::string *       key) override;

  virtual status_t get_statistics(Shard_stats &out_stats) override;

  virtual void debug(const pool_t pool, const unsigned cmd, const uint64_t arg) override;

  virtual IMCAS::memory_handle_t register_direct_memory(void *vaddr, const size_t len) override;

  virtual status_t unregister_direct_memory(const IMCAS::memory_handle_t handle) override;

  virtual status_t free_memory(void *p) override;

  /* IMCAS specific methods */
  virtual status_t find(const IKVStore::pool_t pool,
                        const std::string &    key_expression,
                        const offset_t         offset,
                        offset_t &             out_matched_offset,
                        std::string &          out_matched_key) override;

  virtual status_t invoke_ado(const IKVStore::pool_t            pool,
                              const basic_string_view<byte>     key,
                              const basic_string_view<byte>     request,
                              const uint32_t                    flags,
                              std::vector<IMCAS::ADO_response> &out_response,
                              const size_t                      value_size = 0) override;

  virtual status_t async_invoke_ado(const IMCAS::pool_t               pool,
                                    const basic_string_view<byte>     key,
                                    const basic_string_view<byte>     request,
                                    const ado_flags_t                 flags,
                                    std::vector<IMCAS::ADO_response> &out_response,
                                    async_handle_t &                  out_async_handle,
                                    const size_t                      value_size = 0) override;

  virtual status_t invoke_put_ado(const IKVStore::pool_t            pool,
                                  const basic_string_view<byte>     key,
                                  const basic_string_view<byte>     request,
                                  const basic_string_view<byte>     value,
                                  const size_t                      root_len,
                                  const ado_flags_t                 flags,
                                  std::vector<IMCAS::ADO_response> &out_response) override;
  
  virtual status_t async_invoke_put_ado(const IMCAS::pool_t           pool,
                                        const basic_string_view<byte> key,
                                        const basic_string_view<byte> request,
                                        const basic_string_view<byte> value,
                                        const size_t                  root_len,
                                        const ado_flags_t             flags,
                                        std::vector<ADO_response>&    out_response,
                                        async_handle_t&               out_async_handle) override;

 private:

  component::Itf_ref<component::IFabric_factory>    _factory;
  std::unique_ptr<component::IFabric>               _fabric;
  std::unique_ptr<component::IFabric_endpoint_unconnected> _ep;
  mcas::Buffer_manager<component::IFabric_memory_control> _bm; /* IO buffer manager: must precede opening of connection, which occurs in component::IFabric_client */
  std::unique_ptr<component::IFabric_client>        _transport;
  std::unique_ptr<mcas::client::Connection_handler> _connection;
  Open_connection                                   _open_connection;

 private:
  static void set_debug(unsigned debug_level, const void *ths, const std::string &ip_addr, std::uint16_t port);
  static auto load_factory() -> component::IFabric_factory *;
  /* make fabric: address/provider/device form */
  static auto make_fabric_apd(component::IFabric_factory &,
                          const common::string_view ip_addr,
                          const common::string_view provider,
                          const common::string_view device) -> component::IFabric *;
  /* make fabric: source/ip_addr/provider */
  static auto make_fabric_sip(component::IFabric_factory &,
                          const common::string_view src_addr,
                          const common::string_view interface,
                          const common::string_view provider) -> component::IFabric *;

  void open_transport(const std::string &device,
                      const std::string &ip_addr,
                      const int          port,
                      const std::string &provider);
};

class MCAS_client_factory : public component::IMCAS_factory {
  using string_view = common::string_view;
 public:
  /**
   * Component/interface management
   *
   */
  DECLARE_VERSION(0.1f);

  // clang-format off
  DECLARE_COMPONENT_UUID(0xfac66078, 0xcb8a, 0x4724, 0xa454, 0xd1, 0xd8, 0x8d, 0xe2, 0xdb, 0x87);
  // clang-format on

  void *query_interface(component::uuid_t &itf_uuid) override
  {
    if (itf_uuid == component::IMCAS_factory::iid()) {
      return static_cast<component::IMCAS_factory *>(this);
    }
    else if (itf_uuid == component::IKVStore_factory::iid()) {
      return static_cast<component::IKVStore_factory *>(this);
    }
    else
      return NULL;  // we don't support this interface
  }

  void unload() override { delete this; }

  /* NIC/souuce/dest version of create */
  component::IMCAS *mcas_create_nsd(unsigned          debug_level,
                                unsigned          patience,
                                const string_view owner,
                                const string_view src_nic_device,
                                const string_view src_ip_addr,
                                const string_view dest_addr_with_port,
                                const string_view other) override;

  component::IKVStore *create(unsigned           debug_level,
                              const string_view owner,
                              const string_view addr,
                              const string_view device) override;

  component::IKVStore *create(unsigned debug_level, const std::map<std::string, std::string> &) override;
};

#endif
