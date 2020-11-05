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
#include <city.h>
#include "tls_session.h"
#include "connection_state.h"
#include "connection_handler.h"

#define CAFILE "/etc/ssl/certs/ca-bundle.trust.crt"

static void print_info(gnutls_session_t session);
static void print_logs(int level, const char* msg);

static constexpr const unsigned TLS_DEBUG_LEVEL = 3;

/* static constructor called once */
static void __attribute__((constructor)) Global_ctor()
{
  gnutls_global_init();
  gnutls_global_set_log_level(0); /* 0-9 higher more verbose */
  gnutls_global_set_log_function(print_logs);
}

static void __attribute__((destructor)) Global_dtor() { gnutls_global_deinit(); }

static constexpr const char * cipher_suite = "NORMAL:+AEAD";

namespace mcas
{

Connection_TLS_session::~Connection_TLS_session() {
  
  if (_aead_key.size > 0) {
    delete[] _aead_key.data;
    gnutls_aead_cipher_deinit(_cipher_handle);
  }
}

unsigned TLS_transport::debug_level() {
  return TLS_DEBUG_LEVEL;
}

int TLS_transport::gnutls_pull_timeout_func(gnutls_transport_ptr_t, unsigned int)
{
  return 0;
}

ssize_t TLS_transport::gnutls_pull_func(gnutls_transport_ptr_t connection,
                                        void* buffer,
                                        size_t buffer_size)
{  
  assert(connection);
  auto p_connection = reinterpret_cast<Connection_handler*>(connection);

  if(p_connection->_tls_buffer.remaining() >= buffer_size) {
    if(debug_level() > 2) {
      PLOG("TLS pull: taking %lu bytes from remaining (%lu)",
           buffer_size, p_connection->_tls_buffer.remaining());
    }
    p_connection->_tls_buffer.pull(buffer, buffer_size);
    return buffer_size;
  }

  if(debug_level() > 4)
    PLOG("TLS posting receive:");
  
  p_connection->post_recv_buffer(p_connection->allocate_recv());

  while(!p_connection->check_for_posted_recv_complete()) {
     p_connection->check_network_completions();
  }
  
  auto iob = p_connection->posted_recv();
  assert(iob);

  void * base_v = iob->base();
  uint64_t * base = reinterpret_cast<uint64_t*>(base_v);

  if(debug_level() > 2)
    PLOG("TLS received: iob_len=%lu payload-len=%lu (%p)", iob->length(), base[0], reinterpret_cast<void*>(iob));

  /* copy off what is received into Byte_buffer to take piecemeal */
  p_connection->_tls_buffer.push(reinterpret_cast<void*>(&base[1]), base[0]);

  /* clean up recv buffer */
  p_connection->free_recv_buffer();

  p_connection->_tls_buffer.pull(buffer, buffer_size);
  return buffer_size;
}

ssize_t TLS_transport::gnutls_vec_push_func(gnutls_transport_ptr_t connection,
                                            const giovec_t * iovec,
                                            int iovec_cnt)
{ 
  auto p_connection = reinterpret_cast<Connection_handler*>(connection);
  p_connection->check_network_completions();

  /* TODO length check */
  auto iobs = p_connection->allocate_send();
  void * base_v = iobs->base();
  uint64_t * base = reinterpret_cast<uint64_t*>(base_v);

  char * ptr = reinterpret_cast<char*>(&base[1]);
  size_t size = 0;
  
  for(int i=0; i<iovec_cnt; i++) {
    memcpy(ptr, iovec[i].iov_base, iovec[i].iov_len);
    size += iovec[i].iov_len;
    ptr += iovec[i].iov_len;
  }

  base[0] = size; /* prefix buffer with size */
  iobs->set_length(size + sizeof(uint64_t));
  p_connection->post_send_buffer(&*iobs, "TLS packet (server-send)", __func__);

  return size; /* return size of payload */
}


Connection_state Connection_TLS_session::process_tls_session()
{
  if(!_session) {

    initialize_certs();

    /* initialize session */
    if (gnutls_init(&_session, GNUTLS_SERVER) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_init() failed");

    if (gnutls_priority_set(_session, _priority) != GNUTLS_E_SUCCESS) /* set cipher suite */
      throw General_exception("gnutls_priority_set() failed");

    if (gnutls_credentials_set(_session, GNUTLS_CRD_CERTIFICATE, _x509_cred) != GNUTLS_E_SUCCESS)
      throw General_exception("gnutls_credentials_set() failed");

    /* request that client provides certificate to identify themselves */
    gnutls_certificate_server_set_request(_session, GNUTLS_CERT_REQUIRE);  // GNUTLS_CERT_IGNORE);

    //    gnutls_handshake_set_timeout(_session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);

    /* set up transport over fabric connection (via Connection_handler) */
    gnutls_transport_set_ptr(_session, _connection);
    gnutls_transport_set_vec_push_function(_session, TLS_transport::gnutls_vec_push_func);
    gnutls_transport_set_pull_function(_session, TLS_transport::gnutls_pull_func);
    gnutls_transport_set_pull_timeout_function(_session, TLS_transport::gnutls_pull_timeout_func);

    return Connection_state::WAIT_TLS_HANDSHAKE;
  }

  /* check if handshake recv complete */
  if(_connection->check_for_posted_recv_complete()) {
    /* initiate handshake */
    int rc;
    if ((rc = gnutls_handshake(_session)) < 0) {

      if (rc == GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR) {
        /* check certificate verification status */
        gnutls_datum_t out;
        auto           type   = gnutls_certificate_type_get(_session);
        auto           status = gnutls_session_get_verify_cert_status(_session);
        if (gnutls_certificate_verification_status_print(status, type, &out, 0) != GNUTLS_E_SUCCESS)
          throw General_exception("gnutls_certificate_verification_status_print() failed");

        CPLOG(1, "TLS: bad certificate; closing client session");

        /* send error result */
        auto result = GNUTLS_E_CERTIFICATE_VERIFICATION_ERROR;
        gnutls_record_send(_session, &result, sizeof(result));

        gnutls_deinit(_session);
        gnutls_free(out.data);

        return Connection_state::CLOSE_CONNECTION;
      }
      else throw General_exception("GNU tls handshake has failed (%s)\n\n", gnutls_strerror(rc));
    }

    print_info(_session);

    /* extra client identifier - see https://www.gnutls.org/manual/gnutls.html#X509-certificate-API */
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

      /* for now, do a 64-bit hash on DN as the identifier. Something
         more secure means we may have to increase auth_id field
       */
      _x509_auth_id = CityHash64(dn, dn_len);
      CPLOG(1, "Client ID: %lX", _x509_auth_id);
      
      /* for reference, other possible extractions */

      /* email extraction */
      // std::string dn_str(dn, dn_len);
      // std::smatch m;
      // std::regex expr("EMAIL=([^,]+)");

      // if(std::regex_search(dn_str, m, expr) &&
      //    gnutls_x509_crt_check_email(cert, m[1].str().c_str(), 0)) {
      //   CPLOG(1, "email extracted as authentication ID (%s)", m[1].str().c_str());

      //     _client_auth_id = m[1].str();
      // }
      // else {
      //   PWRN("unable to extract EMAIL from certificate");
      //   _client_auth_id = "invalid";
      // }

      /* fingerprint extraction */
      // char fingerprint[256];
      // size_t fingerprint_len = sizeof(fingerprint);
      // if(gnutls_x509_crt_get_fingerprint(cert,
      //                                    GNUTLS_DIG_SHA256,
      //                                    fingerprint, &fingerprint_len) != GNUTLS_E_SUCCESS)
      //   throw General_exception("gnutls_x509_crt_get_fingerprint() failed");
      // PINF("Fingerpint: %lu bytes long (%s)", fingerprint_len, fingerprint);

      // /* serial number extraction */
      // if(gnutls_x509_crt_get_serial(cert, serial, &serial_len) != GNUTLS_E_SUCCESS)
      //   throw General_exception("gnutls_x509_crt_get_serial() failed");
      // assert(serial_len < 40);
      // serial[serial_len] = '\0';
    }
    
    /* send success result */
    auto result = 0;
    gnutls_record_send(_session, &result, sizeof(result));
    CPLOG(2, "TLS handshake complete");

    return Connection_state::WAIT_NEW_MSG_RECV; // finally, move to next state
  }

  return Connection_state::WAIT_TLS_HANDSHAKE;
}

void Connection_TLS_session::initialize_certs()
{
  int err;

  if ((err = gnutls_certificate_allocate_credentials(&_x509_cred)) != GNUTLS_E_SUCCESS)
    throw General_exception("crypto-engine: gnutls_certificate_allocate_credentials() failed (%s)",
                            gnutls_strerror(err));

  if ((err = gnutls_certificate_set_x509_trust_file(_x509_cred, CAFILE, GNUTLS_X509_FMT_PEM))
      != GNUTLS_E_SUCCESS) 
    throw General_exception("crypto-engine: gnutls_certificate_set_x509_trust_file() failed (%s)",
                            gnutls_strerror(err));

  if ((err = gnutls_certificate_set_x509_key_file(_x509_cred,
                                                  _security_options.cert_file.c_str(),
                                                  _security_options.key_file.c_str(),
                                                  GNUTLS_X509_FMT_PEM)) !=
      GNUTLS_E_SUCCESS)
    throw General_exception("crypto-engine: gnutls_certificate_set_x509_key_file() "
                            "failed (key file=%s) (cert file=%s) (%s)",                              
                            _security_options.key_file.c_str(),
                            _security_options.cert_file.c_str(),
                            gnutls_strerror(err));

  /* Instead of the default options as shown above one could specify
   * additional options such as server precedence in ciphersuite selection
   * as follows:
   * gnutls_priority_init2(&priority_cache,
   *                       "%SERVER_PRECEDENCE",
   *                       NULL, GNUTLS_PRIORITY_INIT_DEF_APPEND);
   */
  if (gnutls_priority_init(&_priority, cipher_suite, NULL) != GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_priority_init() failed");

#if GNUTLS_VERSION_NUMBER >= 0x030506
  if (gnutls_certificate_set_known_dh_params(_x509_cred, GNUTLS_SEC_PARAM_MEDIUM) != GNUTLS_E_SUCCESS)
    throw General_exception("gnutls_certificate_set_known_dh_params() failed");
#endif
    
}


}


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
