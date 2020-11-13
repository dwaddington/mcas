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

#![allow(dead_code)]
#![feature(vec_into_raw_parts)]
#![allow(unused_imports)]
#![allow(clippy::missing_safety_doc)]
#![allow(clippy::not_unsafe_ptr_arg_deref)]


extern crate json;
extern crate alloc;
extern crate libc;

mod ado_plugin;
mod status;

use core::ptr::{null_mut};
use libc::{c_char, c_uchar, c_uint, c_void, size_t};
use status::Status;
use std::ffi::CStr;
use std::ffi::CString;
use std::fmt;
use std::slice;

type KeyHandle = *mut c_void;

/* helpers */
fn convert_to_string(ptr: *const c_uchar, len: size_t) -> String {
    let slice = unsafe { slice::from_raw_parts(ptr, len) };
    let str: String = std::str::from_utf8(slice).unwrap().to_string();
    str
}

#[repr(u32)]
pub enum KeyLifetimeFlags {
    None = 0x0,
    AdoLifetimeUnlock = 0x21,
    NoImplicitUnlock = 0x22,
}

#[repr(C)]
pub struct Value {
    pub _buffer: *mut u8,
    pub _buffer_size: size_t,
}

impl Default for Value {
    fn default() -> Value {
        Value {
            _buffer: null_mut(),
            _buffer_size: 0,
        }
    }
}
impl fmt::Debug for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Value")
            .field("buffer", &self._buffer)
            .field("buffer_size", &self._buffer_size)
            .finish()
    }
}

impl Value {
    pub fn new() -> Value {
        Value {
            _buffer_size: 0,
            _buffer: null_mut(),
        }
    }
    pub unsafe fn copy_to(&self, _src: *const u8, _len: usize) -> Result<i32, i32> {
        if _len >= self._buffer_size {
            return Err(-1);
        }
        std::ptr::copy_nonoverlapping(_src, self._buffer, _len);
        Ok(0)
    }
    pub fn copy_string_to(&self, _str: String) -> Result<i32, i32> {
        let (ptr, len, _) = _str.into_raw_parts();
        unsafe { self.copy_to(ptr, len)? };
        Ok(0)
    }
    pub fn as_string(&self) -> CString {
        let v: &[u8] = unsafe { slice::from_raw_parts(self._buffer, self._buffer_size) };
        let cstr: CString = std::ffi::CString::new(v).expect("CString::new failed");
        cstr
    }
}

type Request = Value;

#[repr(C)]
pub struct Response {
    pub _buffer: *mut u8,
    pub _buffer_size: size_t,
    pub _used_size: size_t,
    pub _layer_id: u32,
}

impl fmt::Debug for Response {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Response")
            .field("buffer", &self._buffer)
            .field("buffer_size", &self._buffer_size)
            .field("used_size", &self._used_size)
            .field("layer_id", &self._layer_id)
            .finish()
    }
}

impl Response {
    pub unsafe fn copy_to(&self, src: *const u8, len: usize) -> Result<i32, i32> {
        if len >= self._buffer_size {
            return Err(-1);
        }
        std::ptr::copy_nonoverlapping(src, self._buffer, len);
        Ok(0)
    }

    pub unsafe fn copy_string_to(&self, _str: String) -> Result<i32, i32> {
        let (ptr, len, _) = _str.into_raw_parts();
        self.copy_to(ptr, len)?;
        Ok(0)
    }
}

/* TODO

 std::function<status_t(const std::string&     key_expression,
                           const offset_t         begin_position,
                           const IKVIndex::find_t find_type,
                           offset_t&              out_matched_position,
                           std::string&           out_matched_key)>
        find_key;

 std::function<status_t(const common::epoch_time_t             t_begin,
                           const common::epoch_time_t             t_end,
                           component::IKVStore::pool_iterator_t&  iterator,
                           component::IKVStore::pool_reference_t& reference)>
        iterate;

std::function<status_t(const uint64_t option)>
        configure;
*/

/// Callback table (implemented on C/C++ side)
extern "C" {
    fn callback_allocate_pool_memory(context: *const c_void, size: size_t) -> Value;

    fn callback_free_pool_memory(context: *const c_void, value: Value) -> Status;

    fn callback_get_pool_info(context: *const c_void) -> *mut c_void;

    fn callback_create_key(
        context: *const c_void,
        work_id: u64,
        key: *const c_char,
        value_size: size_t,
        flags: KeyLifetimeFlags,
        out_value: &mut Value,
        out_key_handle: &mut KeyHandle,
    ) -> Status;

    fn callback_open_key(
        context: *const c_void,
        work_id: u64,
        key: *const c_char,
        flags: KeyLifetimeFlags,
        out_value: &mut Value,
        out_key_handle: &mut KeyHandle,
    ) -> Status;

    fn callback_unlock_key(context: *const c_void, work_id: u64, key_handle: KeyHandle) -> Status;

    fn callback_resize_value(
        context: *const c_void,
        work_id: u64,
        key: *const c_char,
        new_value_size: size_t,
        out_new_value: &mut Value,
    ) -> Status;

    fn debug_break();
}

pub struct ADOCallback {
    _context: *const c_void,
    _work_id: u64,
}

impl ADOCallback {

    /// Allocate memory from the pool
    ///
    /// Arguments:
    ///
    /// * `size`: Size of memory to allocate in bytes.
    /// * returns Value structure (pointer,len) pair.
    ///
    /// # Examples
    ///
    /// ```
    /// let my_mem : Value = _services.allocate_pool_memory(128);
    /// ```
    pub fn allocate_pool_memory(&self, size: size_t) -> Value {
        unsafe { callback_allocate_pool_memory(self._context, size) }
    }

    /// Free memory from the pool
    ///
    /// Arguments:
    ///
    /// * `value`: Value structure previously returned from allocate_pool_memory
    ///
    /// # Examples
    ///
    /// ```
    /// _services.free_pool_memory(my_mem);
    /// ```    
    pub fn free_pool_memory(&self, value: Value) -> Status {
        unsafe { callback_free_pool_memory(self._context, value) }
    }

    /// Resize existing value associated with key.  If this
    /// is the invoke-target key then the new value is returned,
    /// otherwise, the key-value pair will need relocking
    ///
    /// Arguments:
    ///
    /// * `key`: Key
    /// * `new_size`: Size to resize to in bytes
    ///
    /// # Examples
    ///
    /// ```
    /// _services.resize_value("myKey", 256);
    /// ```    
    pub fn resize_value(&self, key: String, new_size: size_t) -> Option<Value> {
        let str = std::ffi::CString::new(key).expect("CString::new failed");
        let strptr = str.as_ptr();
        let mut new_value = Value::new();
        unsafe {
            callback_resize_value(
                self._context,
                self._work_id,
                strptr,
                new_size,
                &mut new_value,
            )
        };

        if new_value._buffer.is_null() {
            None
        } else {
            Some(new_value)
        }
    }

    /// Get pool information
    ///
    /// # Examples
    ///
    /// ```
    /// let info : String  = _services.get_pool_info();
    /// ```    
    pub fn get_pool_info(&self) -> String {
        let ptr = unsafe { callback_get_pool_info(self._context) };
        let s = unsafe {
            CStr::from_ptr(ptr as *const i8)
                .to_string_lossy()
                .into_owned()
        };

        let rv = s;
        unsafe { libc::free(ptr) };
        rv
    }

    /// Create new key-value pair in index (this pool)
    ///
    /// Arguments:
    ///
    /// * `key`: String based keyhandle
    /// * `value_size`: Size of value in bytes
    /// * `flags` : Flags that govern implicit locking
    /// * `out_value` : Returned value (ptr,len) in pool memory
    /// * `out_key_handle` : Handle that can be used to release lock explicitly
    /// * return status
    ///
    /// # Examples
    ///
    /// ```
    /// let mut value = Value::new();
    /// let mut key_handle : KeyHandle = ptr::null_mut();
    /// let rc = _services.create_key(keyname.to_string(),
    ///                              256, None, &mut value, &mut key_handle);
    /// assert!(rc == Status::Ok);
    /// ```    
    pub fn create_key(
        &self,
        key: String,
        value_size: size_t,
        flags: Option<KeyLifetimeFlags>,
        out_value: &mut Value,
        out_key_handle: &mut KeyHandle,
    ) -> Status {
        let str = std::ffi::CString::new(key).expect("CString::new failed");
        let strptr = str.as_ptr();
        match flags {
            None => unsafe {
                callback_create_key(
                    self._context,
                    self._work_id,
                    strptr,
                    value_size,
                    KeyLifetimeFlags::None,
                    out_value,
                    out_key_handle,
                )
            },
            Some(f) => unsafe {
                callback_create_key(
                    self._context,
                    self._work_id,
                    strptr,
                    value_size,
                    f,
                    out_value,
                    out_key_handle,
                )
            },
        }
    }

    /// Open existing key-value pair in index (this pool)
    ///
    /// Arguments:
    ///
    /// * `key`: String based keyhandle
    /// * `flags` : Flags that govern implicit locking
    /// * `out_value` : Returned value (ptr,len) in pool memory
    /// * `out_key_handle` : Handle that can be used to release lock explicitly
    /// * return status
    ///
    /// # Examples
    ///
    /// ```
    /// let mut value = Value::new();
    /// let mut key_handle : KeyHandle = ptr::null_mut();
    /// let rc = _services.create_key(keyname.to_string(),
    ///                               None, &mut value, &mut key_handle);
    /// assert!(rc == Status::Ok);
    /// ```    
    pub fn open_key(
        &self,
        key: String,
        flags: Option<KeyLifetimeFlags>,
        out_value: &mut Value,
        out_key_handle: &mut KeyHandle,
    ) -> Status {
        let str = std::ffi::CString::new(key).expect("CString::new failed");
        let strptr = str.as_ptr();
        match flags {
            None => unsafe {
                callback_open_key(
                    self._context,
                    self._work_id,
                    strptr,
                    KeyLifetimeFlags::None,
                    out_value,
                    out_key_handle,
                )
            },
            Some(f) => unsafe {
                callback_open_key(
                    self._context,
                    self._work_id,
                    strptr,
                    f,
                    out_value,
                    out_key_handle,
                )
            },
        }
    }

    /// Unlock explicitly locked key.  
    ///
    /// # Examples
    ///
    /// ```
    /// _services.unlock(key_handle)
    /// ```    
    pub fn unlock_key(&self, key_handle: KeyHandle) -> Status {
        unsafe { callback_unlock_key(self._context, self._work_id, key_handle) }
    }
}

#[no_mangle]
pub extern "C" fn ffi_do_work(
    context: *const c_void,
    work_id: u64,
    key: *const c_uchar,
    key_len: size_t,
    attached_value: &Value,
    detached_value: &Value,
    work_request: *mut u8,
    work_request_len: size_t,
    new_root: bool,
    response: &mut Response,
) -> Status {
    /* create slice from potentially non terminated C string */
    let slice = unsafe { slice::from_raw_parts(key, key_len) };

    /* create String from slice */
    let rstr: String = std::str::from_utf8(slice).unwrap().to_string();

    let req = Request {
        _buffer: work_request,
        _buffer_size: work_request_len,
    };

    let services = ADOCallback {
        _context: context,
        _work_id: work_id,
    };

    ado_plugin::do_work(
        &services,
        &rstr,
        attached_value,
        detached_value,
        &req,
        new_root,
        response,
    )
}

#[no_mangle]
pub extern "C" fn ffi_register_mapped_memory(
    shard_base: u64,
    local_base: u64,
    size: size_t,
) -> Status {
    ado_plugin::register_mapped_memory(shard_base, local_base, size)
}

#[no_mangle]
pub extern "C" fn ffi_launch_event(
    auth_id: u64,
    pool_name_ptr: *const c_uchar,
    pool_name_len: size_t,
    pool_size: size_t,
    pool_flags: c_uint,
    memory_type: c_uint,
    expected_obj_count: size_t,
    json_params_ptr: *const c_uchar,
    json_params_len: size_t,
) {
    let pool_name = convert_to_string(pool_name_ptr, pool_name_len);
    let json_params = convert_to_string(json_params_ptr, json_params_len);
    ado_plugin::launch_event(
        auth_id,
        &pool_name,
        pool_size,
        pool_flags,
        memory_type,
        expected_obj_count,
        &json_params,
    );
}

#[no_mangle]
pub extern "C" fn ffi_cluster_event(
    sender_ptr: *const c_uchar,
    sender_len: size_t,
    event_type_ptr: *const c_uchar,
    event_type_len: size_t,
    message_ptr: *const c_uchar,
    message_len: size_t,
) {
    let sender = convert_to_string(sender_ptr, sender_len);
    let event_type = convert_to_string(event_type_ptr, event_type_len);
    let message = convert_to_string(message_ptr, message_len);
    ado_plugin::cluster_event(&sender, &event_type, &message);
}

#[no_mangle]
pub extern "C" fn ffi_shutdown()
{
    ado_plugin::shutdown();
}
