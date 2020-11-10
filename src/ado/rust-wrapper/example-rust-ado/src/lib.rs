#![allow(dead_code)]
#![feature(vec_into_raw_parts)]

extern crate alloc;
extern crate libc;

mod ado_plugin;

use libc::{c_int, c_char, c_uchar, c_void, size_t};
use std::ffi::CString;
//use std::ffi::CStr;
use std::fmt;
use std::slice;
use core::ptr::null_mut;

type Status = c_int;
type KeyHandle = *mut c_void;

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
                           flags : KeyLifetimeFlags,
                           out_value: &mut Value,
                           out_key_handle : &mut KeyHandle) -> Status;

    fn callback_open_key(context : *const c_void,
                         work_id : u64,
                         key : *const c_char,
                         flags : KeyLifetimeFlags,
                         out_value: &mut Value,
                         out_key_handle : &mut KeyHandle) -> Status;

    fn callback_unlock_key(context : *const c_void,
                           work_id : u64,
                           key_handle : KeyHandle) -> Status;

    fn debug_break();
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
    
    pub fn create_key(&self,
                      key : String,
                      value_size : size_t,
                      flags : Option<KeyLifetimeFlags>,
                      out_value : &mut Value,
                      out_key_handle : &mut KeyHandle) -> Status
    {
        let str = std::ffi::CString::new(key).expect("CString::new failed");
        let strptr = str.as_ptr();
        match flags {
            None => return unsafe { callback_create_key(self._context,
                                                        self._work_id,
                                                        strptr,
                                                        value_size,
                                                        KeyLifetimeFlags::None,
                                                        out_value,
                                                        out_key_handle) },
            Some(f) => return unsafe { callback_create_key(self._context,
                                                           self._work_id,
                                                           strptr,
                                                           value_size,
                                                           f,
                                                           out_value,
                                                           out_key_handle) }
        }
    }
    
    pub fn open_key(&self,
                    key : String,
                    flags : Option<KeyLifetimeFlags>,
                    out_value : &mut Value,
                    out_key_handle : &mut KeyHandle) -> Status
    {
        let str = std::ffi::CString::new(key).expect("CString::new failed");
        let strptr = str.as_ptr();
        match flags {
            None => return unsafe { callback_open_key(self._context,
                                                      self._work_id,
                                                      strptr,
                                                      KeyLifetimeFlags::None,
                                                      out_value,
                                                      out_key_handle) },
            Some(f) => return unsafe { callback_open_key(self._context,
                                                         self._work_id,
                                                         strptr,
                                                         f,
                                                         out_value,
                                                         out_key_handle) }
        }
    }

    pub fn unlock_key(&self, key_handle : KeyHandle) -> Status
    {
        return unsafe { callback_unlock_key(self._context,
                                            self._work_id,
                                            key_handle) };
    }
        
}

#[no_mangle]
pub extern fn ffi_do_work(context : *const c_void,
                          work_id: u64,
                          key : *const c_uchar,
                          key_len : size_t,
                          attached_value : &Value,
                          detached_value : &Value,
                          work_request : *mut u8,
                          work_request_len : size_t,
                          new_root : bool,
                          response : &mut Response) -> Status
{
    /* create slice from potentially non terminated C string */
    let slice = unsafe { slice::from_raw_parts(key, key_len) };

    /* create String from slice */
    let rstr : String = std::str::from_utf8(slice).unwrap().to_string();

    let req = Request { _buffer : work_request,
                        _buffer_size : work_request_len};

    let services = ADOCallback { _context : context, _work_id : work_id };

    return ado_plugin::do_work(&services,
                               rstr,
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

