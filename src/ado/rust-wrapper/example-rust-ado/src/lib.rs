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
    pub buffer: *mut u8,
    pub buffer_size: size_t,
}

impl Value {
    pub fn new() -> Value {
        Value {
            buffer_size: 0,
            buffer: null_mut()
        }
    }

    pub fn copy_to(&self, _src : *const u8, _len : usize) -> Result<i32,i32> {
        if _len >= self.buffer_size {
            return Err(-1);
        }        
        unsafe { std::ptr::copy_nonoverlapping(_src, self.buffer, _len) } ;
        Ok(0)
    }

    pub fn copy_string_to(&self, _str : String) -> Result<i32,i32>  {
        let (ptr, len, _) = _str.into_raw_parts();
        self.copy_to(ptr, len)?;
        Ok(0)
    }

    pub fn as_string(&self) -> CString {
        let v : &[u8] = unsafe { slice::from_raw_parts(self.buffer, self.buffer_size) };
        let cstr : CString = std::ffi::CString::new(v).expect("CString::new failed");
        return cstr;
    }


}

type Request = Value;

#[repr(C)]
pub struct Response {
    pub buffer: *mut u8,
    pub buffer_size : size_t,
    pub used_size : size_t,
    pub layer_id : u32,
}

impl Response {

    pub fn copy_to(&self, _src : *const u8, _len : usize) -> Result<i32,i32> {
        if _len >= self.buffer_size {
            return Err(-1);
        }        
        unsafe { std::ptr::copy_nonoverlapping(_src, self.buffer, _len) } ;
        Ok(0)
    }

    pub fn copy_string_to(&self, _str : String) -> Result<i32,i32>  {
        let (ptr, len, _) = _str.into_raw_parts();
        self.copy_to(ptr, len)?;
        Ok(0)
    }

    pub fn set_by_string(&mut self, _strvalue : &str) {

//        let _v = unsafe { slice::from_raw_parts_mut(self.buffer, self.buffer_size) };
  //      _v[0] = 'X' as i8;
    //    _v[1] = 0;
    }
}



/* callback functions provided by C++-side */
extern {
    fn callback_allocate_pool_memory(_callback_ptr : *const c_void,
                                     _size: size_t) -> Value;

    fn callback_free_pool_memory(_callback_ptr : *const c_void,
                                 _value : Value) -> Status;

    fn callback_create_key(_work_id : u64,
                           _key : *const c_char,
                           _value_size : size_t,
                           _out_value: &mut Value) -> Status;
    fn debug_break();
    fn set_response(response_str: *const c_char );
}


/* ADO call back functions available to RUST side */
fn ado_create_key(_work_id : u64,
                  _key : String,
                  _value_size : size_t) -> Status
{
    let strptr = std::ffi::CString::new(_key).expect("CString::new failed").as_ptr();
    let mut out_value = Value::new();
    return unsafe { callback_create_key(_work_id,
                                        strptr,
                                        _value_size,
                                        &mut out_value) };
}

fn allocate_pool_memory(_callback_ptr : *const c_void,
                        _size: size_t) -> Value
{
    return unsafe { callback_allocate_pool_memory(_callback_ptr, _size) };
}

fn free_pool_memory(_callback_ptr : *const c_void,
                    _value : Value) -> Status
{
    return unsafe { callback_free_pool_memory(_callback_ptr, _value) };
}


#[no_mangle]
pub extern fn ffi_do_work(_callback_ptr : *const c_void,
                          _work_id: u64,
                          _key : *const c_char,
                          _attached_value : &Value,
                          _detached_value : &Value,
                          _work_request : *mut u8,
                          _work_request_len : size_t,
                          _new_root : bool,
                          _response : &mut Response) -> Status
{
    let rstr = unsafe { CStr::from_ptr(_key).to_str().unwrap() };

    let req = Request { buffer : _work_request,
                        buffer_size : _work_request_len};

    return ado_plugin::do_work(_callback_ptr,
                               _work_id,
                               rstr.to_string(),
                               _attached_value,
                               _detached_value,
                               &req,
                               _new_root,
                               _response);
}


#[no_mangle]
pub extern fn ffi_register_mapped_memory(_shard_base: u64,
                                         _local_base: u64,
                                         _size: size_t) -> Status
{
    return ado_plugin::register_mapped_memory(_shard_base, _local_base, _size);
}


// #[cfg(test)]
// mod tests {
//     #[test]
//     fn it_works() {
//         assert_eq!(2 + 2, 4);
//     }
// }
