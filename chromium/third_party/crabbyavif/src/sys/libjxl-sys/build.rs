// Copyright 2025 Google LLC
//
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

// Build rust library and bindings for libjxl.
// libjxl.cmd must have been run first.

use std::env;
use std::path::Path;
use std::path::PathBuf;

fn main() -> Result<(), String> {
    println!("cargo:rerun-if-changed=build.rs");
    if !cfg!(feature = "libjxl") {
        // The feature is disabled at the top level. Do not build this dependency.
        return Ok(());
    }

    let build_target = std::env::var("TARGET").unwrap();
    assert!(!build_target.contains("android"));
    let build_dir = "build";

    let project_root = PathBuf::from(env!("CARGO_MANIFEST_DIR"));
    let abs_library_dir = PathBuf::from(&project_root).join("libjxl");

    let abs_build_dir = PathBuf::from(&abs_library_dir).join(build_dir);
    let objects = [
        ["jxl", "lib", "."],
        ["jxl_cms", "lib", "."],
        ["lcms2", "third_party", "."],
        ["hwy", "third_party", "highway"],
        ["brotlicommon", "third_party", "brotli"],
        ["brotlienc", "third_party", "brotli"],
    ];
    let object_dir = |object: &[&str; 3]| {
        PathBuf::from(&abs_build_dir)
            .join(object[1])
            .join(object[2])
    };
    let object_file = |object: &[&str; 3]| {
        PathBuf::from(&object_dir(object)).join(if cfg!(target_os = "windows") {
            format!("{}.lib", object[0])
        } else {
            format!("lib{}.a", object[0])
        })
    };

    if !objects
        .iter()
        .all(|object| Path::new(&object_file(object)).exists())
    {
        return Err(
            "libjxl binaries could not be found locally. Disable the jpegxl feature \
            or build the dependency locally by running libjxl.cmd from sys/libjxl-sys."
                .into(),
        );
    }
    for object in objects {
        let prefix = if cfg!(target_os = "windows") { "lib" } else { "" };
        println!("cargo:rustc-link-lib=static={}{}", prefix, object[0]);
        println!("cargo:rustc-link-search={}", object_dir(&object).display());
    }
    let include_paths = vec![format!(
        "-I{}",
        abs_build_dir.join("lib").join("include").display()
    )];

    // Generate bindings.
    let header_file = PathBuf::from(&project_root).join("wrapper.h");
    let outdir = std::env::var("OUT_DIR").expect("OUT_DIR not set");
    let outfile = PathBuf::from(&outdir).join("libjxl_bindgen.rs");
    let mut bindings = bindgen::Builder::default()
        .header(header_file.into_os_string().into_string().unwrap())
        .clang_args(&include_paths)
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        .layout_tests(false)
        .generate_comments(false);
    let allowlist = vec![
        "JxlBasicInfo",
        "JxlColorEncodingSetToSRGB",
        "JxlDecoder",
        "JxlDecoderCloseInput",
        "JxlDecoderCreate",
        "JxlDecoderDestroy",
        "JxlDecoderGetBasicInfo",
        "JxlDecoderProcessInput",
        "JxlDecoderReleaseInput",
        "JxlDecoderSetImageOutBuffer",
        "JxlDecoderSetInput",
        "JxlDecoderStatus",
        "JxlDecoderSubscribeEvents",
        "JxlEncoder",
        "JxlEncoderAddImageFrame",
        "JxlEncoderAllowExpertOptions",
        "JxlEncoderCloseInput",
        "JxlEncoderCreate",
        "JxlEncoderDestroy",
        "JxlEncoderDistanceFromQuality",
        "JxlEncoderFrameSettingsCreate",
        "JxlEncoderFrameSettingsSetOption",
        "JxlEncoderGetError",
        "JxlEncoderInitBasicInfo",
        "JxlEncoderInitFrameHeader",
        "JxlEncoderProcessOutput",
        "JxlEncoderProcessOutput",
        "JxlEncoderSetBasicInfo",
        "JxlEncoderSetColorEncoding",
        "JxlEncoderSetFrameDistance",
        "JxlEncoderSetFrameHeader",
        "JxlEncoderSetFrameLossless",
        "JxlEncoderStatus",
        "JxlEncoderVersion",
        "JxlFrameHeader",
        "JxlPixelFormat",
    ];
    for item in allowlist {
        bindings = bindings.allowlist_item(item);
    }
    let bindings = bindings.generate().map_err(|err| err.to_string())?;
    bindings
        .write_to_file(outfile.as_path())
        .map_err(|err| err.to_string())?;
    Ok(())
}
