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
from .check import methodcheck

class MemoryReference():
    def __init__(self, internal_handle, mr, memview):
        self.handle = internal_handle
        self.mr = mr
        self.buffer = memview

    def __del__(self):
        print("releasing named memory @", hex(pymmcore.memoryview_addr(self.buffer)))
        self.mr.release_named_memory_by_handle(self.handle)

    def addr(self):
        '''
        For debugging. Get address of memory view
        '''
        return (hex(pymmcore.memoryview_addr(self.buffer)), len(self.buffer))

    def tx_begin(self):
        '''
        Start consistent transaction
        '''
        print('tx_begin')

    def tx_commit(self):
        '''
        Commit consistent transaction
        '''
        print('tx_commit')
        
    def persist(self):
        '''
        Flush any cached memory (normally for persistence)
        '''
        self.mr._MemoryResource_persist_memory_view(self.buffer)


        
class MemoryResource(pymmcore.MemoryResource):
    '''
    MemoryResource represents a heap allocator and physical memory
    resources.  It is backed by an MCAS store component.

    '''
    def __init__(self, name, size_mb):
        self._named_memory = {}
        super().__init__(pool_name=name, size_mb=size_mb)
    
    @methodcheck(types=[str,int,int,bool])
    def create_named_memory(self, name, size, alignment=8, zero=True):
        '''
        Create a contiguous piece of memory and name it
        '''
        (handle, mem) = super()._MemoryResource_create_named_memory(name, size, alignment, zero)
        if handle == None:
            return None

        return MemoryReference(handle,self,mem)

    @methodcheck(types=[str])
    def open_named_memory(self, name):
        '''
        Open existing name memory
        '''
        (handle, mem) = super()._MemoryResource_open_named_memory(name)
        if handle == None:
            return None
        return MemoryReference(handle,self,mem)

    @methodcheck(types=[MemoryReference])
    def release_named_memory(self, ref : MemoryReference):
        '''
        Release a contiguous piece of memory (i.e. unlock)
        '''
        super()._MemoryResource_release_named_memory(ref.handle)

    @methodcheck(types=[int])
    def release_named_memory_by_handle(self, handle):
        '''
        Release a contiguous piece of memory (i.e. unlock)
        '''
        super()._MemoryResource_release_named_memory(handle)

    @methodcheck(types=[str])
    def erase_named_memory(self, name):
        '''
        Erase a named-memory object from the memory resource
        '''
        super()._MemoryResource_erase_named_memory(name)

