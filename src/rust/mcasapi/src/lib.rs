#![feature(asm)]
#![feature(slice_fill)]

#[allow(dead_code)]
mod mcas;

#[allow(clippy::redundant_static_lifetimes)]
#[allow(dead_code)]
#[allow(unused_imports)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
#[allow(non_snake_case)]
#[allow(unused_variables)]
mod mcasapi_wrapper;

#[cfg(test)]
mod tests {

    use crate::mcas;
    use std::thread;
    use std::time::Duration;

    #[test]
    fn core() {
        let mut session =
            mcas::Session::new("10.0.0.101:11911", "mlx5_0").expect("session creation failed");

        {
            let mut pool = session
                .create_pool("myPool", 1024 * 1024, 0)
                .expect("create_pool failed");

            pool.configure("AddIndex::VolatileTree")
                .expect("pool config failed");

            pool.put("cat", "Jenny").unwrap();

            let x = pool.get("cat").unwrap();
            let xs = String::from_utf8(x.clone()).unwrap();
            println!("result=>{:?}< also as UTF8 ({})", x, xs);

            let mut m = session.allocate_direct_memory(64).unwrap();
            println!("allocate memory=>{:?}<", m);

            let data: &mut [u8] = m.slice();
            data[0] = 1;
            data[1] = 2;
            data[2] = 3;
            println!("slice=>{:?}<", data);

            /* put direct */
            pool.put_direct("dd", &m).expect("put_direct failed");

            data.fill(0); // reset elements to 0

            println!("slice=>{:?}<", data);

            /* get direct */
            let sz = pool.get_direct("dd", &m).expect("get_direct failed");
            println!("got {:?} bytes", sz);
            println!("slice after get_direct =>{:?}<", data);

            /* ADO invocation */
            {
                /* set up target key */
                pool.put("adokey", "Bonjour").expect("put failed");
                let mut response_v = pool
                    .invoke_ado("adokey", "ADO::HelloResponse", 8)
                    .expect("invoke_ado failed");
                println!("response count {}", response_v.count());
                println!("response vector {:?}", response_v);

                let response: &mut [u8] = response_v
                    .slice(0)
                    .expect("getting individual response failed");

                println!("response {:?}", std::str::from_utf8(response).unwrap());
            }

            /* ADO put invocation */
            {
                let mut response_v = pool
                    .invoke_put_ado("adokey2", "someValue", "ADO::HelloResponse", 8)
                    .expect("invoke_ado failed");
                println!("response count {}", response_v.count());
                println!("response vector {:?}", response_v);

                let response: &mut [u8] = response_v
                    .slice(0)
                    .expect("getting individual response failed");

                println!("response {:?}", std::str::from_utf8(response).unwrap());
            }
        } /* implicitly close pool */

        session.delete_pool("myPool").expect("pool deletion failed")
    }

    #[test]
    fn async_calls() {
        let mut session =
            mcas::Session::new("10.0.0.101:11911", "mlx5_0").expect("session creation failed");

        {
            let mut pool = session
                .create_pool("myPool", 1024 * 1024, 0)
                .expect("create_pool failed");

            /* async put */
            {
                let mut handle = pool
                    .async_put("asyncKey", "someValue")
                    .expect("async_put failed");
                println!("handle: {:?}", handle);

                while handle.check_completion().expect("check completion failed") == false {
                    println!("waiting {:?}", handle);
                }

                /* get it back */
                {
                    let x = pool.get("asyncKey").unwrap();
                    let xs = String::from_utf8(x.clone()).unwrap();
                    println!("async result=>{:?}< also as UTF8 ({})", x, xs);
                }
            }

            /* async put direct */
            {
                let mut m = session.allocate_direct_memory(64).unwrap();
                println!("allocate memory=>{:?}<", m);

                let data: &mut [u8] = m.slice();
                data[0] = b'A';
                data[1] = b'B';
                data[2] = b'C';
                data[3] = 0;
                println!("slice=>{:?}<", data);

                let mut handle = pool
                    .async_put_direct("asyncDirectKey", &m)
                    .expect("async_put_direct failed");
                println!("handle: {:?}", handle);

                while handle.check_completion().expect("check completion failed") == false {
                    println!("async_put_direct waiting for response ... {:?}", handle);
                    thread::sleep(Duration::from_millis(1));
                }

                /* get it back (normal way) */
                {
                    let x = pool.get("asyncDirectKey").unwrap();
                    let xs = String::from_utf8(x.clone()).unwrap();
                    println!("async result=>{:?}< also as UTF8 ({})", x, xs);
                }
            }

            /* async get direct */
            {
                /* make buffer larger than existing value to check return size */
                let mut m = session.allocate_direct_memory(80).unwrap();
                println!("allocate memory=>{:?}<", m);

                let data: &mut [u8] = m.slice();
                data.fill(0);

                let mut handle = pool
                    .async_get_direct("asyncDirectKey", &mut m)
                    .expect("async_get_direct failed");
                println!("handle: {:?}", handle);

                while handle.check_completion().expect("check completion failed") == false {
                    println!("async_put_direct waiting for response ... {:?}", handle);
                    thread::sleep(Duration::from_millis(1));
                }
                println!("handle: {:?}", handle);
                println!("slice after async_get_direct =>{:?}<", data);
            }
        } /* implicitly close pool */

        session.delete_pool("myPool").expect("pool deletion failed")
    }
}
