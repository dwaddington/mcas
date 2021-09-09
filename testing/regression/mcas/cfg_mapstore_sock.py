#!/usr/bin/python3

from cfg_mapstore import cfg_mapstore
from net_providers import sockets

class cfg_mapstore_sock(cfg_mapstore):
    def __init__(self, ipaddr, net, core=0):
        cfg_mapstore.__init__(self, ipaddr, core)
        self.merge(sockets())

if __name__ == '__main__':
    from argparse_cfg_mapstore import argparse_cfg_mapstore
    parser = argparse_cfg_mapstore(description='Generate a JSON document for mapstore sockets testing.')
    parser.add_argument("net", nargs='?', help="Net device e.g. mlx5_0 for the server (though now determined by ipaddr")
    args = parser.parse_args()
    print(cfg_mapstore_sock(args.ipaddr, args.net, args.core).json())
