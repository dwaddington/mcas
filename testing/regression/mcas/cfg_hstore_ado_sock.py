#!/usr/bin/python3

from cfg_hstore_ado import cfg_hstore_ado
from net_providers import sockets

class cfg_hstore_ado_sock(cfg_hstore_ado):
    def __init__(self, hstoretype, dax_prefix, ipaddr, numa_node=0, port=None, mm_plugin_path=None, count=1, accession=0):
        cfg_hstore_ado.__init__(self, hstoretype, dax_prefix, ipaddr, numa_node, port, mm_plugin_path, count, accession)
        self.merge(sockets())

if __name__ == '__main__':
    from argparse_cfg_hstore import argparse_cfg_hstore
    parser = argparse_cfg_hstore(description='Generate a JSON document for hstore+socket testing.')
    args = parser.parse_args()
    print(cfg_hstore_ado_sock(args.store, args.dax_prefix, args.ipaddr, args.numa_node, args.port, args.mm_plugin_path, args.shard_count, args.region).json())
