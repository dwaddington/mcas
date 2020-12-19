#[allow(dead_code)]
#[allow(non_camel_case_types)]
#[allow(non_upper_case_globals)]
#[allow(non_snake_case)]
mod mcasapi_wrapper;


#[cfg(test)]
mod tests {

    use crate::mcasapi_wrapper::{mcas_open_session_ex, mcas_close_session};
    use std::ffi::CString;
     
    #[test]
    fn open_session() {
        let ip = CString::new("10.0.0.101::11911").expect("bad string").as_ptr();
        let device = CString::new("mlx5_0").expect("bad string").as_ptr();
        let session = unsafe { mcas_open_session_ex(ip, device, 3, 30) };

        println!("session opened");
        unsafe { mcas_close_session(session) };
        println!("session closed");
            
    }
}
