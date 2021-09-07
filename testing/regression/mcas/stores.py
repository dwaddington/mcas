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

class hstore_mm(dm):
    """ hstore-mm backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "hstore-mm"})

class hstore_mt(dm):
    """ hstore-mt backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "hstore-mt"})

class mapstore(dm):
    """ mapstore backend (within a shard) """
    def __init__(self):
        dm.__init__(self, {"default_backend": "mapstore"})

if __name__ == '__main__':
    print("hstore", hstore().json())
    print("hstore_cc", hstore_cc().json())
    print("hstore_mm", hstore_mm().json())
    print("hstore_mt", hstore_mt().json())
    print("mapstore", mapstore().json())
