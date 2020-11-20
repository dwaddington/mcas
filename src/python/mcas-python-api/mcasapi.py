import mcas
import pickle

from enum import Enum

class AdoFlags(Enum):
    NONE = 0x0
    CREATE_ON_DEMAND = 0x2
    CREATE_ONLY = 0x4
    NO_OVERWRITE = 0x8
    DETACHED = 0x10
    READ_ONLY = 0x20
    ZERO_NEW_VALUE = 0x40

"""
Save arbitrary Python data to KV store
"""
def pickle_put(pool, keyname, item):
    pickled_item = pickle.dumps(item)
    pool.put_direct(keyname, bytearray(pickled_item))

"""
Load arbitrary Python data from KV store
"""
def pickle_get(pool, keyname):
    bytearray_item = pool.get_direct(keyname)
    return pickle.loads(bytes(bytearray_item))
