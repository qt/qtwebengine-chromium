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

#![allow(non_upper_case_globals)]

use crate::codecs::*;
use crate::encoder::Sample;
use crate::encoder::ScalingMode;
use crate::image::Image;
use crate::image::YuvRange;
#[cfg(test)]
use crate::internal_utils::are_images_equal;
use crate::parser::obu::Av2SequenceHeader;
#[cfg(test)]
use crate::reformat::rgb::Format;
use crate::utils::pixels::Pixels;
use crate::utils::IFraction;
use crate::*;

use avm_sys::bindings::*;

use std::cmp;
use std::ffi::CStr;
use std::ffi::CString;
use std::mem::MaybeUninit;
use std::slice;

// OBU types
pub(crate) const AV2_OBU_SEQUENCE_HEADER: u8 = OBU_TYPE_OBU_SEQUENCE_HEADER;
pub(crate) const AV2_OBU_CONTENT_INTERPRETATION: u8 = OBU_TYPE_OBU_CONTENT_INTERPRETATION;

// Chroma sample position
pub(crate) const AV2_CSP_LEFT: u32 = avm_chroma_sample_position_AVM_CSP_LEFT;
pub(crate) const AV2_CSP_CENTER: u32 = avm_chroma_sample_position_AVM_CSP_CENTER;
pub(crate) const AV2_CSP_TOPLEFT: u32 = avm_chroma_sample_position_AVM_CSP_TOPLEFT;

// Color description
pub(crate) const AV2_IDC_EXPLICIT: u32 = avm_color_description_AVM_COLOR_DESC_IDC_EXPLICIT;
pub(crate) const AV2_BT709SDR: u32 = avm_color_description_AVM_COLOR_DESC_IDC_BT709SDR;
pub(crate) const AV2_BT2100PQ: u32 = avm_color_description_AVM_COLOR_DESC_IDC_BT2100PQ;
pub(crate) const AV2_BT2100HLG: u32 = avm_color_description_AVM_COLOR_DESC_IDC_BT2100HLG;
pub(crate) const AV2_SRGB: u32 = avm_color_description_AVM_COLOR_DESC_IDC_SRGB;
pub(crate) const AV2_SRGBSYCC: u32 = avm_color_description_AVM_COLOR_DESC_IDC_SRGBSYCC;

#[derive(Default)]
pub struct Avm {
    // Encoder or decoder.
    context: Option<avm_codec_ctx_t>,

    // Encoder.
    encoder_avm_config: Option<avm_codec_enc_cfg>,
    encoder_config: Option<EncoderConfig>,
    current_layer: u32,

    // Decoder.
    decoder_config: Option<DecoderConfig>,
    decoder_iter: avm_codec_iter_t,
    image: *mut avm_image_t,
}

// Functions mapping from CrabbyAvif structures to AV2 or libavm constants.

fn avm_format(image: &Image, category: Category) -> AvifResult<avm_img_fmt_t> {
    let format = match category {
        Category::Alpha => avm_img_fmt_AVM_IMG_FMT_I420,
        _ => match image.yuv_format {
            PixelFormat::Yuv420 | PixelFormat::Yuv400 => avm_img_fmt_AVM_IMG_FMT_I420,
            PixelFormat::Yuv422 => avm_img_fmt_AVM_IMG_FMT_I422,
            PixelFormat::Yuv444 => avm_img_fmt_AVM_IMG_FMT_I444,
            _ => return AvifError::invalid_argument(),
        },
    };
    Ok(if image.depth > 8 { format | AVM_IMG_FMT_HIGHBITDEPTH } else { format })
}

fn avm_seq_profile(image: &Image, category: Category) -> AvifResult<u32> {
    if image.depth == 12 {
        return Ok(2); // 12-bit is always profile 2.
    }
    if category == Category::Alpha {
        return Ok(0); // Alpha is monochrome, so it is always profile 0.
    }
    match image.yuv_format {
        PixelFormat::Yuv420 | PixelFormat::Yuv400 => Ok(0),
        PixelFormat::Yuv422 => Ok(2),
        PixelFormat::Yuv444 => Ok(1),
        _ => AvifError::invalid_argument(),
    }
}

fn avm_scaling_mode(scaling_mode: &ScalingMode) -> AvifResult<avm_scaling_mode_t> {
    fn get_avm_scaling_mode_1d(mut fraction: IFraction) -> AvifResult<avm_scaling_mode_1d> {
        fraction.is_valid()?;
        fraction.simplify();
        Ok(match fraction {
            IFraction(1, 1) => avm_scaling_mode_1d_AVME_NORMAL,
            IFraction(1, 2) => avm_scaling_mode_1d_AVME_ONETWO,
            // IFraction(1, 3) => avm_scaling_mode_1d_AVME_ONETHREE, // Only exists in libaom.
            IFraction(1, 4) => avm_scaling_mode_1d_AVME_ONEFOUR,
            IFraction(1, 8) => avm_scaling_mode_1d_AVME_ONEEIGHT,
            // IFraction(2, 3) => avm_scaling_mode_1d_AVME_TWOTHREE, // Only exists in libaom.
            IFraction(3, 4) => avm_scaling_mode_1d_AVME_THREEFOUR,
            IFraction(3, 5) => avm_scaling_mode_1d_AVME_THREEFIVE,
            IFraction(4, 5) => avm_scaling_mode_1d_AVME_FOURFIVE,
            _ => return AvifError::not_implemented(),
        })
    }

    Ok(avm_scaling_mode_t {
        h_scaling_mode: get_avm_scaling_mode_1d(scaling_mode.horizontal)?,
        v_scaling_mode: get_avm_scaling_mode_1d(scaling_mode.vertical)?,
    })
}

// Returns true if the packet was added. Returns false if the packet was skipped.
fn add_avm_pkt_to_output_samples(
    pkt: &avm_codec_cx_pkt,
    output_samples: &mut Vec<Sample>,
) -> AvifResult<bool> {
    if pkt.kind != avm_codec_cx_pkt_kind_AVM_CODEC_CX_FRAME_PKT {
        return Ok(false);
    }
    // # Safety: buf and sz are guaranteed to be valid as per libavm API contract. So
    // it is safe to construct a slice from it.
    let encoded_data =
        unsafe { std::slice::from_raw_parts(pkt.data.frame.buf as *const u8, pkt.data.frame.sz) };
    // # Safety: pkt.data is a union. pkt.kind == AVM_CODEC_CX_FRAME_PKT guarantees
    // that pkt.data.frame is the active field of the union (per libavm API contract).
    // So this access is safe.
    let sync = (unsafe { pkt.data.frame.flags } & AVM_FRAME_IS_KEY) != 0;
    output_samples
        .try_reserve(1)
        .map_err(AvifError::map_out_of_memory)?;
    output_samples.push(Sample::create_from(encoded_data, sync)?);
    Ok(true)
}

impl Encoder for Avm {
    fn encode_image(
        &mut self,
        image: &Image,
        category: Category,
        config: &EncoderConfig,
        output_samples: &mut Vec<Sample>,
    ) -> AvifResult<()> {
        if self.context.is_none() {
            // # Safety: Calling a C function.
            let encoder_iface = unsafe { avm_codec_av2_cx() };
            let avm_usage = AVM_USAGE_GOOD_QUALITY;
            let mut cfg_uninit: MaybeUninit<avm_codec_enc_cfg> = MaybeUninit::uninit();
            // # Safety: Calling a C function with valid parameters.
            let err = unsafe {
                avm_codec_enc_config_default(encoder_iface, cfg_uninit.as_mut_ptr(), avm_usage)
            };
            if err != avm_codec_err_t_AVM_CODEC_OK {
                return AvifError::unknown_error(format!(
                    "avm_codec_enc_config_default() failed: {err}"
                ));
            }
            // # Safety: cfg_uninit was initialized in the C function call above.
            let mut avm_config = unsafe { cfg_uninit.assume_init() };
            avm_config.rc_end_usage = avm_rc_mode_AVM_Q;
            avm_config.g_profile = avm_seq_profile(image, category)?;
            avm_config.g_bit_depth = image.depth.into();
            avm_config.g_input_bit_depth = image.depth.into();
            avm_config.g_w = image.width;
            avm_config.g_h = image.height;

            if config.is_single_image {
                avm_config.g_limit = 1;
                avm_config.g_lag_in_frames = 0;
                avm_config.kf_mode = avm_kf_mode_AVM_KF_DISABLED;
                avm_config.kf_max_dist = 0;
            }
            if config.disable_lagged_output {
                avm_config.g_lag_in_frames = 0;
            }
            if config.extra_layer_count > 0 {
                avm_config.g_lag_in_frames = 0;
                avm_config.g_limit = config.extra_layer_count + 1;
            }
            if config.threads > 1 {
                avm_config.g_threads = cmp::min(config.threads, 64);
            }

            avm_config.monochrome =
                (category == Category::Alpha || image.yuv_format == PixelFormat::Yuv400).into();
            // end-usage is the only codec specific option that has to be set before initializing
            // the libavm encoder
            if let Some(value) = config.codec_specific_option(category, "end-usage".into()) {
                avm_config.rc_end_usage = match (value.parse(), &*value) {
                    (Ok(avm_rc_mode_AVM_VBR), _) | (Err(_), "vbr") => avm_rc_mode_AVM_VBR,
                    (Ok(avm_rc_mode_AVM_CBR), _) | (Err(_), "cbr") => avm_rc_mode_AVM_CBR,
                    (Ok(avm_rc_mode_AVM_CQ), _) | (Err(_), "cq") => avm_rc_mode_AVM_CQ,
                    (Ok(avm_rc_mode_AVM_Q), _) | (Err(_), "q") => avm_rc_mode_AVM_Q,
                    _ => return AvifError::invalid_argument(),
                };
            }
            if avm_config.rc_end_usage == avm_rc_mode_AVM_VBR
                || avm_config.rc_end_usage == avm_rc_mode_AVM_CBR
            {
                // cq-level is unused in these modes, so set the min and max quantizer instead.
                let (min, max) = config.min_max_quantizers();
                avm_config.rc_min_quantizer = min as i32;
                avm_config.rc_max_quantizer = max as i32;
            }

            let mut encoder_uninit: MaybeUninit<avm_codec_ctx_t> = MaybeUninit::uninit();
            let init_flags = 0x0;
            // # Safety: Calling a C function with valid parameters.
            let err = unsafe {
                avm_codec_enc_init_ver(
                    encoder_uninit.as_mut_ptr(),
                    encoder_iface,
                    &avm_config as *const _,
                    init_flags,
                    AVM_ENCODER_ABI_VERSION as _,
                )
            };
            if err != avm_codec_err_t_AVM_CODEC_OK {
                return AvifError::unknown_error(format!("avm_codec_enc_init() failed: {err}"));
            }
            // # Safety: encoder_uninit was initialized in the C function call above.
            self.context = Some(unsafe { encoder_uninit.assume_init() });

            if avm_config.rc_end_usage == avm_rc_mode_AVM_CQ
                || avm_config.rc_end_usage == avm_rc_mode_AVM_Q
            {
                self.codec_control(avme_enc_control_id_AVME_SET_QP, config.quantizer())?;
            }
            if config.quantizer() == 0 {
                self.codec_control(avme_enc_control_id_AV2E_SET_LOSSLESS, 1)?;
            }
            if config.tile_rows_log2 != 0 {
                self.codec_control(
                    avme_enc_control_id_AV2E_SET_TILE_ROWS,
                    config.tile_rows_log2,
                )?;
            }
            if config.tile_columns_log2 != 0 {
                self.codec_control(
                    avme_enc_control_id_AV2E_SET_TILE_COLUMNS,
                    config.tile_columns_log2,
                )?;
            }
            if config.extra_layer_count > 0 {
                self.codec_control(
                    avme_enc_control_id_AVME_SET_NUMBER_MLAYERS,
                    config.extra_layer_count + 1,
                )?;
            }
            if let Some(speed) = config.speed {
                self.codec_control(avme_enc_control_id_AVME_SET_CPUUSED, cmp::min(speed, 9))?;
            }
            match category {
                Category::Alpha => {
                    // AVIF specification, Section 4 "Auxiliary Image Items and Sequences":
                    //   The color_range field in the Sequence Header OBU shall be set to 1.
                    self.codec_control(
                        avme_enc_control_id_AV2E_SET_COLOR_RANGE,
                        avm_color_range_AVM_CR_FULL_RANGE,
                    )?
                    // Keep the default AVM_CSP_UNKNOWN value.

                    // CICP (CP/TC/MC) does not apply to the alpha auxiliary image.
                    // Keep default Unspecified (2) colour primaries, transfer characteristics,
                    // and matrix coefficients.
                }
                _ => {
                    // libavm's defaults are AVM_CSP_UNKNOWN and 0 (studio/limited range).
                    // Call avm_codec_control() only if the values are not the defaults.
                    // AV1-ISOBMFF specification, Section 2.3.4:
                    //   The value of full_range_flag in the 'colr' box SHALL match the color_range
                    //   flag in the Sequence Header OBU.
                    if image.yuv_range != YuvRange::Limited {
                        self.codec_control(
                            avme_enc_control_id_AV2E_SET_COLOR_RANGE,
                            avm_color_range_AVM_CR_FULL_RANGE,
                        )?;
                    }
                    // Section 2.3.4 of AV1-ISOBMFF says 'colr' with 'nclx' should be present and
                    // shall match CICP values in the Sequence Header OBU, unless the latter has
                    // 2/2/2 (Unspecified). So set CICP values to 2/2/2 (Unspecified) in the
                    // Sequence Header OBU for simplicity. libavm's defaults are
                    // AVM_CICP_CP_UNSPECIFIED, AVM_CICP_TC_UNSPECIFIED, and
                    // AVM_CICP_MC_UNSPECIFIED. No need to call avm_codec_control().
                }
            }

            let codec_specific_options = config.codec_specific_options(category);
            for (key, value) in &codec_specific_options {
                if key == "end-usage" {
                    // This key is already processed before initialization of the encoder.
                    continue;
                }
                let key_str = CString::new(key.clone()).unwrap();
                let value_str = CString::new(value.clone()).unwrap();
                // # Safety: Calling a C function with valid parameters.
                let err = unsafe {
                    avm_codec_set_option(
                        self.context.unwrap_mut() as *mut _,
                        key_str.as_ptr(),
                        value_str.as_ptr(),
                    )
                };
                if err != avm_codec_err_t_AVM_CODEC_OK {
                    return AvifError::unknown_error(format!(
                        "avm_codec_set_option({key}, {value}) failed: {err}"
                    ));
                }
            }
            // Set tune=SSIM unless explicitly defined by the user.
            if !codec_specific_options.iter().any(|(key, _)| key == "tune") {
                self.codec_control(
                    avme_enc_control_id_AVME_SET_TUNING,
                    avm_tune_metric_AVM_TUNE_SSIM,
                )?;
            }

            self.encoder_avm_config = Some(avm_config);
            self.encoder_config = Some(config.clone());
        } else if self.encoder_config.unwrap_ref() != config {
            let avm_config = self.encoder_avm_config.unwrap_mut();
            if avm_config.g_w != image.width || avm_config.g_h != image.height {
                // Dimension changes are forbidden.
                return AvifError::invalid_argument();
            }
            if self.encoder_config.unwrap_ref().quantizer() != config.quantizer() {
                if avm_config.rc_end_usage == avm_rc_mode_AVM_VBR
                    || avm_config.rc_end_usage == avm_rc_mode_AVM_CBR
                {
                    let (min, max) = config.min_max_quantizers();
                    avm_config.rc_min_quantizer = min as i32;
                    avm_config.rc_max_quantizer = max as i32;
                    // # Safety: Calling a C function with valid parameters.
                    let err = unsafe {
                        avm_codec_enc_config_set(
                            self.context.unwrap_mut() as *mut _,
                            self.encoder_avm_config.unwrap_ref() as *const _,
                        )
                    };
                    if err != avm_codec_err_t_AVM_CODEC_OK {
                        return AvifError::unknown_error(format!(
                            "avm_codec_enc_config_set() failed: {err}"
                        ));
                    }
                } else if avm_config.rc_end_usage == avm_rc_mode_AVM_CQ
                    || avm_config.rc_end_usage == avm_rc_mode_AVM_Q
                {
                    self.codec_control(avme_enc_control_id_AVME_SET_QP, config.quantizer())?;
                }
                self.codec_control(
                    avme_enc_control_id_AV2E_SET_LOSSLESS,
                    if config.quantizer() == 0 { 1 } else { 0 },
                )?;
            }
            if self.encoder_config.unwrap_ref().tile_rows_log2 != config.tile_rows_log2 {
                self.codec_control(
                    avme_enc_control_id_AV2E_SET_TILE_ROWS,
                    config.tile_rows_log2,
                )?;
            }
            if self.encoder_config.unwrap_ref().tile_columns_log2 != config.tile_columns_log2 {
                self.codec_control(
                    avme_enc_control_id_AV2E_SET_TILE_COLUMNS,
                    config.tile_columns_log2,
                )?;
            }
            self.encoder_config = Some(config.clone());
        }
        if self.current_layer > config.extra_layer_count {
            return AvifError::invalid_argument();
        }
        if config.extra_layer_count > 0 {
            return AvifError::not_implemented();
        }
        let scaling_mode = avm_scaling_mode(&self.encoder_config.unwrap_ref().scaling_mode)?;
        if scaling_mode.h_scaling_mode != avm_scaling_mode_1d_AVME_NORMAL
            || scaling_mode.v_scaling_mode != avm_scaling_mode_1d_AVME_NORMAL
        {
            self.codec_control(
                avme_enc_control_id_AVME_SET_SCALEMODE,
                &scaling_mode as *const _,
            )?;
        }
        // # Safety: Zero initializing a C-struct. This is safe because this is the same usage
        // pattern as the equivalent C-code. The relevant fields are populated in the lines below.
        let mut avm_image: avm_image_t = unsafe { std::mem::zeroed() };
        avm_image.fmt = avm_format(image, category)?;
        avm_image.bit_depth = if image.depth > 8 { 16 } else { 8 };
        avm_image.w = image.width;
        avm_image.h = image.height;
        avm_image.d_w = image.width; // Display dimensions equal to decoded dimensions.
        avm_image.d_h = image.height;
        avm_image.bps = match avm_image.fmt {
            avm_img_fmt_AVM_IMG_FMT_I420 => 12,   // 8 + 2 + 2 bits per sample
            avm_img_fmt_AVM_IMG_FMT_I422 => 16,   // 8 + 8 + 2 bits per sample
            avm_img_fmt_AVM_IMG_FMT_I444 => 24,   // 8 + 8 + 8 bits per sample
            avm_img_fmt_AVM_IMG_FMT_I42016 => 24, // 16 + 4 + 4 bits per sample
            avm_img_fmt_AVM_IMG_FMT_I42216 => 32, // 16 + 16 + 4 bits per sample
            avm_img_fmt_AVM_IMG_FMT_I44416 => 48, // 16 + 16 + 16 bits per sample
            _ => 16,
        };
        avm_image.x_chroma_shift = image.yuv_format.chroma_shift_x().0;
        avm_image.y_chroma_shift = image.yuv_format.chroma_shift_y();
        match category {
            Category::Alpha => {
                avm_image.range = avm_color_range_AVM_CR_FULL_RANGE;
                avm_image.monochrome = 1;
                avm_image.x_chroma_shift = 1;
                avm_image.y_chroma_shift = 1;
                avm_image.planes[0] = image.planes[3].unwrap_ref().ptr_generic() as *mut _;
                avm_image.stride[0] = image.row_bytes[3] as i32;
            }
            _ => {
                avm_image.range = image.yuv_range as u32;
                if image.yuv_format == PixelFormat::Yuv400 {
                    avm_image.monochrome = 1;
                    avm_image.x_chroma_shift = 1;
                    avm_image.y_chroma_shift = 1;
                    avm_image.planes[0] = image.planes[0].unwrap_ref().ptr_generic() as *mut _;
                    avm_image.stride[0] = image.row_bytes[0] as i32;
                } else {
                    avm_image.monochrome = 0;
                    for i in 0..=2 {
                        avm_image.planes[i] = image.planes[i].unwrap_ref().ptr_generic() as *mut _;
                        avm_image.stride[i] = image.row_bytes[i] as i32;
                    }
                }
            }
        }
        avm_image.cp = image.color_primaries as u32;
        avm_image.tc = image.transfer_characteristics as u32;
        avm_image.mc = image.matrix_coefficients as u32;
        // TODO: b/392112497 - force keyframes when necessary.
        let mut encode_flags = 0;
        if self.current_layer > 0 {
            encode_flags |= AVM_EFLAG_NO_REF_GF as i64
                | AVM_EFLAG_NO_REF_ARF as i64
                | AVM_EFLAG_NO_REF_BWD as i64
                | AVM_EFLAG_NO_REF_ARF2 as i64;
        }
        let duration = 1;
        // # Safety: Calling a C function with valid parameters.
        let err = unsafe {
            avm_codec_encode(
                self.context.unwrap_mut() as *mut _,
                &avm_image as *const _,
                avm_codec_pts_t::default(),
                duration,
                encode_flags as _,
            )
        };
        if err != avm_codec_err_t_AVM_CODEC_OK {
            return AvifError::unknown_error(format!("avm_codec_encode() failed: {err}"));
        }
        let mut iter: avm_codec_iter_t = std::ptr::null_mut();
        loop {
            // # Safety: Calling a C function with valid parameters.
            let pkt = unsafe {
                avm_codec_get_cx_data(self.context.unwrap_mut() as *mut _, &mut iter as *mut _)
            };
            if pkt.is_null() {
                break;
            }
            // # Safety: pkt is guaranteed to be valid and not null (libavm API contract).
            let pkt = unsafe { *pkt };
            add_avm_pkt_to_output_samples(&pkt, output_samples)?;
        }
        if config.is_single_image
            || (config.extra_layer_count > 0 && config.extra_layer_count == self.current_layer)
        {
            self.finish(output_samples)?;
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                avm_codec_destroy(self.context.unwrap_mut() as *mut _);
            }
            self.context = None;
        }
        if config.extra_layer_count > 0 {
            self.current_layer += 1;
        }
        Ok(())
    }

    fn finish(&mut self, output_samples: &mut Vec<crate::encoder::Sample>) -> AvifResult<()> {
        if self.context.is_none() {
            return Ok(());
        }
        loop {
            // Flush the encoder.
            let duration = 1;
            // # Safety: Calling a C function with valid parameters.
            let err = unsafe {
                avm_codec_encode(
                    self.context.unwrap_mut() as *mut _,
                    std::ptr::null(),
                    avm_codec_pts_t::default(),
                    duration,
                    avm_enc_frame_flags_t::default(),
                )
            };
            if err != avm_codec_err_t_AVM_CODEC_OK {
                return AvifError::unknown_error(format!("Flush avm_codec_encode() failed: {err}"));
            }
            let mut got_packet = false;
            let mut iter: avm_codec_iter_t = std::ptr::null_mut();
            loop {
                // # Safety: Calling a C function with valid parameters.
                let pkt = unsafe {
                    avm_codec_get_cx_data(self.context.unwrap_mut() as *mut _, &mut iter as *mut _)
                };
                if pkt.is_null() {
                    break;
                }
                // # Safety: pkt is guaranteed to be valid and not null (libavm API contract).
                let pkt = unsafe { *pkt };
                got_packet = add_avm_pkt_to_output_samples(&pkt, output_samples)?;
            }
            if !got_packet {
                break;
            }
        }
        Ok(())
    }

    fn get_codec_config(
        &self,
        _image: &Image,
        _is_single_image: bool,
        _is_lossless: bool,
        output_samples: &[crate::encoder::Sample],
    ) -> AvifResult<CodecConfiguration> {
        // Harvest codec configuration from AV2 sequence header.
        Ok(CodecConfiguration::Av2(
            Av2SequenceHeader::parse_from_obus(&output_samples[0].data)?.config,
        ))
    }
}

impl Decoder for Avm {
    fn codec(&self) -> CodecChoice {
        CodecChoice::Avm
    }

    fn initialize(&mut self, config: &DecoderConfig) -> AvifResult<()> {
        self.decoder_config = Some(config.clone());
        Ok(())
    }

    fn get_next_image(
        &mut self,
        av2_payload: &[u8],
        spatial_id: u8,
        image: &mut Image,
        category: Category,
        _item: Option<&Item>,
    ) -> AvifResult<()> {
        if self.context.is_none() {
            // # Safety: Calling a C function.
            let decoder_iface = unsafe { avm_codec_av2_dx() };
            let avm_config = avm_codec_dec_cfg {
                threads: self.decoder_config.unwrap_ref().max_threads,
                w: Default::default(),
                h: Default::default(),
                path_parakit: Default::default(),
                suffix_parakit: Default::default(),
            };
            let mut context_uninit: MaybeUninit<avm_codec_ctx_t> = MaybeUninit::uninit();
            // # Safety: Calling a C function with valid parameters.
            let err = unsafe {
                avm_codec_dec_init_ver(
                    context_uninit.as_mut_ptr(),
                    decoder_iface,
                    &avm_config as *const _,
                    avm_codec_flags_t::default(),
                    AVM_DECODER_ABI_VERSION as _,
                )
            };
            if err != avm_codec_err_t_AVM_CODEC_OK {
                return AvifError::unknown_error(format!("avm_codec_dec_init() failed: {err}"));
            }
            // # Safety: encoder_uninit was initialized in the C function call above.
            self.context = Some(unsafe { context_uninit.assume_init() });

            self.codec_control(
                avm_dec_control_id_AV2D_SET_OUTPUT_ALL_LAYERS,
                if self.decoder_config.unwrap_ref().all_layers { 1 } else { 0 },
            )?;
            self.codec_control(
                avm_dec_control_id_AV2D_SET_OPERATING_POINT,
                self.decoder_config.unwrap_ref().operating_point as i32,
            )?;

            self.decoder_iter = std::ptr::null();
        }

        let mut next_frame;
        let mut target_spatial_id = 0xff;
        let mut av2_payload_can_be_used = true;
        loop {
            // # Safety: Calling a C function with valid parameters.
            next_frame = unsafe {
                avm_codec_get_frame(
                    self.context.unwrap_mut() as *mut _,
                    (&mut self.decoder_iter) as *mut _,
                )
            };
            if !next_frame.is_null() {
                if target_spatial_id != 0xff {
                    // # Safety: Dereferencing a non-NULL pointer.
                    let mlayer_id = unsafe { (*next_frame).mlayer_id };
                    if target_spatial_id == mlayer_id {
                        // Found the correct spatial_id.
                        break;
                    }
                } else {
                    // Got an image!
                    break;
                }
            } else if av2_payload_can_be_used {
                self.decoder_iter = std::ptr::null();
                let user_priv = std::ptr::null_mut();
                // # Safety: Calling a C function with valid parameters.
                let err = unsafe {
                    avm_codec_decode(
                        self.context.unwrap_mut() as *mut _,
                        av2_payload.as_ptr(),
                        av2_payload.len(),
                        user_priv,
                    )
                };
                if err != avm_codec_err_t_AVM_CODEC_OK {
                    return AvifError::unknown_error(format!("avm_codec_decode() failed: {err}"));
                }
                target_spatial_id = spatial_id.into();
                av2_payload_can_be_used = false;
            } else {
                break;
            }
        }

        if !next_frame.is_null() {
            self.image = next_frame;
        } else if category == Category::Alpha && !self.image.is_null() {
            // Special case: reuse last alpha frame
        } else {
            return AvifError::unknown_error("avm_codec_get_frame() failed");
        }
        // # Safety: Dereferencing a non-NULL pointer.
        let avm_image = &unsafe { *self.image };

        if category == Category::Color {
            // Color (YUV) planes - set image to correct size / format, fill color
            let yuv_format = if avm_image.monochrome != 0 {
                PixelFormat::Yuv400
            } else {
                match avm_image.fmt {
                    avm_img_fmt_AVM_IMG_FMT_I420
                    | avm_img_fmt_AVM_IMG_FMT_AVMI420
                    | avm_img_fmt_AVM_IMG_FMT_I42016 => PixelFormat::Yuv420,
                    avm_img_fmt_AVM_IMG_FMT_I422 | avm_img_fmt_AVM_IMG_FMT_I42216 => {
                        PixelFormat::Yuv422
                    }
                    avm_img_fmt_AVM_IMG_FMT_I444 | avm_img_fmt_AVM_IMG_FMT_I44416 => {
                        PixelFormat::Yuv444
                    }
                    fmt => {
                        return AvifError::unknown_error(format!(
                            "Unrecognized AVM pixel format {fmt}"
                        ))
                    }
                }
            };

            if image.width != 0
                && image.height != 0
                && (image.width != avm_image.d_w
                    || image.height != avm_image.d_h
                    || image.depth as u32 != avm_image.bit_depth
                    || image.yuv_format != yuv_format)
            {
                image.free_planes(&ALL_PLANES);
            }
            image.width = avm_image.d_w;
            image.height = avm_image.d_h;
            image.depth = avm_image
                .bit_depth
                .try_into()
                .map_err(AvifError::map_unknown_error)?;
            image.yuv_format = yuv_format;
            image.yuv_range = match avm_image.range {
                avm_color_range_AVM_CR_STUDIO_RANGE => YuvRange::Limited,
                avm_color_range_AVM_CR_FULL_RANGE => YuvRange::Full,
                range => return AvifError::bmff_parse_failed(format!("Invalid AVM range {range}")),
            };
            image.chroma_sample_position = match avm_image.csp {
                // Horizontal offset 0, vertical offset 0.5
                avm_chroma_sample_position_AVM_CSP_LEFT => ChromaSamplePosition::Vertical,
                // Horizontal offset 0.5, vertical offset 0.5
                avm_chroma_sample_position_AVM_CSP_CENTER => ChromaSamplePosition::Unknown,
                // Horizontal offset 0, vertical offset 0
                avm_chroma_sample_position_AVM_CSP_TOPLEFT => ChromaSamplePosition::Colocated,
                _ => ChromaSamplePosition::Unknown,
            };

            image.color_primaries = (avm_image.cp as u16).into();
            image.transfer_characteristics = (avm_image.tc as u16).into();
            image.matrix_coefficients = (avm_image.mc as u16).into();

            image.free_planes(&YUV_PLANES);

            // CrabbyAvif's Image assumes that a depth of 8 bits means an 8-bit buffer.
            // avm_image does not. The buffer depth depends on fmt|AVM_IMG_FMT_HIGHBITDEPTH, even for 8-bit values.
            if image.depth <= 8 && (avm_image.fmt & AVM_IMG_FMT_HIGHBITDEPTH) != 0 {
                image.allocate_planes(category)?;
                for plane in 0..image.yuv_format.plane_count() {
                    let plane_width = image.width(Plane::from(plane));
                    let plane_height = image.height(Plane::from(plane));
                    let src_plane = avm_image.planes[plane] as *mut u16;
                    if src_plane.is_null() {
                        return AvifError::unknown_error("AVM returned a NULL buffer");
                    }
                    let src_stride_in_bytes = avm_image.stride[plane];
                    if src_stride_in_bytes % 2 != 0 {
                        return AvifError::unknown_error(
                            "AVM returned an odd stride for 16-bit samples",
                        );
                    }
                    let src_stride_in_samples: usize = (src_stride_in_bytes / 2)
                        .try_into()
                        .map_err(AvifError::map_unknown_error)?;
                    // # Safety: planes and stride are guaranteed to be valid as per
                    // libavm API contract. So it is safe to construct slices.
                    let src_plane = unsafe {
                        slice::from_raw_parts(
                            src_plane,
                            checked_add!(
                                checked_mul!(
                                    src_stride_in_samples,
                                    checked_sub!(plane_height, 1)?
                                )?,
                                plane_width
                            )?,
                        )
                    };
                    for y in 0..plane_height {
                        let src_row = &src_plane
                            [y * src_stride_in_samples..y * src_stride_in_samples + plane_width];
                        let dst_row = image.row_mut(Plane::from(plane), y as u32)?;
                        for (x, dst_pixel) in dst_row.iter_mut().enumerate().take(plane_width) {
                            *dst_pixel = src_row[x] as u8;
                        }
                    }
                }
            } else {
                // Steal the pointers from the decoder's image directly.
                for plane in 0..image.yuv_format.plane_count() {
                    image.row_bytes[plane] = avm_image.stride[plane]
                        .try_into()
                        .map_err(AvifError::map_unknown_error)?;
                    image.planes[plane] = Some(Pixels::from_raw_pointer(
                        avm_image.planes[plane],
                        image.depth.into(),
                        image.height,
                        image.row_bytes[plane],
                    )?);
                    image.image_owns_planes[plane] = false;
                }
            }
        } else {
            // Alpha plane as an auxiliary image item

            if image.width != 0
                && image.height != 0
                && (image.width != avm_image.d_w
                    || image.height != avm_image.d_h
                    || image.depth as u32 != avm_image.bit_depth)
            {
                // Alpha plane doesn't match previous alpha plane decode, bail out.
                return AvifError::unknown_error(format!(
                        "Alpha plane does not match existing image dimensions: {}-bit {}x{} vs {}-bit {}x{}",
                        image.depth, image.width, image.height, avm_image.bit_depth, avm_image.d_w, avm_image.d_h,
                    ));
            }
            image.width = avm_image.d_w;
            image.height = avm_image.d_h;
            image.depth = avm_image
                .bit_depth
                .try_into()
                .map_err(AvifError::map_unknown_error)?;

            image.free_planes(&A_PLANE);

            if image.depth <= 8 && (avm_image.fmt & AVM_IMG_FMT_HIGHBITDEPTH) != 0 {
                image.allocate_planes(category)?;
                let plane_width = image.width(Plane::A);
                let plane_height = image.height(Plane::A);
                let src_plane = avm_image.planes[0] as *mut u16;
                if src_plane.is_null() {
                    return AvifError::unknown_error("AVM returned a NULL buffer");
                }
                let src_stride_in_bytes = avm_image.stride[0];
                if src_stride_in_bytes % 2 != 0 {
                    return AvifError::unknown_error(
                        "AVM returned an odd stride for 16-bit samples",
                    );
                }
                let src_stride_in_samples: usize = (src_stride_in_bytes / 2)
                    .try_into()
                    .map_err(AvifError::map_unknown_error)?;
                // # Safety: planes[0] and stride[0] are guaranteed to be valid as per
                // libavm API contract. So it is safe to construct a slice.
                let src_plane = unsafe {
                    slice::from_raw_parts(
                        src_plane,
                        checked_add!(
                            checked_mul!(src_stride_in_samples, checked_sub!(plane_height, 1)?)?,
                            plane_width
                        )?,
                    )
                };
                for y in 0..plane_height {
                    let src_row = &src_plane
                        [y * src_stride_in_samples..y * src_stride_in_samples + plane_width];
                    let dst_row = image.row_mut(Plane::A, y as u32)?;
                    for (x, dst_pixel) in dst_row.iter_mut().enumerate().take(plane_width) {
                        *dst_pixel = src_row[x] as u8;
                    }
                }
            } else {
                // Steal the pointers from the decoder's image directly
                image.row_bytes[Plane::A.as_usize()] = avm_image.stride[0]
                    .try_into()
                    .map_err(AvifError::map_unknown_error)?;
                image.planes[Plane::A.as_usize()] = Some(Pixels::from_raw_pointer(
                    avm_image.planes[0],
                    image.depth.into(),
                    image.height,
                    image.row_bytes[0],
                )?);
                image.image_owns_planes[Plane::A.as_usize()] = false;
            }
        }

        Ok(())
    }

    fn get_next_image_grid(
        &mut self,
        _payloads: &[Vec<u8>],
        _spatial_id: u8,
        _grid_image_helper: &mut GridImageHelper,
    ) -> AvifResult<()> {
        AvifError::not_implemented() // TODO: b/437292541 - Implement
    }
}

impl Drop for Avm {
    fn drop(&mut self) {
        if self.context.is_some() {
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                avm_codec_destroy(self.context.unwrap_mut() as *mut _);
            }
        }
    }
}

impl Avm {
    pub(crate) fn version() -> String {
        let version = match unsafe { CStr::from_ptr(avm_codec_version_str()) }.to_str() {
            Ok(s) => s.to_owned(),
            Err(_) => String::new(),
        };
        format!("avm: {version}")
    }

    // Convenience avm_codec_control() wrapper.
    fn codec_control<T>(&mut self, key: u32, value: T) -> AvifResult<()> {
        // # Safety: Calling a C function with valid parameters.
        let err =
            unsafe { avm_codec_control(self.context.unwrap_mut() as *mut _, key as i32, value) };
        if err != avm_codec_err_t_AVM_CODEC_OK {
            return AvifError::unknown_error(format!("avm_codec_control({key}) failed: {err}"));
        }
        Ok(())
    }
}

#[test]
fn avm_enc_dec_test() -> Result<(), AvifError> {
    let mut encoder = Avm::default();
    let mut config = EncoderConfig::default();
    config.is_single_image = true;
    config.quality = 100.0;

    let image = {
        let mut image = Image {
            width: 1,
            height: 1,
            depth: 8,
            yuv_format: PixelFormat::Yuv444,
            ..Default::default()
        };
        image.allocate_planes(Category::Color)?;
        let mut rgb = reformat::rgb::Image::create_from_yuv(&image);
        rgb.format = Format::Rgb;
        rgb.allocate()?;
        rgb.row_mut(0)?[0] = 42;
        rgb.row_mut(0)?[1] = 80;
        rgb.row_mut(0)?[2] = 0;
        rgb.convert_to_yuv(&mut image)?;
        image
    };

    let mut output_samples = vec![];
    encoder.encode_image(&image, Category::Color, &config, &mut output_samples)?;
    encoder.finish(&mut output_samples)?;
    assert_eq!(output_samples.len(), 1);
    let output_sample = &output_samples[0].data;
    let codec_config = Av2SequenceHeader::parse_from_obus(output_sample)?.config;

    let mut decoder = Avm::default();
    let mut decoded = Image::default();
    decoder.initialize(&DecoderConfig {
        operating_point: 0,
        all_layers: false,
        width: image.width,
        height: image.height,
        depth: image.depth,
        max_threads: 0,
        image_size_limit: None,
        max_input_size: 0,
        codec_config: CodecConfiguration::Av2(codec_config),
        category: Category::Color,
        android_mediacodec_output_color_format: AndroidMediaCodecOutputColorFormat::default(),
    })?;
    decoder.get_next_image(output_sample, 0xff, &mut decoded, Category::Color, None)?;

    assert!(are_images_equal(&image, &decoded)?);
    Ok(())
}
