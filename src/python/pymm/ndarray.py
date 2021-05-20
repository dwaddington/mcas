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
    def __init__(self, shape=None, dtype=float, strides=None, order='C'):

        # todo check params
        # todo check and invalidate param 'buffer'
        # save constructor parameters and type
        self.__p_shape = shape
        self.__p_dtype = dtype
        self.__p_strides = strides
        self.__p_order = order

    def make_instance(self, memory_resource: MemoryResource, name: str):
        return shelved_ndarray(memory_resource,
                               name,
                               shape = self.__p_shape,
                               dtype = self.__p_dtype,
                               strides = self.__p_strides,
                               order = self.__p_order);
    def __str__(self):
        print('shadow ndarray')


# decorator to redirect and flush
def redirect_and_flush(F):
    def wrapper(*args):
        print(*args)
        return wrapper

        
# concrete subclass for ndarray
#
class shelved_ndarray(np.ndarray, ShelvedCommon):
    '''
    ndarray that is stored in a memory resource
    '''
    __array_priority__ = -100.0 # what does this do?

    def __new__(subtype, memory_resource, name, shape=None, dtype=float, strides=None, order='C'):

        # determine size of memory needed
        #
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
                
        value_named_memory = memory_resource.open_named_memory(name)
        metadata_key = name + '-meta'

        if value_named_memory == None:
            # create a newly allocated named memory from MemoryResource
            #
            value_named_memory = memory_resource.create_named_memory(name,
                                                                     int(size*_dbytes),
                                                                     8, # alignment
                                                                     True) # zero
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
            #metadata_named_memory = memory_resource.open_named_memory(name + '-meta')
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

    def update_metadata(self, array):
        print("updating metadata...")
        metadata = pymmcore.ndarray_header(array,np.dtype(dtype).str)
        self._memory_resource.put_named_memory(self._metadata_key, metadata)
        

    #
    # reference: https://numpy.org/doc/stable/reference/routines.array-manipulation.html
    #

    # in-place methods need to be transactional
    def fill(self, value):
        return super().value_only_transaction(super().fill, value)

    def byteswap(self, inplace):
        if inplace == True:
            return super().value_only_transaction(super().byteswap, True)
        else:
            return super().byteswap(False)    

    # in-place arithmetic
    def __iadd__(self, value):
        return super().value_only_transaction(super().__iadd__, value)

    def __imul__(self, value):
        return super().value_only_transaction(super().__imul__, value)

    def __isub__(self, value):
        return super().value_only_transaction(super().__isub__, value)
    # TODO... more

    # set item, e.g. x[2] = 2
    def __setitem__(self, position, x):
        return super().value_only_transaction(super().__setitem__, position, x)

    def flip(self, m, axis=None):
        return super().value_only_transaction(super().flip, m, axis)


    # operations that return new views on same data.  we want to change
    # the behavior to give a normal volatile version
    def reshape(self, shape, order='C'):
        x = np.array(self) # copy constructor
        return x.reshape(shape)
        
    def __del__(self):
        pass
        # delete the object (i.e. ref count == 0) means
        # releasing the memory - the object is not actually freed
        #
        #self._memory_resource.release_named_memory(self._value_handle)
        #self._memory_resource.release_named_memory(self._metadata_handle)        

    

    def __array_finalize__(self, obj):
        pass
