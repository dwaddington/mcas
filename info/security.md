# Security

MCAS currently supports authentication through TLS x509 certificates.

To use TLS authentication the client should define the current environment variables:

```
export CERT=/mymcas/dist/certs/client-cert.pem
export KEY=/mymcas/dist/certs/client-privkey.pem
```

These variables are picked up by the MCAS client library and a TLS request
bit is set in the handshake protocol.  Currently, TLS handshake is performed
on the RDMA channel.  The server must be configured with TLS options, e.g.:

```
{
    "security" :
    {
        "cert_path" : "/mcas/dist/certs/mcas-cert.pem",
        "key_path" : "/mcas/dist/certs/mcas-privkey.pem" 
    },
    "shards" :
    [
        {
            "core" : 0,
            "port" : 11911,
            "net"  : "mlx5_0",
            "default_backend" : "hstore",
            "dax_config" : [{ "path": "/dev/dax0.0", "addr": "0x9000000000" }],
            "security_mode" : "tls:auth",
            "security_port" : 11912
        }
    ],
    "net_providers" : "verbs"
}
```

# Access Control Lists

Under development.
