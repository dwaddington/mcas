#!/usr/bin/python3

from configs import config_secure
from stores import mapstore
from shard_protos import shard_proto

class cfg_mapstore_secure(mapstore, config_secure):
    def __init__(self, ipaddr, port=11911, core=0):
        config_secure.__init__(self, shard_proto(ipaddr, mapstore(), cores=range(core, 112), port=port))

if __name__ == '__main__':
    from argparse_cfg_mapstore import argparse_cfg_mapstore
    parser = argparse_cfg_mapstore(description='Generate a JSON document for config+security testing.')
    args = parser.parse_args()
    print(cfg_mapstore_secure(args.ipaddr,args.port,args.core).json())
