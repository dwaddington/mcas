#!/usr/bin/python3

from configs import config_0
from shard_protos import shard_proto_dax
import argparse
import stores
import dax

class hstore_0(config_0):
	def __init__(self, hstoretype, daxtype, addr, port=None, mm_plugin_path=None):
		store_ctor = getattr(stores, hstoretype.replace('-', '_')) # hstore, hstore-cc, hstore-mc, hstore-mr
		h = store_ctor()
		if port is not None:
			h.merge({"port": port})
		if mm_plugin_path is not None:
			h.merge({"mm_plugin_path": mm_plugin_path})
		dax_ctor = getattr(dax, daxtype) # devdax or fsdax
		config_0.__init__(self, shard_proto_dax(addr, h, dax_ctor()))

from sys import argv

parser = argparse.ArgumentParser(description='Generate a JSON document for hstore testing.')
parser.add_argument("store", help="store type e.g. hstore")
parser.add_argument("daxtype", choices=("devdax", "fsdax"), help="DAX type (devdas or fsdax)")
parser.add_argument("ipaddr", help="IP address of the port to use for the server")
parser.add_argument("port", type=int, nargs='?', help="IP port number of the port to use for the server")
parser.add_argument('--mm-plugin-path', help='storage plugin path')

args = parser.parse_args()

print(hstore_0(args.store, args.daxtype, args.ipaddr, args.port, args.mm_plugin_path).json())
