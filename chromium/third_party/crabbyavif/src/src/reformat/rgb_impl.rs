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

use super::rgb;
use super::rgb::*;

use crate::image::Plane;
use crate::image::YuvRange;
use crate::internal_utils::*;
use crate::*;

use std::cmp::min;

macro_rules! unorm_value8 {
    ($row:expr, $index:expr, $table:expr) => {
        // # Safety:
        // # $table has 255 values. $row is of type u8, so the $table access is always valid.
        // # $index is guaranteed to be within the bounds of $row[].
        unsafe { *$table.get_unchecked(*$row.get_unchecked($index) as usize) }
    };
}

macro_rules! unorm_value16 {
    ($row:expr, $index:expr, $max_channel:expr, $table:expr) => {
        // # Safety:
        // # $table has 1024 or 4096 values depending on image depth. the clamp to $max_channel
        //   makes the $table access always valid.
        // # $index is guaranteed to be within the bounds of $row[].
        unsafe { *$table.get_unchecked(min(*$row.get_unchecked($index), $max_channel) as usize) }
    };
}

macro_rules! to_unorm8 {
    ($bias_y: expr, $range_y: expr, $v: expr) => {
        clamp_i32((0.5 + ($v * $range_y + $bias_y)).floor() as i32, 0, 255) as u8
    };
}

macro_rules! to_unorm16 {
    ($bias_y: expr, $range_y: expr, $max_channel: expr, $v: expr) => {
        clamp_i32(
            (0.5 + ($v * $range_y + $bias_y)).floor() as i32,
            0,
            $max_channel as i32,
        ) as u16
    };
}

// Copies GBR samples to YUV samples. Returns Ok(None) if not implemented.
fn identity_yuv8_to_rgb8_full_range(
    image: &image::Image,
    rgb: &mut rgb::Image,
) -> AvifResult<Option<()>> {
    if image.yuv_format != PixelFormat::Yuv444 || rgb.format == Format::Rgb565 {
        return Ok(None); // Not implemented.
    }

    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let channel_count = rgb.channel_count() as usize;
    for i in 0..image.height {
        let y = image.row(Plane::Y, i)?;
        let u = image.row(Plane::U, i)?;
        let v = image.row(Plane::V, i)?;
        let rgb_pixels = rgb.row_mut(i)?;
        for j in 0..image.width as usize {
            rgb_pixels[(j * channel_count) + r_offset] = v[j];
            rgb_pixels[(j * channel_count) + g_offset] = y[j];
            rgb_pixels[(j * channel_count) + b_offset] = u[j];
        }
    }
    Ok(Some(()))
}

// Copies GBR samples to YUV samples. Returns Ok(None) if not implemented.
fn identity_yuv16_to_rgb16_full_range(
    image: &image::Image,
    rgb: &mut rgb::Image,
) -> AvifResult<Option<()>> {
    if image.yuv_format != PixelFormat::Yuv444 {
        return Ok(None); // Not implemented.
    }

    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let channel_count = rgb.channel_count() as usize;
    for i in 0..image.height {
        let y = image.row16(Plane::Y, i)?;
        let u = image.row16(Plane::U, i)?;
        let v = image.row16(Plane::V, i)?;
        let rgb_pixels = rgb.row16_mut(i)?;
        for j in 0..image.width as usize {
            rgb_pixels[(j * channel_count) + r_offset] = v[j];
            rgb_pixels[(j * channel_count) + g_offset] = y[j];
            rgb_pixels[(j * channel_count) + b_offset] = u[j];
        }
    }
    Ok(Some(()))
}

// This is a macro and not a function because this is invoked per-pixel and there is a non-trivial
// performance impact if this is made into a function call.
macro_rules! store_rgb_pixel8 {
    ($dst:ident, $rgb_565: ident, $index: ident, $r: ident, $g: ident, $b: ident, $r_offset: ident,
     $g_offset: ident, $b_offset: ident, $rgb_channel_count: ident, $rgb_max_channel_f: ident) => {
        let r8 = (0.5 + ($r * $rgb_max_channel_f)) as u8;
        let g8 = (0.5 + ($g * $rgb_max_channel_f)) as u8;
        let b8 = (0.5 + ($b * $rgb_max_channel_f)) as u8;
        if $rgb_565 {
            // References for RGB565 color conversion:
            // * https://docs.microsoft.com/en-us/windows/win32/directshow/working-with-16-bit-rgb
            // * https://chromium.googlesource.com/libyuv/libyuv/+/9892d70c965678381d2a70a1c9002d1cf136ee78/source/row_common.cc#2362
            let r16 = ((r8 >> 3) as u16) << 11;
            let g16 = ((g8 >> 2) as u16) << 5;
            let b16 = (b8 >> 3) as u16;
            let rgb565 = (r16 | g16 | b16).to_le_bytes();
            $dst[($index * $rgb_channel_count) + $r_offset] = rgb565[0];
            $dst[($index * $rgb_channel_count) + $r_offset + 1] = rgb565[1];
        } else {
            $dst[($index * $rgb_channel_count) + $r_offset] = r8;
            $dst[($index * $rgb_channel_count) + $g_offset] = g8;
            $dst[($index * $rgb_channel_count) + $b_offset] = b8;
        }
    };
}

fn yuv8_to_rgb8_color(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_565 = rgb.format == rgb::Format::Rgb565;
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row(Plane::Y, j)?;
        let u_row = image.row(Plane::U, uv_j)?;
        // If V plane is missing, then the format is NV12. In that case, set V
        // as U plane but starting at offset 1.
        let v_row = image.row(Plane::V, uv_j).unwrap_or(&u_row[1..]);
        let dst = rgb.row_mut(j)?;
        for i in 0..image.width as usize {
            let uv_i = if cfg!(feature = "android_mediacodec") {
                (i >> chroma_shift.0) << chroma_shift.1
            } else {
                // chroma_shift.1 is always 0 in this case.
                i >> chroma_shift.0
            };
            let y = unsafe { *table_y.get_unchecked(*y_row.get_unchecked(i) as usize) };
            let cb = unsafe { *table_uv.get_unchecked(*u_row.get_unchecked(uv_i) as usize) };
            let cr = unsafe { *table_uv.get_unchecked(*v_row.get_unchecked(uv_i) as usize) };
            let r = y + (2.0 * (1.0 - kr)) * cr;
            let b = y + (2.0 * (1.0 - kb)) * cb;
            let g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            let r = clamp_f32(r, 0.0, 1.0);
            let g = clamp_f32(g, 0.0, 1.0);
            let b = clamp_f32(b, 0.0, 1.0);
            store_rgb_pixel8!(
                dst,
                rgb_565,
                i,
                r,
                g,
                b,
                r_offset,
                g_offset,
                b_offset,
                rgb_channel_count,
                rgb_max_channel_f
            );
        }
    }
    Ok(())
}

fn yuv16_to_rgb16_color(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row16(Plane::Y, j).unwrap();
        let u_row = image.row16(Plane::U, uv_j).unwrap();
        // If V plane is missing, then the format is P010. In that case, set V
        // as U plane but starting at offset 1.
        let v_row = image.row16(Plane::V, uv_j).unwrap_or(&u_row[1..]);
        let dst = rgb.row16_mut(j)?;
        for i in 0..image.width as usize {
            let uv_i = if cfg!(feature = "android_mediacodec") {
                (i >> chroma_shift.0) << chroma_shift.1
            } else {
                // chroma_shift.1 is always 0 in this case.
                i >> chroma_shift.0
            };
            let y = unorm_value16!(y_row, i, yuv_max_channel, table_y);
            let cb = unorm_value16!(u_row, uv_i, yuv_max_channel, table_uv);
            let cr = unorm_value16!(v_row, uv_i, yuv_max_channel, table_uv);
            let r = y + (2.0 * (1.0 - kr)) * cr;
            let b = y + (2.0 * (1.0 - kb)) * cb;
            let g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            let r = clamp_f32(r, 0.0, 1.0);
            let g = clamp_f32(g, 0.0, 1.0);
            let b = clamp_f32(b, 0.0, 1.0);
            unsafe {
                *dst.get_unchecked_mut((i * rgb_channel_count) + r_offset) =
                    (0.5 + (r * rgb_max_channel_f)) as u16;
                *dst.get_unchecked_mut((i * rgb_channel_count) + g_offset) =
                    (0.5 + (g * rgb_max_channel_f)) as u16;
                *dst.get_unchecked_mut((i * rgb_channel_count) + b_offset) =
                    (0.5 + (b * rgb_max_channel_f)) as u16;
            }
        }
    }
    Ok(())
}

fn yuv16_to_rgb8_color(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_565 = rgb.format == rgb::Format::Rgb565;
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row16(Plane::Y, j)?;
        let u_row = image.row16(Plane::U, uv_j)?;
        // If V plane is missing, then the format is P010. In that case, set V
        // as U plane but starting at offset 1.
        let v_row = image.row16(Plane::V, uv_j).unwrap_or(&u_row[1..]);
        let dst = rgb.row_mut(j)?;
        for i in 0..image.width as usize {
            let uv_i = if cfg!(feature = "android_mediacodec") {
                (i >> chroma_shift.0) << chroma_shift.1
            } else {
                // chroma_shift.1 is always 0 in this case.
                i >> chroma_shift.0
            };
            let y = table_y[min(y_row[i], yuv_max_channel) as usize];
            let cb = table_uv[min(u_row[uv_i], yuv_max_channel) as usize];
            let cr = table_uv[min(v_row[uv_i], yuv_max_channel) as usize];
            let r = y + (2.0 * (1.0 - kr)) * cr;
            let b = y + (2.0 * (1.0 - kb)) * cb;
            let g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            let r = clamp_f32(r, 0.0, 1.0);
            let g = clamp_f32(g, 0.0, 1.0);
            let b = clamp_f32(b, 0.0, 1.0);
            store_rgb_pixel8!(
                dst,
                rgb_565,
                i,
                r,
                g,
                b,
                r_offset,
                g_offset,
                b_offset,
                rgb_channel_count,
                rgb_max_channel_f
            );
        }
    }
    Ok(())
}

fn yuv8_to_rgb16_color(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let chroma_shift = image.yuv_format.chroma_shift_x();
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row(Plane::Y, j).unwrap();
        let u_row = image.row(Plane::U, uv_j).unwrap();
        // If V plane is missing, then the format is NV12. In that case, set V
        // as U plane but starting at offset 1.
        let v_row = image.row(Plane::V, uv_j).unwrap_or_else(|_| &u_row[1..]);
        let dst = rgb.row16_mut(j)?;
        for i in 0..image.width as usize {
            let uv_i = if cfg!(feature = "android_mediacodec") {
                (i >> chroma_shift.0) << chroma_shift.1
            } else {
                // chroma_shift.1 is always 0 in this case.
                i >> chroma_shift.0
            };
            let y = unsafe { *table_y.get_unchecked(*y_row.get_unchecked(i) as usize) };
            let cb = unsafe { *table_uv.get_unchecked(*u_row.get_unchecked(uv_i) as usize) };
            let cr = unsafe { *table_uv.get_unchecked(*v_row.get_unchecked(uv_i) as usize) };
            let r = y + (2.0 * (1.0 - kr)) * cr;
            let b = y + (2.0 * (1.0 - kb)) * cb;
            let g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            let r = clamp_f32(r, 0.0, 1.0);
            let g = clamp_f32(g, 0.0, 1.0);
            let b = clamp_f32(b, 0.0, 1.0);
            unsafe {
                *dst.get_unchecked_mut((i * rgb_channel_count) + r_offset) =
                    (0.5 + (r * rgb_max_channel_f)) as u16;
                *dst.get_unchecked_mut((i * rgb_channel_count) + g_offset) =
                    (0.5 + (g * rgb_max_channel_f)) as u16;
                *dst.get_unchecked_mut((i * rgb_channel_count) + b_offset) =
                    (0.5 + (b * rgb_max_channel_f)) as u16;
            }
        }
    }
    Ok(())
}

fn yuv8_to_rgb8_monochrome(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, _table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_565 = rgb.format == rgb::Format::Rgb565;
    for j in 0..image.height {
        let y_row = image.row(Plane::Y, j)?;
        let dst = rgb.row_mut(j)?;
        for i in 0..image.width as usize {
            let y = table_y[y_row[i] as usize];
            store_rgb_pixel8!(
                dst,
                rgb_565,
                i,
                y,
                y,
                y,
                r_offset,
                g_offset,
                b_offset,
                rgb_channel_count,
                rgb_max_channel_f
            );
        }
    }
    Ok(())
}

fn yuv16_to_rgb16_monochrome(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, _table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    for j in 0..image.height {
        let y_row = image.row16(Plane::Y, j)?;
        let dst = rgb.row16_mut(j)?;
        for i in 0..image.width as usize {
            let y = table_y[min(y_row[i], yuv_max_channel) as usize];
            let rgb_pixel = (0.5 + (y * rgb_max_channel_f)) as u16;
            dst[(i * rgb_channel_count) + r_offset] = rgb_pixel;
            dst[(i * rgb_channel_count) + g_offset] = rgb_pixel;
            dst[(i * rgb_channel_count) + b_offset] = rgb_pixel;
        }
    }
    Ok(())
}

fn yuv16_to_rgb8_monochrome(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, _table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_565 = rgb.format == rgb::Format::Rgb565;
    for j in 0..image.height {
        let y_row = image.row16(Plane::Y, j)?;
        let dst = rgb.row_mut(j)?;
        for i in 0..image.width as usize {
            let y = table_y[min(y_row[i], yuv_max_channel) as usize];
            store_rgb_pixel8!(
                dst,
                rgb_565,
                i,
                y,
                y,
                y,
                r_offset,
                g_offset,
                b_offset,
                rgb_channel_count,
                rgb_max_channel_f
            );
        }
    }
    Ok(())
}

fn yuv8_to_rgb16_monochrome(
    image: &image::Image,
    rgb: &mut rgb::Image,
    kr: f32,
    kg: f32,
    kb: f32,
) -> AvifResult<()> {
    let (table_y, _table_uv) = unorm_lookup_tables(image, Mode::YuvCoefficients(kr, kg, kb))?;
    let rgb_max_channel_f = rgb.max_channel_f();
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    for j in 0..image.height {
        let y_row = image.row(Plane::Y, j)?;
        let dst = rgb.row16_mut(j)?;
        for i in 0..image.width as usize {
            let y = table_y[y_row[i] as usize];
            let rgb_pixel = (0.5 + (y * rgb_max_channel_f)) as u16;
            dst[(i * rgb_channel_count) + r_offset] = rgb_pixel;
            dst[(i * rgb_channel_count) + g_offset] = rgb_pixel;
            dst[(i * rgb_channel_count) + b_offset] = rgb_pixel;
        }
    }
    Ok(())
}

// Converts RGB samples to YUV samples. Returns Ok(None) if not implemented.
pub(crate) fn yuv_to_rgb_fast(
    image: &image::Image,
    rgb: &mut rgb::Image,
) -> AvifResult<Option<()>> {
    let mode: Mode = image.into();
    Ok(match mode {
        Mode::Identity => match (image.depth, rgb.depth, image.yuv_range) {
            (8, 8, YuvRange::Full) => identity_yuv8_to_rgb8_full_range(image, rgb)?,
            (16, 16, YuvRange::Full) => identity_yuv16_to_rgb16_full_range(image, rgb)?,
            _ => None,
        },
        Mode::YuvCoefficients(kr, kg, kb) => {
            let has_color = image.yuv_format != PixelFormat::Yuv400;
            if !cfg!(feature = "android_mediacodec") {
                // In this case, P010 and NV12 formats are not supported.
                assert_eq!(image.yuv_format.chroma_shift_x().1, 0);
            }
            Some(match (image.depth == 8, rgb.depth == 8, has_color) {
                (true, true, true) => yuv8_to_rgb8_color(image, rgb, kr, kg, kb),
                (false, false, true) => yuv16_to_rgb16_color(image, rgb, kr, kg, kb),
                (false, true, true) => yuv16_to_rgb8_color(image, rgb, kr, kg, kb),
                (true, false, true) => yuv8_to_rgb16_color(image, rgb, kr, kg, kb),
                (true, true, false) => yuv8_to_rgb8_monochrome(image, rgb, kr, kg, kb),
                (false, false, false) => yuv16_to_rgb16_monochrome(image, rgb, kr, kg, kb),
                (false, true, false) => yuv16_to_rgb8_monochrome(image, rgb, kr, kg, kb),
                (true, false, false) => yuv8_to_rgb16_monochrome(image, rgb, kr, kg, kb),
            }?)
        }
        Mode::Ycgco | Mode::YcgcoRe | Mode::YcgcoRo => None, // Not implemented
    })
}

fn bias_and_range_y(image: &image::Image) -> (f32, f32) {
    // Formula specified in ISO/IEC 23091-2.
    if image.yuv_range == YuvRange::Limited {
        (
            (16 << (image.depth - 8)) as f32,
            (219 << (image.depth - 8)) as f32,
        )
    } else {
        (0.0, image.max_channel_f())
    }
}

fn bias_and_range_uv(image: &image::Image) -> (f32, f32) {
    // Formula specified in ISO/IEC 23091-2.
    (
        (1 << (image.depth - 1)) as f32,
        if image.yuv_range == YuvRange::Limited {
            (224 << (image.depth - 8)) as f32
        } else {
            image.max_channel_f()
        },
    )
}

fn unorm_lookup_tables(
    image: &image::Image,
    mode: Mode,
) -> AvifResult<(Vec<f32>, Option<Vec<f32>>)> {
    let count = 1usize << image.depth;
    let mut table_y: Vec<f32> = create_vec_exact(count)?;
    let (bias_y, range_y) = bias_and_range_y(image);
    for cp in 0..count {
        table_y.push(((cp as f32) - bias_y) / range_y);
    }
    if mode == Mode::Identity {
        Ok((table_y, None))
    } else {
        let (bias_uv, range_uv) = bias_and_range_uv(image);
        let mut table_uv: Vec<f32> = create_vec_exact(count)?;
        for cp in 0..count {
            table_uv.push(((cp as f32) - bias_uv) / range_uv);
        }
        Ok((table_y, Some(table_uv)))
    }
}

#[allow(clippy::too_many_arguments)]
fn compute_rgb(
    y: f32,
    cb: f32,
    cr: f32,
    has_color: bool,
    mode: Mode,
    clamped_y: u16,
    yuv_max_channel: u16,
    rgb_max_channel: u16,
    rgb_max_channel_f: f32,
) -> (f32, f32, f32) {
    let r: f32;
    let g: f32;
    let b: f32;
    if has_color {
        match mode {
            Mode::Identity => {
                g = y;
                b = cb;
                r = cr;
            }
            Mode::Ycgco => {
                let t = y - cb;
                g = y + cb;
                b = t - cr;
                r = t + cr;
            }
            Mode::YcgcoRe | Mode::YcgcoRo => {
                // Equations (62) through (65) in https://www.itu.int/rec/T-REC-H.273
                let cg = (0.5 + cb * yuv_max_channel as f32).floor() as i32;
                let co = (0.5 + cr * yuv_max_channel as f32).floor() as i32;
                let t = clamped_y as i32 - (cg >> 1);
                let rgb_max_channel = rgb_max_channel as i32;
                g = clamp_i32(t + cg, 0, rgb_max_channel) as f32 / rgb_max_channel_f;
                let tmp_b = clamp_i32(t - (co >> 1), 0, rgb_max_channel) as f32;
                b = tmp_b / rgb_max_channel_f;
                r = clamp_i32(tmp_b as i32 + co, 0, rgb_max_channel) as f32 / rgb_max_channel_f;
            }
            Mode::YuvCoefficients(kr, kg, kb) => {
                r = y + (2.0 * (1.0 - kr)) * cr;
                b = y + (2.0 * (1.0 - kb)) * cb;
                g = y - ((2.0 * ((kr * (1.0 - kr) * cr) + (kb * (1.0 - kb) * cb))) / kg);
            }
        }
    } else {
        r = y;
        g = y;
        b = y;
    }
    (
        clamp_f32(r, 0.0, 1.0),
        clamp_f32(g, 0.0, 1.0),
        clamp_f32(b, 0.0, 1.0),
    )
}

fn yuv16_to_rgb_any(
    image: &image::Image,
    rgb: &mut rgb::Image,
    alpha_multiply_mode: AlphaMultiplyMode,
    fast_or_no_chroma_subsampling: bool,
) -> AvifResult<()> {
    let mode: Mode = image.into();
    let (table_y, table_uv) = unorm_lookup_tables(image, mode)?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let yuv_has_color = image.has_plane(Plane::U)
        && image.has_plane(Plane::V)
        && image.yuv_format != PixelFormat::Yuv400;
    let rgb_has_color = !rgb.format.is_gray();
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel = rgb.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let chroma_shift = image.yuv_format.chroma_shift_x();
    let image_width_minus_1 = (image.width - 1) as usize;
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row16(Plane::Y, j)?;
        let u_row = image.row16(Plane::U, uv_j).ok();
        let v_row = image.row16(Plane::V, uv_j).ok();
        let a_row = image.row16(Plane::A, j).ok();
        let uv_adj_j = if j == 0
            || (j == image.height - 1 && (j % 2) != 0)
            || image.yuv_format == PixelFormat::Yuv422
        {
            uv_j
        } else if (j % 2) != 0 {
            uv_j + 1
        } else {
            uv_j - 1
        };
        let u_adj_row = image.row16(Plane::U, uv_adj_j).ok();
        let v_adj_row = image.row16(Plane::V, uv_adj_j).ok();
        let (dst, dst16) = if rgb.depth == 8 {
            (
                rgb.row_mut(j).unwrap().as_mut_ptr(),
                std::ptr::null_mut() as _,
            )
        } else {
            (
                std::ptr::null_mut() as _,
                rgb.row16_mut(j).unwrap().as_mut_ptr(),
            )
        };
        #[allow(clippy::needless_range_loop)]
        for i in 0..image.width as usize {
            let clamped_y = min(y_row[i], yuv_max_channel);
            let y = table_y[clamped_y as usize];
            let mut cb = 0.5;
            let mut cr = 0.5;
            if yuv_has_color {
                let u_row = u_row.unwrap();
                let v_row = v_row.unwrap();
                let uv_i = (i >> chroma_shift.0) << chroma_shift.1;
                if fast_or_no_chroma_subsampling {
                    cb = unorm_value16!(u_row, uv_i, yuv_max_channel, table_uv);
                    cr = unorm_value16!(v_row, uv_i, yuv_max_channel, table_uv);
                } else {
                    // Bilinear filtering with weights. See
                    // https://github.com/AOMediaCodec/libavif/blob/0580334466d57fedb889d5ed7ae9574d6f66e00c/src/reformat.c#L657-L685.
                    let uv_adj_i = if i == 0 || (i == image_width_minus_1 && (i % 2) != 0) {
                        uv_i
                    } else if (i % 2) != 0 {
                        uv_i + 1
                    } else {
                        uv_i - 1
                    };

                    let u_adj_row = u_adj_row.unwrap();
                    let unorm_u = [
                        unorm_value16!(u_row, uv_i, yuv_max_channel, table_uv),
                        unorm_value16!(u_row, uv_adj_i, yuv_max_channel, table_uv),
                        unorm_value16!(u_adj_row, uv_i, yuv_max_channel, table_uv),
                        unorm_value16!(u_adj_row, uv_adj_i, yuv_max_channel, table_uv),
                    ];
                    cb = (unorm_u[0] * (9.0 / 16.0))
                        + (unorm_u[1] * (3.0 / 16.0))
                        + (unorm_u[2] * (3.0 / 16.0))
                        + (unorm_u[3] * (1.0 / 16.0));

                    let v_adj_row = v_adj_row.unwrap();
                    let unorm_v = [
                        unorm_value16!(v_row, uv_i, yuv_max_channel, table_uv),
                        unorm_value16!(v_row, uv_adj_i, yuv_max_channel, table_uv),
                        unorm_value16!(v_adj_row, uv_i, yuv_max_channel, table_uv),
                        unorm_value16!(v_adj_row, uv_adj_i, yuv_max_channel, table_uv),
                    ];
                    cr = (unorm_v[0] * (9.0 / 16.0))
                        + (unorm_v[1] * (3.0 / 16.0))
                        + (unorm_v[2] * (3.0 / 16.0))
                        + (unorm_v[3] * (1.0 / 16.0));
                }
            }
            let (mut rc, mut gc, mut bc) = if rgb_has_color {
                compute_rgb(
                    y,
                    cb,
                    cr,
                    yuv_has_color,
                    mode,
                    clamped_y,
                    yuv_max_channel,
                    rgb_max_channel,
                    rgb_max_channel_f,
                )
            } else {
                (clamp_f32(y, 0.0, 1.0), 0.0, 0.0)
            };
            if alpha_multiply_mode != AlphaMultiplyMode::NoOp {
                let unorm_a = min(a_row.unwrap()[i], yuv_max_channel);
                let ac = clamp_f32((unorm_a as f32) / (yuv_max_channel as f32), 0.0, 1.0);
                if ac == 0.0 {
                    rc = 0.0;
                    gc = 0.0;
                    bc = 0.0;
                } else if ac < 1.0 {
                    match alpha_multiply_mode {
                        AlphaMultiplyMode::Multiply => {
                            rc *= ac;
                            gc *= ac;
                            bc *= ac;
                        }
                        AlphaMultiplyMode::UnMultiply => {
                            rc = f32::min(rc / ac, 1.0);
                            gc = f32::min(gc / ac, 1.0);
                            bc = f32::min(bc / ac, 1.0);
                        }
                        _ => {} // Not reached.
                    }
                }
            }
            unsafe {
                if !dst.is_null() {
                    *dst.add((i * rgb_channel_count) + r_offset) =
                        (0.5 + (rc * rgb_max_channel_f)) as u8;
                    if rgb_has_color {
                        *dst.add((i * rgb_channel_count) + g_offset) =
                            (0.5 + (gc * rgb_max_channel_f)) as u8;
                        *dst.add((i * rgb_channel_count) + b_offset) =
                            (0.5 + (bc * rgb_max_channel_f)) as u8;
                    }
                } else {
                    *dst16.add((i * rgb_channel_count) + r_offset) =
                        (0.5 + (rc * rgb_max_channel_f)) as u16;
                    if rgb_has_color {
                        *dst16.add((i * rgb_channel_count) + g_offset) =
                            (0.5 + (gc * rgb_max_channel_f)) as u16;
                        *dst16.add((i * rgb_channel_count) + b_offset) =
                            (0.5 + (bc * rgb_max_channel_f)) as u16;
                    }
                }
            }
        }
    }
    Ok(())
}

fn yuv8_to_rgb_any(
    image: &image::Image,
    rgb: &mut rgb::Image,
    alpha_multiply_mode: AlphaMultiplyMode,
    fast_or_no_chroma_subsampling: bool,
) -> AvifResult<()> {
    let mode: Mode = image.into();
    let (table_y, table_uv) = unorm_lookup_tables(image, mode)?;
    let table_uv = match &table_uv {
        Some(table_uv) => table_uv,
        None => &table_y,
    };
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let yuv_has_color = image.has_plane(Plane::U)
        && image.has_plane(Plane::V)
        && image.yuv_format != PixelFormat::Yuv400;
    let rgb_has_color = !rgb.format.is_gray();
    let yuv_max_channel = image.max_channel();
    let rgb_max_channel = rgb.max_channel();
    let rgb_max_channel_f = rgb.max_channel_f();
    let chroma_shift = image.yuv_format.chroma_shift_x();
    let image_width_minus_1 = (image.width - 1) as usize;
    for j in 0..image.height {
        let uv_j = j >> image.yuv_format.chroma_shift_y();
        let y_row = image.row(Plane::Y, j)?;
        let u_row = image.row(Plane::U, uv_j).ok();
        let v_row = image.row(Plane::V, uv_j).ok();
        let a_row = image.row(Plane::A, j).ok();
        let uv_adj_j = if j == 0
            || (j == image.height - 1 && (j % 2) != 0)
            || image.yuv_format == PixelFormat::Yuv422
        {
            uv_j
        } else if (j % 2) != 0 {
            uv_j + 1
        } else {
            uv_j - 1
        };
        let u_adj_row = image.row(Plane::U, uv_adj_j).ok();
        let v_adj_row = image.row(Plane::V, uv_adj_j).ok();
        let (dst, dst16) = if rgb.depth == 8 {
            (
                rgb.row_mut(j).unwrap().as_mut_ptr(),
                std::ptr::null_mut() as _,
            )
        } else {
            (
                std::ptr::null_mut() as _,
                rgb.row16_mut(j).unwrap().as_mut_ptr(),
            )
        };
        for i in 0..image.width as usize {
            let clamped_y = unsafe { *y_row.get_unchecked(i) as u16 };
            let y = unsafe { *table_y.get_unchecked(clamped_y as usize) };
            let mut cb = 0.5;
            let mut cr = 0.5;
            if yuv_has_color {
                let u_row = u_row.unwrap();
                let v_row = v_row.unwrap();
                let uv_i = (i >> chroma_shift.0) << chroma_shift.1;
                if fast_or_no_chroma_subsampling {
                    cb = unorm_value8!(u_row, uv_i, table_uv);
                    cr = unorm_value8!(v_row, uv_i, table_uv);
                } else {
                    // Bilinear filtering with weights. See
                    // https://github.com/AOMediaCodec/libavif/blob/0580334466d57fedb889d5ed7ae9574d6f66e00c/src/reformat.c#L657-L685.
                    let uv_adj_i = if i == 0 || (i == image_width_minus_1 && (i % 2) != 0) {
                        uv_i
                    } else if (i % 2) != 0 {
                        uv_i + 1
                    } else {
                        uv_i - 1
                    };

                    let u_adj_row = u_adj_row.unwrap();
                    let unorm_u = [
                        unorm_value8!(u_row, uv_i, table_uv),
                        unorm_value8!(u_row, uv_adj_i, table_uv),
                        unorm_value8!(u_adj_row, uv_i, table_uv),
                        unorm_value8!(u_adj_row, uv_adj_i, table_uv),
                    ];
                    cb = (unorm_u[0] * (9.0 / 16.0))
                        + (unorm_u[1] * (3.0 / 16.0))
                        + (unorm_u[2] * (3.0 / 16.0))
                        + (unorm_u[3] * (1.0 / 16.0));

                    let v_adj_row = v_adj_row.unwrap();
                    let unorm_v = [
                        unorm_value8!(v_row, uv_i, table_uv),
                        unorm_value8!(v_row, uv_adj_i, table_uv),
                        unorm_value8!(v_adj_row, uv_i, table_uv),
                        unorm_value8!(v_adj_row, uv_adj_i, table_uv),
                    ];
                    cr = (unorm_v[0] * (9.0 / 16.0))
                        + (unorm_v[1] * (3.0 / 16.0))
                        + (unorm_v[2] * (3.0 / 16.0))
                        + (unorm_v[3] * (1.0 / 16.0));
                }
            }
            let (mut rc, mut gc, mut bc) = if rgb_has_color {
                compute_rgb(
                    y,
                    cb,
                    cr,
                    yuv_has_color,
                    mode,
                    clamped_y,
                    yuv_max_channel,
                    rgb_max_channel,
                    rgb_max_channel_f,
                )
            } else {
                (clamp_f32(y, 0.0, 1.0), 0.0, 0.0)
            };
            if alpha_multiply_mode != AlphaMultiplyMode::NoOp {
                let unorm_a = a_row.unwrap()[i];
                let ac = clamp_f32((unorm_a as f32) / (yuv_max_channel as f32), 0.0, 1.0);
                if ac == 0.0 {
                    rc = 0.0;
                    gc = 0.0;
                    bc = 0.0;
                } else if ac < 1.0 {
                    match alpha_multiply_mode {
                        AlphaMultiplyMode::Multiply => {
                            rc *= ac;
                            gc *= ac;
                            bc *= ac;
                        }
                        AlphaMultiplyMode::UnMultiply => {
                            rc = f32::min(rc / ac, 1.0);
                            gc = f32::min(gc / ac, 1.0);
                            bc = f32::min(bc / ac, 1.0);
                        }
                        _ => {} // Not reached.
                    }
                }
            }
            unsafe {
                if !dst.is_null() {
                    *dst.add((i * rgb_channel_count) + r_offset) =
                        (0.5 + (rc * rgb_max_channel_f)) as u8;
                    if rgb_has_color {
                        *dst.add((i * rgb_channel_count) + g_offset) =
                            (0.5 + (gc * rgb_max_channel_f)) as u8;
                        *dst.add((i * rgb_channel_count) + b_offset) =
                            (0.5 + (bc * rgb_max_channel_f)) as u8;
                    }
                } else {
                    *dst16.add((i * rgb_channel_count) + r_offset) =
                        (0.5 + (rc * rgb_max_channel_f)) as u16;
                    if rgb_has_color {
                        *dst16.add((i * rgb_channel_count) + g_offset) =
                            (0.5 + (gc * rgb_max_channel_f)) as u16;
                        *dst16.add((i * rgb_channel_count) + b_offset) =
                            (0.5 + (bc * rgb_max_channel_f)) as u16;
                    }
                }
            }
        }
    }
    Ok(())
}

pub(crate) fn yuv_to_rgb_any(
    image: &image::Image,
    rgb: &mut rgb::Image,
    alpha_multiply_mode: AlphaMultiplyMode,
) -> AvifResult<()> {
    let fast_or_no_chroma_subsampling = image.yuv_format == PixelFormat::Yuv444
        || matches!(
            rgb.chroma_upsampling,
            ChromaUpsampling::Fastest | ChromaUpsampling::Nearest
        );
    if !fast_or_no_chroma_subsampling
        && image.chroma_sample_position != ChromaSamplePosition::CENTER
    {
        return AvifError::not_implemented();
    }
    if image.depth > 8 {
        yuv16_to_rgb_any(
            image,
            rgb,
            alpha_multiply_mode,
            fast_or_no_chroma_subsampling,
        )
    } else {
        yuv8_to_rgb_any(
            image,
            rgb,
            alpha_multiply_mode,
            fast_or_no_chroma_subsampling,
        )
    }
}

#[derive(Debug, Default, Copy, Clone)]
struct YUVBlock(f32, f32, f32);

pub(crate) fn rgb_gray_to_yuv(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    let rgb_channel_count = rgb.channel_count() as usize;
    let gray_offset = rgb.format.r_offset();
    let rgb_max_channel_f = rgb.max_channel_f();
    let (bias_y, range_y) = bias_and_range_y(image);
    let yuv_max_channel = image.max_channel();
    for j in 0..image.height {
        for i in 0..image.width as usize {
            let gray_pixel = if rgb.depth == 8 {
                let src = rgb.row(j)?;
                src[(i * rgb_channel_count) + gray_offset] as f32 / rgb_max_channel_f
            } else {
                let src = rgb.row16(j)?;
                src[(i * rgb_channel_count) + gray_offset] as f32 / rgb_max_channel_f
            };
            // TODO: b/410088660 - handle alpha multiply/unmultiply.
            let gray_pixel = to_unorm(bias_y, range_y, yuv_max_channel, gray_pixel);
            if image.depth == 8 {
                let dst_y = image.row_mut(Plane::Y, j)?;
                dst_y[i] = gray_pixel as u8;
            } else {
                let dst_y = image.row16_mut(Plane::Y, j)?;
                dst_y[i] = gray_pixel;
            }
        }
    }
    let chroma_value = (image.max_channel() / 2) + 1;
    image.fill_plane_with_value(Plane::U, chroma_value)?;
    image.fill_plane_with_value(Plane::V, chroma_value)?;
    Ok(())
}

fn rgb_pixel_to_yuv_pixel(
    mode: Mode,
    r: f32,
    g: f32,
    b: f32,
    rgb_max_channel_f: f32,
    range_y: f32,
    range_uv: f32,
) -> YUVBlock {
    match mode {
        Mode::YuvCoefficients(kr, kg, kb) => {
            let y = (kr * r) + (kg * g) + (kb * b);
            YUVBlock(
                y,
                (b - y) / (2.0 * (1.0 - kb)),
                (r - y) / (2.0 * (1.0 - kr)),
            )
        }
        Mode::Identity => {
            // Formulas 41,42,43 from https://www.itu.int/rec/T-REC-H.273-201612-S.
            YUVBlock(g, b, r)
        }
        Mode::YcgcoRe | Mode::YcgcoRo => {
            // Formulas 58,59,60,61 from https://www.itu.int/rec/T-REC-H.273-202407-P.
            let r = ((r * rgb_max_channel_f).clamp(0.0, rgb_max_channel_f) + 0.5).floor() as i32;
            let g = ((g * rgb_max_channel_f).clamp(0.0, rgb_max_channel_f) + 0.5).floor() as i32;
            let b = ((b * rgb_max_channel_f).clamp(0.0, rgb_max_channel_f) + 0.5).floor() as i32;
            let co = r - b;
            let t = b + (co >> 1);
            let cg = g - t;
            YUVBlock(
                (t + (cg >> 1)) as f32 / range_y,
                cg as f32 / range_uv,
                co as f32 / range_uv,
            )
        }
        Mode::Ycgco => {
            // Formulas 44,45,46 from https://www.itu.int/rec/T-REC-H.273-201612-S.
            YUVBlock(
                0.5 * g + 0.25 * (r + b),
                0.5 * g - 0.25 * (r + b),
                0.5 * (r - b),
            )
        }
    }
}

// Reads an RGB pixel and converts it into YUV.
macro_rules! yuv_pixel {
    ($rgb:expr, $src16:expr, $src:expr, $i:expr, $rgb_channel_count:expr, $r_offset:expr, $g_offset:expr, $b_offset:expr, $rgb_max_channel_f:expr, $mode:expr, $range_y:expr, $range_uv:expr) => {{
        let rgb_pixel = unsafe {
            if $rgb.depth > 8 {
                [
                    *$src16.add(($i * $rgb_channel_count) + $r_offset) as f32 / $rgb_max_channel_f,
                    *$src16.add(($i * $rgb_channel_count) + $g_offset) as f32 / $rgb_max_channel_f,
                    *$src16.add(($i * $rgb_channel_count) + $b_offset) as f32 / $rgb_max_channel_f,
                ]
            } else {
                [
                    *$src.add(($i * $rgb_channel_count) + $r_offset) as f32 / 255.0,
                    *$src.add(($i * $rgb_channel_count) + $g_offset) as f32 / 255.0,
                    *$src.add(($i * $rgb_channel_count) + $b_offset) as f32 / 255.0,
                ]
            }
        };
        // TODO: b/410088660 - handle alpha multiply/unmultiply.
        rgb_pixel_to_yuv_pixel(
            $mode,
            rgb_pixel[0],
            rgb_pixel[1],
            rgb_pixel[2],
            $rgb_max_channel_f,
            $range_y,
            $range_uv,
        )
    }};
}

fn rgb_to_yuv_420(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_max_channel_f = rgb.max_channel_f();
    let mode = (image as &image::Image).into();
    let (bias_y, range_y) = bias_and_range_y(image);
    let (bias_uv, range_uv) = if mode == Mode::Identity {
        (bias_y, range_y)
    } else {
        bias_and_range_uv(image)
    };
    let yuv_max_channel = image.max_channel();
    let width = image.width as usize;

    for j in (0..image.height - 1).step_by(2) {
        let uv_j = j >> 1;
        let (dst_y16, dst_y16_1, dst_u16, dst_v16, dst_y, dst_y_1, dst_u, dst_v) =
            if image.depth > 8 {
                (
                    image.row16_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                    image.row16_mut(Plane::Y, j + 1).unwrap().as_mut_ptr(),
                    image.row16_mut(Plane::U, uv_j).unwrap().as_mut_ptr(),
                    image.row16_mut(Plane::V, uv_j).unwrap().as_mut_ptr(),
                    std::ptr::null_mut(),
                    std::ptr::null_mut(),
                    std::ptr::null_mut(),
                    std::ptr::null_mut(),
                )
            } else {
                (
                    std::ptr::null_mut(),
                    std::ptr::null_mut(),
                    std::ptr::null_mut(),
                    std::ptr::null_mut(),
                    image.row_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                    image.row_mut(Plane::Y, j + 1).unwrap().as_mut_ptr(),
                    image.row_mut(Plane::U, uv_j).unwrap().as_mut_ptr(),
                    image.row_mut(Plane::V, uv_j).unwrap().as_mut_ptr(),
                )
            };
        let (src16, src16_1, src, src_1) = if rgb.depth > 8 {
            (
                rgb.row16(j).unwrap().as_ptr(),
                rgb.row16(j + 1).unwrap().as_ptr(),
                std::ptr::null(),
                std::ptr::null(),
            )
        } else {
            (
                std::ptr::null(),
                std::ptr::null(),
                rgb.row(j).unwrap().as_ptr(),
                rgb.row(j + 1).unwrap().as_ptr(),
            )
        };
        for i in (0..width - 1).step_by(2) {
            let yuv_pixel = [
                yuv_pixel!(
                    rgb,
                    src16,
                    src,
                    i,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
                yuv_pixel!(
                    rgb,
                    src16,
                    src,
                    i + 1,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
                yuv_pixel!(
                    rgb,
                    src16_1,
                    src_1,
                    i,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
                yuv_pixel!(
                    rgb,
                    src16_1,
                    src_1,
                    i + 1,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
            ];
            let avg_u = (yuv_pixel[0].1 + yuv_pixel[1].1 + yuv_pixel[2].1 + yuv_pixel[3].1) / 4.0;
            let avg_v = (yuv_pixel[0].2 + yuv_pixel[1].2 + yuv_pixel[2].2 + yuv_pixel[3].2) / 4.0;
            let uv_i = i >> 1;
            unsafe {
                if image.depth > 8 {
                    *dst_y16.add(i) = to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[0].0);
                    *dst_y16.add(i + 1) =
                        to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[1].0);
                    *dst_y16_1.add(i) =
                        to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[2].0);
                    *dst_y16_1.add(i + 1) =
                        to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[3].0);
                    *dst_u16.add(uv_i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, avg_u);
                    *dst_v16.add(uv_i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, avg_v);
                } else {
                    *dst_y.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel[0].0);
                    *dst_y.add(i + 1) = to_unorm8!(bias_y, range_y, yuv_pixel[1].0);
                    *dst_y_1.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel[2].0);
                    *dst_y_1.add(i + 1) = to_unorm8!(bias_y, range_y, yuv_pixel[3].0);
                    *dst_u.add(uv_i) = to_unorm8!(bias_uv, range_uv, avg_u);
                    *dst_v.add(uv_i) = to_unorm8!(bias_uv, range_uv, avg_v);
                }
            }
        }
        // Last column.
        if width % 2 != 0 {
            let i = width - 1;
            let yuv_pixel = [
                yuv_pixel!(
                    rgb,
                    src16,
                    src,
                    i,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
                yuv_pixel!(
                    rgb,
                    src16_1,
                    src_1,
                    i,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
            ];
            let avg_u = (yuv_pixel[0].1 + yuv_pixel[1].1) / 2.0;
            let avg_v = (yuv_pixel[0].2 + yuv_pixel[1].2) / 2.0;
            let uv_i = i >> 1;
            unsafe {
                if image.depth > 8 {
                    *dst_y16.add(i) = to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[0].0);
                    *dst_y16_1.add(i) =
                        to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[1].0);
                    *dst_u16.add(uv_i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, avg_u);
                    *dst_v16.add(uv_i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, avg_v);
                } else {
                    *dst_y.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel[0].0);
                    *dst_y_1.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel[1].0);
                    *dst_u.add(uv_i) = to_unorm8!(bias_uv, range_uv, avg_u);
                    *dst_v.add(uv_i) = to_unorm8!(bias_uv, range_uv, avg_v);
                }
            }
        }
    }
    // Last row.
    if image.height % 2 != 0 {
        let j = image.height - 1;
        let uv_j = j >> 1;
        let (dst_y16, dst_u16, dst_v16, dst_y, dst_u, dst_v) = if image.depth > 8 {
            (
                image.row16_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                image.row16_mut(Plane::U, uv_j).unwrap().as_mut_ptr(),
                image.row16_mut(Plane::V, uv_j).unwrap().as_mut_ptr(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
            )
        } else {
            (
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                image.row_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                image.row_mut(Plane::U, uv_j).unwrap().as_mut_ptr(),
                image.row_mut(Plane::V, uv_j).unwrap().as_mut_ptr(),
            )
        };
        let (src16, src) = if rgb.depth > 8 {
            (rgb.row16(j).unwrap().as_ptr(), std::ptr::null())
        } else {
            (std::ptr::null(), rgb.row(j).unwrap().as_ptr())
        };
        for i in (0..width).step_by(2) {
            let is_last_column = width == 1 || i > width - 2;
            let yuv_pixel = [
                yuv_pixel!(
                    rgb,
                    src16,
                    src,
                    i,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
                if is_last_column {
                    YUVBlock(0.0, 0.0, 0.0)
                } else {
                    yuv_pixel!(
                        rgb,
                        src16,
                        src,
                        i + 1,
                        rgb_channel_count,
                        r_offset,
                        g_offset,
                        b_offset,
                        rgb_max_channel_f,
                        mode,
                        range_y,
                        range_uv
                    )
                },
            ];
            let (avg_u, avg_v) = if is_last_column {
                (yuv_pixel[0].1, yuv_pixel[0].2)
            } else {
                (
                    (yuv_pixel[0].1 + yuv_pixel[1].1) / 2.0,
                    (yuv_pixel[0].2 + yuv_pixel[1].2) / 2.0,
                )
            };
            let uv_i = i >> 1;
            unsafe {
                if image.depth > 8 {
                    *dst_y16.add(i) = to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[0].0);
                    if !is_last_column {
                        *dst_y16.add(i + 1) =
                            to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[1].0);
                    }
                    *dst_u16.add(uv_i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, avg_u);
                    *dst_v16.add(uv_i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, avg_v);
                } else {
                    *dst_y.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel[0].0);
                    if !is_last_column {
                        *dst_y.add(i + 1) = to_unorm8!(bias_y, range_y, yuv_pixel[1].0);
                    }
                    *dst_u.add(uv_i) = to_unorm8!(bias_uv, range_uv, avg_u);
                    *dst_v.add(uv_i) = to_unorm8!(bias_uv, range_uv, avg_v);
                }
            }
        }
    }
    Ok(())
}

fn rgb_to_yuv_422(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_max_channel_f = rgb.max_channel_f();
    let mode = (image as &image::Image).into();
    let (bias_y, range_y) = bias_and_range_y(image);
    let (bias_uv, range_uv) = if mode == Mode::Identity {
        (bias_y, range_y)
    } else {
        bias_and_range_uv(image)
    };
    let yuv_max_channel = image.max_channel();
    let width = image.width as usize;

    for j in 0..image.height {
        let (src16, src) = if rgb.depth > 8 {
            (rgb.row16(j).unwrap().as_ptr(), std::ptr::null())
        } else {
            (std::ptr::null(), rgb.row(j).unwrap().as_ptr())
        };
        let (dst_y16, dst_u16, dst_v16, dst_y, dst_u, dst_v) = if image.depth > 8 {
            (
                image.row16_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                image.row16_mut(Plane::U, j).unwrap().as_mut_ptr(),
                image.row16_mut(Plane::V, j).unwrap().as_mut_ptr(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
            )
        } else {
            (
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                image.row_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                image.row_mut(Plane::U, j).unwrap().as_mut_ptr(),
                image.row_mut(Plane::V, j).unwrap().as_mut_ptr(),
            )
        };
        for i in (0..width - 1).step_by(2) {
            let yuv_pixel = [
                yuv_pixel!(
                    rgb,
                    src16,
                    src,
                    i,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
                yuv_pixel!(
                    rgb,
                    src16,
                    src,
                    i + 1,
                    rgb_channel_count,
                    r_offset,
                    g_offset,
                    b_offset,
                    rgb_max_channel_f,
                    mode,
                    range_y,
                    range_uv
                ),
            ];
            let avg_u = (yuv_pixel[0].1 + yuv_pixel[1].1) / 2.0;
            let avg_v = (yuv_pixel[0].2 + yuv_pixel[1].2) / 2.0;
            let uv_i = i >> 1;
            unsafe {
                if image.depth > 8 {
                    *dst_y16.add(i) = to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[0].0);
                    *dst_y16.add(i + 1) =
                        to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel[1].0);
                    *dst_u16.add(uv_i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, avg_u);
                    *dst_v16.add(uv_i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, avg_v);
                } else {
                    *dst_y.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel[0].0);
                    *dst_y.add(i + 1) = to_unorm8!(bias_y, range_y, yuv_pixel[1].0);
                    *dst_u.add(uv_i) = to_unorm8!(bias_uv, range_uv, avg_u);
                    *dst_v.add(uv_i) = to_unorm8!(bias_uv, range_uv, avg_v);
                }
            }
        }
        // Last column.
        if width % 2 != 0 {
            let i = width - 1;
            let yuv_pixel = yuv_pixel!(
                rgb,
                src16,
                src,
                i,
                rgb_channel_count,
                r_offset,
                g_offset,
                b_offset,
                rgb_max_channel_f,
                mode,
                range_y,
                range_uv
            );
            let uv_i = i >> 1;
            unsafe {
                if image.depth > 8 {
                    *dst_y16.add(i) = to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel.0);
                    *dst_u16.add(uv_i) =
                        to_unorm16!(bias_uv, range_uv, yuv_max_channel, yuv_pixel.1);
                    *dst_v16.add(uv_i) =
                        to_unorm16!(bias_uv, range_uv, yuv_max_channel, yuv_pixel.2);
                } else {
                    *dst_y.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel.0);
                    *dst_u.add(uv_i) = to_unorm8!(bias_uv, range_uv, yuv_pixel.1);
                    *dst_v.add(uv_i) = to_unorm8!(bias_uv, range_uv, yuv_pixel.2);
                }
            }
        }
    }
    Ok(())
}

fn rgb_to_yuv_444(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_max_channel_f = rgb.max_channel_f();
    let mode = (image as &image::Image).into();
    let (bias_y, range_y) = bias_and_range_y(image);
    let (bias_uv, range_uv) = if mode == Mode::Identity {
        (bias_y, range_y)
    } else {
        bias_and_range_uv(image)
    };
    let yuv_max_channel = image.max_channel();
    let width = image.width as usize;

    for j in 0..image.height {
        let (src16, src) = if rgb.depth > 8 {
            (rgb.row16(j).unwrap().as_ptr(), std::ptr::null())
        } else {
            (std::ptr::null(), rgb.row(j).unwrap().as_ptr())
        };
        let (dst_y16, dst_u16, dst_v16, dst_y, dst_u, dst_v) = if image.depth > 8 {
            (
                image.row16_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                image.row16_mut(Plane::U, j).unwrap().as_mut_ptr(),
                image.row16_mut(Plane::V, j).unwrap().as_mut_ptr(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
            )
        } else {
            (
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                std::ptr::null_mut(),
                image.row_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                image.row_mut(Plane::U, j).unwrap().as_mut_ptr(),
                image.row_mut(Plane::V, j).unwrap().as_mut_ptr(),
            )
        };
        for i in 0..width {
            let yuv_pixel = yuv_pixel!(
                rgb,
                src16,
                src,
                i,
                rgb_channel_count,
                r_offset,
                g_offset,
                b_offset,
                rgb_max_channel_f,
                mode,
                range_y,
                range_uv
            );
            unsafe {
                if image.depth > 8 {
                    *dst_y16.add(i) = to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel.0);
                    *dst_u16.add(i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, yuv_pixel.1);
                    *dst_v16.add(i) = to_unorm16!(bias_uv, range_uv, yuv_max_channel, yuv_pixel.2);
                } else {
                    *dst_y.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel.0);
                    *dst_u.add(i) = to_unorm8!(bias_uv, range_uv, yuv_pixel.1);
                    *dst_v.add(i) = to_unorm8!(bias_uv, range_uv, yuv_pixel.2);
                }
            }
        }
    }
    Ok(())
}

fn rgb_to_yuv_400(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    let r_offset = rgb.format.r_offset();
    let g_offset = rgb.format.g_offset();
    let b_offset = rgb.format.b_offset();
    let rgb_channel_count = rgb.channel_count() as usize;
    let rgb_max_channel_f = rgb.max_channel_f();
    let mode = (image as &image::Image).into();
    let (bias_y, range_y) = bias_and_range_y(image);
    let yuv_max_channel = image.max_channel();
    let width = image.width as usize;

    for j in 0..image.height {
        let (src16, src) = if rgb.depth > 8 {
            (rgb.row16(j).unwrap().as_ptr(), std::ptr::null())
        } else {
            (std::ptr::null(), rgb.row(j).unwrap().as_ptr())
        };
        let (dst_y16, dst_y) = if image.depth > 8 {
            (
                image.row16_mut(Plane::Y, j).unwrap().as_mut_ptr(),
                std::ptr::null_mut(),
            )
        } else {
            (
                std::ptr::null_mut(),
                image.row_mut(Plane::Y, j).unwrap().as_mut_ptr(),
            )
        };
        for i in 0..width {
            let yuv_pixel = yuv_pixel!(
                rgb,
                src16,
                src,
                i,
                rgb_channel_count,
                r_offset,
                g_offset,
                b_offset,
                rgb_max_channel_f,
                mode,
                range_y,
                0.0
            );
            unsafe {
                if image.depth > 8 {
                    *dst_y16.add(i) = to_unorm16!(bias_y, range_y, yuv_max_channel, yuv_pixel.0);
                } else {
                    *dst_y.add(i) = to_unorm8!(bias_y, range_y, yuv_pixel.0);
                }
            }
        }
    }
    Ok(())
}

pub(crate) fn rgb_to_yuv(rgb: &rgb::Image, image: &mut image::Image) -> AvifResult<()> {
    match image.yuv_format {
        PixelFormat::Yuv420 => rgb_to_yuv_420(rgb, image),
        PixelFormat::Yuv422 => rgb_to_yuv_422(rgb, image),
        PixelFormat::Yuv444 => rgb_to_yuv_444(rgb, image),
        PixelFormat::Yuv400 => rgb_to_yuv_400(rgb, image),
        _ => Err(AvifError::NotImplemented),
    }
}

// TODO - b/410088660: this can be a macro since it's per pixel?
fn to_unorm(bias_y: f32, range_y: f32, max_channel: u16, v: f32) -> u16 {
    clamp_i32(
        (0.5 + (v * range_y + bias_y)).floor() as i32,
        0,
        max_channel as i32,
    ) as u16
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn yuv_to_rgb() {
        fn create_420(
            matrix_coefficients: MatrixCoefficients,
            y: &[&[u8]],
            u: &[&[u8]],
            v: &[&[u8]],
        ) -> image::Image {
            let mut yuv = image::Image {
                width: y[0].len() as u32,
                height: y.len() as u32,
                depth: 8,
                yuv_format: PixelFormat::Yuv420,
                matrix_coefficients,
                yuv_range: YuvRange::Limited,
                ..Default::default()
            };
            assert!(yuv.allocate_planes(Category::Color).is_ok());
            for plane in image::YUV_PLANES {
                let samples = if plane == Plane::Y {
                    &y
                } else if plane == Plane::U {
                    &u
                } else {
                    &v
                };
                assert_eq!(yuv.height(plane), samples.len());
                for y in 0..yuv.height(plane) {
                    assert_eq!(yuv.width(plane), samples[y].len());
                    for x in 0..yuv.width(plane) {
                        yuv.row_mut(plane, y as u32).unwrap()[x] = samples[y][x];
                    }
                }
            }
            yuv
        }
        fn assert_near(yuv: &image::Image, r: &[&[u8]], g: &[&[u8]], b: &[&[u8]]) {
            let mut dst = rgb::Image::create_from_yuv(yuv);
            dst.format = rgb::Format::Rgb;
            dst.chroma_upsampling = ChromaUpsampling::Bilinear;
            assert!(dst.allocate().is_ok());
            assert!(yuv_to_rgb_any(yuv, &mut dst, AlphaMultiplyMode::NoOp).is_ok());
            assert_eq!(dst.height, r.len() as u32);
            assert_eq!(dst.height, g.len() as u32);
            assert_eq!(dst.height, b.len() as u32);
            for y in 0..dst.height {
                assert_eq!(dst.width, r[y as usize].len() as u32);
                assert_eq!(dst.width, g[y as usize].len() as u32);
                assert_eq!(dst.width, b[y as usize].len() as u32);
                for x in 0..dst.width {
                    let i = (x * dst.pixel_size()) as usize;
                    let pixel = &dst.row(y).unwrap()[i..i + 3];
                    assert_eq!(pixel[0], r[y as usize][x as usize]);
                    assert_eq!(pixel[1], g[y as usize][x as usize]);
                    assert_eq!(pixel[2], b[y as usize][x as usize]);
                }
            }
        }

        // Testing identity 4:2:0 -> RGB would be simpler to check upsampling
        // but this is not allowed (not a real use case).
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/
                &[
                    &[0, 100, 200],  //
                    &[10, 110, 210], //
                    &[50, 150, 250],
                ],
                /*u=*/
                &[
                    &[0, 100], //
                    &[10, 110],
                ],
                /*v=*/
                &[
                    &[57, 57], //
                    &[57, 57],
                ],
            ),
            /*r=*/
            &[
                &[0, 0, 101], //
                &[0, 0, 113], //
                &[0, 43, 159],
            ],
            /*g=*/
            &[
                &[89, 196, 255],  //
                &[100, 207, 255], //
                &[145, 251, 255],
            ],
            /*b=*/
            &[
                &[0, 0, 107], //
                &[0, 0, 124], //
                &[0, 0, 181],
            ],
        );

        // Extreme values.
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/ &[&[0]],
                /*u=*/ &[&[0]],
                /*v=*/ &[&[0]],
            ),
            /*r=*/ &[&[0]],
            /*g=*/ &[&[136]],
            /*b=*/ &[&[0]],
        );
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/ &[&[255]],
                /*u=*/ &[&[255]],
                /*v=*/ &[&[255]],
            ),
            /*r=*/ &[&[255]],
            /*g=*/ &[&[125]],
            /*b=*/ &[&[255]],
        );

        // Top-right square "bleeds" into other samples during upsampling.
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/
                &[
                    &[0, 0, 255, 255],
                    &[0, 0, 255, 255],
                    &[0, 0, 0, 0],
                    &[0, 0, 0, 0],
                ],
                /*u=*/
                &[
                    &[0, 255], //
                    &[0, 0],
                ],
                /*v=*/
                &[
                    &[0, 255], //
                    &[0, 0],
                ],
            ),
            /*r=*/
            &[
                &[0, 0, 255, 255],
                &[0, 0, 255, 255],
                &[0, 0, 0, 0],
                &[0, 0, 0, 0],
            ],
            /*g=*/
            &[
                &[136, 59, 202, 125],
                &[136, 78, 255, 202],
                &[136, 116, 78, 59],
                &[136, 136, 136, 136],
            ],
            /*b=*/
            &[
                &[0, 0, 255, 255],
                &[0, 0, 255, 255],
                &[0, 0, 0, 0],
                &[0, 0, 0, 0],
            ],
        );

        // Middle square does not "bleed" into other samples during upsampling.
        assert_near(
            &create_420(
                MatrixCoefficients::Bt601,
                /*y=*/
                &[
                    &[0, 0, 0, 0],
                    &[0, 255, 255, 0],
                    &[0, 255, 255, 0],
                    &[0, 0, 0, 0],
                ],
                /*u=*/
                &[
                    &[0, 0], //
                    &[0, 0],
                ],
                /*v=*/
                &[
                    &[0, 0], //
                    &[0, 0],
                ],
            ),
            /*r=*/
            &[
                &[0, 0, 0, 0],
                &[0, 74, 74, 0],
                &[0, 74, 74, 0],
                &[0, 0, 0, 0],
            ],
            /*g=*/
            &[
                &[136, 136, 136, 136],
                &[136, 255, 255, 136],
                &[136, 255, 255, 136],
                &[136, 136, 136, 136],
            ],
            /*b=*/
            &[
                &[0, 0, 0, 0],
                &[0, 20, 20, 0],
                &[0, 20, 20, 0],
                &[0, 0, 0, 0],
            ],
        );
    }
}
