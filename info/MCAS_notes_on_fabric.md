## Fabric Background

The MCAS client and server use the libfabric package to communicate.
A typical system (with libfabric insalled) has many libfabric "interfaces" with various attributes.
You can use libfabric's fi_info command to view all interfaces.
(An MCAS build places a copy of fi_info in the ./dist/bin directory.)

```
$ ./dist/bin/fi_info
```

MCAS requires:
 --provider verbs (for Infiniband)
 or
 --provider sockets (for TCP)

 --ep_type==FI_EP_MSG

 an interface with an IPv4 address (not IPv6)

You can get a list of feasible interfaces by specifying the desired provider (verbs for InfiniBand; sockets for TCP), required ep_type, and reachable IPv4 address on the desired network:

```
./dist/bin/fi_info --provider verbs --ep_type FI_EP_MSG --node 10.0.0.1
$ ../debug/dist/bin/fi_info --provider verbs --ep_type FI_EP_MSG --node 10.0.0.1
  provider: verbs
      fabric: IB-0xfe80000000000000
      domain: mlx5_0
      version: 110.10
      type: FI_EP_MSG
      protocol: FI_PROTO_RDMA_CM_IB_RC
```

If no interface matches the specifications, wou will receive error -61:

```
$ ../debug/dist/bin/fi_info --provider verbs --ep_type FI_EP_MSG --node 10.0.1.4
fi_getinfo: -61
```

## Specifying the server interface

These parameters in the server "configuration file" specify the libfabric interface on which the server will listen:

- net_providers: the provider typei (only one), either verbs or socket
- addr: the IPv4 address
- net: the physcical interface

If you do not specify net_providers, the server will try to use verbs and ockets, in that order.

Only one of the "addr" and "net" prameters need be specified; they both server to eligible libfabric interfaces.

If the parameters match more than one interface, the first interface (in the order returned by fi_getinfo) is used.

If combination of "net_providers" and "net" parameters matches no interface, ssome flavor of error 61 will appear in the log:

```
fabric_runtime_error (61)
```

If the addr IP address does not resolve to an interface, the attempt to "listen" will fail:

```
listen: listen failure fabric_runtime_error "Invalid argument"
```

## Specifying the client interface

The client needs only the server address. The client will try both possible provider modes (verbs and sockets) on the specified address and the port (if no port is specified).

If the server is unreachable or is not listening, The client log will contain an error:
