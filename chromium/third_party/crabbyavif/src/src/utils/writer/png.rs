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

use crate::image::*;
use crate::reformat::rgb;
use crate::utils::*;

use std::ffi::CString;
use std::fs::File;
use std::io::Write;
use std::ptr;

use super::Writer;

use libpng_sys::bindings::*;

#[derive(Default)]
pub struct PngWriter {
    pub depth: Option<u8>,
    pub compression_level: Option<i32>,
}

struct PngWriterNative {
    png: png_structp,
    info: png_infop,
}

impl Default for PngWriterNative {
    fn default() -> Self {
        Self {
            png: ptr::null_mut(),
            info: ptr::null_mut(),
        }
    }
}

impl Drop for PngWriterNative {
    fn drop(&mut self) {
        if !self.png.is_null() {
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                png_destroy_write_struct((&mut self.png) as *mut _, (&mut self.info) as *mut _);
            }
        }
    }
}

/// # Safety
/// C-callback function. So it has to be unsafe.
unsafe extern "C" fn crabbyavif_png_write_data(
    png_ptr: png_structp,
    data: png_bytep,
    length: png_size_t,
) {
    // # Safety: Calling a C function with valid parameters.
    let io_ptr = unsafe { png_get_io_ptr(png_ptr) };
    // # Safety: The best we can do is trust the pointer and buffer length reported by the libpng.
    let data = unsafe { std::slice::from_raw_parts(data, length as _) };
    // # Safety: Dereferencing a valid pointer that was set in png_set_write_fn.
    let file = unsafe { &mut *(io_ptr as *mut File) };
    if file.write_all(data).is_err() {
        // # Safety: Calling a C function with valid parameters.
        unsafe {
            png_error(png_ptr, CString::new("Write Error").unwrap().as_ptr());
        }
    }
}

/// # Safety
/// C-callback function. So it has to be unsafe.
unsafe extern "C" fn crabbyavif_png_output_flush(_png_ptr: png_structp) {}

impl Writer for PngWriter {
    fn write_frame(&mut self, file: &mut File, image: &Image) -> AvifResult<()> {
        if image.matrix_coefficients == MatrixCoefficients::YcgcoRo {
            return Err(AvifError::UnknownError(
                "YcgcoRo cannot be used with PNG because it has an even bit depth.".into(),
            ));
        }
        let mut rgb_depth = self.depth.unwrap_or(0);
        if rgb_depth == 0 {
            rgb_depth = if image.depth > 8 { 16 } else { 8 };
        }
        if image.matrix_coefficients == MatrixCoefficients::YcgcoRe {
            if image.depth != 10 {
                return Err(AvifError::UnknownError(
                    "YcgcoRe can only be used with bit depth 10.".into(),
                ));
            }
            if self.depth.unwrap_or(0) != 0 && self.depth.unwrap_or(0) != 8 {
                return Err(AvifError::UnknownError(
                    "Cannot request non-8 bits for YCgCo-Re.".into(),
                ));
            }
            rgb_depth = 8;
        }
        let copy_y_plane = image.yuv_format == PixelFormat::Yuv400
            && !image.alpha_present
            && image.depth == 8
            && rgb_depth == 8
            && image.clap.is_none()
            && image.irot_angle.is_none()
            && image.imir_axis.is_none();
        let mut rgb = rgb::Image::create_from_yuv(image);
        let color_type;
        if copy_y_plane {
            color_type = PNG_COLOR_TYPE_GRAY;
        } else {
            rgb.depth = rgb_depth;
            match (image.yuv_format, image.alpha_present) {
                (PixelFormat::Yuv400, true) => {
                    color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
                    rgb.format = rgb::Format::GrayA;
                }
                (PixelFormat::Yuv400, false) => {
                    color_type = PNG_COLOR_TYPE_GRAY;
                    rgb.format = rgb::Format::Gray;
                }
                (_, true) => {
                    // TODO - b/479429854: Support specifying chroma upsampling.
                    color_type = if image.is_opaque() {
                        rgb.format = rgb::Format::Rgb;
                        PNG_COLOR_TYPE_RGB
                    } else {
                        PNG_COLOR_TYPE_RGBA
                    };
                }
                (_, false) => {
                    // TODO - b/479429854: Support specifying chroma upsampling.
                    color_type = PNG_COLOR_TYPE_RGB;
                    rgb.format = rgb::Format::Rgb;
                }
            }
            rgb.allocate()?;
            rgb.convert_from_yuv(image)?;
        }
        let height = image.height;
        let mut png = PngWriterNative {
            // # Safety: Calling a C function with valid parameters.
            png: unsafe {
                png_create_write_struct(
                    PNG_LIBPNG_VER_STRING.as_ptr() as _,
                    ptr::null_mut(),
                    None,
                    None,
                )
            },
            ..Default::default()
        };
        if png.png.is_null() {
            return Err(AvifError::UnknownError(
                "png_create_write_struct failed".into(),
            ));
        }
        // # Safety: Calling C functions with valid parameters.
        unsafe {
            png_set_benign_errors(png.png, 1);
            png.info = png_create_info_struct(png.png);
        }
        if png.info.is_null() {
            return Err(AvifError::UnknownError(
                "png_create_info_struct failed".into(),
            ));
        }
        // # Safety: Calling C functions with valid parameters.
        unsafe {
            png_set_write_fn(
                png.png,
                file as *mut File as *mut _,
                Some(crabbyavif_png_write_data),
                Some(crabbyavif_png_output_flush),
            );
            png_set_option(
                png.png,
                PNG_SKIP_sRGB_CHECK_PROFILE as _,
                PNG_OPTION_ON as _,
            );
            if let Some(compression_level) = self.compression_level {
                png_set_compression_level(png.png, compression_level);
            }
            png_set_IHDR(
                png.png,
                png.info,
                image.width,
                image.height,
                rgb_depth as _,
                color_type as _,
                PNG_INTERLACE_NONE as _,
                PNG_COMPRESSION_TYPE_DEFAULT as _,
                PNG_FILTER_TYPE_DEFAULT as _,
            );
        }
        let icc_profile_name = CString::new("libavif").unwrap();
        if image.icc.is_empty() {
            if image.color_primaries == ColorPrimaries::Srgb
                && image.transfer_characteristics == TransferCharacteristics::Srgb
            {
                // # Safety: Calling a C function with valid parameters.
                unsafe {
                    png_set_sRGB_gAMA_and_cHRM(png.png, png.info, PNG_sRGB_INTENT_PERCEPTUAL as _);
                }
            } else {
                if let Some(primaries) = image.color_primaries.values() {
                    // # Safety: Calling a C function with valid parameters.
                    unsafe {
                        png_set_cHRM(
                            png.png,
                            png.info,
                            primaries[6] as _,
                            primaries[7] as _,
                            primaries[0] as _,
                            primaries[1] as _,
                            primaries[2] as _,
                            primaries[3] as _,
                            primaries[4] as _,
                            primaries[5] as _,
                        );
                    }
                }
                if let Some(gamma) = image.transfer_characteristics.gamma() {
                    // # Safety: Calling a C function with valid parameters.
                    unsafe {
                        png_set_gAMA(png.png, png.info, (1.0 / gamma) as _);
                    }
                }
            }
        } else {
            // If there is an ICC profile, the CICP values are irrelevant and only the ICC
            // profile is written. If we could extract the primaries/transfer curve from the
            // ICC profile, then they could be written in cHRM/gAMA chunks.
            let size = u32_from_usize(image.icc.len())?;
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                png_set_iCCP(
                    png.png,
                    png.info,
                    icc_profile_name.as_ptr(),
                    0,
                    image.icc.as_ptr() as _,
                    size,
                );
            }
        }
        if !image.exif.is_empty() {
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                png_set_eXIf_1(
                    png.png,
                    png.info,
                    u32_from_usize(image.exif.len())?,
                    image.exif.as_ptr() as _,
                );
            }
        }
        let text: png_text;
        let xmp_key = CString::new("XML:com.adobe.xmp").unwrap();
        let mut xmp = image.xmp.clone();
        if !xmp.is_empty() {
            xmp.push(0);
            text = png_text {
                compression: PNG_ITXT_COMPRESSION_NONE as _,
                key: xmp_key.as_ptr() as _,
                text: xmp.as_mut_ptr() as _,
                text_length: 0,
                itxt_length: xmp.len() as _,
                lang: ptr::null_mut(),
                lang_key: ptr::null_mut(),
            };
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                png_set_text(png.png, png.info, (&text) as *const _, 1);
            }
        }
        // # Safety: Calling a C function with valid parameters.
        unsafe {
            png_write_info(png.png, png.info);
        }
        if image.icc.is_empty() {
            let cicp: [png_byte; 5] = [b'c', b'I', b'C', b'P', 0];
            let cicp_data: [png_byte; 4] = [
                image.color_primaries as _,
                image.transfer_characteristics as _,
                MatrixCoefficients::Identity as _,
                1, // full_range
            ];
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                png_write_chunk(png.png, cicp.as_ptr() as _, cicp_data.as_ptr() as _, 4);
            }
        }
        let mut row_pointers: Vec<png_bytep> = create_vec_exact(usize_from_u32(height)?)?;
        if copy_y_plane {
            for y in 0..height {
                if image.depth == 8 {
                    row_pointers.push(image.row(Plane::Y, y)?.as_ptr() as _);
                } else {
                    row_pointers.push(image.row16(Plane::Y, y)?.as_ptr() as _);
                }
            }
        } else {
            for y in 0..height {
                if rgb.depth == 8 {
                    row_pointers.push(rgb.row(y)?.as_ptr() as _);
                } else {
                    row_pointers.push(rgb.row16(y)?.as_ptr() as _);
                }
            }
        }
        if rgb_depth > 8 {
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                png_set_swap(png.png);
            }
        }
        // # Safety: Calling C functions with valid parameters.
        unsafe {
            png_write_image(png.png, row_pointers.as_mut_ptr());
            png_write_end(png.png, ptr::null_mut());
        }
        Ok(())
    }
}
