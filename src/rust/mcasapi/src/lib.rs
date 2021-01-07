#![feature(asm)]

#[allow(dead_code)]
mod mcas;

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

            pool.configure("AddIndex::VolatileTree").expect("pool config failed");
            
            pool.put("cat", "Jenny").unwrap();
            
            let x = pool.get("cat").unwrap();
            let xs = String::from_utf8(x.clone()).unwrap();
            println!("result=>{:?}< also as UTF8 ({})", x, xs);

        } // implicitly close pool
        
        session.delete_pool("myPool").expect("pool deletion failed")
    }
}
