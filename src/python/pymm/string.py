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
import flatbuffers
import weakref
import PyMM.Meta.Header as Header
import PyMM.Meta.Constants as Constants
import PyMM.Meta.DataType as DataType

from flatbuffers import util
from .memoryresource import MemoryResource
from .shelf import Shadow
from .shelf import ShelvedCommon

class string(Shadow):
    '''
    string object that is stored in the memory resource.  because
    the object is string, read/write access is expected to be slow
    '''
    def __init__(self, string_value):
        self.string_value = string_value

    def make_instance(self, memory_resource: MemoryResource, name: str):
        '''
        Create a concrete instance from the shadow
        '''
        return shelved_string(memory_resource, name, self.string_value)

    def existing_instance(memory_resource: MemoryResource, name: str):
        '''
        Determine if an persistent named memory object corresponds to this type
        '''                        
        buffer = memory_resource.get_named_memory(name)
        if buffer is None:
            return (False, None)

        hdr_size = util.GetSizePrefix(buffer, 0)
        if(hdr_size != 28):
            return (False, None)

        root = Header.Header()
        hdr = root.GetRootAsHeader(buffer[4:], 0) # size prefix is 4 bytes
        print("detected 'string' type - magic:{}, hdrsize:{}".format(hex(int(hdr.Magic())),hdr_size))
        
        if(hdr.Magic() != Constants.Constants().Magic):
            return (False, None)

        if(hdr.Type() != DataType.DataType().Utf8String):
            return (False, None)
        
        return (True, shelved_string(memory_resource, name, buffer[hdr_size + 4:]))


class shelved_string(ShelvedCommon):
    def __init__(self, memory_resource, name, string_value):

        if not isinstance(name, str):
            raise RuntimeException("invalid name type")

        memref = memory_resource.open_named_memory(name)

        if memref == None:
            # create new value
            builder = flatbuffers.Builder(128)
            # create header
            Header.HeaderStart(builder)
            Header.HeaderAddMagic(builder, Constants.Constants().Magic)
            Header.HeaderAddVersion(builder, Constants.Constants().Version)
            Header.HeaderAddType(builder, DataType.DataType().Utf8String)
            hdr = Header.HeaderEnd(builder)
            builder.FinishSizePrefixed(hdr)
            hdr_ba = builder.Output()

            # allocate memory
            hdr_len = len(hdr_ba)
            value_len = len(string_value) + hdr_len

            memref = memory_resource.create_named_memory(name, value_len, 1, False)
            # copy into memory resource
            memref.buffer[0:hdr_len] = hdr_ba
            memref.buffer[hdr_len:] = bytes(string_value, 'utf-8')

            self.view = memoryview(memref.buffer[hdr_len:])
        else:
            self.view = memoryview(memref.buffer[32:])

        # hold a reference to the memory resource
        self._memory_resource = memory_resource
        self._value_named_memory = memref

    def __repr__(self):
        # TODO - some how this is keeping a reference? gc.collect() clears it.
        return str(self.view,'utf-8')
    
    def __len__(self):
        return len(self.view)

    def __getitem__(self, key):
        '''
        Magic method for slice handling
        '''
        s = str(self.view,'utf-8')
        if isinstance(key, int):
            return s[key]
        if isinstance(key, slice):
            return s.__getitem__(key)
        else:
            raise TypeError

    
