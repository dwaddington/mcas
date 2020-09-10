#!/usr/bin/python3

from configs import config_0
from shard_protos import shard_proto_dax
from stores import hstore
from dax import devdax

class hstore_devdax_0(config_0):
	def __init__(self, addr, port=None):
		h = hstore()
		if port is not None:
			h.merge({"port": port})
		config_0.__init__(self, shard_proto_dax(addr, h, devdax()))

from sys import argv

# If, as for testing dax region conflicts, the port number is specified
if len(argv) > 2:
	print(hstore_devdax_0(argv[1], int(argv[2])).json())
else:
	print(hstore_devdax_0(argv[1]).json())
