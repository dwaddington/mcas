#!/usr/bin/python3

from configs import config_0
from stores import mapstore
from shard_protos import shard_proto

class mapstore_0(config_0):
    def __init__(self, addr):
        config_0.__init__(self, shard_proto(addr, mapstore()))

from sys import argv

print(mapstore_0(argv[1]).json())
