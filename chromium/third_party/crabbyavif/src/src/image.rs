// Copyright 2024 Google LLC
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

use crate::decoder::ProgressiveState;
use crate::internal_utils::*;
use crate::reformat::coeffs::*;
use crate::utils::clap::*;
use crate::utils::pixels::*;
use crate::*;

#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Plane {
    Y = 0,
    U = 1,
    V = 2,
    A = 3,
}

impl From<usize> for Plane {
    fn from(plane: usize) -> Self {
        match plane {
            1 => Plane::U,
            2 => Plane::V,
            3 => Plane::A,
            _ => Plane::Y,
        }
    }
}

impl Plane {
    pub(crate) fn as_usize(&self) -> usize {
        match self {
            Plane::Y => 0,
            Plane::U => 1,
            Plane::V => 2,
            Plane::A => 3,
        }
    }
}

/// cbindgen:ignore
pub const MAX_PLANE_COUNT: usize = 4;
pub const YUV_PLANES: [Plane; 3] = [Plane::Y, Plane::U, Plane::V];
pub const A_PLANE: [Plane; 1] = [Plane::A];
pub const ALL_PLANES: [Plane; MAX_PLANE_COUNT] = [Plane::Y, Plane::U, Plane::V, Plane::A];

#[repr(C)]
#[derive(Clone, Copy, Debug, Default, PartialEq)]
// VideoFullRangeFlag as specified in ISO/IEC 23091-2/ITU-T H.273.
pub enum YuvRange {
    Limited = 0,
    #[default]
    Full = 1,
}

#[derive(Default)]
pub struct Image {
    pub width: u32,
    pub height: u32,
    pub depth: u8,

    pub yuv_format: PixelFormat,
    pub yuv_range: YuvRange,
    pub chroma_sample_position: ChromaSamplePosition,

    pub alpha_present: bool,
    pub alpha_premultiplied: bool,

    pub row_bytes: [u32; MAX_PLANE_COUNT],

    pub planes: [Option<Pixels>; MAX_PLANE_COUNT],

    pub color_primaries: ColorPrimaries,
    pub transfer_characteristics: TransferCharacteristics,
    pub matrix_coefficients: MatrixCoefficients,

    pub clli: Option<ContentLightLevelInformation>,
    pub pasp: Option<PixelAspectRatio>,
    pub clap: Option<CleanAperture>,
    pub irot_angle: Option<u8>,
    pub imir_axis: Option<u8>,

    pub exif: Vec<u8>,
    pub icc: Vec<u8>,
    pub xmp: Vec<u8>,

    pub image_sequence_track_present: bool,
    pub progressive_state: ProgressiveState,
}

pub struct PlaneData {
    pub width: u32,
    pub height: u32,
    pub row_bytes: u32,
    pub pixel_size: u32,
}

impl Image {
    // Creates an instance with all the properties of self but with pixels and
    // Exif, XMP, ICC metadata left unallocated.
    pub(crate) fn shallow_clone(&self) -> Self {
        Self {
            // Fields requiring dynamic allocation.
            row_bytes: [0; MAX_PLANE_COUNT],
            planes: [const { None }; MAX_PLANE_COUNT],
            exif: vec![],
            icc: vec![],
            xmp: vec![],

            // All other field values can be copied.
            ..*self
        }
    }

    pub(crate) fn is_supported_depth(depth: u8) -> bool {
        matches!(depth, 8 | 10 | 12 | 16)
    }

    pub(crate) fn depth_valid(&self) -> bool {
        Self::is_supported_depth(self.depth)
    }

    pub fn max_channel(&self) -> u16 {
        if !self.depth_valid() {
            0
        } else {
            ((1i32 << self.depth) - 1) as u16
        }
    }

    pub(crate) fn max_channel_f(&self) -> f32 {
        self.max_channel() as f32
    }

    pub fn has_plane(&self, plane: Plane) -> bool {
        let plane_index = plane.as_usize();
        if self.planes[plane_index].is_none() || self.row_bytes[plane_index] == 0 {
            return false;
        }
        self.planes[plane_index].unwrap_ref().has_data()
    }

    pub fn has_alpha(&self) -> bool {
        self.has_plane(Plane::A)
    }

    pub(crate) fn has_same_properties(&self, other: &Image) -> bool {
        self.width == other.width && self.height == other.height && self.depth == other.depth
    }

    // TODO: b/392112497 - remove this annotation once encoder feature is enabled by default.
    #[allow(dead_code)]
    pub(crate) fn has_same_cicp(&self, other: &Image) -> bool {
        self.depth == other.depth
            && self.yuv_format == other.yuv_format
            && self.yuv_range == other.yuv_range
            && self.chroma_sample_position == other.chroma_sample_position
            && self.color_primaries == other.color_primaries
            && self.transfer_characteristics == other.transfer_characteristics
            && self.matrix_coefficients == other.matrix_coefficients
    }

    pub fn has_same_properties_and_cicp(&self, other: &Image) -> bool {
        self.has_same_properties(other) && self.has_same_cicp(other)
    }

    pub fn width(&self, plane: Plane) -> usize {
        match plane {
            Plane::Y | Plane::A => self.width as usize,
            Plane::U => match self.yuv_format {
                PixelFormat::Yuv444
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => self.width as usize,
                PixelFormat::Yuv420 | PixelFormat::Yuv422 => (self.width as usize).div_ceil(2),
                PixelFormat::None | PixelFormat::Yuv400 => 0,
            },
            Plane::V => match self.yuv_format {
                PixelFormat::Yuv444 => self.width as usize,
                PixelFormat::Yuv420 | PixelFormat::Yuv422 => (self.width as usize).div_ceil(2),
                PixelFormat::None
                | PixelFormat::Yuv400
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => 0,
            },
        }
    }

    pub fn height(&self, plane: Plane) -> usize {
        match plane {
            Plane::Y | Plane::A => self.height as usize,
            Plane::U => match self.yuv_format {
                PixelFormat::Yuv444 | PixelFormat::Yuv422 => self.height as usize,
                PixelFormat::Yuv420
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => (self.height as usize).div_ceil(2),
                PixelFormat::None | PixelFormat::Yuv400 => 0,
            },
            Plane::V => match self.yuv_format {
                PixelFormat::Yuv444 | PixelFormat::Yuv422 => self.height as usize,
                PixelFormat::Yuv420 => (self.height as usize).div_ceil(2),
                PixelFormat::None
                | PixelFormat::Yuv400
                | PixelFormat::AndroidP010
                | PixelFormat::AndroidNv12
                | PixelFormat::AndroidNv21 => 0,
            },
        }
    }

    pub fn plane_data(&self, plane: Plane) -> Option<PlaneData> {
        if !self.has_plane(plane) {
            return None;
        }
        Some(PlaneData {
            width: self.width(plane) as u32,
            height: self.height(plane) as u32,
            row_bytes: self.row_bytes[plane.as_usize()],
            pixel_size: if self.depth == 8 { 1 } else { 2 },
        })
    }

    pub fn row(&self, plane: Plane, row: u32) -> AvifResult<&[u8]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.as_usize()]
            .unwrap_ref()
            .slice(start, row_bytes)
    }

    // Same as row() but only returns `width` pixels (extra row padding is excluded).
    pub fn row_exact(&self, plane: Plane, row: u32) -> AvifResult<&[u8]> {
        let width = self.width(plane);
        Ok(&self.row(plane, row)?[0..width])
    }

    pub fn row_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u8]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.as_usize()]
            .unwrap_mut()
            .slice_mut(start, row_bytes)
    }

    // Same as row_mut() but only returns `width` pixels (extra row padding is excluded).
    pub fn row_exact_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u8]> {
        let width = self.width(plane);
        Ok(&mut self.row_mut(plane, row)?[0..width])
    }

    pub fn row16(&self, plane: Plane, row: u32) -> AvifResult<&[u16]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes / 2;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.as_usize()]
            .unwrap_ref()
            .slice16(start, row_bytes)
    }

    // Same as row16() but only returns `width` pixels (extra row padding is excluded).
    pub fn row16_exact(&self, plane: Plane, row: u32) -> AvifResult<&[u16]> {
        let width = self.width(plane);
        Ok(&self.row16(plane, row)?[0..width])
    }

    pub fn row16_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u16]> {
        let plane_data = self.plane_data(plane).ok_or(AvifError::NoContent)?;
        let row_bytes = plane_data.row_bytes / 2;
        let start = checked_mul!(row, row_bytes)?;
        self.planes[plane.as_usize()]
            .unwrap_mut()
            .slice16_mut(start, row_bytes)
    }

    // Same as row16_mut() but only returns `width` pixels (extra row padding is excluded).
    pub fn row16_exact_mut(&mut self, plane: Plane, row: u32) -> AvifResult<&mut [u16]> {
        let width = self.width(plane);
        Ok(&mut self.row16_mut(plane, row)?[0..width])
    }

    // Returns a view with the same image properties as self and pointing to
    // the pixel values of self. Copies all Exif, XMP and ICC data if any.
    #[cfg(feature = "cli")]
    pub fn cropped_image(&self) -> AvifResult<Image> {
        match self.clap {
            Some(clap) => {
                match CropRect::create_from(&clap, self.width, self.height, self.yuv_format) {
                    Ok(rect) => {
                        let mut image = self.shallow_clone();
                        image.width = rect.width;
                        image.height = rect.height;
                        for plane in ALL_PLANES {
                            if self.planes[plane.as_usize()].is_none() {
                                continue;
                            }
                            let (x, y) = if plane == Plane::Y || plane == Plane::A {
                                (usize_from_u32(rect.x)?, rect.y)
                            } else {
                                (
                                    usize_from_u32(image.yuv_format.apply_chroma_shift_x(rect.x))?,
                                    image.yuv_format.apply_chroma_shift_y(rect.y),
                                )
                            };
                            let ptr = if image.depth == 8 {
                                let row = self.row(plane, y)?;
                                // SAFETY: rect is a valid rectangle that is guaranteed to be
                                // within the image bounds. So this pointer is pointing to a valid
                                // buffer.
                                unsafe { row.as_ptr().add(x) as *mut u8 }
                            } else {
                                let row = self.row16(plane, y)?;
                                // SAFETY: rect is a valid rectangle that is guaranteed to be
                                // within the image bounds. So this pointer is pointing to a valid
                                // buffer.
                                unsafe { row.as_ptr().add(x) as *mut u8 }
                            };
                            image.planes[plane.as_usize()] = Some(Pixels::from_raw_pointer(
                                ptr,
                                image.depth as _,
                                u32_from_usize(image.height(plane))?,
                                self.row_bytes[plane.as_usize()],
                            )?);
                            image.row_bytes[plane.as_usize()] = self.row_bytes[plane.as_usize()];
                        }
                        image.exif = self.exif.try_clone()?;
                        image.xmp = self.xmp.try_clone()?;
                        image.icc = self.icc.try_clone()?;
                        Ok(image)
                    }
                    Err(e) => Err(e),
                }
            }
            None => Err(AvifError::InvalidArgument),
        }
    }

    #[cfg(feature = "libyuv")]
    pub(crate) fn plane_ptrs(&self) -> [*const u8; 4] {
        ALL_PLANES.map(|x| {
            if self.has_plane(x) {
                self.planes[x.as_usize()].unwrap_ref().ptr()
            } else {
                std::ptr::null()
            }
        })
    }

    #[cfg(any(feature = "libyuv", feature = "sharpyuv"))]
    pub(crate) fn plane_ptrs_mut(&mut self) -> [*mut u8; 4] {
        ALL_PLANES.map(|x| {
            if self.has_plane(x) {
                self.planes[x.as_usize()].unwrap_mut().ptr_mut_generic()
            } else {
                std::ptr::null_mut()
            }
        })
    }

    #[cfg(feature = "libyuv")]
    pub(crate) fn plane16_ptrs(&self) -> [*const u16; 4] {
        ALL_PLANES.map(|x| {
            if self.has_plane(x) {
                self.planes[x.as_usize()].unwrap_ref().ptr16()
            } else {
                std::ptr::null()
            }
        })
    }

    #[cfg(any(feature = "libyuv", feature = "jpegxl"))]
    pub(crate) fn plane_row_bytes(&self) -> AvifResult<[i32; 4]> {
        Ok(ALL_PLANES.map(|x| {
            if self.has_plane(x) {
                i32_from_u32(self.plane_data(x).unwrap().row_bytes).unwrap()
            } else {
                0
            }
        }))
    }

    #[cfg(any(feature = "dav1d", feature = "avm"))]
    pub(crate) fn free_planes(&mut self, planes: &[Plane]) {
        for plane in planes {
            let plane = plane.as_usize();
            self.planes[plane] = None;
            self.row_bytes[plane] = 0;
        }
    }

    #[cfg(feature = "dav1d")]
    pub(crate) fn clear_chroma_planes(&mut self) {
        self.free_planes(&[Plane::U, Plane::V])
    }

    pub(crate) fn allocate_planes_with_default_values(
        &mut self,
        category: Category,
        default_values: [u16; 4],
    ) -> AvifResult<()> {
        let pixel_size: usize = if self.depth == 8 { 1 } else { 2 };
        for plane in category.planes() {
            let plane = *plane;
            let plane_index = plane.as_usize();
            let width = round2_usize(self.width(plane));
            let plane_size = checked_mul!(width, round2_usize(self.height(plane)))?;
            self.planes[plane_index] = Some(if self.depth == 8 {
                Pixels::Buffer(Vec::new())
            } else {
                Pixels::Buffer16(Vec::new())
            });
            let pixels = self.planes[plane_index].unwrap_mut();
            pixels.resize(plane_size, default_values[plane_index])?;
            self.row_bytes[plane_index] = u32_from_usize(checked_mul!(width, pixel_size)?)?;
        }
        Ok(())
    }

    pub fn allocate_planes(&mut self, category: Category) -> AvifResult<()> {
        self.allocate_planes_with_default_values(category, [0, 0, 0, self.max_channel()])
    }

    #[cfg(feature = "encoder")]
    pub(crate) fn copy_and_pad(&mut self, image: &Image) -> AvifResult<()> {
        if image.width > self.width || image.height > self.height {
            return AvifError::invalid_argument();
        }
        self.allocate_planes(Category::Color)?;
        if image.has_alpha() {
            self.allocate_planes(Category::Alpha)?;
        }
        for plane in ALL_PLANES {
            let src_plane = match image.plane_data(plane) {
                Some(pd) => pd,
                None => continue,
            };
            if self.depth == 8 {
                for y in 0..src_plane.height {
                    let src_row = image.row_exact(plane, y)?;
                    let dst_row = self.row_mut(plane, y)?;
                    let dst_slice = &mut dst_row[0..src_row.len()];
                    dst_slice.copy_from_slice(src_row);
                    let dst_slice = &mut dst_row[src_row.len()..];
                    dst_slice.fill(*src_row.last().unwrap());
                }
            } else {
                for y in 0..src_plane.height {
                    let src_row = image.row16_exact(plane, y)?;
                    let dst_row = self.row16_mut(plane, y)?;
                    let dst_slice = &mut dst_row[0..src_row.len()];
                    dst_slice.copy_from_slice(src_row);
                    let dst_slice = &mut dst_row[src_row.len()..];
                    dst_slice.fill(*src_row.last().unwrap());
                }
            }
        }
        Ok(())
    }

    pub(crate) fn convert_rgba16_to_yuva(&self, rgba: [u16; 4]) -> [u16; 4] {
        let r = rgba[0] as f32 / 65535.0;
        let g = rgba[1] as f32 / 65535.0;
        let b = rgba[2] as f32 / 65535.0;
        let coeffs = calculate_yuv_coefficients(self.color_primaries, self.matrix_coefficients);
        let y = coeffs[0] * r + coeffs[1] * g + coeffs[2] * b;
        let u = (b - y) / (2.0 * (1.0 - coeffs[2]));
        let v = (r - y) / (2.0 * (1.0 - coeffs[0]));
        let uv_bias = (1 << (self.depth - 1)) as f32;
        let max_channel = self.max_channel_f();
        [
            (y * max_channel).clamp(0.0, max_channel) as u16,
            (u * max_channel + uv_bias).clamp(0.0, max_channel) as u16,
            (v * max_channel + uv_bias).clamp(0.0, max_channel) as u16,
            ((rgba[3] as f32) / 65535.0 * max_channel).round() as u16,
        ]
    }

    #[cfg(any(feature = "encoder", feature = "png"))]
    pub(crate) fn is_opaque(&self) -> bool {
        if let Some(plane_data) = self.plane_data(Plane::A) {
            let opaque_value = self.max_channel();
            if self.depth == 8 {
                for y in 0..plane_data.height {
                    let row = self.row_exact(Plane::A, y).unwrap();
                    if !row.iter().all(|pixel| *pixel == opaque_value as u8) {
                        return false;
                    }
                }
            } else {
                for y in 0..plane_data.height {
                    let row = self.row16_exact(Plane::A, y).unwrap();
                    if !row.iter().all(|pixel| *pixel == opaque_value) {
                        return false;
                    }
                }
            }
        }
        true
    }

    pub(crate) fn fill_plane_with_value(&mut self, plane: Plane, value: u16) -> AvifResult<()> {
        if let Some(plane_data) = self.plane_data(plane) {
            if self.depth == 8 {
                for y in 0..plane_data.height {
                    let row =
                        &mut self.row_exact_mut(plane, y).unwrap()[..plane_data.width as usize];
                    row.fill(value as u8);
                }
            } else {
                for y in 0..plane_data.height {
                    let row =
                        &mut self.row16_exact_mut(plane, y).unwrap()[..plane_data.width as usize];
                    row.fill(value);
                }
            }
        }
        Ok(())
    }

    #[cfg(feature = "png")]
    pub(crate) fn remove_trailing_null_from_xmp(&mut self) {
        if self.xmp.len() >= 2
            && self.xmp[self.xmp.len() - 1] == 0
            && self.xmp[self.xmp.len() - 2] != 0
        {
            self.xmp.pop();
        }
    }
}
