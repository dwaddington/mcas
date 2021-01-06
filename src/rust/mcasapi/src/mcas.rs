
#[allow(dead_code)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
#[allow(non_snake_case)]
#[allow(unused_variables)]
use crate::mcasapi_wrapper::*;

use std::ffi::{CString};
use std::os::raw::{c_char, c_void};

// USE THIS FOR DEBUG BREAK
//        unsafe { asm!("int3") };

fn c_convert(slice: &str) -> (CString, *const c_char) {
    let heap = CString::new(slice).unwrap();
    let heapref: *const c_char = heap.as_ptr();
    (heap, heapref)
}

fn c_convert_void(slice: &str) -> (CString, *const c_void) {
    let heap = CString::new(slice).unwrap();
    let heapref = heap.as_ptr() as *const c_void;
    (heap, heapref)
}

fn zero_vec(size: u64) -> Vec<u8> {
    vec![0; size as usize]
}

//----------------------------------------------------------------------------
pub struct Pool {
    pool: mcas_pool_t,
}

impl Drop for Pool {
    fn drop(&mut self) {
        let rc = unsafe { mcas_close_pool(self.pool) };
        if rc != 0 {
            panic!("unable to drop mcas::Pool object");
        }
    }
}

type ReturnCode = i32;

impl Pool {
    pub fn put(&mut self, key: &str, value: &str) -> ReturnCode
    {
        let (_key, key_cstr) = c_convert(key);
        let (_v, v_cstr) = c_convert_void(value);
        unsafe { mcas_put_ex(self.pool, key_cstr, v_cstr, value.len() as u64, 0 /* flags */) }
    }

    pub fn get(&mut self, key: &str) -> Vec<u8>
    {
//        let *mut *mut ::std::os::raw::c_void : out_value = std::ptr::null();
        let mut value_size : size_t = 0;
        let mut value_ptr : *mut std::os::raw::c_void = std::ptr::null_mut();
        let (_key, key_cstr) = c_convert(key);
        let x = unsafe { mcas_get(self.pool, key_cstr, &mut value_ptr, &mut value_size) };
        println!("value len={}", value_size);
        let mut result =  zero_vec(value_size + 1);
//        let result = unsafe { String::from_raw_parts(value_ptr as *mut u8, value_size as usize, value_size as usize) } ;
        
        // unsafe { mcas_free_memory(self.pool.session, value_ptr) };
        unsafe {std::ptr::copy_nonoverlapping::<u8>(value_ptr as *const u8, result.as_mut_ptr(), value_size as usize) };
        return result;
    }
}

//----------------------------------------------------------------------------
pub struct Session {
    session: mcas_session_t,
}

impl Session {
    pub fn new(url: &str, net_device: &str) -> Self {
        let (_ip, ip_cstr) = c_convert(url);
        let (_device, device_cstr) = c_convert(net_device);
        let session = unsafe { mcas_open_session_ex(ip_cstr, device_cstr, 3, 30) };
        Session { session }
    }

    pub fn open_pool(&mut self, pool_name: &str, flags: mcas_flags_t) -> Pool {
        let (_name, name_cstr) = c_convert(pool_name);
        let mut mcas_pool = mcas_pool_t {
            session: self.session,
            handle: 0,
        };
        let _rc = unsafe { mcas_open_pool(self.session, name_cstr, flags, &mut mcas_pool) };
        Pool { pool: mcas_pool }
    }

    pub fn create_pool(&mut self, pool_name: &str, size: u64, flags: mcas_flags_t) -> Pool {
        let (_name, name_cstr) = c_convert(pool_name);
        let mut mcas_pool = mcas_pool_t {
            session: self.session,
            handle: 0,
        };
        let _rc = unsafe { mcas_create_pool(self.session, name_cstr, size, flags, &mut mcas_pool) };
        Pool { pool: mcas_pool }
    }
}

impl Drop for Session {
    fn drop(&mut self) {
        unsafe {
            mcas_close_session(self.session);
        };
    }
}
