#!/usr/bin/python3

from dm import dm
import copy

class shard_proto_ado_mixin(dm):
    """ additions to a shard for ado testing """
    def __init__(self):
        dm.__init__(self, {
            "ado_plugins" : ["libcomponent-adoplugin-testing.so"],
        })

# Each shard_proto is a class derived from dm, to indicate that
# some "dict deep merges" may be needed.
# For multiple shards, may want to make shard_proto a generator.

class shard_proto(dm):
    """ shard prototype """
    def __init__(self, ipaddr, backend, cores=[], port=11911):
        #
        # steppable itens
        #
        self.ipaddr = ipaddr
        self.port = port
        self.backend = backend
        self.cores = cores
        self.core_ix = 0
        dm.__init__(self, {
            "core" : self.cores[self.core_ix],
            "addr" : ipaddr,
            "port" : port,
        })
        self.merge(backend)

    def incr(self,n):
        self.core_ix = self.core_ix + n
        self.port = self.port + n

    def step(self,n):
        # error: must create a shard_proto from all keys in self.
        s = copy.copy(self)
        s.incr(n)
        return s

class shard_proto_net(dm):
    """ shard prototype with network device """
    def __init__(self, ipaddr, backend, net, cores=[], port=11911):
        self.cores = cores
        self.core_ix = 0
        dm.__init__(self, {
            "core" : self.cores[core_ix],
            "addr" : ipaddr,
            "port" : port,
            "net"  : net
        })
        self.merge(backend)

class shard_proto_dax(shard_proto):
    """ shard prototype (for dax) """
    def __init__(self, ipaddr, backend, path, cores=[], port=11911):
        shard_proto.__init__(self, ipaddr, backend, cores, port=port)
        self.path = path
        self.merge({"dax_config": path.value()})

    def incr(self,n):
        shard_proto.incr(self, n)
        self.path = self.path.step(n)

class shard_proto_ado(shard_proto):
    """ shard prototype (for ado) """
    def __init__(self, ipaddr, backend, cores=[], port=11911):
        self.cores = cores
        shard_proto.__init__(self, ipaddr, backend, cores=cores, port=port)
        self.merge(shard_proto_ado_mixin())

class shard_proto_dax_ado(shard_proto_dax):
    """ shard prototype (for dax, ado) """
    def __init__(self, ipaddr, backend, path, cores=[], port=11911):
        shard_proto_dax.__init__(self, ipaddr, backend, path, cores=cores, port=port)
        self.merge(shard_proto_ado_mixin())
