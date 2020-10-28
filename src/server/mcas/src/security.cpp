#include "security.h"

#include <future>
#include <api/components.h>
#include <common/exceptions.h>
#include <common/logging.h>
#include <common/net.h>
#include <common/dump_utils.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <fstream>

#define LOG_PREFIX "Shard_security: "

namespace mcas
{

Shard_security::Shard_security(const boost::optional<std::string> cert_path,
                               const boost::optional<std::string> key_path,
                               const boost::optional<std::string> mode,
                               const boost::optional<std::string> ipaddr,
                               const boost::optional<std::string> net_device,
                               const unsigned port,
                               const unsigned debug_level)
  : common::log_source(debug_level),
    _cert_path(cert_path ? *cert_path : ""),
    _key_path(key_path ? *key_path : ""),
    _auth_enabled(!_cert_path.empty()), /* if no certificate path is given, then authentication is turned off */
    _mode(security_mode_t::NONE),
    _ipaddr(ipaddr ? *ipaddr : ""),
    _port(port),
    _crypto(nullptr)
{
  if(_auth_enabled) {
    if(_ipaddr.empty()) { /* if no address given, use IP address associated with net device */
      if(!net_device) throw General_exception("ipaddr or net device must be provided");
      _ipaddr = common::get_ip_from_eth_device(common::get_eth_device_from_rdma_device(*net_device));
    }
    
    if(*mode == "tls-hmac") {
      CPLOG(1, LOG_PREFIX "security mode TLS HMAC (port=%u)(ipaddr=%s)", port, _ipaddr.c_str());
      _mode = security_mode_t::TLS_HMAC;

      PLOG(LOG_PREFIX "enabled (cert_path:%s)(ipaddr:%s)(port:%u)", _cert_path.c_str(), _ipaddr.c_str(), port);

      // /* create crypto component */
      // {
      //   using namespace component;
      //   IBase *comp = load_component("libcomponent-tls.so", tls_factory);

      //   assert(comp);
      //   auto fact = make_itf_ref(static_cast<ICrypto_factory *>(comp->query_interface(ICrypto_factory::iid())));
      //   std::map<std::string, std::string> params;
      //   _crypto = make_itf_ref(static_cast<ICrypto *>(fact->create(debug_level, params)));
      //   PNOTICE("_crypto component instance: %p", reinterpret_cast<void*>(_crypto.get()));

      //   if (_crypto->initialize(CIPHER_SUITE,
      //                           _mcas_cert_path + "mcas-cert.pem",
      //                           _mcas_cert_path + "mcas-privkey.pem") != S_OK)
      //     throw General_exception("crypto initialization failed");
      // }

      // /* set up async task to do work of accepting connection */
      // _tls_session = std::async(std::launch::async,
      //                           &Shard_security::tls_session_entry,
      //                           this);

    }

    CPLOG(1, LOG_PREFIX "initialization OK");
  }
}

Shard_security::~Shard_security()
{
  if(_crypto) {
    PNOTICE("Shard security object deletion");
    _crypto->shutdown();
    PNOTICE("Crypto shutdown OK");
    PNOTICE("interrupting thread");

  }
}

// void Shard_security::tls_session_entry()
// {
//   PNOTICE("GNU TLS accepting session (%s,%u)", _ipaddr.c_str(), _port);

//   try {
//     auto session = _crypto->accept_cert_session(_ipaddr.c_str(), _port, 100);

//     std::string key3;
//     _crypto->export_key(session, "SOMELABEL", 0, 8, key3);
//     hexdump(key3.c_str(), 8);

//   }
//   catch(General_exception&) {
//   }
//   PNOTICE("GNU TLS session accepted");


//   // while(1) {
//   //   PLOG("TLS session");
//   //   sleep(1);
//   // }
  
// }

}  // namespace mcas


#if 0
namespace mcas
{
class Shard_security_state : private common::log_source {
public:
  Shard_security_state(const std::string &certs_path,
                       const unsigned debug_level_)
    : common::log_source(debug_level_), _cert(nullptr), _cert_base64{}
  {
    try {
      FILE *fp = fopen(certs_path.c_str(), "r");
      if (!fp) return;  // throw Constructor_exception("unable to open cert file");

      _cert = PEM_read_X509(fp, nullptr, nullptr /* passwd callback */, nullptr);
      if (_cert) {
        /* check certificate */
        STACK_OF(X509) *sk = sk_X509_new_null();
        sk_X509_push(sk, _cert);

        X509_NAME *subj = X509_get_subject_name(_cert);

        char cert_email[256];
        X509_NAME_get_text_by_NID(subj, NID_pkcs9_emailAddress, cert_email, 256);
        PLOG("Cert email: %s", cert_email);
        assert(std::string(cert_email) == "daniel.waddington@ibm.com");
        if (debug_level() > 3) {
          for (int i = 0; i < X509_NAME_entry_count(subj); i++) {
            X509_NAME_ENTRY *e = X509_NAME_get_entry(subj, i);
            ASN1_STRING *    d = X509_NAME_ENTRY_get_data(e);
            unsigned char *  c;
            if (ASN1_STRING_to_UTF8(&c, d)) {
              PLOG("X509: %s", c);
              OPENSSL_free(c);
            }
          }
        }

        CPLOG(0, "X509 certificate OK.");
      }

      fclose(fp);

      std::ifstream t(certs_path);
      _cert_base64.assign(std::istreambuf_iterator<char>(t), std::istreambuf_iterator<char>());
    }
    catch (...) {
      PLOG("unable to load certificate. disabling authentication");
      _cert = nullptr;
    }
  }

  Shard_security_state(const Shard_security_state &) = delete;
  Shard_security_state &operator=(const Shard_security_state &) = delete;

  ~Shard_security_state()
  {
    if (_cert) X509_free(_cert);
  }

private:
  X509 *      _cert;
  std::string _cert_base64;
};

#endif
