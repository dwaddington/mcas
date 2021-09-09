#!/usr/bin/python3

from argparse_cfg_ipaddr import argparse_cfg_ipaddr

class argparse_cfg_mapstore(argparse_cfg_ipaddr):
    def __init__(self, description='Generate a JSON document for mapstore testing.'):
        argparse_cfg_ipaddr.__init__(self, description)
        self.add_argument("--core", type=int, default=0, help="base of CPUs cores to use")
