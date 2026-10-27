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

use crate::decoder::Decoder;
use crate::gainmap::GainMap;
use crate::AvifError;
use crate::AvifResult;
use crate::*;

use super::Config;
use super::Reader;

pub struct AvifReader {
    path: String,
    decoder: Decoder,
}

impl AvifReader {
    pub fn create(path: &str) -> AvifResult<Self> {
        let path = path.to_string();
        let mut decoder = Decoder::default();
        decoder.set_io_file(&path)?;
        decoder.parse()?;
        Ok(Self { path, decoder })
    }
}

impl Reader for AvifReader {
    fn read_frame(&mut self, config: &Config) -> AvifResult<(Image, u64, Option<GainMap>)> {
        self.decoder.next_image()?;
        let image = self.decoder.image().unwrap();
        if let Some(yuv_format) = config.yuv_format {
            if yuv_format != image.yuv_format {
                return AvifError::not_implemented();
            }
        }
        if let Some(depth) = config.depth {
            if depth != image.depth {
                return AvifError::not_implemented();
            }
        }
        if let Some(matrix_coefficients) = config.matrix_coefficients {
            if matrix_coefficients != image.matrix_coefficients {
                return AvifError::not_implemented();
            }
        }
        Ok((
            image.try_deep_clone()?,
            (self.decoder.image_timing().duration * 1000.0).round() as u64,
            if self.decoder.gainmap_present() {
                Some(self.decoder.gainmap().try_deep_clone()?)
            } else {
                None
            },
        ))
    }

    fn has_more_frames(&mut self) -> bool {
        self.decoder.image_index() < 0
            || (self.decoder.image_index() as u32) < self.decoder.image_count()
    }
}
