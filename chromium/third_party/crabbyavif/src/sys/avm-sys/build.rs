// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Build rust library and bindings for libavm.

use std::env;
use std::path::Path;
use std::path::PathBuf;

fn main() -> Result<(), String> {
    println!("cargo:rerun-if-changed=build.rs");
    if !cfg!(feature = "avm") {
        // The feature is disabled at the top level. Do not build this dependency.
        return Ok(());
    }

    let build_target = std::env::var("TARGET").unwrap();
    assert!(!build_target.contains("android"));
    let build_dir = "build.CrabbyAvif"; // As created by avm.cmd.

    let project_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let abs_library_dir = PathBuf::from(&project_root).join("avm");
    let abs_object_dir = PathBuf::from(&abs_library_dir).join(build_dir);
    let library_file = PathBuf::from(&abs_object_dir).join("libavm.a");
    if !Path::new(&library_file).exists() {
        return Err(
            "avm binaries could not be found locally. Disable the avm feature \
            or build the dependency locally by running avm.cmd from sys/avm-sys."
                .into(),
        );
    }
    println!("cargo:rustc-link-search={}", abs_object_dir.display());
    println!("cargo:rustc-link-lib=static=avm");

    let include_paths = vec![
        format!("-I{}", PathBuf::from(&abs_library_dir).display()),
        // For config/avm_config.h, located in sys/avm-sys/avm/build.CrabbyAvif,
        // included by sys/avm-sys/avm/avm/avm_codec.h.
        format!("-I{}", abs_object_dir.display()),
    ];

    // libavm dependencies. The paths are relative to build_dir.
    let deps = [
        "third_party/tensorflow/tensorflow/lite/c/tensorflow-lite/libtensorflow-lite.a",
        "_deps/flatbuffers-build/libflatbuffers.a",
        "_deps/ruy-build/ruy/libruy_frontend.a",
        "_deps/ruy-build/ruy/libruy_pack_avx.a",
        "_deps/ruy-build/ruy/libruy_have_built_path_for_avx2_fma.a",
        "_deps/ruy-build/ruy/libruy_prepacked_cache.a",
        "_deps/ruy-build/ruy/libruy_allocator.a",
        "_deps/ruy-build/ruy/libruy_have_built_path_for_avx512.a",
        "_deps/ruy-build/ruy/libruy_prepare_packed_matrices.a",
        "_deps/ruy-build/ruy/libruy_apply_multiplier.a",
        "_deps/ruy-build/ruy/libruy_have_built_path_for_avx.a",
        "_deps/ruy-build/ruy/libruy_system_aligned_alloc.a",
        "_deps/ruy-build/ruy/libruy_blocking_counter.a",
        "_deps/ruy-build/ruy/libruy_kernel_arm.a",
        "_deps/ruy-build/ruy/libruy_thread_pool.a",
        "_deps/ruy-build/ruy/libruy_block_map.a",
        "_deps/ruy-build/ruy/libruy_kernel_avx2_fma.a",
        "_deps/ruy-build/ruy/libruy_trmul.a",
        "_deps/ruy-build/ruy/libruy_context.a",
        "_deps/ruy-build/ruy/libruy_kernel_avx512.a",
        "_deps/ruy-build/ruy/libruy_tune.a",
        "_deps/ruy-build/ruy/libruy_context_get_ctx.a",
        "_deps/ruy-build/ruy/libruy_kernel_avx.a",
        "_deps/ruy-build/ruy/libruy_wait.a",
        "_deps/ruy-build/ruy/libruy_cpuinfo.a",
        "_deps/ruy-build/ruy/libruy_pack_arm.a",
        "_deps/ruy-build/ruy/libruy_ctx.a",
        "_deps/ruy-build/ruy/libruy_pack_avx2_fma.a",
        "_deps/ruy-build/ruy/libruy_denormal.a",
        "_deps/ruy-build/ruy/libruy_pack_avx512.a",
        "_deps/fft2d-build/libfft2d_fftsg.a",
        "_deps/fft2d-build/libfft2d_fftsg2d.a",
        "_deps/xnnpack-build/libXNNPACK.a",
        "pthreadpool/libpthreadpool.a",
        "_deps/cpuinfo-build/libcpuinfo.a",
        "_deps/abseil-cpp-build/absl/base/libabsl_base.a",
        "_deps/abseil-cpp-build/absl/base/libabsl_raw_logging_internal.a",
        "_deps/abseil-cpp-build/absl/strings/libabsl_strings.a",
        "_deps/farmhash-build/libfarmhash.a",
    ];
    for dep in deps {
        let tokens = dep.split('/');
        let mut library_folder_path = PathBuf::from(&abs_object_dir);
        let mut library_name: Option<&str> = None;
        for token in tokens {
            if token.starts_with("lib") && token.ends_with(".a") {
                // Trimmed because rustc wraps the library name to "lib{name}.a".
                library_name = Some(&token[3..token.len() - 2]);
            } else {
                library_folder_path = library_folder_path.join(token);
            }
        }
        println!("cargo:rustc-link-search={}", library_folder_path.display());
        println!("cargo:rustc-link-lib=static={}", library_name.unwrap());
    }

    // Generate bindings.
    let header_file = PathBuf::from(&project_root).join("wrapper.h");
    let outfile = PathBuf::from(&project_root).join("avm.rs");
    let bindings = bindgen::Builder::default()
        .header(header_file.into_os_string().into_string().unwrap())
        .clang_args(&include_paths)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .layout_tests(false)
        .generate_comments(false);
    // TODO: b/437292541 - Add an allowlist to only generate bindings for necessary items.
    let bindings = bindings.generate().map_err(|err| err.to_string())?;
    bindings
        .write_to_file(outfile.as_path())
        .map_err(|err| err.to_string())?;
    println!(
        "cargo:rustc-env=CRABBYAVIF_AVM_BINDINGS_RS={}",
        outfile.display()
    );
    Ok(())
}
