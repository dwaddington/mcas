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
import pymm
import numpy as np

from .memoryresource import MemoryResource
from .check import methodcheck

class shelf():
    def __init__(self, name, size_mb=32):
        self.name = name
        self.mr = MemoryResource(name, size_mb)
        # todo iterate data and check value-metadata pairs
        print(self.mr)

    def __setattr__(self, name, value):
#        if name in self.__dict__:
            # we need to explicitly delete the original shelved variable
#            print("ALREADY THRER!!!!")

        if isinstance(value, pymm.ndarray):
#            if name in self.__dict__:
#                old_value = self.__dict__[name]
#                tmp_name = name + 'pending'
#                new_instance = value.make_instance(self.mr, tmp_name)
#                
#                self.__dict__[name] = 
#            else:
#                print("NEW INSTANCE")
            self.__dict__[name] = value.make_instance(self.mr, name)
            return
        else:
            self.__dict__[name] = value




# NUMPY
#
# Note: all invocations define a transaction boundary
#-----------------------------------------------------
# import numpy as np
# import pyarrow as pa
#
# myShelf = pymm.shelf("/mnt/pmem0/data.pm")
#
# >> create an array in persistent memory
#
# myShelf.x = pymm.ndarray((9,9),dtype=np.uint8)
#
# >> create reference to array in persistent memoty
#
# X = myShelf.x
#
# >> in place modification (zero-copy)
#
# X.fill(8)
# X[1,1] = 99
#
# >> copy to DRAM
#
# c = X.copy()
#
# >> replication in persistent memory
#
# myShelf.z = X   or myshelf.z = myshelf.x
#
# >> remove from persistent memory
#
# myShelf.erase(myShelf.z)?
# myShelf.erase(z)



# APACHE ARROW
#------------------------------------------------
# import numpy as np
# import pyarrow as pa
#
# myShelf = pymm.shelf("/mnt/pmem0/data.pm")
#
# >> create an array of strings builder
#
# myShelf.x = pymm.arrow.StringBuilder()
#
# X = myShelf.x
#
# X.append("hello")
# X.append("world");
#
# X.finish();
#
# >> access immutable array (StringScalar type)
#
# print(X[0]) --> <pyarrow.StringScalar: 'hello'>
#
# >> create a schema ...
#

