#!/usr/bin/python3

from configs import config_0
from shard_protos import shard_proto_dax
from stores import hstore
from net_providers import sockets
from dax import devdax

class hstore_devdax_sock_0(config_0):
    def __init__(self, addr):
        config_0.__init__(self, shard_proto_dax(addr, hstore(), devdax()))
        self.merge(sockets())

from sys import argv

print(hstore_devdax_sock_0(argv[1]).json())
