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
import struct

import PyMM.Meta.Header as Header
import PyMM.Meta.Constants as Constants
import PyMM.Meta.DataType as DataType

from flatbuffers import util
from .memoryresource import MemoryResource
from .shelf import Shadow
from .shelf import ShelvedCommon

class number(Shadow):
    '''
    floatnumber object that is stored in the memory resource.  
    '''
    def __init__(self, number_value):
        self.number_value = number_value

    def make_instance(self, memory_resource: MemoryResource, name: str):
        '''
        Create a concrete instance from the shadow
        '''
        return shelved_number(memory_resource, name, number_value=self.number_value)

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

        if(hdr.Magic() != Constants.Constants().Magic):
            return (False, None)

        if (hdr.Type() == DataType.DataType().NumberFloat or
            hdr.Type() == DataType.DataType().NumberInteger):
            return (True, shelved_number(memory_resource, name, buffer[hdr_size + 4:]))

        # not a string
        return (False, None)


class shelved_number(ShelvedCommon):
    '''Shelved string with multiple encoding support'''
    def __init__(self, memory_resource, name, number_value):

        memref = memory_resource.open_named_memory(name)

        if memref == None:
            # create new value
            builder = flatbuffers.Builder(32)
            # create header
            Header.HeaderStart(builder)
            Header.HeaderAddMagic(builder, Constants.Constants().Magic)
            Header.HeaderAddVersion(builder, Constants.Constants().Version)

            if isinstance(number_value, float):
                Header.HeaderAddType(builder, DataType.DataType().NumberFloat)
                self._type = DataType.DataType().NumberFloat
            else:
                Header.HeaderAddType(builder, DataType.DataType().NumberInteger)
                self._type = DataType.DataType().NumberInteger
                                
            hdr = Header.HeaderEnd(builder)
            builder.FinishSizePrefixed(hdr)
            hdr_ba = builder.Output()

            # allocate memory
            hdr_len = len(hdr_ba)

            if isinstance(number_value, float):
                value_bytes = str.encode(number_value.hex())
            else:
                value_bytes = number_value.to_bytes((number_value.bit_length() + 7) // 8, 'big')
                
            value_len = hdr_len + len(value_bytes) 

            memref = memory_resource.create_named_memory(name, value_len, 1, False)
            # copy into memory resource
            memref.buffer[0:hdr_len] = hdr_ba
            memref.buffer[hdr_len:] = value_bytes
        else:

            hdr_size = util.GetSizePrefix(memref.buffer, 0)
            if hdr_size != 28:
                raise RuntimeError("invalid header for '{}'; prior version?".format(varname))
            
            root = Header.Header()
            hdr = root.GetRootAsHeader(memref.buffer[4:], 0) # size prefix is 4 bytes
            
            if(hdr.Magic() != Constants.Constants().Magic):
                raise RuntimeError("bad magic number - corrupt data?")
                
            self._type = hdr.Type()

        # set up the view of the data
        self._view = memoryview(memref.buffer[32:])

        # hold a reference to the memory resource
        self._memory_resource = memory_resource

    def _get_value(self):
        '''
        Materialize the value from persistent bytes
        '''
        if self._type == DataType.DataType().NumberFloat:
            return float.fromhex((bytearray(self._view)).decode())
        else:
            return int.from_bytes(bytearray(self._view),'big')


    def __repr__(self):
        return str(self._get_value())

    def __float__(self):
        return float(self._get_value())

    def __int__(self):
        return int(self._get_value())

        
