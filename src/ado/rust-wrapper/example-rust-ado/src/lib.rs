#![allow(dead_code)]
#![allow(unused_imports)]
#![feature(vec_into_raw_parts)]

extern crate alloc;
extern crate libc;

mod ado_plugin;

use libc::{c_int, c_char, c_void, size_t};
use std::ffi::CString;
use std::ffi::CStr;
use std::ptr;
use std::str;
use std::slice;
use core::ptr::null_mut;
use std::borrow::Cow;

type Status = c_int;
type ConstString = *const *const std::os::raw::c_char;
type ResponseBuffer = Vec<u8>;  // ? need to work out response buffers.

#[repr(C)]
pub struct Value {
    pub _buffer: *mut u8,
    pub _buffer_size: size_t,
}

impl Value {
    pub fn new() -> Value {
        Value {
            _buffer_size: 0,
            _buffer: null_mut()
        }
    }
    pub fn copy_to(&self, _src : *const u8, _len : usize) -> Result<i32,i32> {
        if _len >= self._buffer_size {
            return Err(-1);
        }        
        unsafe { std::ptr::copy_nonoverlapping(_src, self._buffer, _len) } ;
        Ok(0)
    }
    pub fn copy_string_to(&self, _str : String) -> Result<i32,i32>  {
        let (ptr, len, _) = _str.into_raw_parts();
        self.copy_to(ptr, len)?;
        Ok(0)
    }
    pub fn as_string(&self) -> CString {
        let v : &[u8] = unsafe { slice::from_raw_parts(self._buffer, self._buffer_size) };
        let cstr : CString = std::ffi::CString::new(v).expect("CString::new failed");
        return cstr;
    }
}

type Request = Value;

#[repr(C)]
pub struct Response {
    pub _buffer: *mut u8,
    pub _buffer_size : size_t,
    pub _used_size : size_t,
    pub _layer_id : u32,
}

impl Response {
    pub fn copy_to(&self, _src : *const u8, _len : usize) -> Result<i32,i32> {
        if _len >= self._buffer_size {
            return Err(-1);
        }        
        unsafe { std::ptr::copy_nonoverlapping(_src, self._buffer, _len) } ;
        Ok(0)
    }

    pub fn copy_string_to(&self, _str : String) -> Result<i32,i32>  {
        let (ptr, len, _) = _str.into_raw_parts();
        self.copy_to(ptr, len)?;
        Ok(0)
    }
}

/* callback functions provided by C++-side */
extern {
    fn callback_allocate_pool_memory(context : *const c_void,
                                     size: size_t) -> Value;

    fn callback_free_pool_memory(context : *const c_void,
                                 value : Value) -> Status;

    fn callback_create_key(context : *const c_void,
                           work_id : u64,
                           key : *const c_char,
                           value_size : size_t,
                           out_value: &mut Value) -> Status;
    fn debug_break();
    fn set_response(response_str: *const c_char );
}

pub struct ADOCallback {
    _context : *const c_void,
    _work_id : u64,
}

impl ADOCallback {
    pub fn new(context : *const c_void, work_id: u64) -> ADOCallback {
        ADOCallback {
            _context : context,
            _work_id : work_id,
        }
    }
    pub fn allocate_pool_memory(&self, size: size_t) -> Value
    {
        return unsafe { callback_allocate_pool_memory(self._context, size) };
    }
    pub fn free_pool_memory(&self, value : Value) -> Status
    {
        return unsafe { callback_free_pool_memory(self._context, value) };
    }
    pub fn create_key(&self, key : String, value_size : size_t) -> Status
    {
        let str = std::ffi::CString::new(key).expect("CString::new failed");
        let strptr = str.as_ptr();
        let mut out_value = Value::new();
        return unsafe { callback_create_key(self._context,
                                            self._work_id,
                                            strptr,
                                            value_size,
                                            &mut out_value) };
    }
}

#[no_mangle]
pub extern fn ffi_do_work(context : *const c_void,
                          work_id: u64,
                          key : *const c_char,
                          attached_value : &Value,
                          detached_value : &Value,
                          work_request : *mut u8,
                          work_request_len : size_t,
                          new_root : bool,
                          response : &mut Response) -> Status
{
    let rstr = unsafe { CStr::from_ptr(key).to_str().unwrap() };

    let req = Request { _buffer : work_request,
                        _buffer_size : work_request_len};

    let services = ADOCallback { _context : context, _work_id : work_id };

    return ado_plugin::do_work(&services,
                               rstr.to_string(),
                               attached_value,
                               detached_value,
                               &req,
                               new_root,
                               response);
}

#[no_mangle]
pub extern fn ffi_register_mapped_memory(_shard_base: u64,
                                         _local_base: u64,
                                         _size: size_t) -> Status
{
    return ado_plugin::register_mapped_memory(_shard_base, _local_base, _size);
}

