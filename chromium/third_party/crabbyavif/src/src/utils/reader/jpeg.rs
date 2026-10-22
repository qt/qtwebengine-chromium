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
use crate::utils::pixels::Pixels;
use crate::utils::*;

use super::xmp;
use super::Config;
use super::Reader;

use crate::internal_utils::stream::IStream;

use std::fs::File;
use std::io::{BufReader, Read, Seek, SeekFrom};

use zune_jpeg::zune_core::bytestream::ZCursor;
use zune_jpeg::JpegDecoder;

pub struct JpegReader {
    filename: String,
}

impl JpegReader {
    pub fn create(filename: &str) -> AvifResult<Self> {
        Ok(Self {
            filename: filename.into(),
        })
    }
}

impl Reader for JpegReader {
    fn read_frame(&mut self, config: &Config) -> AvifResult<(Image, u64, Option<GainMap>)> {
        let file = File::open(self.filename.clone()).map_err(AvifError::map_unknown_error)?;
        let mut decoder = JpegDecoder::new(BufReader::new(file));
        decoder
            .decode_headers()
            .map_err(|e| AvifError::UnknownError(format!("jpeg header decode error: {e:?}")))?;
        let info = decoder
            .info()
            .ok_or(AvifError::UnknownError("jpeg info not found".into()))?;
        if info.components != 3 {
            return AvifError::unknown_error(format!(
                "jpeg components was something other than 3: {}",
                info.components
            ));
        }
        let width = info.width as u32;
        let height = info.height as u32;
        let icc = decoder.icc_profile().unwrap_or_default();
        let exif = info.exif_data.clone().unwrap_or_default();
        let xmp = info.xmp_data.clone().unwrap_or_default();
        let (irot_angle, imir_axis) = exif::get_orientation(&exif)?;
        let rgb_bytes = decoder
            .decode()
            .map_err(|e| AvifError::UnknownError(format!("jpeg decode error: {e:?}")))?;
        let rgb = rgb::Image {
            width,
            height,
            depth: 8,
            format: rgb::Format::Rgb,
            pixels: Some(Pixels::Buffer(rgb_bytes)),
            row_bytes: width * 3,
            ..Default::default()
        };
        let mut yuv = Image {
            width,
            height,
            depth: config.depth.unwrap_or(8),
            yuv_format: config.yuv_format.unwrap_or(PixelFormat::Yuv420),
            yuv_range: YuvRange::Full,
            matrix_coefficients: config
                .matrix_coefficients
                .unwrap_or(MatrixCoefficients::Bt601),
            icc,
            exif,
            xmp,
            irot_angle,
            imir_axis,
            ..Default::default()
        };
        let mut gainmap = None;
        if let (Some(mpf_data), Some(mpf_offset)) = (
            &info.multi_picture_information,
            info.multi_picture_information_offset,
        ) {
            if let Ok(aux_images) = extract_aux_images(mpf_data, u32_from_u64(mpf_offset)?) {
                let mut file =
                    File::open(self.filename.clone()).map_err(AvifError::map_unknown_error)?;
                gainmap = get_gainmap(&mut file, &aux_images, &yuv)?;
            }
        }
        if let Some(gainmap) = &mut gainmap {
            gainmap.alt_color_primaries = yuv.color_primaries;
            gainmap.alt_transfer_characteristics = TransferCharacteristics::Pq;
            gainmap.alt_matrix_coefficients = yuv.matrix_coefficients;
            gainmap.alt_plane_depth = 8;
            gainmap.alt_plane_count = if yuv.yuv_format == PixelFormat::Yuv400
                && gainmap.image.yuv_format == PixelFormat::Yuv400
            {
                1
            } else {
                3
            };
            gainmap.alt_icc = yuv.icc.clone();
        }
        rgb.convert_to_yuv(&mut yuv)?;
        Ok((yuv, 0, gainmap))
    }

    fn has_more_frames(&mut self) -> bool {
        false
    }
}

fn extract_aux_images(mpf_data: &[u8], mpf_offset: u32) -> AvifResult<Vec<(u32, u32)>> {
    let mut stream = IStream::create(mpf_data);
    let is_big_endian = stream.get_slice(2)? == b"MM";
    let read_u16 = |stream: &mut IStream| -> AvifResult<u16> {
        if is_big_endian {
            stream.read_u16()
        } else {
            stream.read_u16_le()
        }
    };
    let read_u32 = |stream: &mut IStream| -> AvifResult<u32> {
        if is_big_endian {
            stream.read_u32()
        } else {
            stream.read_u32_le()
        }
    };
    let magic = read_u16(&mut stream)?;
    if magic != 42 {
        return Err(AvifError::UnknownError("Invalid MPF magic number".into()));
    }
    let first_ifd_offset = usize_from_u32(read_u32(&mut stream)?)?;
    stream = IStream::create(&mpf_data[first_ifd_offset..]);
    let num_entries = read_u16(&mut stream)?;
    let mut num_images = 0;
    let mut mp_entry_offset = 0;
    for _ in 0..num_entries {
        let tag_id = read_u16(&mut stream)?;
        stream.skip_u16()?; // tag_type
        stream.skip_u32()?; // count
        if tag_id == 45056 {
            // MPFVersion
            if stream.get_slice(4)? != b"0100" {
                return Err(AvifError::UnknownError("Invalid MPF version".into()));
            }
            continue;
        }
        let value = read_u32(&mut stream)?;
        match tag_id {
            // NumberOfImages
            45057 => num_images = value,
            // MPEntry
            45058 => mp_entry_offset = usize_from_u32(value)?,
            _ => {}
        }
    }
    if num_images < 2 || mp_entry_offset == 0 {
        return Ok(Vec::new());
    }
    stream = IStream::create(&mpf_data[mp_entry_offset..]);
    let mut aux_images = Vec::new();
    for _ in 0..num_images {
        stream.skip_u32()?; // attr
        let size = read_u32(&mut stream)?;
        let offset = read_u32(&mut stream)?;
        stream.skip_u32()?; // dep
        if offset != 0 {
            aux_images.push((checked_add!(mpf_offset, offset)?, size));
        }
    }
    Ok(aux_images)
}

fn get_gainmap(
    file: &mut File,
    aux_images: &[(u32, u32)],
    base_image: &Image,
) -> AvifResult<Option<GainMap>> {
    for &(offset, size) in aux_images {
        file.seek(SeekFrom::Start(offset as u64))
            .map_err(AvifError::map_unknown_error)?;
        let mut data = vec![0u8; size as usize];
        if file.read_exact(&mut data).is_err() {
            continue;
        }
        let mut decoder = JpegDecoder::new(ZCursor::new(&data));
        if decoder.decode_headers().is_err() {
            continue;
        }
        let info = match decoder.info() {
            Some(info) => info,
            None => continue,
        };

        if let Some(xmp) = &info.xmp_data {
            if let Ok((mut metadata, is_apple)) = xmp::parse_gainmap_metadata(xmp) {
                if is_apple && metadata.alternate_hdr_headroom.0 == 0 {
                    match exif::apple_headroom(&base_image.exif) {
                        Ok(Some(headroom)) if headroom > 0.0 => {
                            metadata.alternate_hdr_headroom = headroom.into();
                            metadata.max = [headroom.into(); 3];
                        }
                        _ => return Ok(None),
                    }
                }
                let width = info.width as u32;
                let height = info.height as u32;
                let rgb_bytes = match decoder.decode() {
                    Ok(b) => b,
                    _ => continue,
                };
                let format = match info.components {
                    1 => rgb::Format::Gray,
                    3 => rgb::Format::Rgb,
                    _ => continue,
                };
                let rgb = rgb::Image {
                    width,
                    height,
                    depth: 8,
                    format,
                    pixels: Some(Pixels::Buffer(rgb_bytes)),
                    row_bytes: width * format.channel_count(),
                    ..Default::default()
                };
                let mut image = Image {
                    width,
                    height,
                    depth: 8,
                    yuv_format: match format {
                        rgb::Format::Gray => PixelFormat::Yuv400,
                        _ => PixelFormat::Yuv444,
                    },
                    matrix_coefficients: MatrixCoefficients::Bt601,
                    ..Default::default()
                };
                rgb.convert_to_yuv(&mut image)?;
                return Ok(Some(GainMap {
                    image,
                    metadata,
                    ..Default::default()
                }));
            }
        }
    }
    Ok(None)
}
