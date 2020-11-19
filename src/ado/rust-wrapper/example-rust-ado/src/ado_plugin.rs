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
 Here is an example ADO plugin implementation which would need implemented
 for a specific application
*/

use libc::timespec;
use std::fmt::Write;
use std::ptr;
use std::time::{SystemTime, UNIX_EPOCH};

use crate::{
    size_t, status::Status, AdoCallback, AdoPlugin, FindType, IteratorHandle, KeyHandle,
    KeyLifetimeFlags, Reference, Request, Response, Value, PoolIterator, KeyIterator
};

/// Implement the plugin trait (this is an example)
///
impl AdoPlugin for crate::Plugin {
    fn launch_event(
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

    fn cluster_event(sender: &str, event_type: &str, message: &str) {
        println!(
            "[RUST]: cluster event ({},{},{})",
            sender, event_type, message
        );
    }

    fn do_work(
        services: &AdoCallback,
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
            let rc =
                services.create_key(keyname.to_string(), 256, None, &mut value, &mut key_handle);

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

        /* create some keys then iterate over them */
        {
            let count = 5;
            let mut value_vec = Vec::<Value>::new();
            value_vec.resize_with(count, Default::default);
            let mut handle_vec = Vec::<KeyHandle>::new();
            handle_vec.resize_with(count, || { ptr::null_mut() });

            for i in 0..count {
                let mut key_name = String::new();
                write!(key_name, "Object-{}", i).expect("write! failed");
                let s = key_name;

                handle_vec[i] = ptr::null_mut();
                let rc = services.create_key(
                    s.clone(),
                    256,
                    None,
                    &mut value_vec[i],
                    &mut handle_vec[i],
                );

                assert!(rc == Status::Ok, "failed to create key");

                println!("[RUST]: created key {}", s);
            }

            let mut rc = Status::Ok;
            let mut handle: IteratorHandle = ptr::null_mut();
            while rc == Status::Ok {
                /* now lets iterate over them */
                let mut reference: Reference = Reference::new();
                rc = services.iterate(
                    timespec {
                        tv_sec: 0,
                        tv_nsec: 0,
                    },
                    timespec {
                        tv_sec: 0,
                        tv_nsec: 0,
                    },
                    &mut handle,
                    &mut reference,
                );

                if rc == Status::Ok {
                    println!("[RUST]: iteration result {:?}", reference);
                }
            }
            
            
            /* lets try the Iterator trait version */
            {
                let iter = services.new_pool_iterator();
                for r in iter {
                    println!("[RUST]: iterator --> result {:?}", r);
                }
            }
            
            /* iterate via key find - index must be enabled */
            rc = Status::Ok;
            let mut position: i64 = 0;
            let mut count = 0;
            while rc == Status::Ok {
                let mut matched: String = String::new();

                rc = services.find_key("Object.*", FindType::Regex, &mut position, &mut matched);

                if rc == Status::Ok {
                    println!(
                        "[RUST]: find key returned '{}' at position {}",
                        matched, position
                    );
                    count += 1;
                }
                position += 1;
            }
            assert!(count == 5);

            /* now try through Iterator trait */
            {
                let iter = services.new_key_iterator("Object.*".to_string(), FindType::Regex);
                for key in iter {
                    println!("[RUST]: key iterator --> returned '{}'", key)
                }
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

    fn register_mapped_memory(shard_base: u64, local_base: u64, size: size_t) -> Status {
        println!(
            "[RUST]: register_mapped_memory (shard@{:#X} local@{:#X} size={})",
            shard_base, local_base, size
        );
        Status::Ok
    }

    fn shutdown() {
        println!("[RUST]: received shutdown notfication");
    }
}
