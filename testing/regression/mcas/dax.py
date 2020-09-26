#!/usr/bin/python3

from dm import dm

class fsdax(dm):
    """ fsdax specification (within a shard) """
    def __init__(self, region=0):
        dm.__init__(self, {"path": "/mnt/pmem%d" % (region,)})

class devdax(dm):
    """ devdax specification (within a shard) """
    def __init__(self, region=0, accession=0):
        dm.__init__(self, {"path": "/dev/dax%d.%d" % (region, accession)})

if __name__ == '__main__':
    print("fsdax:", fsdax().json())
    print("devdax:", devdax().json())
