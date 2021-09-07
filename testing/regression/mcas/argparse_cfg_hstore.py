#!/usr/bin/python3

from argparse_cfg_ipaddr import argparse_cfg_ipaddr

class argparse_cfg_hstore(argparse_cfg_ipaddr):
    def __init__(self, description='Generate a JSON document for hstore testing.'):
        argparse_cfg_ipaddr.__init__(self, description)
        self.add_argument("store", help="store type e.g. hstore", choices=["hstore", "hstore-cc", "hstore-mm", "hstore-mt", "hstore-cc-pe", "hstore-mm-pe"])
        self.add_argument("dax_prefix", help="DAX prefix (e.g. /dev/dax0, /mnt/pmem1)")
        self.add_argument('--mm-plugin-path', help='storage plugin path')
        self.add_argument("--region", type=int, default=0, help="region within numa node to use")
        self.add_argument("--numa-node", type=int, default=0, help="numa node from which to pick CPU cores")
