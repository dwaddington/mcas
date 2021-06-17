#!/usr/bin/python3

from dm import dm

class hstore(dm):
    """ hstore backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "hstore"})

class hstore_cc(dm):
    """ hstore-cc backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "hstore-cc"})

class hstore_mc(dm):
    """ hstore-mc backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "hstore-mc"})

class mapstore(dm):
    """ mapstore backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "mapstore"})

if __name__ == '__main__':
    print("hstore", hstore().json())
    print("hstore_cc", hstore_cc().json())
    print("hstore_mc", hstore_mc().json())
    print("mapstore", mapstore().json())
