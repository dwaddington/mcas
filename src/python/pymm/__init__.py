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

def testX():
    import pymm
    import numpy as np
    y = pymm.ndarray(shape=(4,4),dtype=np.uint8)
    print("created ndarray subclass");
#    hdr = pymm.pymmcore.ndarray_header(y);
    print("hdr=", pymm.pymmcore.ndarray_header(y))
    return None

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


###pointer, read_only_flag = a.__array_interface__['data']
