# Security

MCAS currently supports authentication through TLS x509 certificates.

To use TLS authentication the client should define the current environment variables:

```
export CERT=/mymcas/dist/certs/client-cert.pem
export KEY=/mymcas/dist/certs/client-privkey.pem
```

These variables are picked up by the MCAS client library and a TLS request
bit is set in the handshake protocol.  Currently, TLS handshake is performed
on the RDMA channel.

# Access Control Lists

Under development.
