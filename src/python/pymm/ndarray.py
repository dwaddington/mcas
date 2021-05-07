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


dtypedescr = np.dtype

class ndarray(np.ndarray):
    '''
    ndarray that is stored in a memory resource
    '''
    __array_priority__ = -100.0 # what does this do?

    def __new__(subtype, global_name, shape=None, dtype=float, buffer=None, strides=None, order='C'):

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
        #        shape, dtype=float, buffer=None, offset=0, strides=None, order=None
        self = np.ndarray.__new__(subtype, dtype=dtype, shape=shape, buffer=mm,
                                  strides=strides, order=order)

        self._mm = mm
        
        return self

    def __del__(self):
        # free memory - not for persistent?
        pymmcore.free_direct_memory(self._mm)

    def __array_finalize__(self, obj): pass
