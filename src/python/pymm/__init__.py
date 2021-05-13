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

from .ndarray import ndarray
from .shelf import shelf
from .memoryresource import MemoryResource

class CustomList(list):
    def __getslice__(self,i,j):
        return CustomList(list.__getslice__(self, i, j))
    def __add__(self,rhs):
        return CustomList(list.__add__(self,rhs))
    def __mul__(self,rhs):
        return CustomList(list.__mul__(self,rhs))
    def __getitem__(self, item):
        result = list.__getitem__(self, item)
        try:
            return CustomList(result)
        except TypeError:
            return result

def testX():
    import pymm
    import numpy as np
    y = pymm.ndarray(shape=(4,4),dtype=np.uint8)
    print("created ndarray subclass");
#    hdr = pymm.pymmcore.ndarray_header(y);
    print("hdr=", pymm.pymmcore.ndarray_header(y))
    return None

def test_shelf():
    import pymm
    import numpy as np

    s = pymm.shelf('myShelf')
    s.x = pymm.ndarray((8,8),dtype=np.uint8)
    print(s.x)
    return s

def test1():
    print('test1 running...')
    mr = MemoryResource()
    return None

def test2():
    import pymm
    import numpy as np
    x = np.ndarray((8,8),dtype=np.uint8)
    print(pymm.pymmcore.ndarray_header(x))
    print(pymm.pymmcore.ndarray_header_size(x))

def test3():
    import pymm
    import numpy as np
    x = pymm.ndarray('foo',(4,4),dtype=np.uint8)
    print(x.__name__)
    print(x)

def test_arrow0():
    import pymm
    import pyarrow as pa
    data = [
        pa.array([1, 2, 3, 4]),
        pa.array(['foo', 'bar', 'baz', None]),
        pa.array([True, None, False, True])
    ]
    
    raw_buffer = pymm.pymmcore.allocate_direct_memory(2048)
    raw_buffer2 = pymm.pymmcore.allocate_direct_memory(2048)
    print(hex(pymm.pymmcore.memoryview_addr(raw_buffer)))
    buffer = pa.py_buffer(raw_buffer);
    buffer2 = pa.py_buffer(raw_buffer2);
    return pa.Array.from_buffers(pa.uint8(), 10, [buffer, buffer2])


###pointer, read_only_flag = a.__array_interface__['data']

# data = [
#     pa.array([1, 2, 3, 4]),
#     pa.array(['foo', 'bar', 'baz', None]),
#     pa.array([True, None, False, True])
# ]
# batch = pa.RecordBatch.from_arrays(data, ['f0', 'f1', 'f2'])
# table = pa.Table.from_batches([batch])

# raw_buffer = pymm.pymmcore.allocate_direct_memory(2048)
# raw_buffer2 = pymm.pymmcore.allocate_direct_memory(2048)
# print(hex(pymm.pymmcore.memoryview_addr(raw_buffer)))
# buffer = pa.py_buffer(raw_buffer);
# buffer2 = pa.py_buffer(raw_buffer2);
# return pa.Array.from_buffers(pa.uint8(), 10, [buffer, buffer2])
