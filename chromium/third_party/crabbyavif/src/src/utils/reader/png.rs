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

use crate::gainmap::GainMap;
use crate::parser::exif;
use crate::reformat::*;
use crate::utils::*;
use crate::AvifError;
use crate::AvifResult;

use super::icc;
use super::Config;
use super::Reader;

use std::ffi::c_char;
use std::ffi::c_int;
use std::ffi::CStr;
use std::ffi::CString;
use std::fs::File;
use std::io::Read;
use std::ptr;
use std::slice;

use libpng_sys::bindings::*;

pub struct PngReader {
    filename: String,
}

fn set_png_cicp(image: &mut Image, cicp: &[png_byte; 4]) {
    image.color_primaries = (cicp[0] as u16).into();
    image.transfer_characteristics = (cicp[1] as u16).into();
    // PNG specification Third Edition Section 4.3:
    //  RGB is currently the only supported color model in PNG, and as such Matrix Coefficients shall be set to 0.
    // Note that we don't set image.matrix_coefficients to this value as the Avif's YUV matrix is independent from the PNG's.
    if cicp[2] != 0 {
        println!(
            "Warning: Unsupported PNG CICP matrix coefficients value {}. Expected to be 0.",
            cicp[2]
        );
    }
    // Limited range PNG files are uncommon and would require conversion to full range which we don't support for now for simplicity.
    // See also https://github.com/w3c/png/issues/312#issuecomment-2325281113 and https://svgees.us/blog/cICP.html#the-other-two-numbers
    // Similarly, we don't set image.yuv_range to this value as the Avif's YUV range is independent from the PNG's.
    if cicp[3] != 1 {
        println!(
            "Warning: Unsupported PNG CICP full range flag value {}. Expected to be 1.",
            cicp[3]
        );
    }
}

struct UnknownCicpChunkData {
    image: *mut Image,
    cicp_chunk_read: bool,
}

/// # Safety
/// C-callback function. So it has to be unsafe.
unsafe extern "C" fn crabbyavif_png_read_unknown_chunk(
    png_ptr: png_structp,
    chunk: png_unknown_chunkp,
) -> c_int {
    // # Safety: Dereferencing a valid pointer that was set in png_set_read_user_chunk_fn.
    let data = unsafe { &mut *(png_get_user_chunk_ptr(png_ptr) as *mut UnknownCicpChunkData) };
    // # Safety: Dereferencing a valid pointer provided by libpng.
    let chunk = unsafe { &*chunk };
    if chunk.name[0] == b'c'
        && chunk.name[1] == b'I'
        && chunk.name[2] == b'C'
        && chunk.name[3] == b'P'
        && chunk.size >= 4
    {
        data.cicp_chunk_read = true;
        // # Safety: The best we can do is trust the pointer and buffer length reported by the libpng.
        let cicp_data = unsafe { slice::from_raw_parts(chunk.data, 4) };
        // # Safety: Dereferencing a valid pointer.
        unsafe {
            set_png_cicp(
                &mut *data.image,
                &[cicp_data[0], cicp_data[1], cicp_data[2], cicp_data[3]],
            );
        }
    }
    1
}

fn compare_c_str(c_ptr: *const c_char, rust_str: &str) -> bool {
    if c_ptr.is_null() {
        return false;
    }
    // # Safety: The best we can do is trust that this is a valid C-string.
    let c_str = unsafe { CStr::from_ptr(c_ptr) };
    match c_str.to_str() {
        Ok(converted_s) => converted_s == rust_str,
        Err(_) => false,
    }
}

fn extract_exif_and_xmp(
    image: &mut Image,
    png: png_structp,
    info: png_infop,
    ignore_exif: &mut bool,
    ignore_xmp: &mut bool,
) -> AvifResult<()> {
    if !*ignore_exif {
        let mut exif_size = 0u32;
        let mut exif: png_bytep = ptr::null_mut();
        unsafe {
            if png_get_eXIf_1(png, info, &mut exif_size as *mut _, &mut exif as *mut _)
                == PNG_INFO_eXIf
            {
                if exif_size == 0 || exif.is_null() {
                    return Err(AvifError::UnknownError(
                        "Exif extraction failed. Empty eXIf chunk".into(),
                    ));
                }
                let exif_slice = slice::from_raw_parts(exif, usize_from_u32(exif_size)?);
                image.exif = create_vec_exact(exif_slice.len())?;
                image.exif.extend_from_slice(exif_slice);
                let _ = exif::set_orientation(&mut image.exif, 1);
                *ignore_exif = true;
            }
        }
    }
    unsafe {
        let mut text_chunks: png_textp = ptr::null_mut();
        let num_text_chunks = png_get_text(png, info, &mut text_chunks as *mut _, ptr::null_mut());
        for i in 0..num_text_chunks {
            if *ignore_exif && *ignore_xmp {
                break;
            }
            let text = &*text_chunks.add(i as _);
            let length = if text.compression == PNG_ITXT_COMPRESSION_NONE as _
                || text.compression == PNG_ITXT_COMPRESSION_zTXt as _
            {
                text.itxt_length
            } else {
                text.text_length
            };
            let exif_prefix = "Exif\0\0".as_bytes();
            let xmp_prefix = "http://ns.adobe.com/xap/1.0/\0".as_bytes();
            if !*ignore_exif && compare_c_str(text.key, "Raw profile type exif") {
                let exif =
                    icc::copy_raw_profile(slice::from_raw_parts(text.text as *const u8, length))?;
                image.exif = create_vec_exact(exif.len())?;
                image.exif.extend_from_slice(&exif);
                if image.exif.starts_with(exif_prefix) {
                    image.exif.drain(..exif_prefix.len());
                }
                let _ = exif::set_orientation(&mut image.exif, 1);
                *ignore_exif = true;
            } else if !*ignore_xmp && compare_c_str(text.key, "Raw profile type xmp") {
                let xmp =
                    icc::copy_raw_profile(slice::from_raw_parts(text.text as *const u8, length))?;
                image.xmp = create_vec_exact(xmp.len())?;
                image.xmp.extend_from_slice(&xmp);
                if image.xmp.starts_with(xmp_prefix) {
                    image.xmp.drain(..xmp_prefix.len());
                }
                *ignore_xmp = true;
            } else if compare_c_str(text.key, "Raw profile type APP1")
                || compare_c_str(text.key, "Raw profile type app1")
            {
                // This can be either exif, xmp or something else.
                let data =
                    icc::copy_raw_profile(slice::from_raw_parts(text.text as *const u8, length))?;
                if !*ignore_exif && data.starts_with(exif_prefix) {
                    image.exif = create_vec_exact(data.len() - exif_prefix.len())?;
                    image.exif.extend_from_slice(&data[exif_prefix.len()..]);
                    let _ = exif::set_orientation(&mut image.exif, 1);
                    *ignore_exif = true;
                } else if !*ignore_xmp && data.starts_with(xmp_prefix) {
                    image.xmp = create_vec_exact(data.len() - xmp_prefix.len())?;
                    image.xmp.extend_from_slice(&data[xmp_prefix.len()..]);
                    *ignore_xmp = true;
                }
            } else if !*ignore_xmp && compare_c_str(text.key, "XML:com.adobe.xmp") {
                if length == 0 {
                    return Err(AvifError::UnknownError(
                        "XMP extraction failed: empty XML:com.adobe.xmp payload".into(),
                    ));
                }
                let xmp = slice::from_raw_parts(text.text as *const u8, length);
                image.xmp = create_vec_exact(xmp.len())?;
                image.xmp.extend_from_slice(xmp);
                *ignore_xmp = true;
            }
        }
    }
    image.remove_trailing_null_from_xmp();
    Ok(())
}

struct PngReaderNative {
    png: png_structp,
    info: png_infop,
}

impl Default for PngReaderNative {
    fn default() -> Self {
        Self {
            png: ptr::null_mut(),
            info: ptr::null_mut(),
        }
    }
}

impl Drop for PngReaderNative {
    fn drop(&mut self) {
        if !self.png.is_null() {
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                png_destroy_read_struct(
                    (&mut self.png) as *mut _,
                    (&mut self.info) as *mut _,
                    ptr::null_mut(),
                );
            }
        }
    }
}

/// # Safety
/// C-callback function. So it has to be unsafe.
unsafe extern "C" fn crabbyavif_png_read_data(
    png_ptr: png_structp,
    data: png_bytep,
    length: png_size_t,
) {
    // # Safety: Calling a C function with valid parameters.
    let io_ptr = unsafe { png_get_io_ptr(png_ptr) };
    // # Safety: Dereferencing a valid pointer that was set in png_set_read_fn.
    let file = unsafe { &mut *(io_ptr as *mut File) };
    // # Safety: The best we can do is trust the pointer and buffer length reported by the libpng.
    let data = unsafe { std::slice::from_raw_parts_mut(data, length as _) };
    if file.read_exact(data).is_err() {
        // libpng uses longjmp for errors. Since we don't set an error handler, the default
        // handler will be used.
        // # Safety: Calling a C function with valid parameters.
        unsafe {
            png_error(png_ptr, CString::new("Read Error").unwrap().as_ptr());
        }
    }
}

impl PngReader {
    pub fn create(filename: &str) -> AvifResult<Self> {
        Ok(Self {
            filename: filename.into(),
        })
    }
}

impl Reader for PngReader {
    fn read_frame(&mut self, config: &Config) -> AvifResult<(Image, u64, Option<GainMap>)> {
        let mut file = File::open(&self.filename)
            .map_err(|_| AvifError::UnknownError("failed to open file".into()))?;
        let mut header = [0u8; 8];
        file.read_exact(&mut header)
            .map_err(|_| AvifError::UnknownError("cannot read png header".into()))?;
        if unsafe { png_sig_cmp(header.as_mut_ptr() as _, 0, 8) } != 0 {
            return Err(AvifError::UnknownError("not a png".into()));
        }

        let mut png = PngReaderNative {
            png: unsafe {
                png_create_read_struct(
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
                "png_create_read_struct failed".into(),
            ));
        }
        unsafe {
            png_set_benign_errors(png.png, 1);
            png.info = png_create_info_struct(png.png);
        }
        if png.info.is_null() {
            return Err(AvifError::UnknownError(
                "png_create_info_struct failed".into(),
            ));
        }

        let mut yuv = Image::default();
        let mut unknown_chunk_data = UnknownCicpChunkData {
            image: &mut yuv,
            cicp_chunk_read: false,
        };
        unsafe {
            png_set_read_user_chunk_fn(
                png.png,
                (&mut unknown_chunk_data) as *mut _ as *mut _,
                Some(crabbyavif_png_read_unknown_chunk),
            );

            png_set_read_fn(
                png.png,
                &mut file as *mut File as *mut _,
                Some(crabbyavif_png_read_data),
            );
            png_set_sig_bytes(png.png, 8);
            png_read_info(png.png, png.info);

            let raw_width = png_get_image_width(png.png, png.info);
            let raw_height = png_get_image_height(png.png, png.info);
            let raw_color_type = png_get_color_type(png.png, png.info);
            let raw_bit_depth = png_get_bit_depth(png.png, png.info);

            if config.image_size_limit > 0 {
                let total_pixels = (raw_width as u64) * (raw_height as u64);
                if total_pixels > config.image_size_limit as u64 {
                    return Err(AvifError::UnknownError(format!(
                        "Too big PNG dimensions ({} x {} > {} px)",
                        raw_width, raw_height, config.image_size_limit
                    )));
                }
            }

            if raw_color_type == PNG_COLOR_TYPE_PALETTE as _ {
                png_set_palette_to_rgb(png.png);
            }
            if raw_color_type == PNG_COLOR_TYPE_GRAY as _ && raw_bit_depth < 8 {
                png_set_expand_gray_1_2_4_to_8(png.png);
            }
            if png_get_valid(png.png, png.info, PNG_INFO_tRNS) != 0 {
                png_set_tRNS_to_alpha(png.png);
            }
            let raw_color_type_is_gray = raw_color_type == PNG_COLOR_TYPE_GRAY as _
                || raw_color_type == PNG_COLOR_TYPE_GRAY_ALPHA as _;

            let bit_depth = if raw_bit_depth == 16 {
                png_set_swap(png.png);
                16
            } else {
                8
            };
            png_read_update_info(png.png, png.info);

            yuv.width = raw_width;
            yuv.height = raw_height;
            yuv.matrix_coefficients = config
                .matrix_coefficients
                .unwrap_or(MatrixCoefficients::Bt601);
            if yuv.matrix_coefficients == MatrixCoefficients::YcgcoRo {
                return Err(AvifError::UnknownError(
                    "YcgcoRo cannot be used with PNG because it has an even bit depth.".into(),
                ));
            }
            yuv.yuv_format = match config.yuv_format {
                Some(yuv_format) => yuv_format,
                None => {
                    if raw_color_type_is_gray {
                        PixelFormat::Yuv400
                    } else if matches!(
                        yuv.matrix_coefficients,
                        MatrixCoefficients::Identity | MatrixCoefficients::YcgcoRe
                    ) {
                        // Identity and YCgCo-R are valid only with Yuv444.
                        PixelFormat::Yuv444
                    } else {
                        PixelFormat::Yuv444
                    }
                }
            };
            yuv.depth = match config.depth {
                Some(depth) => depth,
                None => {
                    if bit_depth == 8 {
                        8
                    } else if config.allow_sample_transform {
                        16
                    } else {
                        12
                    }
                }
            };
            if yuv.matrix_coefficients == MatrixCoefficients::YcgcoRe {
                if bit_depth != 8 {
                    return Err(AvifError::UnknownError(
                        "YcgcoRe cannot be used on 16 bit input because it adds two bits.".into(),
                    ));
                }
                if let Some(d) = config.depth {
                    if d != 10 {
                        return Err(AvifError::UnknownError(format!(
                            "Cannot request {} bits for YCgCo-Re as it uses two extra bits",
                            d
                        )));
                    }
                }
                yuv.depth = 10;
            }
            if !config.ignore_icc {
                let mut cicp: [png_byte; 4] = [0; 4];
                let mut icc_profile_name: png_charp = ptr::null_mut();
                let mut icc_compression_type: c_int = 0;
                let mut iccp_data: png_bytep = ptr::null_mut();
                let mut iccp_data_len = 0u32;
                let mut srgb_intent: c_int = 0;
                if png_get_cICP(
                    png.png,
                    png.info,
                    (&mut cicp[0]) as *mut _,
                    (&mut cicp[1]) as *mut _,
                    (&mut cicp[2]) as *mut _,
                    (&mut cicp[3]) as *mut _,
                ) != 0
                {
                    set_png_cicp(&mut yuv, &cicp);
                } else if unknown_chunk_data.cicp_chunk_read {
                    // Already handled in crabbyavif_png_read_unknown_chunk
                } else if png_get_iCCP(
                    png.png,
                    png.info,
                    &mut icc_profile_name as *mut _,
                    &mut icc_compression_type as *mut _,
                    &mut iccp_data as *mut _,
                    &mut iccp_data_len as *mut _,
                ) == PNG_INFO_iCCP
                {
                    if (!raw_color_type_is_gray && yuv.yuv_format == PixelFormat::Yuv400)
                        || (raw_color_type_is_gray && yuv.yuv_format != PixelFormat::Yuv400)
                    {
                        return Err(AvifError::UnknownError("Image contains an ICC profile that is incompatible with the requested output. Pass --ignore-icc to discard the ICC profile".into()));
                    }
                    let icc_slice =
                        slice::from_raw_parts(iccp_data, usize_from_u32(iccp_data_len)?);
                    yuv.icc = create_vec_exact(icc_slice.len())?;
                    yuv.icc.extend_from_slice(icc_slice);
                } else if png_get_sRGB(png.png, png.info, (&mut srgb_intent) as *mut _)
                    == PNG_INFO_sRGB
                {
                    yuv.color_primaries = ColorPrimaries::Srgb;
                    yuv.transfer_characteristics = TransferCharacteristics::Srgb;
                } else {
                    let mut should_generate_icc = false;
                    let mut gamma: f64 = 0.0;
                    if png_get_gAMA(png.png, png.info, &mut gamma as *mut _) == PNG_INFO_gAMA {
                        gamma = 1.0 / gamma;
                        yuv.transfer_characteristics =
                            TransferCharacteristics::from_gamma(gamma as f32);
                        if yuv.transfer_characteristics == TransferCharacteristics::Unknown {
                            should_generate_icc = true;
                        }
                    } else {
                        // No gamma information in file. Assume the default value.
                        // PNG specification 1.2 Section 10.5:
                        // Assume a CRT exponent of 2.2 unless detailed calibration measurements
                        // of this particular CRT are available.
                        gamma = 2.2;
                    }
                    let mut png_primaries: [f64; 8] = [0.0; 8];
                    let primaries: [f32; 8];
                    if png_get_cHRM(
                        png.png,
                        png.info,
                        &mut png_primaries[6] as *mut _,
                        &mut png_primaries[7] as *mut _,
                        &mut png_primaries[0] as *mut _,
                        &mut png_primaries[1] as *mut _,
                        &mut png_primaries[2] as *mut _,
                        &mut png_primaries[3] as *mut _,
                        &mut png_primaries[4] as *mut _,
                        &mut png_primaries[5] as *mut _,
                    ) == PNG_INFO_cHRM
                    {
                        primaries = png_primaries.map(|x| x as f32);
                        yuv.color_primaries = ColorPrimaries::find(&primaries);
                        if yuv.color_primaries == ColorPrimaries::Unknown {
                            should_generate_icc = true;
                        }
                    } else {
                        primaries = ColorPrimaries::Bt709.values().unwrap();
                    }
                    if should_generate_icc {
                        yuv.color_primaries = ColorPrimaries::Unspecified;
                        yuv.transfer_characteristics = TransferCharacteristics::Unspecified;
                        yuv.icc = icc::generate_icc(yuv.yuv_format, gamma as f32, &primaries)
                            .unwrap_or_default();
                    }
                }
            }

            let num_channels = png_get_channels(png.png, png.info);
            let mut rgb = rgb::Image::create_from_yuv(&yuv);
            rgb.depth = bit_depth;
            rgb.format = match num_channels {
                1 => rgb::Format::Gray,
                2 => rgb::Format::GrayA,
                3 => rgb::Format::Rgb,
                4 => rgb::Format::Rgba,
                _ => {
                    return Err(AvifError::UnknownError(format!(
                        "png_get_channels() returned an invalid value: {num_channels}"
                    )));
                }
            };
            rgb.allocate()?;
            let row_bytes = png_get_rowbytes(png.png, png.info);
            if rgb.row_bytes != row_bytes as _ {
                return Err(AvifError::UnknownError(format!(
                    "rowBytes mismatch libavif {} vs libpng {}",
                    rgb.row_bytes, row_bytes
                )));
            }
            let mut row_pointers: Vec<png_bytep> = create_vec_exact(usize_from_u32(rgb.height)?)?;
            let mut row_pointer = rgb.pixels_mut();
            let row_bytes = usize_from_u32(rgb.row_bytes)?;
            for _ in 0..rgb.height {
                row_pointers.push(row_pointer);
                row_pointer = row_pointer.add(row_bytes);
            }
            png_read_image(png.png, row_pointers.as_mut_ptr());
            rgb.convert_to_yuv(&mut yuv)?;

            let mut ignore_exif = config.ignore_exif;
            let mut ignore_xmp = config.ignore_xmp;
            extract_exif_and_xmp(
                &mut yuv,
                png.png,
                png.info,
                &mut ignore_exif,
                &mut ignore_xmp,
            )?;
            if !ignore_exif || !ignore_xmp {
                png_read_end(png.png, png.info);
                extract_exif_and_xmp(
                    &mut yuv,
                    png.png,
                    png.info,
                    &mut ignore_exif,
                    &mut ignore_xmp,
                )?;
            }
            Ok((yuv, 0, None))
        }
    }

    fn has_more_frames(&mut self) -> bool {
        // TODO: b/403090413 - maybe support APNG?
        false
    }
}
