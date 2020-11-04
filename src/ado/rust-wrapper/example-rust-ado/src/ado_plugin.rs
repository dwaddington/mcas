/*
  Here is a skeleton implementation which would need implemented
  for a specific application
 */

use crate::Status;
use crate::Value;
use crate::size_t;
use crate::c_void;

/* callback services */
use crate::allocate_pool_memory;

pub fn do_work(_callback_ptr: *const c_void,
               _work_id: u64,
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

    let newmem = allocate_pool_memory(_callback_ptr, 128);
    println!("[RUST]: newly allocated mem {:?},{:?}", newmem.data, newmem.size);
    return 0;
}

pub fn register_mapped_memory(_shard_base: u64, _local_base: u64, _size: size_t) -> Status
{
    println!("[RUST]: register_mapped_memory (shard@{:#X} local@{:#X} size={})", _shard_base, _local_base, _size);
    return 0;
}



