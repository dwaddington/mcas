#!/bin/bash
certtool --generate-privkey --sec-param High --outfile=mcas-privkey.pem
certtool --generate-self-signed --load-privkey mcas-privkey.pem --outfile=mcas-cert.pem

# altenratives
#
#openssl genrsa -out mcas-privkey.pem 1024
#openssl req -new -x509 -key mcas-privkey.pem -out mcas-cert.pem -days 365

# validate cert/keys
#
# openssl crl2pkcs7 -nocrl -certfile mcas-cert.pem | openssl pkcs7 -print_certs -noout

# check cert/key pair matches
#
openssl rsa -noout -modulus -in ./dist/certs/mcas-privkey.pem | openssl md5
openssl x509 -noout -modulus -in ./dist/certs/mcas-cert.pem | openssl md5


