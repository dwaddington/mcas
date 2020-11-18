#!/usr/bin/python3

from configs import config_0
from stores import mapstore
from shard_protos import shard_proto_net
from net_providers import sockets

class mapstore_0(config_0):
    def __init__(self, addr, net):
        config_0.__init__(self, shard_proto_net(addr, mapstore(), net))
        self.merge(sockets())

from sys import argv


print(mapstore_0(argv[1], argv[2]).json())
