#!/usr/bin/python3

from configs import config_ado0
from shard_protos import shard_proto_ado
from stores import mapstore

class mapstore_ado0(config_ado0):
    def __init__(self, addr):
        config_ado0.__init__(self, shard_proto_ado(addr, mapstore()))

from sys import argv

print(mapstore_ado0(argv[1]).json())
