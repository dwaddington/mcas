#!/usr/bin/python3

from configs import config_0
from shard_protos import shard_proto_dax
from stores import hstore_cc
from dax import devdax

class hstore_cc_devdax_0(config_0):
    def __init__(self, addr):
        config_0.__init__(self, shard_proto_dax(addr, hstore_cc(), devdax()))

from sys import argv

print(hstore_cc_devdax_0(argv[1]).json())
