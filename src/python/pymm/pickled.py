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

from flatbuffers import util
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
        return shelved_pickled(memory_resource, name, pickle.dumps(self.obj))

    def existing_instance(memory_resource: MemoryResource, name: str):
        '''
        Determine if an persistent named memory object corresponds to this type
        '''
        buffer = memory_resource.get_named_memory(name)
        if buffer is None:
            return (False, None)

        hdr_size = util.GetSizePrefix(buffer, 0)

        if(hdr_size != 32):
            return (False, None)
        
        root = Header.Header()
        hdr = root.GetRootAsHeader(buffer[4:], 0) # size prefix is 4 bytes
        print("detected 'pickled' type - magic:{}, hdrsize:{}".format(hex(int(hdr.Magic())),hdr_size))
        
        if(hdr.Magic() != Constants.Constants().Magic):
            return (False, None)

        print("OK! name={}".format(name))
        print("buffer_bytes=", buffer)
        pickle_bytes = buffer[hdr_size + 4:]
        print("pickle_bytes=", pickle_bytes)
        return (True, shelved_pickled(memory_resource, name, pickle_bytes))


class shelved_pickled(ShelvedCommon):
    def __init__(self, memory_resource, name, pickle_bytes):

        if not isinstance(name, str):
            raise RuntimeException("invalid name type")
        
        root = memory_resource.open_named_memory(name)

        if root == None:
            # create new value
            builder = flatbuffers.Builder(128)
            # create header
            Header.HeaderStart(builder)
            Header.HeaderAddMagic(builder, Constants.Constants().Magic)
            Header.HeaderAddVersion(builder, Constants.Constants().Version)
            Header.HeaderAddType(builder, DataType.DataType().Pickled)
            Header.HeaderAddResvd(builder, len(pickle_bytes))
            hdr = Header.HeaderEnd(builder)
            builder.FinishSizePrefixed(hdr)
            hdr_ba = builder.Output()

            # allocate memory
            value_len = len(hdr_ba) + len(pickle_bytes)
            hdr_len = len(hdr_ba)
            memref = memory_resource.create_named_memory(name, value_len, 8, False)
            # copy into memory resource
            print("type of {}".format(type(memref.buffer)))
            memref.buffer[0:hdr_len] = hdr_ba
            memref.buffer[hdr_len:] = pickle_bytes
            print("new pickled hdr={} content={}".format(hdr_ba, pickle_bytes))
            print("full buffer={}".format(memref.buffer[0:].tobytes()))

        self.pickle_bytes = pickle_bytes # create member reference to pickled bytes
        self.cached_object = pickle.loads(pickle_bytes)


    
#class dict(mapping, **kwarg)
#class dict(iterable, **kwarg)
    
