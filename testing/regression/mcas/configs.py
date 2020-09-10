#!/usr/bin/python3

from dm import dm
from install_prefix import install_prefix

class config_0(dm) :
    """ basic configuration """
    def __init__(self, shard_proto_):
        dm.__init__(self, {
                "shards" : [
                    shard_proto_.value()
                ],
            })

from securities import pem_cert

from functools import reduce

def core_count():
    """ count number of cores in the local machine """
    with open("/proc/cpuinfo","r") as procinfo:
        return reduce(lambda a, b: a + b.startswith("processor"), procinfo, 0)

class config_secure0(config_0) :
    """ configuration with a certificate """
    def __init__(self, shard_proto_):
        config_0.__init__(self, shard_proto_)
        self.merge({"security": pem_cert().value()})

def core_clamp(a):
    """ single core a, clamped """
    c = core_count()-1
    return min(a, c)

def core_range(a, b):
    """ clamped range of cores from a to b inclusive """
    c = core_count()-1
    return "%d-%d" %(min(a, c), min(b, c))

class ado_mixin(dm):
    def __init__(self):
        dm.__init__(self, {
            "ado_path" : "%s/bin/ado" % (install_prefix,),
            # Special case for CPUs with less than 8 cores
            # CPU range was 6-8, but will lock-up on 4-core system when clamped to 3-3 and client also clamped to 3
            "resources": { "ado_cores": (core_count() < 8 and core_range(1, 2) or core_range(6,8)), "ado_manager_core": core_clamp(1) }
        })

class config_ado0(config_0):
    """ configuration with ado specification """
    def __init__(self, shard_proto_):
        config_0.__init__(self, shard_proto_)
        self.merge(ado_mixin())

if __name__ == '__main__':
    print("core_count: ", core_count())
    from shard_protos import shard_proto, shard_proto_ado
    from stores import mapstore
    print("config_secure0: ", config_secure0(shard_proto("127.0.0.1", mapstore())).json())
    print("ado_resources: ", ado_mixin().json())
    print("config_ado: ", config_ado0(shard_proto_ado("127.0.0.1", mapstore())).json())
