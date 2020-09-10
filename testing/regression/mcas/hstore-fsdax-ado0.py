#!/usr/bin/python3

from configs import config_ado0
from shard_protos import shard_proto_dax_ado
from stores import hstore
from dax import fsdax

class hstore_fsdax_ado0(config_ado0):
    def __init__(self, addr):
        config_ado0.__init__(self, shard_proto_dax_ado(addr, hstore(), fsdax()))

from sys import argv

print(hstore_fsdax_ado0(argv[1]).json())
