/*
   Copyright [2020] [IBM Corporation]
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
       http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#[derive(PartialEq, Debug)]
#[repr(i32)]
pub enum Status {
    Ok   = 0,
    Fail = -1,
    Invalid = -2,
    InsufficientQuota = -3,
    NotFound = -4,
    Busy = -9,
    Taken = -10,
    BadParam = -13,
    NotImplemented = -18,
    SendTimeout = -20,
    RecvTimeout = -21,
    Timeout = -30,
    MaxReached = -31,
    NoIndex = -32,
    AlreadyLocked = -33,
    IterDisturbed = -34,
}
