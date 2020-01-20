Test X509 certificates and keys.

Private (testing) key and public certificate generation:

```
openssl req -newkey rsa:2048 -nodes -keyout mcas-key.pem -x509 -days 365 -out mcas-cert.pem
```

Combine key and certificate in a PKCS#12 (P12) bundle:

```
openssl pkcs12 -inkey mcas-key.pem -in mcas-cert.pem  -export -out certificate.p12
```

