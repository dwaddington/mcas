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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <gnutls/crypto.h>
#include <libbase64.h>
#include <common/logging.h>
#include <common/exceptions.h>
#include <boost/numeric/conversion/cast.hpp>

#include "tls.h"

#define CAFILE "/etc/ssl/certs/ca-bundle.trust.crt"
//#define X5096_OID_EMAIL "1.2.840.113549.1.9.1"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Weffc++"

/* The OCSP status file contains up to date information about revocation
 * of the server's certificate. That can be periodically be updated
 * using:
 * $ ocsptool --ask --load-cert your_cert.pem --load-issuer your_issuer.pem
 *            --load-signer your_issuer.pem --outfile ocsp-status.der
 */
#define OCSP_STATUS_FILE "ocsp-status.der"

static void print_info(gnutls_session_t session);
static void print_logs(int level, const char* msg);

using namespace component;

/* static constructor called once */
static void __attribute__((constructor)) Global_ctor()
{
  gnutls_global_init();
  gnutls_global_set_log_level(0);
  gnutls_global_set_log_function(print_logs);
}

static void __attribute__((destructor)) Global_dtor() { gnutls_global_deinit(); }

/**
 * @brief      Shared state for all sessions under this component instance
 */
class Crypto_server_state {
public:
  Crypto_server_state() {}

  virtual ~Crypto_server_state()
  {
    gnutls_certificate_free_credentials(_x509_cred);
    gnutls_priority_deinit(_priority);
  }

  status_t initialize(const std::string& cipher_suite,
                      const std::string& cert_file, /* e.g. cert.pem */
                      const std::string& key_file)  /* e.g. key.pem */
  {
    int err;
    assert(!cipher_suite.empty());
    assert(!cert_file.empty());
    assert(!key_file.empty());

    PLOG("Server:%s", cipher_suite.c_str());

    /* initialize credentials amd trust certificates etc. */
    if ((err = gnutls_certificate_allocate_credentials(&_x509_cred)) != GNUTLS_E_SUCCESS) {
      PERR("%s", gnutls_strerror(err));
      throw General_exception("crypto-engine: gnutls_certificate_allocate_credentials() failed");
    }

    if ((err = gnutls_certificate_set_x509_trust_file(_x509_cred, CAFILE, GNUTLS_X509_FMT_PEM)) != GNUTLS_E_SUCCESS) {
      PERR("%s", gnutls_strerror(err));
      throw General_exception("crypto-engine: gnutls_certificate_set_x509_trust_file() failed");
    }

    if ((err = gnutls_certificate_set_x509_key_file(_x509_cred,
                                                    cert_file.c_str(),
                                                    key_file.c_str(),
                                                    GNUTLS_X509_FMT_PEM)) !=
        GNUTLS_E_SUCCESS) {
      PERR("%s", gnutls_strerror(err));
      throw General_exception("crypto-engine: gnutls_certificate_set_x509_key_file() "
                              "failed (key file=%s) (cert file=%s)",                              
                              key_file.c_str(), cert_file.c_str());
    }


    /* CRL file defines expired and black listed certificates
       gnutls_certificate_set_x509_crl_file(x509_cred, CRLFILE, GNUTLS_X509_FMT_PEM));

       Send OCP response
       gnutls_certificate_set_ocsp_status_request_file()
    */

    /* Instead of the default options as shown above one could specify
     * additional options such as server precedence in ciphersuite selection
     * as follows:
     * gnutls_priority_init2(&priority_cache,
     *                       "%SERVER_PRECEDENCE",
     *                       NULL, GNUTLS_PRIORITY_INIT_DEF_APPEND);
     */
    if (gnutls_priority_init(&_priority, cipher_suite.c_str(), NULL) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_priority_init() failed");

#if GNUTLS_VERSION_NUMBER >= 0x030506
    if (gnutls_certificate_set_known_dh_params(_x509_cred, GNUTLS_SEC_PARAM_MEDIUM) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_certificate_set_known_dh_params() failed");
#endif
    
    return S_OK;
  }

public:
  gnutls_certificate_credentials_t _x509_cred;
  gnutls_priority_t                _priority;
};

/**
 * @brief      Base class for both client and server sessions
 */
class Crypto_session_base : public ICrypto::Crypto_session {
public:
  /**
   * @brief      Base constructor
   *
   * @param[in]  is_server  Indicates if server
   * @param[in]  status     Status
   */
  Crypto_session_base(bool is_server, status_t status = S_OK) : _status(status), _is_server(is_server)
  {
    _aead_key.size = 0;
  }

  /**
   * @brief      Base destructor
   */
  virtual ~Crypto_session_base()
  {
    if (_aead_key.size > 0) {
      delete[] _aead_key.data;
      gnutls_aead_cipher_deinit(_cipher_handle);
    }
  }

  bool is_server_side() const override { return _is_server; }

  /**
   * @brief      Get hold of session handle.
   *
   * @return     Session handle
   */
  gnutls_session_t get_session() const { return _session; }

  /**
   * @brief      Initializes the cipher.
   *
   * @param[in]  cipher   The cipher
   * @param[in]  key_str  The key string (size matching chosen cipher)
   */
  void initialize_cipher(gnutls_cipher_algorithm_t cipher, const std::string& key_str)
  {
    _aead_key.size = boost::numeric_cast<unsigned int>(key_str.size());
    _aead_key.data = new unsigned char[_aead_key.size];
    memcpy(_aead_key.data, key_str.data(), _aead_key.size);

    int rc = gnutls_aead_cipher_init(&_cipher_handle, cipher, &_aead_key);
    if (rc != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_aead_cipher_init() failed (%d) %s", rc, gnutls_strerror(rc));
  }

  /**
   * @brief      Gets the aead cipher handle
   *
   * @return     Cipher handle
   */
  const gnutls_aead_cipher_hd_t& get_cipher_handle() const
  {
    assert(_cipher_handle);
    return _cipher_handle;
  }

  /** 
   * @brief Get client unique identifier
   * 
   * @return Base64 encoded client UUID
   */
  const std::string& client_uuid() const
  {
    return _x509_serial_base64;
  }

protected:
  gnutls_session_t        _session;
  int                     _sd     = 0;
  status_t                _status = S_OK;
  const bool              _is_server;
  gnutls_datum_t          _aead_key;
  gnutls_aead_cipher_hd_t _cipher_handle;
  std::string             _x509_serial_base64;
};

/**
 * @brief      Client-side for cert+key session
 */
class Cert_client_session : public Crypto_session_base {
public:
  Cert_client_session(unsigned           debug_level,
                      const std::string& cipher_suite,
                      const std::string& server_ip,
                      const int          server_port,
                      const std::string& username,
                      const std::string& cert_file,
                      const std::string& key_file)
    : Crypto_session_base(false)
  {
    // assert(gnutls_check_version("3.4.6"));
    if (gnutls_global_init() != GNUTLS_E_SUCCESS) throw General_exception("cert-client: gnutls_global_init() failed");

    if (gnutls_certificate_allocate_credentials(&_xcred) != GNUTLS_E_SUCCESS)
      throw General_exception("cert-client: gnutls_certificate_allocate_credentials() failed");

    // if(gnutls_certificate_set_x509_system_trust(_xcred) != GNUTLS_E_SUCCESS)
    //  throw General_exception("cert-client: gnutls_certificate_set_x509_system_trust() failed");

    if (gnutls_certificate_set_x509_key_file(_xcred, cert_file.c_str(), key_file.c_str(), GNUTLS_X509_FMT_PEM) !=
        GNUTLS_E_SUCCESS)
      throw General_exception("cert-client: gnutls_certificate_set_x509_key_file() failed");

    if (gnutls_init(&_session, GNUTLS_CLIENT) != GNUTLS_E_SUCCESS)
      throw General_exception("cert-client: gnutls_init() failed");

    if (debug_level > 0) PLOG("Client:%s", cipher_suite.c_str());

    if (gnutls_priority_init(&_priority, cipher_suite.c_str(), NULL) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_priority_init() failed");

    if (gnutls_priority_set(_session, _priority) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_priority_set() failed");

    if (gnutls_credentials_set(_session, GNUTLS_CRD_CERTIFICATE, _xcred) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_credentials_set() failed");

    /* show some certificate info */
    {
      gnutls_x509_crt_t* crt_list;
      unsigned           crt_list_size = 0;
      int                rc            = gnutls_certificate_get_x509_crt(_xcred, 0, &crt_list, &crt_list_size);
      assert(rc == 0);
      char   dn[128];
      size_t dn_size = 128;
      rc             = gnutls_x509_crt_get_dn(crt_list[0], dn, &dn_size);
      assert(rc == 0);

      if (debug_level > 0) PLOG("Local Cert DN:%s", dn);
    }

    //    gnutls_session_set_verify_cert(session, "www.example.com", 0);

    int                err, ret;
    struct sockaddr_in sa;

    /* connect to server */
    _sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&sa, '\0', sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = boost::numeric_cast<in_port_t>(htons(server_port));
    inet_pton(AF_INET, server_ip.c_str(), &sa.sin_addr);

    err = connect(_sd, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa));
    if (err < 0) throw General_exception("cert-client: connect failed()");

    gnutls_transport_set_int(_session, _sd);
    gnutls_handshake_set_timeout(_session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

    /* perform the TLS handshake */
    do {
      ret = gnutls_handshake(_session);
    } while (ret < 0 && gnutls_error_is_fatal(ret) == 0);

    if (ret < 0) {
      if (ret == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR) {
        /* check certificate verification status */
        gnutls_datum_t out;
        auto           type   = gnutls_certificate_type_get(_session);
        auto           status = gnutls_session_get_verify_cert_status(_session);
        if (gnutls_certificate_verification_status_print(status, type, &out, 0) != GNUTLS_E_SUCCESS)
          throw General_exception("gnutls_certificate_verification_status_print() failed");
        PLOG("Cert verify output: %s\n", out.data);
        gnutls_free(out.data);
      }
      throw General_exception("Client: handshake failed: %s\n", gnutls_strerror(ret));
    }

    if (debug_level > 0) PMAJOR("TLS handshake OK!");

    /* examine peer (server) x509 certificate */
    {
      unsigned              list_size = 0;
      const gnutls_datum_t* der_data  = gnutls_certificate_get_peers(_session, &list_size);
      assert(der_data);

      gnutls_x509_crt_t cert;
      gnutls_x509_crt_init(&cert);
      /* first in list is the peer's certificate, we need to convert from raw DER  */
      if (gnutls_x509_crt_import(cert, der_data, GNUTLS_X509_FMT_DER) != GNUTLS_E_SUCCESS)
        throw General_exception("Client: gnutls_x509_crt_import() failed");

      char   dn[128];
      size_t dn_size = 128;
      if (gnutls_x509_crt_get_dn(cert, dn, &dn_size) != GNUTLS_E_SUCCESS)
        throw General_exception("gnutls_x509_crt_get_dn() failed");

      if (debug_level > 0) PLOG("Server's Cert DN:%s", dn);
    }
  }

  status_t shutdown()
  {
    if (gnutls_bye(_session, GNUTLS_SHUT_WR) != GNUTLS_E_SUCCESS) PWRN("gnutls_bye() failed");

    /* close socket */
    ::shutdown(_sd, SHUT_RDWR);
    ::close(_sd);

    gnutls_deinit(_session);
    gnutls_certificate_free_credentials(_xcred);
    gnutls_global_deinit();
    return S_OK;
  }

private:
  gnutls_certificate_credentials_t _xcred;
  gnutls_priority_t                _priority;
};

/**
 * @brief      Server-side for cert+key session with a client
 */
class Cert_server_session : public Crypto_session_base,
                            private common::log_source
{
public:
  Cert_server_session() = delete;

  Cert_server_session(unsigned debug_level,
                      const std::shared_ptr<Crypto_server_state> state,
                      const std::string& ipaddr,
                      int port)
    : Crypto_session_base(true, S_OK), common::log_source(debug_level), _state(state)
  {
    struct sockaddr_in sa_serv;
    struct sockaddr_in sa_cli;
    int                optval = 1, ret = 0;
    socklen_t          client_len = sizeof(sa_cli);

    auto listen_sd = socket(AF_INET, SOCK_STREAM, 0);
    memset(&sa_serv, '\0', sizeof(sa_serv));

    if(ipaddr.empty())
      sa_serv.sin_family = AF_INET;
    else
      sa_serv.sin_family = inet_addr(ipaddr.c_str());
        
    sa_serv.sin_addr.s_addr = INADDR_ANY;
    sa_serv.sin_port        = boost::numeric_cast<in_port_t>(htons(port));

    if (setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, static_cast<void*>(&optval), sizeof(int)) != 0)
      throw General_exception("setsockopt() failed");

    if (bind(listen_sd, reinterpret_cast<struct sockaddr*>(&sa_serv), sizeof(sa_serv)) != 0)
      throw General_exception("bind() failed");

    if (listen(listen_sd, 1024) != 0)
      throw General_exception("list() failed");

    /* initialize session */
    if (gnutls_init(&_session, GNUTLS_SERVER) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_init() failed");

    if (gnutls_priority_set(_session, _state->_priority) != GNUTLS_E_SUCCESS) /* set cipher suite */
      throw General_exception("gnutls_priority_set() failed");

    if (gnutls_credentials_set(_session, GNUTLS_CRD_CERTIFICATE, _state->_x509_cred) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_credentials_set() failed");

    /* request that client provides certificate to identify themselves */
    gnutls_certificate_server_set_request(_session, GNUTLS_CERT_REQUIRE);  // GNUTLS_CERT_IGNORE);

    // only need if you want to inform server of host name
    // gnutls_server_name_set(session, GNUTLS_NAME_DNS, "www.example.com", strlen("www.example.com"))

    gnutls_handshake_set_timeout(_session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

    char topbuf[512];
    _sd = accept(listen_sd, reinterpret_cast<struct sockaddr*>(&sa_cli), &client_len);

    CPLOG(1, "- connection from %s, port %d\n", inet_ntop(AF_INET, &sa_cli.sin_addr, topbuf, sizeof(topbuf)),
          ntohs(sa_cli.sin_port));

    gnutls_transport_set_int(_session, _sd);

    /* perform handshake */
    if (gnutls_handshake(_session) < 0) {
      ::close(_sd);
      _sd = 0;
      gnutls_deinit(_session);
      PWRN("Client: handshake has failed (%s)\n\n", gnutls_strerror(ret));
      return;
    }

    /* examine peer (client) x509 certificate */
    {
      unsigned              list_size = 0;
      const gnutls_datum_t* der_data  = gnutls_certificate_get_peers(_session, &list_size);
      assert(der_data);

      gnutls_x509_crt_t cert;
      gnutls_x509_crt_init(&cert);
      /* first in list is the peer's certificate, we need to convert from raw DER  */
      if (gnutls_x509_crt_import(cert, der_data, GNUTLS_X509_FMT_DER) != GNUTLS_E_SUCCESS)
        throw General_exception("Client: gnutls_x509_crt_import() failed");

      char   dn[128];
      size_t dn_len = sizeof(dn);
      if (gnutls_x509_crt_get_dn(cert, dn, &dn_len) != GNUTLS_E_SUCCESS)
        throw General_exception("gnutls_x509_crt_get_dn() failed");
      
      CPLOG(1, "Client Cert DN:%s", dn);

      char serial[40];
      size_t serial_len = sizeof(serial);

      /* initially use certificate serial number as a unique identifier
         see https://www.ietf.org/rfc/rfc2459.txt */
      if(gnutls_x509_crt_get_serial(cert, serial, &serial_len) != GNUTLS_E_SUCCESS)
        throw General_exception("gnutls_x509_crt_get_serial() failed");

      /* base64 encode it for easier debugging */
      char serial_base64[40];
      size_t serial_base64_len = sizeof(serial_base64);
      base64_encode(serial, serial_len, reinterpret_cast<char*>(&serial_base64), &serial_base64_len, 0);
      assert(serial_base64_len > 0);
      _x509_serial_base64.assign(serial_base64, serial_base64_len);

      CPINF(1, "Client serial: %s", _x509_serial_base64.c_str());

      // fingerprint extraction
      // char fingerprint[256];
      // size_t fingerprint_len = sizeof(fingerprint);
      // if(gnutls_x509_crt_get_fingerprint(cert,
      //                                    GNUTLS_DIG_SHA256,
      //                                    fingerprint, &fingerprint_len) != GNUTLS_E_SUCCESS)
      //   throw General_exception("gnutls_x509_crt_get_fingerprint() failed");
      // PINF("Fingerpint: %lu bytes long (%s)", fingerprint_len, fingerprint);

    }

    print_info(_session);
  }

  status_t shutdown()
  {
    if (gnutls_bye(_session, GNUTLS_SHUT_WR) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_bye() failed");

    /* close socket */
    ::shutdown(_sd, SHUT_RDWR);
    ::close(_sd);
    _sd = 0;

    gnutls_deinit(_session);
    return S_OK;
  }

  /** 
   * Provide a unique identifier for the client.  Session tokens will be 
   * associated with this UUID for access control to pools.
   * 
   * 
   * @return UUID in base64
   */
  std::string uuid() const { return _x509_serial_base64; }

private:
  const std::shared_ptr<Crypto_server_state> _state;
};

/* Crypto interface methods */

Crypto::Crypto(const unsigned debug_level) : _debug_level(debug_level)
{
  _state = std::make_shared<Crypto_server_state>();
  gnutls_global_set_log_level(debug_level); /* 9 is most verbose */
}

status_t Crypto::initialize(const std::string& cipher_suite, const std::string& cert_file, const std::string& key_file)
{
  return _state->initialize(cipher_suite, cert_file, key_file);
}

ICrypto::session_t Crypto::accept_psk_session(const int                                                     port,
                                              std::function<const std::string(const std::string& username)> key_lookup)
{
  assert(port > 0);
  return nullptr;
}

ICrypto::session_t Crypto::open_psk_session(const std::string& server_ip,
                                            const int          server_port,
                                            const std::string& username,
                                            const std::string& key)
{
  return nullptr;
}

ICrypto::session_t Crypto::accept_cert_session(const std::string& ip_addr,
                                               const int port)
{
  auto session = new Cert_server_session(_debug_level, _state, ip_addr, port);
  _sessions.insert(session);
  return session;
}

ICrypto::session_t Crypto::open_cert_session(const std::string& cipher_suite,
                                             const std::string& server_ip,
                                             const int          server_port,
                                             const std::string& username,
                                             const std::string& cert_file,
                                             const std::string& key_file)
{
  auto session =
    new Cert_client_session(_debug_level, cipher_suite, server_ip, server_port, username, cert_file, key_file);
  _sessions.insert(session);
  return session;
}

status_t Crypto::close_session(const session_t session)
{
  if (_sessions.count(session) != 1) {
    PWRN("invalid parameter to Crypto::close_session");
    return E_INVAL;
  }

  auto rc = session->shutdown();
  _sessions.erase(session);
  return rc;
}

status_t Crypto::export_key(const session_t    session,
                            const std::string& label,
                            const std::string& context,
                            const size_t       out_size,
                            void*              out_key)
{
  if (_sessions.count(session) != 1) {
    PWRN("invalid parameter to Crypto::export_key");
    return E_INVAL;
  }

  gnutls_session_t tls_session = reinterpret_cast<Crypto_session_base*>(session)->get_session();
  assert(tls_session);

  const char* context_str = nullptr;
  size_t      context_len = 0;
  if (!context.empty()) {
    context_str = context.c_str();
    context_len = context.size();
  }

  if (gnutls_prf_rfc5705(tls_session, label.size(), label.c_str(), context_len, context_str, out_size,
                         reinterpret_cast<char*>(out_key)) != GNUTLS_E_SUCCESS) {
    PWRN("gnutls_prf_rfc5705() failed");
    return E_FAIL;
  }

  return S_OK;
}

status_t Crypto::export_key(const session_t    session,
                            const std::string& label,
                            const std::string& context,
                            const size_t       out_size,
                            std::string&       out_key)
{
  if (out_size == 0) return E_INVAL;
  out_key.resize(out_size, 0);
  void* out = const_cast<char*>(out_key.data()); /* would be ok in C++17 */
  return export_key(session, label, context, out_size, out);
}

static gnutls_mac_algorithm_t convert_algorithm_enum(component::ICrypto::mac_algorithm_t algo)
{
  switch (algo) {
  case ICrypto::MAC_MD5:
    return GNUTLS_MAC_MD5;
  case ICrypto::MAC_SHA1:
    return GNUTLS_MAC_SHA1;
  case ICrypto::MAC_RMD160:
    return GNUTLS_MAC_RMD160;
  case ICrypto::MAC_MD2:
    return GNUTLS_MAC_MD2;
  case ICrypto::MAC_SHA256:
    return GNUTLS_MAC_SHA256;
  case ICrypto::MAC_SHA384:
    return GNUTLS_MAC_SHA384;
  case ICrypto::MAC_SHA512:
    return GNUTLS_MAC_SHA512;
  case ICrypto::MAC_SHA224:
    return GNUTLS_MAC_SHA224;
  case ICrypto::MAC_AEAD:
    return GNUTLS_MAC_AEAD;
  default:
    throw General_exception("invalid digest");
  }
  return GNUTLS_MAC_NULL;
}

status_t Crypto::hmac(component::ICrypto::mac_algorithm_t algo,
                      const std::string&                  key,
                      const void*                         in_data,
                      const size_t                        in_data_size,
                      std::string&                        out_digest)
{
  auto calgo      = convert_algorithm_enum(algo);
  auto digest_len = gnutls_hmac_get_len(calgo);
  if (digest_len == 0) return E_INVAL;
  out_digest.resize(digest_len, 0);
  void* out = const_cast<char*>(out_digest.data()); /* would be ok in C++17 */
  if (gnutls_hmac_fast(calgo, key.data(), key.size(), in_data, in_data_size, out) != GNUTLS_E_SUCCESS) {
    PWRN("gnutls_hmac_fast() failed");
    return E_INVAL;
  }
  return S_OK;
}

status_t Crypto::initialize_cipher(const session_t session, const cipher_t cipher, const std::string key)
{
  if (_sessions.count(session) != 1) {
    PWRN("invalid parameter to Crypto::export_key");
    return E_INVAL;
  }

  switch (cipher) {
  case CIPHER_AES_128_GCM: {
    if (key.size() != 16) {
      PWRN("invalid key size (%lu)", key.size());
      return E_INVAL;
    }
    reinterpret_cast<Crypto_session_base*>(session)->initialize_cipher(GNUTLS_CIPHER_AES_128_GCM, key);
    break;
  }
  case CIPHER_AES_256_GCM: {
    if (key.size() != 32) {
      PWRN("invalid key size (%lu)", key.size());
      return E_INVAL;
    }
    reinterpret_cast<Crypto_session_base*>(session)->initialize_cipher(GNUTLS_CIPHER_AES_256_GCM, key);
    break;
  }
  default:
    PWRN("invalid cipher");
    return E_INVAL;
  }
  return S_OK;
}

status_t Crypto::aead_encrypt(const session_t session,
                              const void*     nonce,
                              size_t          nonce_len,
                              const void*     auth,
                              size_t          auth_len,
                              const void*     plain_text,
                              size_t          plain_text_len,
                              void*           out_cipher_text,
                              size_t*         out_cipher_text_len)
{
  if (_sessions.count(session) != 1) {
    PWRN("invalid parameter to Crypto::aead_encrypt");
    return E_INVAL;
  }

  /* This function will encrypt the given data using the algorithm
     specified by the context. The output data will contain the authentication tag. */

  auto rc = gnutls_aead_cipher_encrypt(reinterpret_cast<Crypto_session_base*>(session)->get_cipher_handle(), nonce,
                                       nonce_len, auth, auth_len,
                                       0,  // tag_size use default
                                       plain_text, plain_text_len, out_cipher_text, out_cipher_text_len);
  if (rc != GNUTLS_E_SUCCESS) {
    PWRN("Crypto::aead_encrypt gnutls_aead_cipher_encrypt() failed (%d) %s", rc, gnutls_strerror(rc));
    return E_FAIL;
  }
  return S_OK;
}

status_t Crypto::aead_decrypt(const session_t session,
                              const void*     nonce,
                              size_t          nonce_len,
                              const void*     auth,
                              size_t          auth_len,
                              const void*     cipher_text,
                              size_t          cipher_text_len,
                              void*           out_plain_text,
                              size_t*         out_plain_text_len)
{
  if (_sessions.count(session) != 1) {
    PWRN("invalid parameter to Crypto::aead_decrypt");
    return E_INVAL;
  }

  auto rc = gnutls_aead_cipher_decrypt(reinterpret_cast<Crypto_session_base*>(session)->get_cipher_handle(), nonce,
                                       nonce_len, auth, auth_len,
                                       0,  // tag_size use default
                                       cipher_text, cipher_text_len, out_plain_text, out_plain_text_len);
  if (rc != GNUTLS_E_SUCCESS) {
    PWRN("Crypto::aead_decrypt gnutls_aead_cipher_decrypt() failed (%d) %s", rc, gnutls_strerror(rc));
    return E_FAIL;
  }
  return S_OK;
}

ssize_t Crypto::record_send(const session_t session, const void* data, size_t data_len)
{
  if (_sessions.count(session) != 1) {
    PWRN("invalid parameter to Crypto::record_send");
    return E_INVAL;
  }

  gnutls_session_t tls_session = reinterpret_cast<Crypto_session_base*>(session)->get_session();
  assert(tls_session);
  auto rc = gnutls_record_send(tls_session, data, data_len);
  if (rc < 0)
    PWRN("Crypto::record_send gnutls_record_send() failed (%ld) %s", rc, gnutls_strerror(boost::numeric_cast<int>(rc)));
  return rc;
}

ssize_t Crypto::record_recv(const session_t session, void* data, size_t data_len)
{
  if (_sessions.count(session) != 1) {
    PWRN("invalid parameter to Crypto::record_recv");
    return E_INVAL;
  }

  gnutls_session_t tls_session = reinterpret_cast<Crypto_session_base*>(session)->get_session();
  assert(tls_session);
  auto rc = gnutls_record_recv(tls_session, data, data_len);
  if (rc < 0)
    PWRN("Crypto::record_recv gnutls_record_recv() failed (%ld) %s", rc, gnutls_strerror(boost::numeric_cast<int>(rc)));

  return rc;
}

/* - end of methods ----------- */

extern "C" void* factory_createInstance(component::uuid_t component_id)
{
  if (component_id == Crypto_factory::component_id()) {
    auto fact = new Crypto_factory();
    fact->add_ref();
    return static_cast<void*>(fact);
  }
  else {
    PWRN("request for bad factory type");
    return NULL;
  }
}

#if 0
static void print_x509_certificate_info (gnutls_session_t session)
{
  char dn[128];
  char digest[20];
  //char serial[40];
  size_t dn_len = sizeof (dn);
  size_t digest_size = sizeof (digest);
  //size_t serial_size = sizeof (serial);
  time_t expiret, activet;
  //int algo;
  unsigned i;
  //unsigned bits;
  unsigned cert_list_size = 0;
  const gnutls_datum_t *cert_list;
  gnutls_x509_crt_t cert;
  
  cert_list = gnutls_certificate_get_peers(session, &cert_list_size);
  PLOG("cert_list_size: %u", cert_list_size);
  
  if (cert_list_size > 0 && gnutls_certificate_type_get(session) == GNUTLS_CRT_X509) {  
      if(gnutls_x509_crt_init (&cert) != GNUTLS_E_SUCCESS)
        throw General_exception("gnutls_x509_crt_init failed()");
      
      if(gnutls_x509_crt_import (cert, &cert_list[0], GNUTLS_X509_FMT_PEM) != GNUTLS_E_SUCCESS)
        PWRN("gnutls_x509_crt_import() failed");
  
      PINF("- Certificate info:");
  
      expiret = gnutls_x509_crt_get_expiration_time (cert);
      activet = gnutls_x509_crt_get_activation_time (cert);
      PINF("- Certificate is valid since: %s", ctime (&activet));
      PINF("- Certificate expires: %s", ctime (&expiret));
  
      if(gnutls_fingerprint (GNUTLS_DIG_MD5, &cert_list[0], digest, &digest_size) >= 0)
        {
          PINF("- Certificate fingerprint: ");
          for (i = 0; i < digest_size; i++)
            {
              printf("%.2x ", static_cast<unsigned char>(digest[i]));
            }
          printf("\n");
        }

      int rc = gnutls_x509_crt_get_dn (cert, dn, &dn_len);
      if( rc == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
        PINF("- DN: not available");
      else 
        PINF("- DN: (%d) %s", rc, dn);
  
      rc = gnutls_x509_crt_get_issuer_dn (cert, dn, &dn_len);
      if( rc == GNUTLS_E_REQUESTED_DATA_NOT_AVAILABLE)
        PINF("- Certificate Issuer's DN: not available");
      else
        PINF("- Certificate Issuer's DN: %s", dn);

#if 0    
      if (gnutls_x509_crt_get_serial (cert, serial, &serial_size) >= 0)
        {
          fprintf (stderr, _("- Certificate serial number: "));
          for (i = 0; i < serial_size; i++)
            {
              fprintf (stderr, "%.2x ", (unsigned char) serial[i]);
            }
          fprintf (stderr, "\n");
        }
      algo = gnutls_x509_crt_get_pk_algorithm (cert, &bits);
  
      fprintf (stderr, _("- Certificate public key: "));
      if (algo == GNUTLS_PK_RSA)
        {
          fprintf (stderr, _("RSA\n"));
          fprintf (stderr, ngettext ("- Modulus: %d bit\n",
                                     "- Modulus: %d bits\n", bits), bits);
        }
      else if (algo == GNUTLS_PK_DSA)
        {
          fprintf (stderr, _("DSA\n"));
          fprintf (stderr, ngettext ("- Exponent: %d bit\n",
                                     "- Exponent: %d bits\n", bits), bits);
        }
      else
        fprintf (stderr, _("UNKNOWN\n"));
  
      fprintf (stderr, _("- Certificate version: #%d\n"),
               gnutls_x509_crt_get_version (cert));
  
      gnutls_x509_crt_get_dn (cert, dn, &dn_len);
      fprintf (stderr, "- DN: %s\n", dn);
  
      gnutls_x509_crt_get_issuer_dn (cert, dn, &dn_len);
      fprintf (stderr, _("- Certificate Issuer's DN: %s\n"), dn);
  
      gnutls_x509_crt_deinit (cert);

#endif 
    }
}
#endif

/**
 * @brief      Prints an information.
 *
 * @param[in]  session  Session handle
 *
 */
void print_info(gnutls_session_t session)
{
  gnutls_credentials_type_t cred;
  gnutls_kx_algorithm_t     kx;
  int                       dhe, ecdh;  // group;
  char*                     desc;

  /* get a description of the session connection, protocol,
   * cipher/key exchange */
  desc = gnutls_session_get_desc(session);
  if (desc != NULL) {
    PINF("- Session: %s", desc);
  }

  dhe = ecdh = 0;

  kx = gnutls_kx_get(session);

  /* Check the authentication type used and switch
   * to the appropriate.
   */
  cred = gnutls_auth_get_type(session);
  switch (cred) {
  case GNUTLS_CRD_SRP:
    PINF("- SRP session with username %s", gnutls_srp_server_get_username(session));
    break;

  case GNUTLS_CRD_PSK:
    /* This returns NULL in server side.
     */
    if (gnutls_psk_client_get_hint(session) != NULL)
      PINF("- PSK authentication. PSK hint '%s'", gnutls_psk_client_get_hint(session));
    /* This returns NULL in client side.
     */
    if (gnutls_psk_server_get_username(session) != NULL)
      PINF("- PSK authentication. Connected as '%s'", gnutls_psk_server_get_username(session));

    if (kx == GNUTLS_KX_ECDHE_PSK)
      ecdh = 1;
    else if (kx == GNUTLS_KX_DHE_PSK)
      dhe = 1;
    break;

  case GNUTLS_CRD_ANON: /* anonymous authentication */

    PINF("- Anonymous authentication.");
    if (kx == GNUTLS_KX_ANON_ECDH)
      ecdh = 1;
    else if (kx == GNUTLS_KX_ANON_DH)
      dhe = 1;
    break;

  case GNUTLS_CRD_CERTIFICATE: /* certificate authentication */

    /* Check if we have been using ephemeral Diffie-Hellman.
     */
    if (kx == GNUTLS_KX_DHE_RSA || kx == GNUTLS_KX_DHE_DSS)
      dhe = 1;
    else if (kx == GNUTLS_KX_ECDHE_RSA || kx == GNUTLS_KX_ECDHE_ECDSA)
      ecdh = 1;

    PINF("- Certificate authentication (dhe=%d, ecdh=%d)", dhe, ecdh);

    break;
  default:
    break;
  } /* switch */

  /* read the negotiated group - if any */
  if (ecdh != 0)
    PINF("- Ephemeral ECDH using curve %s", gnutls_ecc_curve_get_name(gnutls_ecc_curve_get(session)));
  else if (dhe != 0)
    PINF("- Ephemeral DH using prime of %d bits", gnutls_dh_get_prime_bits(session));
}

static void print_logs(int level, const char* msg) { printf("GnuTLS [%d]: %s", level, msg); }

#pragma GCC diagnostic pop
