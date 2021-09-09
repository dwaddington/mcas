#!/usr/bin/python3

from configs import config_ado, ado_mixin
from shard_protos import shard_proto_dax_ado
from numa_cores import numa_cores
import stores
import dax
import re

class cfg_hstore_ado(config_ado):
    def __init__(self, hstoretype, dax_prefix, ipaddr, numa_node=0, port=None, mm_plugin_path=None, count=1, accession=0):
        cores=numa_cores(numa_node)
        store_ctor = getattr(stores, hstoretype.replace('-', '_')) # hstore, hstore-cc, hstore-mc, hstore-mr
        h = store_ctor()
        if port is not None:
            h.merge({"port": port})
        if mm_plugin_path is not None:
            h.merge({"mm_plugin_path": mm_plugin_path})
        daxtype = re.match("/mnt/",dax_prefix) and "fsdax" or re.match("/dev/dax",dax_prefix) and "devdax" or None
        if not daxtype:
            raise Exception("Could not determine DAX type from prefix %s" % dax_prefix)
        dax_ctor = getattr(dax, daxtype) # devdax or fsdax
        config_ado.__init__(self, shard_proto_dax_ado(ipaddr, h, dax_ctor(pfx=dax_prefix, accession=accession), cores=cores), ado_mixin(cores[1]), count)

if __name__ == '__main__':
    from argparse_cfg_hstore import argparse_cfg_hstore

    parser = argparse_cfg_hstore('Generate a JSON document for hstore testing.')

    args = parser.parse_args()

    print(cfg_hstore_ado(args.store, args.dax_prefix, args.ipaddr, args.numa_node, args.port, args.mm_plugin_path, args.shard_count, args.region).json())
