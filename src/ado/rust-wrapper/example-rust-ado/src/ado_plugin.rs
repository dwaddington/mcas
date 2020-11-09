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
use crate::ADOCallback;

/* available callback services, see lib.rs */

fn print_type_of<T>(_: &T) {
    println!("{}", std::any::type_name::<T>())
}

pub fn do_work(_services: &ADOCallback,
               _key: String,
               _attached_value : &Value,
               _detached_value : &Value,
               _work_request : &Request,
               _new_root : bool,
               _response : &Response) -> Status
{
    println!("[RUST]: do_work (key={}, attached-value={:?}) new-root={:?}",
             _key, _attached_value._buffer, _new_root);
    println!("[RUST]: request={:#?}", _work_request.as_string());


    /* write something into value memory */
    {
        let mut z = String::new();
        let start = SystemTime::now();
        let since_the_epoch = start.duration_since(UNIX_EPOCH).expect("Time went backwards");
        write!(z, "CLOCK-{:#?}", since_the_epoch).expect("writeln failed");

        _attached_value.copy_string_to(z).expect("copying into value failed");
    }

    /* allocate some memory from pool */
    let newmem = _services.allocate_pool_memory(128);
    println!("[RUST]: newly allocated mem {:?},{:?}",
             newmem._buffer,
             newmem._buffer_size);

    /* release it */
    _services.free_pool_memory(newmem);
    println!("[RUST]: freed memory");

    /* set response */
    {
        let mut z = String::new();
        let start = SystemTime::now();
        let since_the_epoch = start.duration_since(UNIX_EPOCH).expect("Time went backwards");
        write!(z, "RESPONSE-{:#?}", since_the_epoch).expect("writeln failed");

        _response
            .copy_string_to(z)
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
