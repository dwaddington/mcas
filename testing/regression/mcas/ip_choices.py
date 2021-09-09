#!/usr/bin/python3

import json
import subprocess

def choices():
    ij=json.loads(subprocess.check_output(["/usr/sbin/ip", "-4", "-br", "-j", "addr"]))
    return [addr['local'] for ifc in map(lambda x: x['addr_info'], ij) for addr in ifc]

if __name__ == '__main__':
    print("IP choices: ", choices())
