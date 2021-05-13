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

        
        # allocate memory from MemoryResource
        #
        (key_handle, buffer) = memory_resource.get_named_memory(name,
                                                                int(size*_dbytes),
                                                                8, # alignment
                                                                True) # zero

        # construct array using supplied memory
        #        shape, dtype=float, buffer=None, offset=0, strides=None, order=None
        self = np.ndarray.__new__(subtype, dtype=dtype, shape=shape, buffer=buffer,
                                  strides=strides, order=order)

        self._memory_resource = memory_resource
        self._allocations = [buffer]
        self.name = name
        return self

    def __del__(self):
        pass
        # free memory - not for persistent?
        #        for ma in self._allocations:
        #    pymmcore.free_direct_memory(ma)

    def __array_finalize__(self, obj): pass
