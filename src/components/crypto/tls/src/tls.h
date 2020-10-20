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

/*
 * Authors:
 *
 * Daniel G. Waddington (daniel.waddington@ibm.com)
 *
 */

#ifndef __MCAS_CRYPTO_COMPONENT_H__
#define __MCAS_CRYPTO_COMPONENT_H__

#include <memory>
#include <set>
#include <api/components.h>
#include <api/crypto_itf.h>

class Crypto_server_state;

class Crypto : public virtual component::ICrypto {
  friend class Crypto_factory;

 protected:
  Crypto(const unsigned debug_level);

 public:
  /**
   * Destructor
   *
   */
  virtual ~Crypto() {
    PNOTICE("~Crypto(): %p", reinterpret_cast<void*>(this));
  }

  /**
   * Component/interface management
   *
   */
  DECLARE_VERSION(0.1f);

  // clang-format off
  DECLARE_COMPONENT_UUID(0x8c4636c7, 0x84bd, 0x4f88, 0xa6a8, 0x98, 0x53, 0x65, 0x4b, 0xc4, 0xbd);
  // clang-format on

  void* query_interface(component::uuid_t& itf_uuid) override
  {
    return (itf_uuid == component::ICrypto::iid()) ? static_cast<component::ICrypto*>(this) : NULL;
  }

  void unload() override { delete this; }

 public:
  // ICrypto
  status_t initialize(const std::string& cipher_suite,
                      const std::string& cert_file,
                      const std::string& key_file) override;

  session_t accept_psk_session(const int                                                     port,
                               std::function<const std::string(const std::string& username)> get_shared_key) override;

  session_t open_psk_session(const std::string& server_ip,
                             const int          server_port,
                             const std::string& username,
                             const std::string& key) override;

  session_t accept_cert_session(const std::string& ip_addr,
                                const int port,
                                const unsigned int timeout_ms) override;

  session_t open_cert_session(const std::string& cipher_suite,
                              const std::string& server_ip,
                              const int          server_port,
                              const std::string& username,
                              const std::string& cert_file,
                              const std::string& key_file) override;

  status_t close_session(const session_t session) override;

  status_t export_key(const session_t    session,
                      const std::string& label,
                      const std::string& context,
                      const size_t       out_size,
                      void*              out_key) override;

  status_t export_key(const session_t    session,
                      const std::string& label,
                      const std::string& context,
                      const size_t       out_size,
                      std::string&       out_key) override;

  status_t hmac(component::ICrypto::mac_algorithm_t algo,
                const std::string&                  key,
                const void*                         in_data,
                const size_t                        in_data_size,
                std::string&                        out_digest) override;

  status_t initialize_cipher(const session_t session, const cipher_t cipher, const std::string key) override;

  status_t aead_encrypt(const session_t session,
                        const void*     nonce,
                        size_t          nonce_len,
                        const void*     auth,
                        size_t          auth_len,
                        const void*     plain_text,
                        size_t          plain_text_len,
                        void*           out_cipher_text,
                        size_t*         out_cipher_text_len) override;

  status_t aead_decrypt(const session_t session,
                        const void*     nonce,
                        size_t          nonce_len,
                        const void*     auth,
                        size_t          auth_len,
                        const void*     cipher_text,
                        size_t          cipher_text_len,
                        void*           out_plain_text,
                        size_t*         out_plain_text_len) override;

  ssize_t record_send(const session_t session, const void* data, size_t data_len) override;

  ssize_t record_recv(const session_t session, void* data, size_t data_len) override;

 private:
  unsigned                             _debug_level;
  std::shared_ptr<Crypto_server_state> _state;
  std::set<session_t>                  _sessions;
};

class Crypto_factory : public component::ICrypto_factory {
 public:
  /**
   * Component/interface management
   *
   */
  DECLARE_VERSION(0.1f);

  // clang-format off
  DECLARE_COMPONENT_UUID(0xfac636c7, 0x84bd, 0x4f88, 0xa6a8, 0x98, 0x53, 0x65, 0x4b, 0xc4, 0xbd);
  // clang-format on

  void* query_interface(component::uuid_t& itf_uuid) override
  {
    return (itf_uuid == component::ICrypto_factory::iid()) ? static_cast<component::ICrypto_factory*>(this) : NULL;
  }

  void unload() override { delete this; }

// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Winconsistent-missing-override"
  component::ICrypto* create(unsigned debug_level, std::map<std::string, std::string>& /*params*/) override
  {
    auto obj = static_cast<component::ICrypto*>(new Crypto(debug_level));
    obj->add_ref();
    return obj;
  }
// #pragma GCC diagnostic pop
};

#endif
