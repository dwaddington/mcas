#!/usr/bin/python3

from argparse import ArgumentParser
from ip_choices import choices 

class argparse_cfg_ipaddr(ArgumentParser):
    def __init__(self, description='Generate a JSON document for shard testing.'):
        ArgumentParser.__init__(self, description)
        self.add_argument("ipaddr", help="IP address of the port to use for the server", choices=choices())
        self.add_argument("--port", type=int, default=11911, help="IP port number of the port to use for the server")
        self.add_argument("--shard-count", type=int, default=1, help="number of shards to configure")
