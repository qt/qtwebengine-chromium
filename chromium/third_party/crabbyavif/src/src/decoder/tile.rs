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

use crate::decoder::*;
use crate::internal_utils::sampletransform::*;
use crate::*;

use std::num::NonZero;

#[derive(Debug, Default)]
pub struct DecodeSample {
    pub item_id: u32, // 1-based. 0 if it comes from a track.
    pub offset: u64,
    pub size: usize,
    pub spatial_id: u8,
    pub sync: bool,
}

impl DecodeSample {
    pub(crate) fn partial_data<'a>(
        &'a self,
        io: &'a mut Box<impl decoder::IO + ?Sized>,
        buffer: &'a Option<Vec<u8>>,
        size: usize,
    ) -> AvifResult<&'a [u8]> {
        match buffer {
            Some(x) => {
                let start_offset = usize_from_u64(self.offset)?;
                let end_offset = checked_add!(start_offset, size)?;
                let range = start_offset..end_offset;
                check_slice_range(x.len(), &range)?;
                Ok(&x[range])
            }
            None => {
                let data = io.read(self.offset, size)?;
                if data.len() != size {
                    AvifError::truncated_data()
                } else {
                    Ok(data)
                }
            }
        }
    }

    pub(crate) fn data<'a>(
        &'a self,
        io: &'a mut Box<impl decoder::IO + ?Sized>,
        buffer: &'a Option<Vec<u8>>,
    ) -> AvifResult<&'a [u8]> {
        self.partial_data(io, buffer, self.size)
    }
}

#[derive(Debug, Default)]
pub struct DecodeInput {
    pub samples: Vec<DecodeSample>,
    pub all_layers: bool,
    pub decoding_item: DecodingItem,
}

#[derive(Debug, Default)]
pub struct Overlay {
    pub canvas_fill_value: [u16; 4],
    pub width: u32,
    pub height: u32,
    pub horizontal_offsets: Vec<i32>,
    pub vertical_offsets: Vec<i32>,
}

#[derive(Debug, Default)]
pub(crate) struct TileInfo {
    pub tile_count: u32,
    pub decoded_tile_count: u32,
    pub grid: Grid,
    pub overlay: Overlay,
    pub gainmap_metadata: GainMapMetadata,
    pub sample_transform: SampleTransform,
}

impl TileInfo {
    pub(crate) fn is_grid(&self) -> bool {
        self.grid.rows > 0 && self.grid.columns > 0
    }

    pub(crate) fn is_overlay(&self) -> bool {
        !self.overlay.horizontal_offsets.is_empty() && !self.overlay.vertical_offsets.is_empty()
    }

    pub(crate) fn is_sample_transform(&self) -> bool {
        !self.sample_transform.tokens.is_empty()
    }

    pub(crate) fn is_derived_image(&self) -> bool {
        self.is_grid() || self.is_overlay() || self.is_sample_transform()
    }

    pub(crate) fn grid_tile_count(&self) -> AvifResult<u32> {
        if self.is_grid() {
            checked_mul!(self.grid.rows, self.grid.columns)
        } else {
            Ok(1)
        }
    }

    pub(crate) fn decoded_row_count(&self, image_height: u32, tile_height: u32) -> u32 {
        if self.decoded_tile_count == 0 {
            return 0;
        }
        if self.decoded_tile_count == self.tile_count || !self.is_grid() {
            return image_height;
        }
        std::cmp::min(
            (self.decoded_tile_count / self.grid.columns) * tile_height,
            image_height,
        )
    }

    pub(crate) fn is_fully_decoded(&self) -> bool {
        self.tile_count == self.decoded_tile_count
    }
}

pub struct Tile {
    pub width: u32,
    pub height: u32,
    pub operating_point: u8,
    pub image: Image,
    pub input: DecodeInput,
    pub codec_index: usize,
    pub codec_config: CodecConfiguration,
}

impl Tile {
    pub(crate) fn create_from_item(
        item: &mut Item,
        allow_progressive: bool,
        image_count_limit: Option<NonZero<u32>>,
        size_hint: u64,
    ) -> AvifResult<Tile> {
        if size_hint != 0 && item.size as u64 > size_hint {
            return AvifError::bmff_parse_failed("exceeded size_hint");
        }
        let mut tile = Tile {
            width: item.width,
            height: item.height,
            operating_point: item.operating_point(),
            image: Image::default(),
            input: DecodeInput::default(),
            codec_index: 0,
            codec_config: item
                .codec_config()
                .ok_or(AvifError::BmffParseFailed(
                    "missing codec config property".into(),
                ))?
                .clone(),
        };
        let mut layer_sizes: [usize; MAX_AV1_LAYER_COUNT] = [0; MAX_AV1_LAYER_COUNT];
        let mut layer_count: usize = 0;
        let a1lx = item.a1lx();
        let has_a1lx = a1lx.is_some();
        if let Some(a1lx) = a1lx {
            let mut remaining_size: usize = item.size;
            for i in 0usize..3 {
                layer_count += 1;
                if a1lx[i] > 0 {
                    // >= instead of > because there must be room for the last layer
                    if a1lx[i] >= remaining_size {
                        return AvifError::bmff_parse_failed(format!(
                            "a1lx layer index [{i}] does not fit in item size"
                        ));
                    }
                    layer_sizes[i] = a1lx[i];
                    remaining_size -= a1lx[i];
                } else {
                    layer_sizes[i] = remaining_size;
                    remaining_size = 0;
                    break;
                }
            }
            if remaining_size > 0 {
                assert!(layer_count == 3);
                layer_count += 1;
                layer_sizes[3] = remaining_size;
            }
        }
        let lsel;
        let has_lsel;
        match item.lsel() {
            Some(x) => {
                lsel = *x;
                has_lsel = true;
            }
            None => {
                lsel = 0;
                has_lsel = false;
            }
        }
        // Progressive images offer layers via the a1lxProp, but don't specify a layer selection with
        // lsel.
        item.progressive = has_a1lx && (!has_lsel || lsel == 0xFFFF);
        let base_item_offset = if item.extents.len() == 1 { item.extents[0].offset } else { 0 };
        if has_lsel && lsel != 0xFFFF {
            // Layer selection. This requires that the underlying AV1 codec decodes all layers, and
            // then only returns the requested layer as a single frame. To the user of libavif,
            // this appears to be a single frame.
            tile.input.all_layers = true;
            let mut sample_size: usize = 0;
            let layer_id = usize_from_u16(lsel)?;
            if layer_count > 0 {
                // Optimization: If we're selecting a layer that doesn't require the entire image's
                // payload (hinted via the a1lx box).
                if layer_id >= layer_count {
                    return AvifError::invalid_image_grid("lsel layer index not found in a1lx.");
                }
                let layer_id_plus_1 = layer_id + 1;
                for layer_size in layer_sizes.iter().take(layer_id_plus_1) {
                    checked_incr!(sample_size, *layer_size);
                }
            } else {
                // This layer payload subsection is not known. Use the whole payload.
                sample_size = item.size;
            }
            let sample = DecodeSample {
                item_id: item.id,
                offset: base_item_offset,
                size: sample_size,
                spatial_id: lsel as u8,
                sync: true,
            };
            tile.input.samples.push(sample);
        } else if item.progressive && allow_progressive {
            // Progressive image. Decode all layers and expose them all to the
            // user.
            if let Some(limit) = image_count_limit {
                if layer_count as u32 > limit.get() {
                    return AvifError::bmff_parse_failed(
                        "exceeded image_count_limit (progressive)",
                    );
                }
            }
            tile.input.all_layers = true;
            let mut offset = 0;
            for (i, layer_size) in layer_sizes.iter().take(layer_count).enumerate() {
                let sample = DecodeSample {
                    item_id: item.id,
                    offset: checked_add!(base_item_offset, offset)?,
                    size: *layer_size,
                    spatial_id: 0xff,
                    sync: i == 0, // Assume all layers depend on the first layer.
                };
                tile.input.samples.push(sample);
                offset = checked_add!(offset, *layer_size as u64)?;
            }
        } else {
            // Typical case: Use the entire item's payload for a single frame output
            let sample = DecodeSample {
                item_id: item.id,
                offset: base_item_offset,
                size: item.size,
                // Legal spatial_id values are [0,1,2,3], so this serves as a sentinel value for
                // "do not filter by spatial_id"
                spatial_id: 0xff,
                sync: true,
            };
            tile.input.samples.push(sample);
        }
        Ok(tile)
    }

    pub(crate) fn create_from_track(
        track: &Track,
        image_count_limit: Option<NonZero<u32>>,
        size_hint: u64,
        decoding_item: DecodingItem,
    ) -> AvifResult<Tile> {
        let properties = track
            .get_properties()
            .ok_or(AvifError::BmffParseFailed("".into()))?;
        let codec_config = find_property!(properties, CodecConfiguration)
            .ok_or(AvifError::BmffParseFailed("".into()))?
            .clone();
        let mut tile = Tile {
            width: track.width,
            height: track.height,
            operating_point: 0, // No way to set operating point via tracks
            image: Image::default(),
            input: DecodeInput {
                decoding_item,
                ..DecodeInput::default()
            },
            codec_index: 0,
            codec_config,
        };
        let sample_table = &track.sample_table.unwrap_ref();

        if let Some(limit) = image_count_limit {
            let mut limit = limit.get();
            for (chunk_index, _chunk_offset) in sample_table.chunk_offsets.iter().enumerate() {
                // Figure out how many samples are in this chunk.
                let sample_count = sample_table.get_sample_count_of_chunk(chunk_index as u32);
                if sample_count == 0 {
                    return AvifError::bmff_parse_failed("chunk with 0 samples found");
                }
                if sample_count > limit {
                    return AvifError::bmff_parse_failed("exceeded image_count_limit");
                }
                limit -= sample_count;
            }
        }

        let mut sample_size_index: usize = 0;
        for (chunk_index, chunk_offset) in sample_table.chunk_offsets.iter().enumerate() {
            // Figure out how many samples are in this chunk.
            let sample_count = sample_table.get_sample_count_of_chunk(chunk_index as u32);
            if sample_count == 0 {
                return AvifError::bmff_parse_failed("chunk with 0 samples found");
            }

            let mut sample_offset = *chunk_offset;
            for _ in 0..sample_count {
                let sample_size = sample_table.sample_size(sample_size_index)?;
                let sample_size_hint = checked_add!(sample_offset, sample_size as u64)?;
                if size_hint != 0 && sample_size_hint > size_hint {
                    return AvifError::bmff_parse_failed("exceeded size_hint");
                }
                let sample = DecodeSample {
                    item_id: 0,
                    offset: sample_offset,
                    size: sample_size,
                    // Legal spatial_id values are [0,1,2,3], so this serves as a sentinel value for "do
                    // not filter by spatial_id"
                    spatial_id: 0xff,
                    // Assume first sample is always sync (in case stss box was missing).
                    sync: tile.input.samples.is_empty(),
                };
                tile.input.samples.push(sample);
                checked_incr!(sample_offset, sample_size as u64);
                checked_incr!(sample_size_index, 1);
            }
        }
        for sync_sample_number in &sample_table.sync_samples {
            let index = usize_from_u32(*sync_sample_number)?;
            // sample_table.sync_samples is 1-based.
            if index == 0 || index > tile.input.samples.len() {
                return AvifError::bmff_parse_failed(format!("invalid sync sample number {index}"));
            }
            tile.input.samples[index - 1].sync = true;
        }
        Ok(tile)
    }

    pub(crate) fn max_sample_size(&self) -> usize {
        match self.input.samples.iter().max_by_key(|sample| sample.size) {
            Some(sample) => sample.size,
            None => 0,
        }
    }
}

impl Image {
    pub(crate) fn copy_from_tile(
        &mut self,
        tile: &Image,
        grid: &Grid,
        tile_index: u32,
        category: Category,
    ) -> AvifResult<()> {
        // When copying non-alpha planes, the source and destination depth and yuv_format has
        // to be the same. This restriction does not apply to alpha planes since only Y plane is
        // copied.
        if category != Category::Alpha && self.yuv_format != tile.yuv_format {
            return AvifError::invalid_image_grid("mismatch in grid tile yuv formats.");
        }
        let row_index = tile_index / grid.columns;
        let column_index = tile_index % grid.columns;
        for plane in category.planes() {
            let plane = *plane;
            let src_plane = tile.plane_data(plane);
            if src_plane.is_none() {
                continue;
            }
            let src_plane = src_plane.unwrap();
            // If this is the last tile column, clamp to left over width.
            let src_width_to_copy = if column_index == grid.columns - 1 {
                let width_so_far = checked_mul!(src_plane.width, column_index)?;
                checked_sub!(self.width(plane), usize_from_u32(width_so_far)?)?
            } else {
                usize_from_u32(src_plane.width)?
            };

            // If this is the last tile row, clamp to left over height.
            let src_height_to_copy = if row_index == grid.rows - 1 {
                let height_so_far = checked_mul!(src_plane.height, row_index)?;
                checked_sub!(u32_from_usize(self.height(plane))?, height_so_far)?
            } else {
                src_plane.height
            };

            let dst_y_start = checked_mul!(row_index, src_plane.height)?;
            let dst_x_offset = usize_from_u32(checked_mul!(column_index, src_plane.width)?)?;
            let dst_x_offset_end = checked_add!(dst_x_offset, src_width_to_copy)?;
            if self.depth == 8 {
                for y in 0..src_height_to_copy {
                    let src_row = tile.row(plane, y)?;
                    let src_slice = &src_row[0..src_width_to_copy];
                    let dst_row = self.row_mut(plane, checked_add!(dst_y_start, y)?)?;
                    let dst_slice = &mut dst_row[dst_x_offset..dst_x_offset_end];
                    dst_slice.copy_from_slice(src_slice);
                }
            } else {
                for y in 0..src_height_to_copy {
                    let src_row = tile.row16(plane, y)?;
                    let src_slice = &src_row[0..src_width_to_copy];
                    let dst_row = self.row16_mut(plane, checked_add!(dst_y_start, y)?)?;
                    let dst_slice = &mut dst_row[dst_x_offset..dst_x_offset_end];
                    dst_slice.copy_from_slice(src_slice);
                }
            }
        }
        Ok(())
    }

    pub(crate) fn copy_and_overlay_from_tile(
        &mut self,
        tile: &Image,
        tile_info: &TileInfo,
        tile_index: u32,
        category: Category,
    ) -> AvifResult<()> {
        // When copying non-alpha planes, the source and destination depth and yuv_format has
        // to be the same. This restriction does not apply to alpha planes since only Y plane is
        // copied.
        if category != Category::Alpha && self.yuv_format != tile.yuv_format {
            return AvifError::invalid_image_grid("mismatch in overlay tile yuv formats.");
        }
        // This function is used only when |tile| contains pointers and self contains buffers.
        for plane in category.planes() {
            let plane = *plane;
            let src_plane = tile.plane_data(plane);
            let dst_plane = self.plane_data(plane);
            if src_plane.is_none() || dst_plane.is_none() {
                continue;
            }
            let dst_plane = dst_plane.unwrap();
            let tile_index = usize_from_u32(tile_index)?;

            let vertical_offset = tile_info.overlay.vertical_offsets[tile_index] as i128;
            let horizontal_offset = tile_info.overlay.horizontal_offsets[tile_index] as i128;
            let src_height = tile.height as i128;
            let src_width = tile.width as i128;
            let dst_height = dst_plane.height as i128;
            let dst_width = dst_plane.width as i128;

            if matches!(plane, Plane::Y | Plane::A)
                && (vertical_offset + src_height < 0
                    || horizontal_offset + src_width < 0
                    || vertical_offset >= dst_height
                    || horizontal_offset >= dst_width)
            {
                // Entire tile outside of the canvas. It is sufficient to perform this check only
                // for Y and A plane since they are never sub-sampled.
                return Ok(());
            }

            let mut src_y_start: u32;
            let mut src_height_to_copy: u32;
            let mut dst_y_start: u32;
            if vertical_offset >= 0 {
                src_y_start = 0;
                src_height_to_copy = src_height as u32;
                dst_y_start = vertical_offset as u32;
            } else {
                src_y_start = vertical_offset.unsigned_abs() as u32;
                src_height_to_copy = (src_height - vertical_offset.abs()) as u32;
                dst_y_start = 0;
            }

            let mut src_x_start: u32;
            let mut src_width_to_copy: u32;
            let mut dst_x_start: u32;
            if horizontal_offset >= 0 {
                src_x_start = 0;
                src_width_to_copy = src_width as u32;
                dst_x_start = horizontal_offset as u32;
            } else {
                src_x_start = horizontal_offset.unsigned_abs() as u32;
                src_width_to_copy = (src_width - horizontal_offset.abs()) as u32;
                dst_x_start = 0;
            }

            // Clamp width to the canvas width.
            if self.width - dst_x_start < src_width_to_copy {
                src_width_to_copy = self.width - dst_x_start;
            }

            // Clamp height to the canvas height.
            if self.height - dst_y_start < src_height_to_copy {
                src_height_to_copy = self.height - dst_y_start;
            }

            // Apply chroma subsampling to the offsets.
            if plane == Plane::U || plane == Plane::V {
                src_y_start = tile.yuv_format.apply_chroma_shift_y(src_y_start);
                src_height_to_copy = tile.yuv_format.apply_chroma_shift_y(src_height_to_copy);
                dst_y_start = tile.yuv_format.apply_chroma_shift_y(dst_y_start);
                src_x_start = tile.yuv_format.apply_chroma_shift_x(src_x_start);
                src_width_to_copy = tile.yuv_format.apply_chroma_shift_x(src_width_to_copy);
                dst_x_start = tile.yuv_format.apply_chroma_shift_x(dst_x_start);
            }

            let src_y_range = src_y_start..checked_add!(src_y_start, src_height_to_copy)?;
            let dst_x_range = usize_from_u32(dst_x_start)?
                ..usize_from_u32(checked_add!(dst_x_start, src_width_to_copy)?)?;
            let src_x_range = usize_from_u32(src_x_start)?
                ..checked_add!(usize_from_u32(src_x_start)?, dst_x_range.len())?;
            let mut dst_y = dst_y_start;
            if self.depth == 8 {
                for src_y in src_y_range {
                    let src_row = tile.row(plane, src_y)?;
                    let src_slice = &src_row[src_x_range.clone()];
                    let dst_row = self.row_mut(plane, dst_y)?;
                    let dst_slice = &mut dst_row[dst_x_range.clone()];
                    dst_slice.copy_from_slice(src_slice);
                    checked_incr!(dst_y, 1);
                }
            } else {
                for src_y in src_y_range {
                    let src_row = tile.row16(plane, src_y)?;
                    let src_slice = &src_row[src_x_range.clone()];
                    let dst_row = self.row16_mut(plane, dst_y)?;
                    let dst_slice = &mut dst_row[dst_x_range.clone()];
                    dst_slice.copy_from_slice(src_slice);
                    checked_incr!(dst_y, 1);
                }
            }
        }
        Ok(())
    }
}
