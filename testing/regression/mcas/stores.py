#!/usr/bin/python3

from dm import dm

class hstore(dm):
    """ hstore backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "hstore"})

class hstore_cc(dm):
    """ hstorei-cc backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "hstore-cc"})

class mapstore(dm):
    """ mapstore backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "mapstore"})

if __name__ == '__main__':
    print("hstore", hstore().json())
    print("hstore_cc", hstore_cc().json())
    print("mapstore", mapstore().json())
