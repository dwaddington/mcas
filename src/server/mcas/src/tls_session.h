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
#ifndef __CONNECTION_TLS_SESSION_H__
#define __CONNECTION_TLS_SESSION_H__

/** 
 * Class for TLS session handling
 * 
 */
#include <string>
#include <cstring>
#include <unistd.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/crypto.h>
#include <common/exceptions.h>
#include <common/logging.h>
#include <common/byte_buffer.h>
#include <boost/numeric/conversion/cast.hpp>

#include "connection_state.h"
#include "buffer_manager.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Weffc++"

#define CIPHER_SUITE                            \
  "NORMAL:+AEAD"  // PERFORMANCE:+AEAD"
                  // //"PERFORMANCE:-VERS-SSL3.0:-VERS-TLS1.0:-VERS-TLS1.1:-ARCFOUR-128:-PSK:-DHE-PSK:+AEAD"


namespace mcas
{

struct TLS_transport
{
  static unsigned debug_level();
  static ssize_t gnutls_pull_func(gnutls_transport_ptr_t connection, void* buffer, size_t buffer_size);
  static ssize_t gnutls_vec_push_func(gnutls_transport_ptr_t, const giovec_t * , int );
  static int gnutls_pull_timeout_func(gnutls_transport_ptr_t, unsigned int ms);
};


class Connection_handler;

class Connection_TLS_session : private common::log_source
{
  struct security_options_t {
    security_options_t() : ipaddr(),port(0),tls(false),hmac(false) {}
    std::string ipaddr; // interface to bind to
    unsigned    port; // port to bind to
    bool        tls;
    bool        hmac;
    std::string cipher_suite;
    std::string cert_file;
    std::string key_file;

    void print() const {
      PINF("----TLS SECURITY OPTIONS---");
      PINF("ipaddr  : %s", ipaddr.c_str());
      PINF("port    : %u", port);
      PINF("tls     : %s", tls ? "y" : "n");
      PINF("hmac    : %s", hmac ? "y" : "n");
      PINF("cert    : %s", cert_file.c_str());
      PINF("key     : %s", key_file.c_str());
      PINF("--------------------------");
    }
  };

protected:

  Connection_TLS_session(unsigned debug_level,
                         Connection_handler * handler)
    : common::log_source(debug_level),
      _connection(handler)
      , _aead_key{}
  {}
 
  Connection_state process_tls_session();

  virtual ~Connection_TLS_session();

  void set_security_options(bool tls, bool hmac) {
    _security_options.tls = tls;
    _security_options.hmac = hmac;
  }

  void set_security_binding(const std::string& ipaddr, const unsigned port)
  {
    _security_options.ipaddr = ipaddr;
    _security_options.port = port;
  }

  void set_security_params(const std::string& cert_file, const std::string& key_file)
  {
    _security_options.cert_file = cert_file;
    _security_options.key_file = key_file;
  }
    


private:
  void initialize_cipher(gnutls_cipher_algorithm_t cipher, const std::string& key_str)
  {
    _aead_key.size = boost::numeric_cast<unsigned int>(key_str.size());
    _aead_key.data = new unsigned char[_aead_key.size];
    std::memcpy(_aead_key.data, key_str.data(), _aead_key.size);

    int rc;
    if ((rc = gnutls_aead_cipher_init(&_cipher_handle, cipher, &_aead_key)) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_aead_cipher_init() failed (%d) %s", rc, gnutls_strerror(rc));
  }

  void initialize_certs();

private:

  Connection_handler *             _connection;
  security_options_t               _security_options;
  gnutls_session_t                 _session = nullptr;
  gnutls_datum_t                   _aead_key;
  gnutls_aead_cipher_hd_t          _cipher_handle;
  gnutls_certificate_credentials_t _x509_cred;
  gnutls_priority_t                _priority;
  
  // Buffer_manager<component::IFabric_server>::buffer_internal *
  // _posted_handshake_recv_buffer;

};


}

#pragma GCC diagnostic pop

#endif

