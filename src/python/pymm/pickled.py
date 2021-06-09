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
import pickle
import PyMM.Meta.Header as Header
from .memoryresource import MemoryResource
from .shelf import Shadow
from .shelf import ShelvedCommon

class pickled(Shadow):
    '''
    pickled object that is stored in the memory resource.  because
    the object is pickled, read/write access is expected to be slow
    '''
    def __init__(self, obj):
        self.obj = obj

    def make_instance(self, memory_resource: MemoryResource, name: str):
        '''
        Create a concrete instance from the shadow
        '''
        return shelved_pickled(memory_resource, name, self.obj)


class shelved_pickled(ShelvedCommon):
    def __new__(subtype, memory_resource, name, obj):

        if not isinstance(name, str):
            raise RuntimeException("invalid name type")
        
        root = memory_resource.open_named_memory(name)

        if root == None:
            # create new entry
            pickstr = pickle.dumps(obj)
            value_len = 0
            print("value_len-->", value_len)
#            root_memref = memory_resource.create_named_memory(name, len(pickstr),value_len, 8, False)
#            print("len-->", value_len)
#            # prefix used for values corresponding to pickled items
#            member_prefix = "__pickled_" + str(root_memref.addr()[0]) + "__" + name + "_"
#            print(member_prefix)
        else:
            print("pickled already exists!!")
    

#class dict(mapping, **kwarg)
#class dict(iterable, **kwarg)
    
