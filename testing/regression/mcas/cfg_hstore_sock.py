#!/usr/bin/python3

from cfg_hstore import cfg_hstore
from net_providers import sockets

class hstore_sock(cfg_hstore):
    def __init__(self, hstoretype, dax_prefix, ipaddr, numa_node=0, port=None, mm_plugin_path=None, count=1, accession=0):
        cfg_hstore.__init__(self, hstoretype, dax_prefix, ipaddr, numa_node, port=None, mm_plugin_path=None, count=1, accession=0)
        self.merge(sockets())

if __name__ == '__main__':
    from argparse_cfg_hstore import argparse_cfg_hstore
    parser = argparse_cfg_hstore()
    args = parser.parse_args()
    print(cfg_hstore(args.store, args.dax_prefix, args.ipaddr, args.numa_node, args.port, args.mm_plugin_path, args.shard_count, args.region).json())
