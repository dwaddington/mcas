#![allow(dead_code)]
#![allow(unused_imports)]

extern crate alloc;
extern crate libc;

mod ado_plugin;

use libc::{c_int, c_char, c_void, size_t};
use std::ffi::CString;
use std::ffi::CStr;
use std::ptr;
use std::str;
use core::ptr::null_mut;

type Status = c_int;
type ConstString = *const *const std::os::raw::c_char;

#[repr(C)]
pub struct Value {
    pub data: *mut i8,
    pub size: size_t,
}

impl Value {
    pub fn new() -> Value {
        Value {
            size: 0,
            data: null_mut()
        }
    }
}

extern {
    fn callback_allocate_memory(_callback_ptr : *const c_void,
                                _size: size_t) -> Value;
    
    fn callback_create_key(_work_id : u64,
                           _key : *const c_char,
                           _value_size : size_t,
                           _out_value: &mut Value) -> Status;
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
    let v;
    unsafe { v = callback_allocate_memory(_callback_ptr, _size) };
    return v;
}


#[no_mangle]
pub extern fn ffi_do_work(_callback_ptr : *const c_void,
                          _work_id: u64,
                          _key : *const c_char,
                          _attached_value : &Value,
                          _detached_value : &Value,
                          _work_request : *mut u8,
                          _work_request_len : size_t,
                          _new_root : bool) -> Status
{
    let rstr = unsafe { CStr::from_ptr(_key).to_str().unwrap() };
    let slice = unsafe { std::slice::from_raw_parts_mut(_work_request, _work_request_len) };
    
    return ado_plugin::do_work(_callback_ptr,
                               _work_id,
                               rstr.to_string(),
                               _attached_value,
                               _detached_value,
                               slice,
                               _new_root);
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
