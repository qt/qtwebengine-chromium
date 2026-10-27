// Copyright 2025 Google LLC
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

// Not all sub-modules are used by all targets. Ignore dead code warnings.
#![allow(dead_code)]

pub mod avif;
#[cfg(feature = "gif")]
pub mod gif;
#[cfg(feature = "jpeg")]
pub mod jpeg;
#[cfg(feature = "png")]
pub mod png;
pub mod y4m;

#[cfg(feature = "png")]
mod icc;
#[cfg(feature = "jpeg")]
mod xmp;

use crate::gainmap::GainMap;
use crate::image::Image;
use crate::AvifError;
use crate::AvifResult;
use crate::MatrixCoefficients;
use crate::PixelFormat;

#[derive(Default)]
pub struct Config {
    pub yuv_format: Option<PixelFormat>,
    pub depth: Option<u8>,
    pub matrix_coefficients: Option<MatrixCoefficients>,
    pub ignore_icc: bool,
    pub ignore_exif: bool,
    pub ignore_xmp: bool,
    pub image_size_limit: u32,
    pub allow_sample_transform: bool,
}

pub trait Reader {
    // Returns the next frame, its duration in milliseconds, and the gain map if any.
    fn read_frame(&mut self, config: &Config) -> AvifResult<(Image, u64, Option<GainMap>)>;
    // Returns true if the last call to read_frame() returned another frame than the last frame.
    // Meaningless if read_frame() was never called or if read_frame() returned an error.
    fn has_more_frames(&mut self) -> bool;
}

impl dyn Reader {
    pub fn create(path: &str) -> AvifResult<Box<dyn Reader>> {
        Ok(
            match std::path::Path::new(path)
                .extension()
                .and_then(|s| s.to_str())
                .unwrap_or("")
                .to_lowercase()
                .as_str()
            {
                "y4m" => Box::new(y4m::Y4MReader::create(path)?),
                #[cfg(feature = "jpeg")]
                "jpg" | "jpeg" => Box::new(jpeg::JpegReader::create(path)?),
                #[cfg(not(feature = "jpeg"))]
                "jpg" | "jpeg" => {
                    return AvifError::unknown_error(format!(
                    "Cannot read {path} because CrabbyAvif was not compiled with the jpeg feature"
                ))
                }
                #[cfg(feature = "png")]
                "png" => Box::new(png::PngReader::create(path)?),
                #[cfg(not(feature = "png"))]
                "png" => {
                    return AvifError::unknown_error(format!(
                    "Cannot read {path} because CrabbyAvif was not compiled with the png feature"
                ))
                }
                #[cfg(feature = "gif")]
                "gif" => Box::new(gif::GifReader::create(path)?),
                #[cfg(not(feature = "gif"))]
                "gif" => {
                    return AvifError::unknown_error(format!(
                    "Cannot read {path} because CrabbyAvif was not compiled with the gif feature"
                ))
                }
                "avif" => Box::new(avif::AvifReader::create(path)?),
                _ => {
                    return AvifError::unknown_error(format!(
                        "Unknown input file extension for {path}"
                    ))
                }
            },
        )
    }
}
