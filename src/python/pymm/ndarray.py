# 
#    Copyright [2021] [IBM Corporation]
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#        http://www.apache.org/licenses/LICENSE-2.0
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
#

#    Notes: not all methods hooked.

import pymmcore
import numpy as np
import copy

from numpy import uint8, ndarray, dtype, float
from .memoryresource import MemoryResource
from .shelf import Shadow
from .shelf import ShelvedCommon

dtypedescr = np.dtype
    
# shadow type for ndarray
#
class ndarray(Shadow):
    '''
    ndarray that is stored in a memory resource
    '''
    def __init__(self, shape, dtype=float, strides=None, order='C'):

        # todo check params
        # todo check and invalidate param 'buffer'
        # save constructor parameters and type
        self.__p_shape = shape
        self.__p_dtype = dtype
        self.__p_strides = strides
        self.__p_order = order

    def make_instance(self, memory_resource: MemoryResource, name: str):
        '''
        Create a concrete instance from the shadow
        '''
        return shelved_ndarray(memory_resource,
                               name,
                               shape = self.__p_shape,
                               dtype = self.__p_dtype,
                               strides = self.__p_strides,
                               order = self.__p_order)

    def existing_instance(memory_resource: MemoryResource, name: str):
        '''
        Determine if an persistent named memory object corresponds to this type
        '''
        metadata = memory_resource.get_named_memory(name + '-meta')
        if metadata is None:
            return None
        
        pymmcore.ndarray_read_header(memoryview(metadata))
        return shelved_ndarray(memory_resource, name, shape = None)

    def __str__(self):
        print('shadow ndarray')


    def build_from_copy(memory_resource: MemoryResource, name: str, array):
        new_array = shelved_ndarray(memory_resource,
                                    name,
                                    shape = array.shape,
                                    dtype = array.dtype,
                                    strides = array.strides)

        # now copy the data
        #new_array[:] = array
        np.copyto(new_array, array, casting='yes')
        return new_array



    
# concrete subclass for ndarray
#
class shelved_ndarray(np.ndarray, ShelvedCommon):
    '''
    ndarray that is stored in a memory resource
    '''
    __array_priority__ = -100.0 # what does this do?

    def __new__(subtype, memory_resource, name, shape, dtype=float, strides=None, order='C'):

        # determine size of memory needed
        #
        descr = dtypedescr(dtype)
        _dbytes = descr.itemsize

        if not shape is None:
            if not isinstance(shape, tuple):
                shape = (shape,)
            size = np.intp(1)  # avoid default choice of np.int_, which might overflow
            for k in shape:
                size *= k
                
        value_named_memory = memory_resource.open_named_memory(name)
        metadata_key = name + '-meta'

        if value_named_memory == None: # does not exist yet
            #
            # create a newly allocated named memory from MemoryResource
            #
            value_named_memory = memory_resource.create_named_memory(name,
                                                                     int(size*_dbytes),
                                                                     8, # alignment
                                                                     False) # zero
            # construct array using supplied memory
            #        shape, dtype=float, buffer=None, offset=0, strides=None, order=None
            self = np.ndarray.__new__(subtype, dtype=dtype, shape=shape, buffer=value_named_memory.buffer,
                                      strides=strides, order=order)

            # create and store metadata header
            metadata = pymmcore.ndarray_header(self,np.dtype(dtype).str)
            memory_resource.put_named_memory(metadata_key, metadata)

            print("Saved metadata OK ", len(metadata))
            print("shape:", shape)
        else:
            # entity already exists, load metadata            
            metadata = memory_resource.get_named_memory(metadata_key)
            print("Opened metadata OK ", len(metadata))
            hdr = pymmcore.ndarray_read_header(memoryview(metadata))
            print("Read header:", hdr)
            self = np.ndarray.__new__(subtype, dtype=hdr['dtype'], shape=hdr['shape'], buffer=value_named_memory.buffer,
                                      strides=hdr['strides'], order=order)
            print("Recovered ndarray!")

        self._memory_resource = memory_resource
        self._value_named_memory = value_named_memory
        self._metadata_key = metadata_key
        self.name = name
        return self

    def __delete__(self, instance):
        raise RuntimeError('cannot delete item: use shelf erase')

    def __array_wrap__(self, out_arr, context=None):
        # Handle scalars so as not to break ndimage.
        # See http://stackoverflow.com/a/794812/1221924
        print(">--> {} {} ".format(out_arr.ndim, context))
        if out_arr.ndim == 0:
            return out_arr[()]
        return np.ndarray.__array_wrap__(self, out_arr, context)

    def __getattr__(self, name):
        if name not in super().__dict__:
            raise AttributeError("'{}' object has no attribute '{}'".format(type(self),name))
        else:
            return super().__dict__[name]

    def asndarray(self):
        return self.view(np.ndarray)
    
    def update_metadata(self, array):
        print("updating metadata...")
        metadata = pymmcore.ndarray_header(array,np.dtype(dtype).str)
        self._memory_resource.put_named_memory(self._metadata_key, metadata)

    # each type will handle its own transaction methodology.  this
    # is because metadata may be dealt with differently
    #
    def _value_only_transaction(self, F, *args):
        self._value_named_memory.tx_begin()
        result = F(*args)
        self._value_named_memory.tx_commit()
    
    #
    # reference: https://numpy.org/doc/stable/reference/routines.array-manipulation.html
    #

    # in-place methods need to be transactional
    def fill(self, value):
        return self._value_only_transaction(super().fill, value)

    def byteswap(self, inplace):
        if inplace == True:
            return self._value_only_transaction(super().byteswap, True)
        else:
            return super().byteswap(False)    

    # in-place arithmetic
    def __iadd__(self, value):
        return self._value_only_transaction(super().__iadd__, value)

    def __imul__(self, value):
        return self._value_only_transaction(super().__imul__, value)

    def __isub__(self, value):
        return self._value_only_transaction(super().__isub__, value)
    # TODO... more

    # set item, e.g. x[2] = 2
    def __setitem__(self, position, x):
        return self._value_only_transaction(super().__setitem__, position, x)

    def flip(self, m, axis=None):
        return self._value_only_transaction(super().flip, m, axis)


    # operations that return new views on same data.  we want to change
    # the behavior to give a normal volatile version
    def reshape(self, shape, order='C'):
        x = np.array(self) # copy constructor
        return x.reshape(shape)
        

    # def __array_finalize__(self, obj):
    #     # ``self`` is a new object resulting from
    #     # ndarray.__new__(InfoArray, ...), therefore it only has
    #     # attributes that the ndarray.__new__ constructor gave it -
    #     # i.e. those of a standard ndarray.
    #     #
    #     # We could have got to the ndarray.__new__ call in 3 ways:
    #     # From an explicit constructor - e.g. InfoArray():
    #     #    obj is None
    #     #    (we're in the middle of the InfoArray.__new__
    #     #    constructor, and self.info will be set when we return to
    #     #    InfoArray.__new__)
    #     if obj is None: return
    #     # From view casting - e.g arr.view(InfoArray):
    #     #    obj is arr
    #     #    (type(obj) can be InfoArray)
    #     # From new-from-template - e.g infoarr[:3]
    #     #    type(obj) is InfoArray
    #     #
    #     # Note that it is here, rather than in the __new__ method,
    #     # that we set the default value for 'info', because this
    #     # method sees all creation of default objects - with the
    #     # InfoArray.__new__ constructor, but also with
    #     # arr.view(InfoArray).
    #     self.info = getattr(obj, 'info', None)
    #     self._memory_resource = getattr(obj, 'memory_resource', None)
    #     self._value_named_memory = getattr(obj, 'value_named_memory', None)
    #     self._metadata_key = getattr(obj, 'metadata_key', None)
    #     self.name = getattr(obj, 'name', None)

    #     # We do not need to return anything
