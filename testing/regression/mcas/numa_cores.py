#!/usr/bin/python3

from subprocess import Popen, PIPE
import re

def numa_cores(node):
    r = r'node %i cpus:(.*)' % node
    with Popen(["/usr/bin/numactl", "--hardware"], stdout=PIPE) as a:
        for l in a.stdout:
            m = re.match(r, l.decode("utf-8"))
            if m:
                return list(map(lambda x: int(x), re.findall(r'(\d+)', m.group(1))))
        return []

if __name__ == '__main__':
    from argparse import ArgumentParser
    parser = ArgumentParser('Find CPUs for a numa node')
    parser.add_argument("numa", type=int, help="ordinal numa node")
    args = parser.parse_args()
    print(numa_cores(args.numa))
