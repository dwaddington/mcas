#!/usr/bin/python3

from dm import dm
from install_prefix import install_prefix

class config(dm) :
    """ basic configuration """
    def __init__(self, shard_proto_, count_=1):
        dm.__init__(self, {
                "shards" : [
            #        shard_proto_.value()
                ],
            })
        for i in range(0, count_):
            self._value["shards"].append(shard_proto_.step(i).value())

from securities import pem_cert

from functools import reduce

def core_count():
    """ count number of cores in the local machine """
    with open("/proc/cpuinfo","r") as procinfo:
        return reduce(lambda a, b: a + b.startswith("processor"), procinfo, 0)

class config_secure(config) :
    """ configuration with a certificate """
    def __init__(self, shard_proto_):
        config.__init__(self, shard_proto_)
        self.merge({"security": pem_cert().value()})

def core_clamp(a):
    """ single core a, clamped """
    c = core_count()-1
    return min(a, c)

def core_range(a, b):
    """ clamped range of cores from a to b inclusive """
    c = core_count()-1
    return "{}-{}".format(min(a, c), min(b, c))

class ado_mixin(dm):
    def __init__(self, core=1):
        dm.__init__(self, {
            "ado_path" : "{}/bin/ado".format(install_prefix),
            # Special case for CPUs with less than 8 cores
            # CPU range was 6-8, but will lock-up on 4-core system when clamped to 3-3 and client also clamped to 3
            "resources": { "ado_cores": (core_count() < 8 and core_range(1, 2) or core_range(core+5,core+5+2)), "ado_manager_core": core_clamp(core) }
        })

class config_ado(config):
    """ configuration with ado specification """
    def __init__(self, shard_proto_, ado, count_=1):
        config.__init__(self, shard_proto_, count_)
        self.merge(ado)

if __name__ == '__main__':
    print("core_count: ", core_count())
    core=0
    from shard_protos import shard_proto, shard_proto_ado
    from stores import mapstore
    print("config_secure: ", config_secure(shard_proto("127.0.0.1", mapstore(), core=core)).json())
    print("ado_resources: ", ado_mixin(1).json())
    print("config_ado: ", config_ado(shard_proto_ado("127.0.0.1", mapstore(), core=core),ado_mixin(core=core+1)).json())
