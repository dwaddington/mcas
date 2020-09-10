#!/usr/bin/python3

from configs import config_0
from shard_protos import shard_proto_dax
from stores import hstore
from dax import fsdax

class hstore_fsdax_0(config_0):
    def __init__(self, addr):
        config_0.__init__(self, shard_proto_dax(addr, hstore(), fsdax()))

from sys import argv

print(hstore_fsdax_0(argv[1]).json())
