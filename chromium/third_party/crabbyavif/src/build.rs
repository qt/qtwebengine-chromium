#[cfg(feature = "capi")]
use std::path::PathBuf;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=cbindgen.toml");
    #[cfg(feature = "libgav1")]
    {
        // libgav1 needs libstdc++ on *nix and libc++ on mac. TODO: what about windows?
        if cfg!(target_os = "macos") {
            println!("cargo:rustc-link-arg=-lc++");
        } else {
            println!("cargo:rustc-link-arg=-lstdc++");
        }
    }
    #[cfg(feature = "capi")]
    {
        // Generate the C header.
        let crate_path = env!("CARGO_MANIFEST_DIR");
        let config = cbindgen::Config::from_root_or_default(crate_path);
        let header_file = PathBuf::from(crate_path).join("include/avif/avif.h");
        cbindgen::Builder::new()
            .with_crate(crate_path)
            .with_config(config)
            .generate()
            .unwrap()
            .write_to_file(header_file);
    }
}
