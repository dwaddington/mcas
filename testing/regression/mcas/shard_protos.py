#!/usr/bin/python3

from dm import dm

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
    def __init__(self, addr, default_backend):
        dm.__init__(self, {
            "core" : 0,
            "addr" : addr,
        })
        self.merge(default_backend)

class shard_proto_dax(shard_proto):
    """ shard prototype (for dax) """
    def __init__(self, addr, default_backend, path):
        shard_proto.__init__(self, addr, default_backend)
        p = dm({ "region_id": 0, "addr": 0x9000000000 })
        p.merge(path)
        self.merge({"dax_config": [ p.value() ]})

class shard_proto_ado(shard_proto):
    """ shard prototype (for ado) """
    def __init__(self, addr, default_backend):
        shard_proto.__init__(self, addr, default_backend)
        self.merge(shard_proto_ado_mixin())

class shard_proto_dax_ado(shard_proto_dax):
    """ shard prototype (for dax, ado) """
    def __init__(self, addr, default_backend, path):
        shard_proto_dax.__init__(self, addr, default_backend, path)
        self.merge(shard_proto_ado_mixin())
