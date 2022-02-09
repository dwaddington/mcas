#!/usr/bin/python3

from dm import dm
from glob import glob

# Locations for fsdax stores are: /mnt/pmem<n>/a<0 ...>
# Locations for devdax stores are: /dev/dax<n>.*

class fsdax(dm):
    """ fsdax specification (within a shard) """
    def __init__(self, pfx, accession=0):
        self.pfx = pfx
        self.accession = accession
        dm.__init__(self, [{"path": "{}/a{}".format(pfx,accession)}])
    def step(self,n):
        return fsdax(self.pfx, self.accession+n)

class devdax(dm):
    """ devdax specification (within a shard) """
    def __init__(self, pfx, accession=0):
        self.pfx = pfx
        self.accession = accession
        self.devices = glob("{}.*".format(pfx))
        try:
            dm.__init__(self, [{"path": self.devices[accession], "addr": 0x9000000000 + 0x1000000000 * accession}])
        except IndexError as e:
            print("No dax device for accession", accession)
            raise
    def step(self,n):
        return devdax(self.pfx, self.accession+n)


if __name__ == '__main__':
    import argparse
    import re
    parser = argparse.ArgumentParser(description='Generate a JSON document for dax.py testing.')
    parser.add_argument("--prefix", nargs='?', default="/dev/dax0", help="DAX prefix, e,g. /dev/dax0) or /mnt/pmem1")
    parser.add_argument("--accession", nargs='?', type=int, default=0, help="dax device accession")

    args = parser.parse_args()

    daxtype = re.match("/mnt/",args.prefix) and "fsdax" or re.match("/dev/dax",args.prefix) and "devdax" or None
    if daxtype == "fsdax":
        print(fsdax(args.prefix, accession=args.accession).json())
    if daxtype == "devdax":
        print(devdax(args.prefix, accession=args.accession).json())
