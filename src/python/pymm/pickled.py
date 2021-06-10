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
import flatbuffers
import PyMM.Meta.Header as Header
import PyMM.Meta.Constants as Constants
import PyMM.Meta.DataType as DataType

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

    def existing_instance(memory_resource: MemoryResource, name: str):
        '''
        Determine if an persistent named memory object corresponds to this type
        '''
        buffer = memory_resource.get_named_memory(name)
        if buffer is None:
            return (False, None)

        print("recovered...")
        print(list(buffer))

        root = Header.Header()
        print(dir(root))
        root.Init(buffer, 0)
        print("Here!")
        print(root.Magic())
        print(root.Type())
        return (False, None)


class shelved_pickled(ShelvedCommon):
    def __new__(subtype, memory_resource, name, obj):

        if not isinstance(name, str):
            raise RuntimeException("invalid name type")
        
        root = memory_resource.open_named_memory(name)

        if root == None:
            # create new value
            pickstr = pickle.dumps(obj)
            builder = flatbuffers.Builder(128)
            # create header
            hdr = Header.CreateHeader(builder,
                                      Constants.Constants().Magic,
                                      DataType.DataType().Pickled,
                                      Constants.Constants().Version,
                                      len(pickstr))
            builder.Finish(hdr)
            hdr_ba = builder.Output()
            # allocate memory
            
            value_len = len(hdr_ba) + len(pickstr)
            hdr_len = len(hdr_ba)
            memref = memory_resource.create_named_memory(name, value_len, 8, False)
            # copy into memory resource
            memref.buffer[0:hdr_len] = hdr_ba
            memref.buffer[hdr_len:] = pickstr
            print(list(memref.buffer))
        else:
            print("pickled already exists!!")
    

#class dict(mapping, **kwarg)
#class dict(iterable, **kwarg)
    
