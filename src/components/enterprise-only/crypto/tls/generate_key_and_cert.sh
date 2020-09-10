#!/bin/bash
#certtool --generate-privkey --sec-param High --outfile=mcas-privkey.pem
#certtool --generate-self-signed --load-privkey mcas-privkey.pem --template cert.cfg --outfile=mcas-cert.pem
openssl genrsa -out mcas-privkey.pem 1024
openssl req -new -x509 -key mcas-privkey.pem -out mcas-cert.pem -days 1825

