#[allow(dead_code)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
#[allow(non_snake_case)]
#[allow(unused_variables)]
use crate::mcasapi_wrapper::*;

use std::ffi::CString;
use std::os::raw::{c_char, c_void};

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
        } else {
            return Err(rc);
        }
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

        return Ok(result);
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

    pub fn delete_pool(&mut self,
                       pool_name : &str) -> Result<(), Status> {
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
