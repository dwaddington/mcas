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
        return super()._MemoryResource_create_named_memory(name, size, alignment, zero)

    @methodcheck(types=[str])
    def open_named_memory(self, name):
        '''
        Open existing name memory
        '''
        return super()._MemoryResource_open_named_memory(name)

    @methodcheck(types=[int])
    def release_named_memory(self, lock_handle):
        '''
        Release a contiguous piece of memory (i.e. unlock)
        '''
        super()._MemoryResource_release_named_memory(lock_handle)
    

