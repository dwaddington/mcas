/*
  Here is a skeleton implementation which would need implemented
  for a specific application
 */

use std::mem;
use std::vec;
use std::slice;
use std::io::prelude::*;
use std::io::{self, SeekFrom};
use std::fs::File;
use std::ffi::CStr;
use std::fmt::Write;
use std::time::{Duration, SystemTime, UNIX_EPOCH};
use crate::Status;
use crate::Value;
use crate::size_t;
use crate::c_void;
use crate::Response;
use crate::Request;

/* available callback services, see lib.rs */
use crate::allocate_pool_memory; /* allocate memory from the pool */
use crate::free_pool_memory; /* free memory back to the pool */

fn print_type_of<T>(_: &T) {
    println!("{}", std::any::type_name::<T>())
}

pub fn do_work(_callback_ptr: *const c_void,
               _work_id: u64,
               _key: String,
               _attached_value : &Value,
               _detached_value : &Value,
               _work_request : &Request,
               _new_root : bool,
               _response : &Response) -> Status
{
    println!("[RUST]: do_work (workid={:#X}, key={}, attached-value={:?}) new-root={:?}",
             _work_id, _key, _attached_value.buffer, _new_root);
//    println!("[RUST]: request={:?}", _work_request);
//    println!("[RUST]: request={:?}", std::str::from_utf8(_work_request).unwrap());


    /* write something into value memory */
    {
        let mut z = String::new();
        let start = SystemTime::now();
        let since_the_epoch = start.duration_since(UNIX_EPOCH).expect("Time went backwards");
        write!(z, "CLOCK-{:#?}", since_the_epoch).expect("writeln failed");

        _attached_value.copy_string_to(z).expect("copying into value failed");
    }

    /* allocate some memory from pool */
    let newmem = allocate_pool_memory(_callback_ptr, 128);
    println!("[RUST]: newly allocated mem {:?},{:?}", newmem.buffer, newmem.buffer_size);

    /* release it */
    free_pool_memory(_callback_ptr, newmem);
    println!("[RUST]: freed memory");

    /* set response */
    {
        let mut z = String::new();
        let start = SystemTime::now();
        let since_the_epoch = start.duration_since(UNIX_EPOCH).expect("Time went backwards");
        write!(z, "RESPONSE-{:#?}", since_the_epoch).expect("writeln failed");

        _response
            .copy_to(_work_request.buffer, _work_request.buffer_size)
            .expect("copy into response failed");
    }
    
    return 0;
}

pub fn register_mapped_memory(_shard_base: u64, _local_base: u64, _size: size_t) -> Status
{
    println!("[RUST]: register_mapped_memory (shard@{:#X} local@{:#X} size={})", _shard_base, _local_base, _size);
    return 0;
}


pub fn debug_break()
{
    unsafe { crate::debug_break() };
}
