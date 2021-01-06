fn main()
{
    println!("cargo:rustc-link-search=native=/usr/lib64/");
    println!("cargo:rustc-link-search=native=/usr/lib/");
    println!("cargo:rustc-link-search=native=/home/danielwaddington/mcas/build/dist/lib/");
    println!("cargo:rustc-link-lib=common");
    println!("cargo:rustc-link-lib=pthread");
    println!("cargo:rustc-link-lib=numa");
    println!("cargo:rustc-link-lib=dl");
    println!("cargo:rustc-link-lib=rt");
    println!("cargo:rustc-link-lib=mcasapi");

//    println!("cargo:rustc-flags=-L/home/danielwaddington/mcas/build/dist/lib -lnuma -lcommon -lmcasapi");
}

