#!/usr/bin/python3

from configs import config
from stores import mapstore
from shard_protos import shard_proto

class cfg_mapstore(config):
    def __init__(self, ipaddr, count=1, core=0):
        cores = range(core,112)
        config.__init__(self, shard_proto(ipaddr, mapstore(), cores=cores), count)

if __name__ == '__main__':
    from argparse_cfg_mapstore import argparse_cfg_mapstore
    parser = argparse_cfg_mapstore(description='Generate a JSON document for mapstore testing.')
    args = parser.parse_args()
    print(cfg_mapstore(args.ipaddr, count=args.shard_count, core=args.core).json())
