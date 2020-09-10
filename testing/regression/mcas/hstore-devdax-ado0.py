#!/usr/bin/python3

from configs import config_ado0
from shard_protos import shard_proto_dax_ado
from stores import hstore
from dax import devdax

class hstore_devdax_ado0(config_ado0):
    def __init__(self, addr):
        config_ado0.__init__(self, shard_proto_dax_ado(addr, hstore(), devdax()))

from sys import argv

print(hstore_devdax_ado0(argv[1]).json())
