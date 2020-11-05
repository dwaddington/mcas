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
type ResponseBuffer = Vec<u8>;  // ? need to work out response buffers.

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

#[repr(C)]
pub struct Response {
    pub buffer: *mut i8,
    pub buffer_size : size_t,
    pub used_size : size_t,
    pub layer_id : u32,
}

impl Response {

    pub fn copy_in_string(_s : String) {
    }

    pub fn as_string(&self) -> String {

        let x = unsafe { CString::from_raw(self.buffer) };
        let str_slice: &str = x.to_str().unwrap();
        let str_buf: String = str_slice.to_owned();  // if necessary
        return str_buf;
       
//        let &v = unsafe { &Vec::from_raw_parts(self.buffer, self.buffer_size, self.buffer_size) };
  //      return std::str::from_utf8(self.buffer).unwrap();

//        let r = String::from_utf8(v.to_vec()).unwrap();
 //       format!("as strring: {}", r);
        //      return r;
        //src1.iter().collect::<String>()

    }

    pub fn set_by_string(&mut self, _strvalue : &str) {

//        println!("unsafe ptr={:?} , {}", self.buffer, self.buffer_size);
        let mut _z = unsafe { &Vec::from_raw_parts(self.buffer, self.buffer_size, self.buffer_size) };
//        let _z : &mut [i8] = self.buffer; //unsafe { std::slice::from_raw_parts(self.buffer, self.buffer_size) };
        //        let mut _zz : &[u8] = _z.as_bytes();

//        _z[0] = 99;
 //       _z[1] = 99;
  //      _z[2] = 0;
        /*
        for _v in _strvalue.bytes() {            
            _z[count] = _v as i8;
            count += 1;
        }
        _z[count] = 0;
*/

//        let mut v_slice = unsafe{ &*( v_slice as *mut [u8] as *mut [i8] ) };
//        let mut  _dst : &[u8] = z;

        //let _x : &[u8] = _strvalue.as_bytes();
       // self.buffer[0] = 99;
//        self.buffer[.._strvalue.len()].copy_from_slice(_x);
        
//        let _slice = unsafe { std::slice::from_raw_parts(self.buffer, self.buffer_size) };

//        let x = &_slice[.._value.len()];
//        x.clone_from_slice(_slice);
        
        // let _v_slice = &_value[..].as_bytes();  // take a full slice of the string
        // let mut _dst = unsafe { Vec::from_raw_parts(self.buffer, self.buffer_size, self.buffer_size) };
        // _dst[.._v_slice.len()].copy_from_slice(_v_slice);
        // _dst[_v_slice.len()] = 0;
        // self.used_size = _value.len();
        // println!("[RUST]: set_by_string ={:?}", &_dst[0..10]);        

    }
}

//    let response = String::from_utf8(_response_buffer.to_vec()).unwrap();
//    println!("[RUST]: pre-response={:?} len={}", &response[0..10], response.len());

//    let s: String = "abcdefg".to_owned();
 //   

//    let mut response_slice = &response[..s_slice.len()];

//    _response_buffer[0] = 82;
    //    response_slice.copy_from_slice(s_slice.as_bytes());
//    _response_buffer[..s_slice.len()].copy_from_slice(s_slice);


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
    let slice = unsafe { std::slice::from_raw_parts_mut(_work_request, _work_request_len) };
//    let response_slice = unsafe { std::slice::from_raw_parts_mut(_response.buffer, _response.size) };
    
    return ado_plugin::do_work(_callback_ptr,
                               _work_id,
                               rstr.to_string(),
                               _attached_value,
                               _detached_value,
                               slice,
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
