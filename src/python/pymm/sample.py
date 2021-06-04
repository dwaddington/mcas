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
import numpy as np
    
# create new shelf (override any existing myShelf)
#
s = pymm.shelf('myShelf',32,pmem_path='/mnt/pmem0',force_new=True)

# create variable x on shelf (using shadow type)
s.x = pymm.ndarray((1000,1000),dtype=np.float)

# perform in-place (on-shelf) operations
s.x.fill(3)
x_checksum = sum(s.x.tobytes()) # get checksum

# copy persistent to persistent
s.z = s.x

# remove objects from shelf
for item in s.items:
    s.erase(item)
        

