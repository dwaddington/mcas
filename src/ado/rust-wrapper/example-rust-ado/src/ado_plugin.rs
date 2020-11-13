/*
   Copyright [2020] [IBM Corporation]
   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at
       http://www.apache.org/licenses/LICENSE-2.0
   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

/*
 Here is a skeleton implementation which would need implemented
 for a specific application
*/

use std::fmt::Write;
use std::ptr;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::size_t;
use crate::status::Status;
use crate::ADOCallback;
use crate::KeyHandle;
use crate::Request;
use crate::Response;
use crate::Value;

#[allow(unused_imports)]
use crate::KeyLifetimeFlags;

fn print_type_of<T>(_: &T) {
    println!("{}", std::any::type_name::<T>())
}

/// Available callback services, see lib.rs

/// Up-called when the ADO is launched
///
/// Arguments:
///
/// * `auth_id`: Authentication identifier
/// * `pool_name` : Name of pool
/// * `pool_size` : Size of pool in bytes
/// * `pool_flags` : Flags passed to pool open/create (see mcas_itf.h)
/// * `memory_type` : Type of memory (1=>DRAM, 2=>PMEM)
/// * `expected_obj_count` : Expected object count passed to pool creation
/// * `params` : Concatenated additional parameters defined in configuration file (e.g. shard network endpoint)
///
pub fn launch_event(
    auth_id: u64,
    pool_name: &str,
    pool_size: size_t,
    pool_flags: u32,
    memory_type: u32,
    expected_obj_count: size_t,
    params: &str,
) {
    println!(
        "[RUST]: launch_event (auth={}, pool_name={}, pool_size={}, \
              flags={}, memory_type={}, expected_obj_count={})",
        auth_id, pool_name, pool_size, pool_flags, memory_type, expected_obj_count
    );
    println!("[RUST]: params -> {}", params);
}

/// Up-called when a cluster event happens (clustering must be enabled)
///
/// Arguments:
///
/// * `sender`: Authentication identifier
/// * `event_type` : Event type
/// * `message` : Message
///
pub fn cluster_event(sender: &str, event_type: &str, message: &str) {
    println!(
        "[RUST]: cluster event ({},{},{})",
        sender, event_type, message
    );
}

/// Main entry point up-called in response to invoke_ado/invoke_put_ado operations
///
/// Arguments:
///
/// * `services` : Provider of callback API
/// * `key` : Target key name
/// * `attached_value` : Value space associated with the key
/// * `detached_value` : Value space allocated through invoke_put_ado with
///                      IMCAS::ADO_FLAG_DETACHED (i.e. not attached to the key root pointer)
/// * `work_request` : Identifier for the invocation/work request
/// * `new_root` : If the key-value pair has been newly created, this is set to true. Can be
///                used to trigger data structure init.
/// * `response` : Return response from this layer
///
pub fn do_work(
    services: &ADOCallback,
    key: &str,
    attached_value: &Value,
    detached_value: &Value,
    work_request: &Request,
    new_root: bool,
    response: &Response,
) -> Status {
    println!(
        "[RUST]: do_work (key={}, attached-value={:?}, detached-value={:?}) new-root={:?}",
        key, attached_value, detached_value, new_root
    );
    println!("[RUST]: request={:#?}", work_request.as_string());

    /* get pool info */
    {
        let info = services.get_pool_info();
        println!("[RUST]: pool info {}", info);

        let parsed = json::parse(&info).unwrap();
        println!("[RUST]: pool_size={}", parsed["pool_size"]);
    }

    /* write something into value memory, terminate with null for C-side printing */
    {
        let mut z = String::new();
        let start = SystemTime::now();
        let since_the_epoch = start
            .duration_since(UNIX_EPOCH)
            .expect("Time went backwards");
        write!(z, "CLOCK-{:#?}\0", since_the_epoch).expect("writeln failed");

        attached_value
            .copy_string_to(z)
            .expect("copying into value failed");
    }

    /* allocate some memory from pool */
    let newmem = services.allocate_pool_memory(128);
    println!(
        "[RUST]: newly allocated mem {:?},{:?}",
        newmem._buffer, newmem._buffer_size
    );

    /* release it */
    services.free_pool_memory(newmem);
    println!("[RUST]: freed memory");

    /* use request string as a key name */
    let keyname = &work_request.as_string().into_string().unwrap();

    /* create a key */
    {
        let mut value = Value::new();
        let mut key_handle: KeyHandle = ptr::null_mut();
        let rc = services.create_key(keyname.to_string(), 256, None, &mut value, &mut key_handle);
        println!(
            "[RUST]: created key {:#?} handle: {:?} rc:{:?}",
            value, key_handle, rc
        );

        if rc == Status::Ok {
            /* unlock key */
            let rc = services.unlock_key(key_handle);
            assert!(rc == Status::Ok, "service.unlock failed");
        }

        /* resize key - must be unlocked */
        {
            services.resize_value(keyname.to_string(), 128);

            /* because this is not the current target key we
            need to reopen it */
            let rc = services.open_key(keyname.to_string(), None, &mut value, &mut key_handle);
            println!(
                "[RUST]: re-opened key {:#?} handle: {:?} rc:{:?}",
                value, key_handle, rc
            );
            assert!(rc == Status::Ok, "re-open key failed");

            if rc == Status::Ok {
                /* unlock key */
                let rc = services.unlock_key(key_handle);
                assert!(rc == Status::Ok, "service.unlock failed");
            }
        }
    }

    /* open key */
    {
        let mut value = Value::new();
        let mut key_handle: KeyHandle = ptr::null_mut();
        let rc = services.open_key(keyname.to_string(), None, &mut value, &mut key_handle);
        println!(
            "[RUST]: opened key {:#?} handle: {:?} rc:{:?}",
            value, key_handle, rc
        );
        assert!(rc == Status::Ok, "open key failed");

        if rc == Status::Ok {
            /* unlock key */
            let rc = services.unlock_key(key_handle);
            assert!(rc == Status::Ok, "service.unlock failed");
        }
    }

    /* resize current (target) value, need to unlock it first */
    {
        let new_value = services.resize_value(key.to_string(), attached_value._buffer_size - 4);
        println!("[RUST]: resized target value to {:?}", new_value);
    }

    /* set response */
    {
        let mut z = String::new();
        let start = SystemTime::now();
        let since_the_epoch = start
            .duration_since(UNIX_EPOCH)
            .expect("Time went backwards");
        write!(z, "RESPONSE-{:#?}", since_the_epoch).expect("writeln failed");

        unsafe {
            response
                .copy_string_to(z)
                .expect("copy into response failed")
        };
    }

    Status::Ok
}

pub fn register_mapped_memory(shard_base: u64, local_base: u64, size: size_t) -> Status {
    println!(
        "[RUST]: register_mapped_memory (shard@{:#X} local@{:#X} size={})",
        shard_base, local_base, size
    );
    Status::Ok
}

/// Up-called when a shutdown has been requested
///
///
pub fn shutdown() {}
