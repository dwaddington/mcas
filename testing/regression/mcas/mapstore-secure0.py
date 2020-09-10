#!/usr/bin/python3

from configs import config_secure0
from stores import mapstore
from shard_protos import shard_proto

class mapstore_secure0(mapstore, config_secure0):
    def __init__(self, addr):
        config_secure0.__init__(self, shard_proto(addr, mapstore()))

from sys import argv

print(mapstore_secure0(argv[1]).json())
