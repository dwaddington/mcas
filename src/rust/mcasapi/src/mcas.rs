#![allow(dead_code)]
#![allow(non_camel_case_types)]
#![allow(non_upper_case_globals)]
#![allow(non_snake_case)]
#![allow(unused_variables)]
use crate::mcasapi_wrapper::*;

use std::ffi::CString;
use std::os::raw::{c_char, c_void};
use std::slice;

type Status = status_t;

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
/// Pool
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

impl Pool {
    /// Put a key-value pair into pool (copy performed)
    ///
    /// # Examples
    ///
    /// ```ignore
    /// pool.put("cat", "Jenny").expect("put failed");
    /// ```    
    pub fn put(&mut self, key: &str, value: &str) -> Result<(), Status> {
        let (_key, key_cstr) = c_convert(key);
        let (_v, v_cstr) = c_convert_void(value);
        let rc = unsafe {
            mcas_put_ex(
                self.pool,
                key_cstr,
                v_cstr,
                value.len() as u64,
                0, /* flags */
            )
        };
        if rc == 0 {
            return Ok(());
        }
        Err(rc)
    }

    /// Put a key-value pair into pool using direct memory
    ///
    /// # Examples
    ///
    /// ```ignore
    /// let m = session.allocate_direct_memory(1024);
    /// pool.put_direct("mykey", m).expect("put failed");
    /// ```    
    pub fn put_direct(&mut self, key: &str, direct_buffer: &DirectMemory) -> Result<(), Status> {
        let (_key, key_cstr) = c_convert(key);
        let rc = unsafe {
            mcas_put_direct_ex(
                self.pool,
                key_cstr,
                direct_buffer.ptr,
                direct_buffer.size,
                direct_buffer.handle,
                0,
            )
        };

        if rc != 0 {
            return Err(rc);
        }
        Ok(())
    }

    /// Get a value from pool using direct memory
    ///
    /// # Examples
    ///
    /// ```ignore
    /// let m = session.allocate_direct_memory(1024);
    /// pool.get_direct("cat", m);
    /// ```
    pub fn get_direct(
        &mut self,
        key: &str,
        direct_buffer: &DirectMemory,
    ) -> Result<size_t, Status> {
        let (_key, key_cstr) = c_convert(key);
        let mut inout_size = direct_buffer.size;
        let rc = unsafe {
            mcas_get_direct_ex(
                self.pool,
                key_cstr,
                direct_buffer.ptr,
                &mut inout_size,
                direct_buffer.handle,
            )
        };

        if rc != 0 {
            return Err(rc);
        }
        Ok(inout_size)
    }

    /// Get a value from pool (copy performed)
    ///
    /// # Examples
    ///
    /// ```ignore
    /// pool.get("cat").expect("get failed");
    /// ```
    pub fn get(&mut self, key: &str) -> Result<Vec<u8>, Status> {
        let mut value_size: size_t = 0;
        let mut value_ptr: *mut std::os::raw::c_void = std::ptr::null_mut();
        let (_key, key_cstr) = c_convert(key);
        let rc = unsafe { mcas_get(self.pool, key_cstr, &mut value_ptr, &mut value_size) };

        if rc != 0 {
            return Err(rc);
        }

        let mut result = zero_vec(value_size + 1);

        unsafe {
            std::ptr::copy_nonoverlapping::<u8>(
                value_ptr as *const u8,
                result.as_mut_ptr(),
                value_size as usize,
            )
        };

        if (unsafe { mcas_free_memory(self.pool.session, value_ptr) } != 0) {
            panic!("unexpected condition freeing memory from mcas_get");
        }

        Ok(result)
    }

    /// Configure pool
    ///
    /// # Examples
    /// ```ignore
    /// pool.configure("AddIndex::VolatileTree").expect("pool config failed");
    /// ```
    pub fn configure(&mut self, setting: &str) -> Result<(), Status> {
        let (_setting, setting_cstr) = c_convert(setting);
        let rc = unsafe { mcas_configure_pool(self.pool, setting_cstr) };

        if rc != 0 {
            return Err(rc);
        }
        Ok(())
    }

    /// Invoke ADO (Active Data Object)
    ///
    ///
    pub fn invoke_ado(
        &mut self,
        key: &str,
        request: &str,
        value_size: size_t,
    ) -> Result<AdoResponse, Status> {
        let (_key, key_cstr) = c_convert(key);
        let (_request, request_c_void) = c_convert_void(request);
        let mut response_vector: mcas_response_array_t = std::ptr::null_mut();
        let mut response_vector_len: size_t = 0;

        let rc = unsafe {
            mcas_invoke_ado(
                self.pool,
                key_cstr,
                request_c_void,
                request.len() as u64,
                0, // flags
                value_size,
                &mut response_vector,
                &mut response_vector_len,
            )
        };
        if rc != 0 {
            return Err(rc);
        }

        Ok(AdoResponse::new(response_vector, response_vector_len))
    }

    /// Combined put and ADO invoke
    ///
    ///
    pub fn invoke_put_ado(
        &mut self,
        key: &str,
        value: &str,
        request: &str,
        root_len: size_t,
    ) -> Result<AdoResponse, Status> {
        let (_key, key_cstr) = c_convert(key);
        let (_request, request_c_void) = c_convert_void(request);
        let (_v, v_cstr) = c_convert_void(value);
        let mut response_vector: mcas_response_array_t = std::ptr::null_mut();
        let mut response_vector_len: size_t = 0;

        let rc = unsafe {
            mcas_invoke_put_ado(
                self.pool,
                key_cstr,
                request_c_void,
                request.len() as u64,
                v_cstr,
                value.len() as u64,
                root_len,
                0, // flags
                &mut response_vector,
                &mut response_vector_len,
            )
        };
        if rc != 0 {
            return Err(rc);
        }

        Ok(AdoResponse::new(response_vector, response_vector_len))
    }
}

//----------------------------------------------------------------------------
#[derive(Debug)]
pub struct DirectMemory {
    session: mcas_session_t,
    handle: *mut ::std::os::raw::c_void,
    ptr: *mut ::std::os::raw::c_void,
    size: size_t,
}

impl DirectMemory {
    pub fn slice<'a, T>(&mut self) -> &'a mut [T] {
        unsafe { slice::from_raw_parts_mut(self.ptr as *mut T, self.size as usize) }
    }
}

impl Drop for DirectMemory {
    fn drop(&mut self) {
        unsafe {
            mcas_free_direct_memory(self.ptr);
        };
    }
}

//----------------------------------------------------------------------------
pub struct Session {
    session: mcas_session_t,
}

impl Session {
    pub fn new(url: &str, net_device: &str) -> Result<Self, Status> {
        let (_ip, ip_cstr) = c_convert(url);
        let (_device, device_cstr) = c_convert(net_device);
        let mut session: mcas_session_t = std::ptr::null_mut();
        let rc = unsafe { mcas_open_session_ex(ip_cstr, device_cstr, 3, 30, &mut session) };
        if rc != 0 {
            return Err(rc);
        }
        let return_value = Session { session };
        Ok(return_value)
    }

    /// Allocate direct memory for zero-copy operation
    ///
    pub fn allocate_direct_memory(
        &mut self,
        size_in_bytes: size_t,
    ) -> Result<DirectMemory, Status> {
        let mut handle: *mut std::os::raw::c_void = std::ptr::null_mut();
        let mut ptr: *mut std::os::raw::c_void = std::ptr::null_mut();
        let rc = unsafe {
            mcas_allocate_direct_memory(self.session, size_in_bytes, &mut ptr, &mut handle)
        };
        let result = DirectMemory {
            session: self.session,
            handle,
            ptr,
            size: size_in_bytes,
        };
        if rc != 0 {
            return Err(rc);
        }
        Ok(result)
    }

    /// Open a pool and return handle
    ///
    pub fn open_pool(&mut self, pool_name: &str, flags: mcas_flags_t) -> Result<Pool, Status> {
        let (_name, name_cstr) = c_convert(pool_name);
        let mut mcas_pool = mcas_pool_t {
            session: self.session,
            handle: 0,
        };
        let rc = unsafe { mcas_open_pool(self.session, name_cstr, flags, &mut mcas_pool) };

        if rc != 0 {
            return Err(rc);
        }

        let return_value = Pool { pool: mcas_pool };
        Ok(return_value)
    }

    /// Create a new pool
    ///
    pub fn create_pool(
        &mut self,
        pool_name: &str,
        size: u64,
        flags: mcas_flags_t,
    ) -> Result<Pool, Status> {
        let (_name, name_cstr) = c_convert(pool_name);
        let mut mcas_pool = mcas_pool_t {
            session: self.session,
            handle: 0,
        };
        let rc = unsafe { mcas_create_pool(self.session, name_cstr, size, flags, &mut mcas_pool) };

        if rc != 0 {
            return Err(rc);
        }

        let return_value = Pool { pool: mcas_pool };
        Ok(return_value)
    }

    pub fn delete_pool(&mut self, pool_name: &str) -> Result<(), Status> {
        let (_name, name_cstr) = c_convert(pool_name);
        let rc = unsafe { mcas_delete_pool(self.session, name_cstr) };
        if rc != 0 {
            return Err(rc);
        }
        Ok(())
    }
}

impl Drop for Session {
    fn drop(&mut self) {
        unsafe {
            mcas_close_session(self.session);
        };
    }
}

#[derive(Debug)]
pub struct AdoResponse {
    response_array: mcas_response_array_t,
    response_count: size_t,
}

impl AdoResponse {
    pub fn new(response_array: mcas_response_array_t, response_count: size_t) -> AdoResponse {
        AdoResponse {
            response_array,
            response_count,
        }
    }

    pub fn count(&self) -> usize {
        self.response_count as usize
    }

    pub fn slice<'a, T>(&mut self, index: usize) -> Result<&'a mut [T], Status> {
        let mut response_size: size_t = 0;
        let mut response_ptr: *mut std::os::raw::c_void = std::ptr::null_mut();
        unsafe {
            let rc = mcas_get_response(
                self.response_array,
                index as size_t,
                &mut response_ptr,
                &mut response_size,
            );
            if rc != 0 {
                return Err(rc);
            }
        }
        
        let result = unsafe { slice::from_raw_parts_mut(response_ptr as *mut T, response_size as usize) };
        Ok(result)
    }
}

impl Drop for AdoResponse {
    fn drop(&mut self) {
        unsafe { mcas_free_responses(self.response_array) }
    }
}
