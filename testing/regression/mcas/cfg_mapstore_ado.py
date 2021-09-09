#!/usr/bin/python3

from configs import config_ado, ado_mixin
from shard_protos import shard_proto_ado
from stores import mapstore

class mapstore_ado(config_ado):
    def __init__(self, ipaddr, core=0, port=11911, count=1, accession=0):
        cores = range(core,112)
        m = mapstore()
        if port is not None:
            m.merge({"port": port})
        config_ado.__init__(self, shard_proto_ado(ipaddr, m, cores=cores, port=port), ado_mixin(cores[1]), count)

if __name__ == '__main__':
    from argparse_cfg_mapstore import argparse_cfg_mapstore
    parser = argparse_cfg_mapstore(description='Generate a JSON document for mapstore+ADO testing.')
    args = parser.parse_args()
    print(mapstore_ado(args.ipaddr, args.core, args.port).json())
