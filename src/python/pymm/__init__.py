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
import pymm
import pymmcore
import numpy as np

from .ndarray import ndarray
from .shelf import shelf
from .shelf import ShelvedCommon
from .memoryresource import MemoryResource
from .demo import demo

def test_shelf_dtor():
    import pymm
    import gc
    import sys
    
    s = pymm.shelf('myShelf',32,pmem_path='/mnt/pmem0',force_new=True)
    print(type(s))
    s.x = pymm.ndarray((1000,1000),dtype=np.float)
    s.y = pymm.ndarray((1000,1000),dtype=np.float)
    print(s.items)
    t = s.x
#    u = s.x
#    v = s.x

    del s
#    t.fill(8)
#    print(t)
#    del s
#    gc.collect()
    

    print('Shelf deleted explicitly')

#    print('refcnt(t)=', sys.getrefcount(t))
#    print('refcnt(u)=', sys.getrefcount(u))
#    print(t)
    
    
# def testX():
#     import pymm
#     import numpy as np
#     y = pymm.ndarray(shape=(4,4),dtype=np.uint8)
#     print("created ndarray subclass");
# #    hdr = pymm.pymmcore.ndarray_header(y);
#     print("hdr=", pymm.pymmcore.ndarray_header(y))
#     return None

# def test_shelf():
#     import pymm
#     import numpy as np

#     s = pymm.shelf('myShelf')
#     s.x = pymm.ndarray((8,8),dtype=np.uint8)

#     # implicity replace s.x (RHS constructor should succeed before existing s.x is erased)
# #    s.x = pymm.ndarray((3,3),dtype=np.uint8)
#     print(s.x)
#     return s

# def test_memoryresource():
#     import pymm
#     mr = pymm.MemoryResource('zzz', 1024)
#     nm = mr.create_named_memory('zimbar', 8)
#     print(nm.handle,hex(pymm.pymmcore.memoryview_addr(nm.buffer)))

# def test1():
#     print('test1 running...')
#     mr = MemoryResource()
#     return None

# def test2():
#     import pymm
#     import numpy as np
#     x = np.ndarray((8,8),dtype=np.uint8)
#     print(pymm.pymmcore.ndarray_header(x))
#     print(pymm.pymmcore.ndarray_header_size(x))

# def test3():
#     import pymm
#     import numpy as np
#     x = pymm.ndarray('foo',(4,4),dtype=np.uint8)
#     print(x.__name__)
#     print(x)

# def test_arrow0():
#     import pymm
#     import pyarrow as pa
#     data = [
#         pa.array([1, 2, 3, 4]),
#         pa.array(['foo', 'bar', 'baz', None]),
#         pa.array([True, None, False, True])
#     ]
    
#     raw_buffer = pymm.pymmcore.allocate_direct_memory(2048)
#     raw_buffer2 = pymm.pymmcore.allocate_direct_memory(2048)
#     print(hex(pymm.pymmcore.memoryview_addr(raw_buffer)))
#     buffer = pa.py_buffer(raw_buffer);
#     buffer2 = pa.py_buffer(raw_buffer2);
#     return pa.Array.from_buffers(pa.uint8(), 10, [buffer, buffer2])


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
