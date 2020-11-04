#![allow(dead_code)]
#![allow(unused_imports)]

extern crate alloc;
extern crate libc;

use libc::{c_int, c_char, size_t};
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
    fn callback_allocate_memory(_size: size_t) -> Status;
    fn callback_create_key(_work_id : i64,
                           _key : *const c_char,
                           _value_size : size_t,
                           _out_value: &mut Value) -> Status;
}


/* ADO call back functions available to RUST side */
fn ado_create_key(_work_id : i64,
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
    
fn ado_allocate_memory(_size: size_t) -> Status
{
    unsafe { callback_allocate_memory(9) };
    return 0;
}


#[no_mangle]
pub extern fn ffi_do_work(_work_id: i64,
                          _key : *const c_char,
                          _attached_value : &Value,
                          _detached_value : &Value,
                          _work_request : *mut u8,
                          _work_request_len : size_t,
                          _new_root : bool) -> Status
{
    let rstr = unsafe { CStr::from_ptr(_key).to_str().unwrap() };
    let slice = unsafe { std::slice::from_raw_parts_mut(_work_request, _work_request_len) };
    
    return ado_plugin::do_work(_work_id,
                               rstr.to_string(),
                               _attached_value,
                               _detached_value,
                               slice,
                               _new_root);
}


#[no_mangle]
pub extern fn ffi_register_mapped_memory(_shard_base: i64,
                                         _local_base: i64,
                                         _size: size_t) -> Status
{
    return ado_plugin::register_mapped_memory(_shard_base, _local_base, _size);
}

/*
  Here is the skeleton implementation which would need implementation 
*/
mod ado_plugin {

    use crate::Status;
    use crate::Value;
    use crate::size_t;
    
    pub fn do_work(_work_id: i64,
                   _key: String,
                   _attached_value : &Value,
                   _detached_value : &Value,
                   _work_request : &[u8],
                   _new_root : bool) -> Status
    {
        println!("[RUST]: do_work (workid={:#X}, key={}, attached-value={:?}) new-root={:?}",
                 _work_id, _key, _attached_value.data, _new_root);
        println!("[RUST]: request={:?}", _work_request);
        println!("[RUST]: request={:?}", std::str::from_utf8(_work_request).unwrap());
        return 0;
    }

    pub fn register_mapped_memory(_shard_base: i64, _local_base: i64, _size: size_t) -> Status
    {
        println!("[RUST]: register_mapped_memory (shard@{:#X} local@{:#X} size={})", _shard_base, _local_base, _size);
        return 0;
    }

}


// #[cfg(test)]
// mod tests {
//     #[test]
//     fn it_works() {
//         assert_eq!(2 + 2, 4);
//     }
// }
