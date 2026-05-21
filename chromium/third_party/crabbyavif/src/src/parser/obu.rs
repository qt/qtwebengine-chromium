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

use crate::image::YuvRange;
use crate::internal_utils::stream::*;
use crate::internal_utils::*;
use crate::parser::mp4box::*;
use crate::*;

#[derive(Debug)]
struct ObuHeader {
    obu_type: u8,
    size: u32,
}

#[derive(Debug, Default)]
pub struct Av1SequenceHeader {
    reduced_still_picture_header: bool,
    max_width: u32,
    max_height: u32,
    bit_depth: u8,
    yuv_format: PixelFormat,
    pub color_primaries: ColorPrimaries,
    pub transfer_characteristics: TransferCharacteristics,
    pub matrix_coefficients: MatrixCoefficients,
    pub yuv_range: YuvRange,
    pub config: Av1CodecConfiguration,
}

impl Av1SequenceHeader {
    fn parse_profile(&mut self, stream: &mut IStream) -> AvifResult<()> {
        self.config.seq_profile = stream.read_bits(3)? as u8;
        if self.config.seq_profile > 2 {
            return AvifError::bmff_parse_failed("invalid seq_profile");
        }
        let still_picture = stream.read_bool()?;
        self.reduced_still_picture_header = stream.read_bool()?;
        if self.reduced_still_picture_header && !still_picture {
            return AvifError::bmff_parse_failed("invalid reduced_still_picture_header");
        }
        if self.reduced_still_picture_header {
            self.config.seq_level_idx0 = stream.read_bits(5)? as u8;
        } else {
            let mut buffer_delay_length = 0;
            let mut decoder_model_info_present_flag = false;
            let timing_info_present_flag = stream.read_bool()?;
            if timing_info_present_flag {
                // num_units_in_display_tick
                stream.skip_bits(32)?;
                // time_scale
                stream.skip_bits(32)?;
                let equal_picture_interval = stream.read_bool()?;
                if equal_picture_interval {
                    // num_ticks_per_picture_minus_1
                    stream.skip_uvlc()?;
                }
                decoder_model_info_present_flag = stream.read_bool()?;
                if decoder_model_info_present_flag {
                    let buffer_delay_length_minus_1 = stream.read_bits(5)?;
                    buffer_delay_length = buffer_delay_length_minus_1 + 1;
                    // num_units_in_decoding_tick
                    stream.skip_bits(32)?;
                    // buffer_removal_time_length_minus_1
                    stream.skip_bits(5)?;
                    // frame_presentation_time_length_minus_1
                    stream.skip_bits(5)?;
                }
            }
            let initial_display_delay_present_flag = stream.read_bool()?;
            let operating_points_cnt_minus_1 = stream.read_bits(5)?;
            let operating_points_cnt = operating_points_cnt_minus_1 + 1;
            for i in 0..operating_points_cnt {
                // operating_point_idc
                stream.skip_bits(12)?;
                let seq_level_idx = stream.read_bits(5)?;
                if i == 0 {
                    self.config.seq_level_idx0 = seq_level_idx as u8;
                }
                if seq_level_idx > 7 {
                    let seq_tier = stream.read_bits(1)?;
                    if i == 0 {
                        self.config.seq_tier0 = seq_tier as u8;
                    }
                }
                if decoder_model_info_present_flag {
                    let decoder_model_present_for_this_op = stream.read_bool()?;
                    if decoder_model_present_for_this_op {
                        // decoder_buffer_delay
                        stream.skip_bits(buffer_delay_length as usize)?;
                        // encoder_buffer_delay
                        stream.skip_bits(buffer_delay_length as usize)?;
                        // low_delay_mode_flag
                        stream.skip_bits(1)?;
                    }
                }
                if initial_display_delay_present_flag {
                    let initial_display_delay_present_for_this_op = stream.read_bool()?;
                    if initial_display_delay_present_for_this_op {
                        // initial_display_delay_minus_1
                        stream.skip_bits(4)?;
                    }
                }
            }
        }
        Ok(())
    }

    fn parse_frame_max_dimensions(&mut self, stream: &mut IStream) -> AvifResult<()> {
        let frame_width_bits_minus_1 = stream.read_bits(4)?;
        let frame_height_bits_minus_1 = stream.read_bits(4)?;
        let max_frame_width_minus_1 = stream.read_bits(frame_width_bits_minus_1 as usize + 1)?;
        let max_frame_height_minus_1 = stream.read_bits(frame_height_bits_minus_1 as usize + 1)?;
        self.max_width = checked_add!(max_frame_width_minus_1, 1)?;
        self.max_height = checked_add!(max_frame_height_minus_1, 1)?;
        let frame_id_numbers_present_flag =
            if self.reduced_still_picture_header { false } else { stream.read_bool()? };
        if frame_id_numbers_present_flag {
            // delta_frame_id_length_minus_2
            stream.skip_bits(4)?;
            // additional_frame_id_length_minus_1
            stream.skip_bits(3)?;
        }
        Ok(())
    }

    fn parse_enabled_features(&mut self, stream: &mut IStream) -> AvifResult<()> {
        // use_128x128_superblock
        stream.skip_bits(1)?;
        // enable_filter_intra
        stream.skip_bits(1)?;
        // enable_intra_edge_filter
        stream.skip_bits(1)?;
        if self.reduced_still_picture_header {
            return Ok(());
        }
        // enable_interintra_compound
        stream.skip_bits(1)?;
        // enable_masked_compound
        stream.skip_bits(1)?;
        // enable_warped_motion
        stream.skip_bits(1)?;
        // enable_dual_filter
        stream.skip_bits(1)?;
        let enable_order_hint = stream.read_bool()?;
        if enable_order_hint {
            // enable_jnt_comp
            stream.skip_bits(1)?;
            // enable_ref_frame_mvs
            stream.skip_bits(1)?;
        }
        let seq_choose_screen_content_tools = stream.read_bool()?;
        let seq_force_screen_content_tools = if seq_choose_screen_content_tools {
            2 // SELECT_SCREEN_CONTENT_TOOLS
        } else {
            stream.read_bits(1)?
        };
        if seq_force_screen_content_tools > 0 {
            let seq_choose_integer_mv = stream.read_bool()?;
            if !seq_choose_integer_mv {
                // seq_force_integer_mv
                stream.skip_bits(1)?;
            }
        }
        if enable_order_hint {
            // order_hint_bits_minus_1
            stream.skip_bits(3)?;
        }
        Ok(())
    }

    fn parse_color_config(&mut self, stream: &mut IStream) -> AvifResult<()> {
        self.config.high_bitdepth = stream.read_bool()?;
        if self.config.seq_profile == 2 && self.config.high_bitdepth {
            self.config.twelve_bit = stream.read_bool()?;
            self.bit_depth = if self.config.twelve_bit { 12 } else { 10 };
        } else {
            self.bit_depth = if self.config.high_bitdepth { 10 } else { 8 };
        }
        if self.config.seq_profile != 1 {
            self.config.monochrome = stream.read_bool()?;
        }
        let color_description_present_flag = stream.read_bool()?;
        if color_description_present_flag {
            self.color_primaries = (stream.read_bits(8)? as u16).into();
            self.transfer_characteristics = (stream.read_bits(8)? as u16).into();
            self.matrix_coefficients = (stream.read_bits(8)? as u16).into();
        } else {
            self.color_primaries = ColorPrimaries::Unspecified;
            self.transfer_characteristics = TransferCharacteristics::Unspecified;
            self.matrix_coefficients = MatrixCoefficients::Unspecified;
        }
        if self.config.monochrome {
            let color_range = stream.read_bool()?;
            self.yuv_range = if color_range { YuvRange::Full } else { YuvRange::Limited };
            self.config.chroma_subsampling_x = 1;
            self.config.chroma_subsampling_y = 1;
            self.yuv_format = PixelFormat::Yuv400;
            return Ok(());
        } else if self.color_primaries == ColorPrimaries::Bt709
            && self.transfer_characteristics == TransferCharacteristics::Srgb
            && self.matrix_coefficients == MatrixCoefficients::Identity
        {
            self.yuv_range = YuvRange::Full;
            self.yuv_format = PixelFormat::Yuv444;
        } else {
            let color_range = stream.read_bool()?;
            self.yuv_range = if color_range { YuvRange::Full } else { YuvRange::Limited };
            match self.config.seq_profile {
                0 => {
                    self.config.chroma_subsampling_x = 1;
                    self.config.chroma_subsampling_y = 1;
                    self.yuv_format = PixelFormat::Yuv420;
                }
                1 => {
                    self.yuv_format = PixelFormat::Yuv444;
                }
                2 => {
                    if self.bit_depth == 12 {
                        self.config.chroma_subsampling_x = stream.read_bits(1)? as u8;
                        if self.config.chroma_subsampling_x == 1 {
                            self.config.chroma_subsampling_y = stream.read_bits(1)? as u8;
                        }
                    } else {
                        self.config.chroma_subsampling_x = 1;
                    }
                    self.yuv_format = if self.config.chroma_subsampling_x == 1 {
                        if self.config.chroma_subsampling_y == 1 {
                            PixelFormat::Yuv420
                        } else {
                            PixelFormat::Yuv422
                        }
                    } else {
                        PixelFormat::Yuv444
                    };
                }
                _ => {} // Not reached.
            }
            if self.config.chroma_subsampling_x == 1 && self.config.chroma_subsampling_y == 1 {
                // chroma_sample_position.
                stream.skip_bits(2)?;
            }
        }
        // separate_uv_delta_q
        stream.skip_bits(1)?;
        Ok(())
    }

    fn parse_obu_header(stream: &mut IStream) -> AvifResult<ObuHeader> {
        // Section 5.3.2 of AV1 specification.
        // https://aomediacodec.github.io/av1-spec/#obu-header-syntax
        let obu_forbidden_bit = stream.read_bits(1)?;
        if obu_forbidden_bit != 0 {
            return AvifError::bmff_parse_failed("invalid obu_forbidden_bit");
        }
        let obu_type = stream.read_bits(4)? as u8;
        let obu_extension_flag = stream.read_bool()?;
        let obu_has_size_field = stream.read_bool()?;
        // obu_reserved_1bit
        stream.skip_bits(1)?; // "The value is ignored by a decoder."

        if obu_extension_flag {
            // temporal_id
            stream.skip_bits(3)?;
            // spatial_id
            stream.skip_bits(2)?;
            // extension_header_reserved_3bits
            stream.skip_bits(3)?;
        }

        let size = if obu_has_size_field {
            stream.read_uleb128()?
        } else {
            u32_from_usize(stream.bytes_left()?)? // sz - 1 - obu_extension_flag
        };

        Ok(ObuHeader { obu_type, size })
    }

    pub(crate) fn parse_from_obus(data: &[u8]) -> AvifResult<Self> {
        let mut stream = IStream::create(data);

        while stream.has_bytes_left()? {
            let obu = Self::parse_obu_header(&mut stream)?;
            if obu.obu_type != /*OBU_SEQUENCE_HEADER=*/1 {
                // Not a sequence header. Skip this obu.
                stream.skip(usize_from_u32(obu.size)?)?;
                continue;
            }
            let mut stream = stream.sub_stream(&BoxSize::FixedSize(usize_from_u32(obu.size)?))?;
            let mut sequence_header = Av1SequenceHeader::default();
            sequence_header.parse_profile(&mut stream)?;
            sequence_header.parse_frame_max_dimensions(&mut stream)?;
            sequence_header.parse_enabled_features(&mut stream)?;
            // enable_superres
            stream.skip_bits(1)?;
            // enable_cdef
            stream.skip_bits(1)?;
            // enable_restoration
            stream.skip_bits(1)?;
            sequence_header.parse_color_config(&mut stream)?;
            // film_grain_params_present
            stream.skip_bits(1)?;
            return Ok(sequence_header);
        }
        AvifError::bmff_parse_failed("could not parse sequence header")
    }
}

#[cfg(feature = "avm")]
#[derive(Debug, Default)]
pub struct Av2SequenceHeader {
    single_picture_header_flag: bool,
    max_width: u32,
    max_height: u32,
    pub color_primaries: ColorPrimaries,
    pub transfer_characteristics: TransferCharacteristics,
    pub matrix_coefficients: MatrixCoefficients,
    pub yuv_range: YuvRange,
    pub config: Av2CodecConfiguration,
}

#[cfg(feature = "avm")]
impl Av2SequenceHeader {
    fn parse_profile(&mut self, stream: &mut IStream) -> AvifResult<()> {
        self.config.seq_profile = stream.read_bits(3)? as u8;
        if self.config.seq_profile > 2 {
            return AvifError::bmff_parse_failed(format!(
                "invalid seq_profile {}",
                self.config.seq_profile
            ));
        }
        self.single_picture_header_flag = stream.read_bool()?;
        if !self.single_picture_header_flag {
            stream.skip_bits(3)?; // seq_lcr_id
            stream.skip_bits(1)?; // still_picture
            return AvifError::not_implemented();
        }
        self.config.seq_level_idx0 = stream.read_bits(5)? as u8;
        self.config.seq_tier_0 =
            if self.config.seq_level_idx0 > 7 && !self.single_picture_header_flag {
                // TODO: b/437292541 - Check if single_tier_0 is seq_tier_0.
                stream.read_bits(1)? // single_tier_0
            } else {
                0
            } as u8;
        Ok(())
    }

    fn parse_frame_max_dimensions(&mut self, stream: &mut IStream) -> AvifResult<()> {
        let frame_width_bits_minus_1 = stream.read_bits(4)?;
        let frame_height_bits_minus_1 = stream.read_bits(4)?;
        let max_frame_width_minus_1 = stream.read_bits(frame_width_bits_minus_1 as usize + 1)?;
        let max_frame_height_minus_1 = stream.read_bits(frame_height_bits_minus_1 as usize + 1)?;
        self.max_width = checked_add!(max_frame_width_minus_1, 1)?;
        self.max_height = checked_add!(max_frame_height_minus_1, 1)?;
        if stream.read_bool()? {
            // conf_window_flag
            stream.skip_uvlc()?; // conf_win_left_offset
            stream.skip_uvlc()?; // conf_win_right_offset
            stream.skip_uvlc()?; // conf_win_top_offset
            stream.skip_uvlc()?; // conf_win_bottom_offset
        }

        Ok(())
    }

    fn parse_chroma_format_bitdepth(&mut self, stream: &mut IStream) -> AvifResult<()> {
        let chroma_format_idc = stream.read_uvlc()?;
        (
            self.config.monochrome,
            self.config.chroma_subsampling_x,
            self.config.chroma_subsampling_y,
        ) = match chroma_format_idc {
            0 => (false, 1, 1), // 4:2:0
            1 => (true, 1, 1),  // 4:0:0
            2 => (false, 0, 0), // 4:4:4
            3 => (false, 1, 0), // 4:2:2
            _ => {
                return AvifError::bmff_parse_failed(format!(
                    "invalid chroma_format_idc {chroma_format_idc}",
                ))
            }
        };
        self.config.bitdepth_idx = stream
            .read_uvlc()?
            .try_into()
            .map_err(AvifError::map_unknown_error)?;
        if self.config.bitdepth_idx > 2 {
            return AvifError::bmff_parse_failed(format!(
                "invalid bitdepth_idx {}",
                self.config.bitdepth_idx
            ));
        }
        Ok(())
    }

    fn parse_content_interpretation(&mut self, stream: &mut IStream) -> AvifResult<()> {
        stream.read_bits(2)?; // ci_scan_type_idc
        let color_description_present = stream.read_bool()?; // ci_color_description_present_flag
        let chroma_sample_position_present = stream.read_bool()?; // ci_chroma_sample_position_present_flag
        stream.read_bool()?; // ci_aspect_ratio_info_present_flag
        stream.read_bool()?; // ci_timing_info_present_flag
        stream.read_bool()?; // ci_extension_present_flag
        stream.read_bool()?; // reserved_bit

        if color_description_present {
            // Override the default CICP values.

            use crate::codecs::avm::*;
            let color_description_idc = stream.read_rg(2)?; // color_description_idc
            if color_description_idc == 0xFFFFFFFF {
                return AvifError::bmff_parse_failed(format!(
                    "invalid color_description_idc {color_description_idc}",
                ));
            }
            (
                self.color_primaries,
                self.transfer_characteristics,
                self.matrix_coefficients,
            ) = match color_description_idc {
                AV2_IDC_EXPLICIT => (
                    // Explicitly signaled
                    ColorPrimaries::from(stream.read_bits(8)? as u16),
                    TransferCharacteristics::from(stream.read_bits(8)? as u16),
                    MatrixCoefficients::from(stream.read_bits(8)? as u16),
                ),
                AV2_BT709SDR => (
                    // BT.709 SDR
                    ColorPrimaries::Bt709,          // 1
                    TransferCharacteristics::Bt709, // 1
                    MatrixCoefficients::Bt470bg,    // 5
                ),
                AV2_BT2100PQ => (
                    // BT.2100 PQ
                    ColorPrimaries::Bt2100,        // 9
                    TransferCharacteristics::Pq,   // 16
                    MatrixCoefficients::Bt2020Ncl, // 9
                ),
                AV2_BT2100HLG => (
                    // BT.2100 HLG
                    ColorPrimaries::Bt2100,                // 9
                    TransferCharacteristics::Bt2020_10bit, // 14
                    MatrixCoefficients::Bt2020Ncl,         // 9
                ),
                AV2_SRGB => (
                    // sRGB
                    ColorPrimaries::Bt709,         // 1
                    TransferCharacteristics::Srgb, // 13
                    MatrixCoefficients::Identity,  // 0
                ),
                AV2_SRGBSYCC => (
                    // sYCC
                    ColorPrimaries::Bt709,         // 1
                    TransferCharacteristics::Srgb, // 13
                    MatrixCoefficients::Bt470bg,   // 5
                ),
                _ => (
                    // Reserved
                    ColorPrimaries::Unspecified,          // 2
                    TransferCharacteristics::Unspecified, // 2
                    MatrixCoefficients::Unspecified,      // 2
                ),
            };
            self.yuv_range = if stream.read_bool()? { YuvRange::Full } else { YuvRange::Limited };
        // color_range
        } else {
            // Keep the default CICP values.
        }

        if chroma_sample_position_present {
            use crate::codecs::avm::AV2_CSP_CENTER;
            use crate::codecs::avm::AV2_CSP_LEFT;
            use crate::codecs::avm::AV2_CSP_TOPLEFT;

            let chroma_sample_position = stream.read_uvlc()?; // ci_chroma_sample_position_0
            self.config.chroma_sample_position = match chroma_sample_position {
                // Horizontal offset 0, vertical offset 0.5
                AV2_CSP_LEFT => ChromaSamplePosition::Vertical,
                // Horizontal offset 0.5, vertical offset 0.5
                AV2_CSP_CENTER => ChromaSamplePosition::Unknown,
                // Horizontal offset 0, vertical offset 0
                AV2_CSP_TOPLEFT => ChromaSamplePosition::Colocated,
                _ => ChromaSamplePosition::Unknown,
            }
        }
        Ok(())
    }

    fn parse_obu_header(stream: &mut IStream) -> AvifResult<ObuHeader> {
        let obu_size = stream.read_uleb128()?;
        let obu_header_extension_flag = stream.read_bool()?;
        let obu_type = stream.read_bits(5)? as u8;
        stream.skip_bits(2)?; // obu_tlayer_id
        if obu_header_extension_flag {
            stream.skip_bits(8)?; // obu_mlayer_id, obu_xlayer_id
        }
        let obu_header_size = if obu_header_extension_flag { 2 } else { 1 };
        if obu_size < obu_header_size {
            return AvifError::bmff_parse_failed(format!("invalid obu_size {obu_size}",));
        }
        let obu_payload_size = obu_size - obu_header_size;
        if obu_payload_size as usize > stream.bytes_left()? {
            return AvifError::bmff_parse_failed(format!("invalid obu_size {obu_size}",));
        }
        Ok(ObuHeader {
            obu_type,
            size: obu_payload_size,
        })
    }

    pub(crate) fn parse_from_obus(data: &[u8]) -> AvifResult<Self> {
        // TODO: b/437292541 - Match AV2 specification once finalized.
        let mut stream = IStream::create(data);
        let mut sequence_header = None;

        while stream.has_bytes_left()? {
            let obu = Self::parse_obu_header(&mut stream)?;
            if obu.obu_type == crate::codecs::avm::AV2_OBU_SEQUENCE_HEADER {
                if sequence_header.is_some() {
                    return AvifError::bmff_parse_failed("Multiple Sequence Header OBUs");
                }
                sequence_header = Some({
                    let mut stream =
                        stream.sub_stream(&BoxSize::FixedSize(usize_from_u32(obu.size)?))?;
                    let mut sequence_header = Av2SequenceHeader::default();
                    let seq_header_id = stream.read_uvlc()?;
                    if seq_header_id >= 16 {
                        return AvifError::bmff_parse_failed(format!(
                            "invalid seq_header_id {seq_header_id}",
                        ));
                    }
                    sequence_header.parse_profile(&mut stream)?;
                    sequence_header.parse_frame_max_dimensions(&mut stream)?;
                    sequence_header.parse_chroma_format_bitdepth(&mut stream)?;
                    sequence_header.color_primaries = ColorPrimaries::Unspecified;
                    sequence_header.transfer_characteristics = TransferCharacteristics::Unspecified;
                    sequence_header.matrix_coefficients = MatrixCoefficients::Unspecified;
                    sequence_header.yuv_range = YuvRange::Limited;
                    sequence_header.config.chroma_sample_position = ChromaSamplePosition::Unknown;
                    sequence_header
                });
            } else if obu.obu_type == crate::codecs::avm::AV2_OBU_CONTENT_INTERPRETATION {
                if let Some(sequence_header) = &mut sequence_header {
                    sequence_header.parse_content_interpretation(
                        &mut stream.sub_stream(&BoxSize::FixedSize(usize_from_u32(obu.size)?))?,
                    )?;
                } else {
                    return AvifError::bmff_parse_failed(
                        "Content Interpretation OBU found before Sequence Header OBU",
                    );
                }
            } else {
                // Skip this OBU.
                stream.skip(usize_from_u32(obu.size)?)?;
                continue;
            }
        }
        if let Some(sequence_header) = sequence_header {
            return Ok(sequence_header);
        }
        AvifError::bmff_parse_failed("missing AV2 Sequence Header OBU")
    }
}
