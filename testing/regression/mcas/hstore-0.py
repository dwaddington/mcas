#!/usr/bin/python3

from configs import config_0
from shard_protos import shard_proto_dax
import stores
import dax

class hstore_0(config_0):
	def __init__(self, hstoretype, daxtype, addr, port=None):
		store_ctor = getattr(stores, hstoretype.replace('-', '_')) # hstore or hstore-cc
		h = store_ctor()
		if port is not None:
			h.merge({"port": port})
		dax_ctor = getattr(dax, daxtype) # devdax or fsdax
		config_0.__init__(self, shard_proto_dax(addr, h, dax_ctor()))

from sys import argv

# If, as for testing dax region conflicts, the port number is specified
if len(argv) == 5:
	print(hstore_0(argv[1], argv[2], argv[3], int(argv[4])).json())
elif len(argv) == 4:
	print(hstore_0(argv[1], argv[2], argv[3]).json())
else:
	print("Usage: %s: {hstore|hstore-cc} {devdax|fadax} address [port]" % argv[0])
