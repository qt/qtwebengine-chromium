#![allow(warnings)]
pub mod bindings {
    #[cfg(not(android_soong))]
    include!(env!("CRABBYAVIF_DAV1D_BINDINGS_RS"));
    // Android's soong build system does not support setting environment variables. Set the source
    // file name directly relative to the OUT_DIR environment variable.
    #[cfg(android_soong)]
    include!(concat!(env!("OUT_DIR"), "/dav1d_bindgen.rs"));
}
