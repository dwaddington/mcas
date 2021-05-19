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

from numpy import uint8, ndarray, dtype, float
from .memoryresource import MemoryResource

dtypedescr = np.dtype

# shadow type for ndarray
#
class ndarray:
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
        

# concrete subclass for ndarray
#
class shelved_ndarray(np.ndarray):
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
            
            metadata_named_memory = memory_resource.create_named_memory(name + '-meta', len(metadata), 0, False);
            metadata_named_memory.buffer[:] = metadata
            print("Saved metadata OK ", len(metadata_named_memory.buffer))
            print("shape:", shape)
        else:
            # entity already exists, load metadata            
            metadata_named_memory = memory_resource.open_named_memory(name + '-meta')
            print("Opened metadata OK ", len(metadata_named_memory.buffer))
            hdr = pymmcore.ndarray_read_header(metadata_buffer)
            print("Read header:", hdr)
            self = np.ndarray.__new__(subtype, dtype=hdr['dtype'], shape=hdr['shape'], buffer=value_named_memory.buffer,
                                      strides=hdr['strides'], order=order)
            print("Recovered ndarray!")

        self._memory_resource = memory_resource
        self._value_named_memory = value_named_memory
        self._metadata_named_memory = metadata_named_memory
        self.name = name
        return self

    def convert_dtype(type_num):
        pass
            
    def __del__(self):
        pass
        # delete the object (i.e. ref count == 0) means
        # releasing the memory - the object is not actually freed
        #
        #self._memory_resource.release_named_memory(self._value_handle)
        #self._memory_resource.release_named_memory(self._metadata_handle)        

    

    def __array_finalize__(self, obj):
        pass
