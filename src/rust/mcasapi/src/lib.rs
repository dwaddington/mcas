#![feature(asm)]
#![feature(slice_fill)]

#[allow(dead_code)]
mod mcas;

#[allow(clippy::redundant_static_lifetimes)]
#[allow(dead_code)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
#[allow(non_snake_case)]
#[allow(unused_variables)]
mod mcasapi_wrapper;

#[cfg(test)]
mod tests {

    use crate::mcas;
    #[test]
    fn foo() {
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
}
