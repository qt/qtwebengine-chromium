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
    fn read_frame(&mut self, config: &Config) -> AvifResult<(Image, u64, Option<GainMap>)>;
    fn has_more_frames(&mut self) -> bool;
}
