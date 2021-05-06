# __init__.py
import numpy as np
import pymmcore
#from pymmcore import *

dtypedescr = np.dtype

#class FooMemoryResource(pymmcore.MemoryResource): pass
#from .numeric import uint8, ndarray, dtype

class ndarray(np.ndarray):

    __array_priority__ = -100.0 # what does this do?

    def __new__(subtype, dtype=np.uint8, shape=None, order='C'):

        # determine size of memory needed
        descr = dtypedescr(dtype)
        _dbytes = descr.itemsize

        if shape is None:
            raise ValueError("don't know how to handle no shape")
        else:
            if not isinstance(shape, tuple):
                shape = (shape,)
            size = np.intp(1)  # avoid default choice of np.int_, which might overflow
            for k in shape:
                size *= k

        # allocate memory
        mm = pymmcore.allocate_direct_memory(int(size*_dbytes))

        # construct array using supplied memory
        self = np.ndarray.__new__(subtype, shape=shape, dtype=dtype, buffer=mm,
                                  order=order)

        self._mm = mm
        
        return self

    def __del__(self):
        # free memory - not for persistent?
        pymmcore.free_direct_memory(self._mm)

    def __array_finalize__(self, obj):
        print('In array_finalize:')
        print('   self type is %s' % type(self))
        print('   obj type is %s' % type(obj))
        
        # if hasattr(obj, '_mmap') and np.may_share_memory(self, obj):
        #     self._mmap = obj._mmap
        #     self.filename = obj.filename
        #     self.offset = obj.offset
        #     self.mode = obj.mode
        # else:
        #     self._mmap = None
        #     self.filename = None
        #     self.offset = None
        #     self.mode = None

    # def __new__(cls, *args, **kwargs):
    #     print('In __new__ with class %s' % cls)
    #     return super(ndarray, cls).__new__(cls, *args, **kwargs)

#     def __init__(self, *args, **kwargs):
#         # in practice you probably will not need or want an __init__
#         # method for your subclass
#         for arg in args:
#             print("ARG:", arg);
#         for key, value in kwargs.items():
#             print("KWARG: {} is {}".format(key,value))
# #        print('In __init__ with class %s' % self.__class__)

def testX():
    import pymm
    import numpy as np
    y = pymm.ndarray(shape=(4,4),dtype=np.uint8)
    print("created ndarray subclass");
#    hdr = pymm.pymmcore.ndarray_header(y);
    print("size=", pymm.pymmcore.ndarray_header_size(y))
    return None

def test1():
    print('test1 running...')
    mr = MemoryResource()
    return None


###pointer, read_only_flag = a.__array_interface__['data']
