#!/usr/bin/python3

from configs import config_ado0
from shard_protos import shard_proto_dax_ado
from stores import hstore
from net_providers import sockets
from dax import fsdax

class hstore_fsdax_ado0_sock(config_ado0):
    def __init__(self, addr):
        config_ado0.__init__(self, shard_proto_dax_ado(addr, hstore(), fsdax()))
        self.merge(sockets())

from sys import argv

print(hstore_fsdax_ado0_sock(argv[1]).json())
