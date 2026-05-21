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

#![allow(non_upper_case_globals)]

use crate::codecs::*;
use crate::encoder::Sample;
use crate::image::Image;
use crate::internal_utils::are_images_equal;
use crate::internal_utils::stream::OStreamLittleEndian;
use crate::internal_utils::u32_from_usize;
#[cfg(test)]
use crate::parser::mp4box::ColorInformation;
use crate::parser::mp4box::ItemProperty;
use crate::parser::mp4box::JpegXlCodecConfiguration;
#[cfg(test)]
use crate::parser::mp4box::PixelInformation;
#[cfg(test)]
use crate::parser::mp4box::PlanePixelInformation;
use crate::reformat::rgb::Format;
use crate::utils::pixels::ChannelIdc;
use crate::*;

use libjxl_sys::bindings::*;

use std::mem::MaybeUninit;
use std::ptr::null;
use std::ptr::null_mut;

#[derive(Default)]
pub struct Libjxl {
    // Encoding
    encoder: *mut JxlEncoder,
    expected_header: Option<Vec<u8>>,

    // Decoding
    decoder: *mut JxlDecoder,
    reconstructed_jxl: Option<Vec<u8>>,
}

// Convenient error mapping.
trait JxlEncoderStatusTrait {
    fn map_enc_err(self, encoder: *mut JxlEncoder) -> Result<(), AvifError>;
}
impl JxlEncoderStatusTrait for JxlEncoderStatus {
    fn map_enc_err(self, encoder: *mut JxlEncoder) -> Result<(), AvifError> {
        match self {
            JxlEncoderStatus_JXL_ENC_SUCCESS => Ok(()),
            JxlEncoderStatus_JXL_ENC_ERROR => {
                let error = unsafe { JxlEncoderGetError(encoder) };
                AvifError::unknown_error(format!("JxlEncoderError {error}",))
            }
            _ => AvifError::unknown_error(format!("Unexpected JxlEncoderStatus {self}")),
        }
    }
}
trait JxlDecoderStatusTrait {
    fn map_dec_err(self) -> Result<(), AvifError>;
}
impl JxlDecoderStatusTrait for JxlDecoderStatus {
    fn map_dec_err(self) -> Result<(), AvifError> {
        match self {
            JxlDecoderStatus_JXL_DEC_SUCCESS => Ok(()),
            _ => AvifError::unknown_error(format!("Unexpected JxlDecoderStatus {self}")),
        }
    }
}

fn rgb_image_to_jxl_pixel_format(
    rgb: &reformat::rgb::Image,
) -> Result<(JxlPixelFormat, usize), AvifError> {
    let data_type = match rgb.depth {
        8 => JxlDataType_JXL_TYPE_UINT8,
        9..16 => JxlDataType_JXL_TYPE_UINT16,
        _ => return AvifError::unknown_error(format!("Unexpected depth {}", rgb.depth)),
    };
    let min_row_bytes = checked_mul!(rgb.width, rgb.channel_count() * rgb.pixel_size())?;
    let size = checked_add!(
        checked_mul!(rgb.row_bytes as usize, rgb.height as usize - 1)?,
        min_row_bytes as usize
    )?;
    Ok((
        JxlPixelFormat {
            num_channels: rgb.channel_count(),
            data_type,
            endianness: JxlEndianness_JXL_NATIVE_ENDIAN,
            align: if rgb.row_bytes == min_row_bytes { 0 } else { rgb.row_bytes as usize },
        },
        size,
    ))
}

impl Encoder for Libjxl {
    fn encode_image(
        &mut self,
        image: &Image,
        category: Category,
        config: &EncoderConfig,
        _output_samples: &mut Vec<Sample>,
    ) -> AvifResult<()> {
        // TODO: b/456440247 - Remove RGB->YUV->RGB unnecessary conversion or pass YUV samples to libjxl.
        //                     Note that the libjxl API requires a single buffer anyway.
        let mut rgb = reformat::rgb::Image::create_from_yuv(image);
        rgb.format = match (image.yuv_format, image.alpha_present) {
            (PixelFormat::Yuv420 | PixelFormat::Yuv422 | PixelFormat::Yuv444, false) => Format::Rgb,
            (PixelFormat::Yuv420 | PixelFormat::Yuv422 | PixelFormat::Yuv444, true) => Format::Rgba,
            _ => return AvifError::not_implemented(),
        };
        rgb.allocate()?;
        rgb.convert_from_yuv(image)?;
        let rgb = rgb;
        let is_lossless = config.quality == 100.0;
        if is_lossless {
            // Make sure the YUV->RGB->YUV conversion is lossless too.
            let mut yuv = Image {
                planes: [None, None, None, None],
                ..image.shallow_clone()
            };
            yuv.allocate_planes(Category::Color)?;
            rgb.convert_to_yuv(&mut yuv)?;
            if !are_images_equal(image, &yuv)? {
                return AvifError::unknown_error(
                    "Could not convert YUV to RGB losslessly for JPEG XL",
                );
            }
        }

        let is_gray = rgb.format.channel_count() < 3; // Meaning is_monochrome.
        let jxl_header = reconstruct_jxl_header(ContainerFeatures {
            width: rgb.width,
            height: rgb.height,
            num_channels: rgb.format.channel_count() - rgb.format.has_alpha() as u32,
            num_extra: rgb.format.has_alpha().into(),
            bit_depth: rgb.depth,
            float_sample: false, // container_float_sample
            premultiplied_alpha: rgb.premultiply_alpha,
            codec_config: &self.get_codec_configuration(image, config.is_single_image, is_lossless),
            colr_nclx_colour_primaries: ColorPrimaries::Srgb as u32, // TODO: b/456440247 - Use color_primaries
            colr_nclx_transfer_characteristics: TransferCharacteristics::Srgb as u32, // TODO: b/456440247 - Use transfer_characteristics
            intensity_target: image.clli.map_or(0, |clli| clli.max_cll.into()),
        })?;

        match category {
            Category::Color => {}
            Category::Alpha => unreachable!(), // Should be a channel, not an auxiliary item.
            Category::Gainmap => return AvifError::not_implemented(),
        }
        let num_channels = rgb.pixel_size();
        let num_alpha_channels = if rgb.has_alpha() { 1 } else { 0 };

        if self.encoder.is_null() {
            // # Safety: Calling a C function.
            let encoder = unsafe { JxlEncoderCreate(null()) };
            if encoder.is_null() {
                return AvifError::unknown_error("JxlEncoderCreate() failed.");
            }
            self.encoder = encoder;

            let mut basic_info: MaybeUninit<JxlBasicInfo> = MaybeUninit::uninit();
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderInitBasicInfo(basic_info.as_mut_ptr()) };
            // # Safety: basic_info was initialized in the C function above.
            let mut basic_info = unsafe { basic_info.assume_init() };
            basic_info.xsize = rgb.width;
            basic_info.ysize = rgb.height;
            basic_info.bits_per_sample = rgb.depth.into();
            basic_info.uses_original_profile = is_lossless.into();
            basic_info.num_color_channels = num_channels - num_alpha_channels;
            if rgb.has_alpha() {
                basic_info.num_extra_channels = 1;
                basic_info.alpha_bits = basic_info.bits_per_sample;
                basic_info.alpha_premultiplied = rgb.premultiply_alpha.into();
                // JxlEncoderSetExtraChannelInfo() does not need to be called for alpha
                // apparently.
            }
            if !config.is_single_image {
                basic_info.have_animation = true.into();
                // tps_numerator and tps_denominator do not matter.
            }
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderSetBasicInfo(encoder, &basic_info) }.map_enc_err(encoder)?;

            let mut color_encoding: MaybeUninit<JxlColorEncoding> = MaybeUninit::uninit();
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlColorEncodingSetToSRGB(color_encoding.as_mut_ptr(), is_gray.into()) };
            // # Safety: color_encoding was initialized in the C function above.
            let mut color_encoding = unsafe { color_encoding.assume_init() };
            // TODO: b/456440247 - This should be RELATIVE but PERCEPTUAL forces
            //                     color_encoding.all_default to be false.
            color_encoding.rendering_intent = JxlRenderingIntent_JXL_RENDERING_INTENT_PERCEPTUAL;
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderSetColorEncoding(encoder, &color_encoding) }.map_enc_err(encoder)?;

            if let Some(speed) = config.speed {
                if speed >= 11 {
                    // # Safety: Calling a C function with valid parameters.
                    unsafe { JxlEncoderAllowExpertOptions(encoder) };
                }
            }

            self.expected_header = Some(jxl_header);
        } else if config.is_single_image {
            return AvifError::unknown_error("Cannot add another frame to a single image");
        } else if Some(jxl_header) != self.expected_header {
            return AvifError::unknown_error(
                "Cannot add another frame with different features to a JPEG XL animation",
            );
        }
        let encoder = self.encoder;

        // # Safety: Calling a C function with valid parameters.
        let frame_settings = unsafe { JxlEncoderFrameSettingsCreate(encoder, null()) };
        if frame_settings.is_null() {
            return AvifError::unknown_error("JxlEncoderFrameSettingsCreate() failed.");
        }

        if is_lossless {
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderSetFrameLossless(frame_settings, true.into()) }
                .map_enc_err(encoder)?;
        } else {
            // # Safety: Calling a C function with valid parameters.
            let distance = unsafe { JxlEncoderDistanceFromQuality(config.quality) };
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderSetFrameDistance(frame_settings, distance) }.map_enc_err(encoder)?;
        }
        if let Some(speed) = config.speed {
            let e = JxlEncoderFrameSettingId_JXL_ENC_FRAME_SETTING_EFFORT;
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderFrameSettingsSetOption(frame_settings, e, speed.into()) }
                .map_enc_err(encoder)?;
        }

        // TODO: b/456440247 - Check if a non-zero duration is necessary for sequences.
        if !config.is_single_image {
            let mut frame_header: MaybeUninit<JxlFrameHeader> = MaybeUninit::uninit();
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderInitFrameHeader(frame_header.as_mut_ptr()) };
            // # Safety: frame_header was initialized in the C function above.
            let mut frame_header = unsafe { frame_header.assume_init() };
            frame_header.duration = 1;
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderSetFrameHeader(frame_settings, &frame_header) }
                .map_enc_err(encoder)?;
        }

        let (pixel_format, size) = rgb_image_to_jxl_pixel_format(&rgb)?;
        // # Safety: Calling a C function with valid parameters.
        unsafe {
            JxlEncoderAddImageFrame(frame_settings, &pixel_format, rgb.pixels().cast(), size)
        }
        .map_enc_err(encoder)?;

        Ok(())
    }

    fn finish(&mut self, output_samples: &mut Vec<Sample>) -> AvifResult<()> {
        if self.encoder.is_null() {
            return Ok(());
        }
        let encoded_bytes_with_header = self.encode()?;

        let expected_header = self.expected_header.as_ref().unwrap();
        if encoded_bytes_with_header.len() <= expected_header.len()
            || encoded_bytes_with_header[..expected_header.len()] != *expected_header
        {
            return AvifError::unknown_error(format!(
                "Unexpected JPEG XL header {:?} vs {:?}",
                &encoded_bytes_with_header[..expected_header.len()],
                *expected_header
            ));
        }
        let mut encoded_bytes_without_header = encoded_bytes_with_header;
        encoded_bytes_without_header.drain(..expected_header.len());

        output_samples
            .try_reserve_exact(1)
            .map_err(AvifError::map_out_of_memory)?;
        output_samples.push(Sample {
            data: encoded_bytes_without_header,
            sync: true,
        });

        Ok(())
    }

    fn get_codec_config(
        &self,
        image: &Image,
        is_single_image: bool,
        is_lossless: bool,
        _output_samples: &[crate::encoder::Sample],
    ) -> AvifResult<CodecConfiguration> {
        Ok(CodecConfiguration::JpegXl(self.get_codec_configuration(
            image,
            is_single_image,
            is_lossless,
        )))
    }
}

impl Libjxl {
    fn encode(&mut self) -> AvifResult<Vec<u8>> {
        let encoder = self.encoder;

        // # Safety: Calling a C function with valid parameters.
        unsafe { JxlEncoderCloseInput(encoder) };

        let mut data: Vec<u8> = vec![]; // Vector of encoded bytes, growing by chunks.
        let mut chunk = [0; 64]; // Arbitrary chunk size.
        loop {
            let mut avail_out = chunk.len();
            let mut next_out: *mut u8 = chunk.as_mut_ptr();
            // # Safety: Calling a C function with valid parameters.
            let status = unsafe { JxlEncoderProcessOutput(encoder, &mut next_out, &mut avail_out) };
            // From the libjxl API:
            //   It is guaranteed that, if *avail_out >= 32, at least one byte of output will be written.
            assert!(avail_out < chunk.len());
            // avail_out now contains the number of unused bytes among the chunk.len() bytes
            // that were passed to JxlEncoderProcessOutput().
            let written_bytes = &chunk[..chunk.len() - avail_out];

            data.try_reserve(written_bytes.len())
                .map_err(AvifError::map_out_of_memory)?;
            data.extend_from_slice(written_bytes);
            if status != JxlEncoderStatus_JXL_ENC_NEED_MORE_OUTPUT {
                status.map_enc_err(encoder)?; // JxlEncoderStatus_JXL_ENC_SUCCESS is expected.
                return Ok(data);
            }
        }
    }

    fn get_codec_configuration(
        &self,
        image: &Image,
        is_single_image: bool,
        is_lossless: bool,
    ) -> JpegXlCodecConfiguration {
        // Deduced from libjxl behavior.
        JpegXlCodecConfiguration {
            have_animation: !is_single_image,
            modular_16bit_buffers: image.depth <= 12, // See libjxl's SetUIntSamples().
            xyb_encoded: !is_lossless,
            level: 5,
        }
    }
}

impl Decoder for Libjxl {
    fn codec(&self) -> CodecChoice {
        CodecChoice::Libjxl
    }

    fn initialize(&mut self, _: &DecoderConfig) -> AvifResult<()> {
        Ok(())
    }

    fn get_next_image(
        &mut self,
        payload: &[u8],
        spatial_id: u8,
        image: &mut Image,
        category: Category,
        item: Option<&Item>,
        #[cfg(feature = "android_mediacodec")] _signal_eos: bool,
    ) -> AvifResult<()> {
        // TODO: b/456440247 - Support tracks
        let item = if let Some(item) = item { item } else { return AvifError::not_implemented() };

        if spatial_id != 0xff {
            return AvifError::unknown_error(format!(
                "spatial_id {spatial_id} is not supported with JPEG XL"
            ));
        }
        match category {
            Category::Color => {}
            Category::Alpha => unreachable!(), // Should be a channel, not an auxiliary item.
            Category::Gainmap => return AvifError::not_implemented(),
        }

        if self.decoder.is_null() {
            // # Safety: Calling a C function.
            let decoder = unsafe { JxlDecoderCreate(null()) };
            if decoder.is_null() {
                return AvifError::unknown_error("JxlDecoderCreate() failed.");
            }
            self.decoder = decoder;

            const EVENTS: i32 =
                (JxlDecoderStatus_JXL_DEC_BASIC_INFO | JxlDecoderStatus_JXL_DEC_FULL_IMAGE) as i32;
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlDecoderSubscribeEvents(decoder, EVENTS) }.map_dec_err()?;

            // Prepend the JPEG XL payload which is stripped from its header with a reconstructed header.

            let pixi = item.pixi().ok_or_else(|| {
                AvifError::bmff_parse_failed::<(), _>("pixi is mandatory with hxlI").unwrap_err()
            })?;
            let premultiplied_alpha = item.alpi().is_some_and(|alpi| alpi.is_premultiplied);
            let codec_config = item
                .properties
                .iter()
                .find_map(|property| {
                    if let ItemProperty::CodecConfiguration(CodecConfiguration::JpegXl(
                        codec_config,
                    )) = property
                    {
                        Some(codec_config)
                    } else {
                        None
                    }
                })
                .ok_or_else(|| {
                    AvifError::bmff_parse_failed::<(), _>("hxlC is mandatory with hxlI")
                        .unwrap_err()
                })?;
            let _colr_nclx = item.colr_nclx().ok_or_else(|| {
                AvifError::bmff_parse_failed::<(), _>("colr nclx is mandatory with hxlI")
                    .unwrap_err()
            })?;
            self.reconstructed_jxl = Some(reconstruct_jxl_header(ContainerFeatures {
                width: item.width,
                height: item.height,
                num_channels: pixi.num_color_channels()?,
                num_extra: u32_from_usize(pixi.num_channels_with_idc(ChannelIdc::Alpha))?,
                bit_depth: pixi.bit_depth()?,
                float_sample: false, // TODO: b/456440247 - Support
                premultiplied_alpha,
                codec_config,
                colr_nclx_colour_primaries: ColorPrimaries::Srgb as u32, // TODO: b/456440247 - Use colr_nclx.color_primaries
                colr_nclx_transfer_characteristics: TransferCharacteristics::Srgb as u32, // TODO: b/456440247 - Use colr_nclx.transfer_characteristics
                intensity_target: item.clli().map_or(0, |clli| clli.max_cll.into()),
            })?);
            // JxlDecoderSetInput() could be called twice to avoid concatenating the reconstructed
            // header and the signaled payload but JxlDecoderReleaseInput() does not return 0.
            self.reconstructed_jxl
                .unwrap_mut()
                .extend_from_slice(payload);
            let reconstructed_len = self.reconstructed_jxl.unwrap_ref().len();
            // # Safety: Calling a C function with valid parameters.
            unsafe {
                JxlDecoderSetInput(
                    decoder,
                    self.reconstructed_jxl.unwrap_ref().as_ptr(),
                    reconstructed_len,
                )
            }
            .map_dec_err()?;
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlDecoderCloseInput(decoder) };

            // # Safety: Calling a C function with valid parameters.
            let status = unsafe { JxlDecoderProcessInput(decoder) };
            if status != JxlDecoderStatus_JXL_DEC_BASIC_INFO {
                return AvifError::unknown_error(format!(
                    "Unexpected JxlDecoderStatus {status} instead of BASIC_INFO"
                ));
            }
        }
        assert!(self.reconstructed_jxl.is_some());

        let decoder = self.decoder;
        let mut basic_info: MaybeUninit<JxlBasicInfo> = MaybeUninit::uninit();
        // # Safety: Calling a C function with valid parameters.
        unsafe { JxlDecoderGetBasicInfo(decoder, basic_info.as_mut_ptr()) }.map_dec_err()?;
        // # Safety: basic_info was initialized in the C function above.
        let basic_info = unsafe { basic_info.assume_init() };

        // # Safety: Calling a C function with valid parameters.
        let status = unsafe { JxlDecoderProcessInput(decoder) };
        if status != JxlDecoderStatus_JXL_DEC_NEED_IMAGE_OUT_BUFFER {
            return AvifError::unknown_error(format!(
                "Unexpected JxlDecoderStatus {status} instead of NEED_IMAGE_OUT_BUFFER"
            ));
        }

        assert_eq!(image.width, 0);
        image.width = basic_info.xsize;
        image.height = basic_info.ysize;
        image.depth = basic_info.bits_per_sample.try_into().unwrap();

        // TODO: b/456440247 - Use information from pixi with px_flags&1=1 to fill these values?
        image.yuv_format = PixelFormat::Yuv444; // Expect RGB for now.
        image.yuv_range = YuvRange::Full;
        image.chroma_sample_position = ChromaSamplePosition::Unknown;
        // TODO: b/456440247 - Use information from colr nclx to fill these values?
        image.color_primaries = ColorPrimaries::Unspecified;
        image.transfer_characteristics = TransferCharacteristics::Unspecified;
        image.matrix_coefficients = MatrixCoefficients::Unspecified;

        image.allocate_planes(Category::Color)?;
        match basic_info.num_extra_channels {
            0 => {}
            1 => {
                image.alpha_present = true;
                image.allocate_planes(Category::Alpha)?
            }
            n => return AvifError::unknown_error(format!("Unexpected {n} extra JPEG XL channels")),
        }

        // TODO: b/456440247 - Remove RGB->YUV->RGB unnecessary conversion.
        let mut rgb = reformat::rgb::Image::create_from_yuv(image);
        rgb.format = match (image.yuv_format, image.alpha_present) {
            (PixelFormat::Yuv420 | PixelFormat::Yuv422 | PixelFormat::Yuv444, false) => Format::Rgb,
            (PixelFormat::Yuv420 | PixelFormat::Yuv422 | PixelFormat::Yuv444, true) => Format::Rgba,
            _ => return AvifError::not_implemented(),
        };
        rgb.allocate()?;

        let (pixel_format, size) = rgb_image_to_jxl_pixel_format(&rgb)?;
        // # Safety: Calling a C function with valid parameters.
        unsafe {
            JxlDecoderSetImageOutBuffer(decoder, &pixel_format, rgb.pixels_mut().cast(), size)
        }
        .map_dec_err()?;

        // # Safety: Calling a C function with valid parameters.
        let status = unsafe { JxlDecoderProcessInput(decoder) };
        if status != JxlDecoderStatus_JXL_DEC_FULL_IMAGE {
            return AvifError::unknown_error(format!(
                "Unexpected JxlDecoderStatus {status} instead of FULL_IMAGE"
            ));
        }
        // Note that JxlDecoderProcessInput() could be called a final time, expecting SUCCESS.
        // Whether this was the last frame or not is unknown here, so it is skipped.

        rgb.convert_to_yuv(image)?;
        Ok(())
    }

    fn get_next_image_grid(
        &mut self,
        _payloads: &[Vec<u8>],
        _spatial_id: u8,
        _grid_image_helper: &mut GridImageHelper,
    ) -> AvifResult<()> {
        AvifError::not_implemented() // TODO: b/456440247
    }
}

impl Drop for Libjxl {
    fn drop(&mut self) {
        if !self.encoder.is_null() {
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlEncoderDestroy(self.encoder) };
            self.encoder = null_mut();
        }
        if !self.decoder.is_null() {
            // # Safety: Calling a C function with valid parameters.
            unsafe { JxlDecoderDestroy(self.decoder) };
            self.decoder = null_mut();
        }
    }
}

impl Libjxl {
    pub(crate) fn version() -> String {
        // # Safety: Calling a C function.
        let version = unsafe { JxlEncoderVersion() };
        format!(
            "jxl: v{}.{}.{}",
            version / 1000000,
            (version % 1000000) / 1000,
            version % 1000
        )
    }
}

// FindAspectRatio() from libjxl's headers.cc
fn find_aspect_ratio(xsize: u32, ysize: u32) -> u32 {
    for (r, num, den) in [
        (1, 1, 1),   // square
        (2, 12, 10), //
        (3, 4, 3),   // camera
        (4, 3, 2),   // mobile camera
        (5, 16, 9),  // camera/display
        (6, 5, 4),   //
        (7, 2, 1),   //
    ] {
        let fixed_aspect_ratio = ysize as u64 * num as u64 / den;
        if xsize == fixed_aspect_ratio as u32 {
            return r;
        }
    }
    0 // Must send xsize instead
}

impl OStreamLittleEndian {
    // See Section B.2.2 of ISO/IEC 18181-1.
    fn enum_table(&mut self, value: u32) -> AvifResult<()> {
        match value {
            0 => self.write_bits(0b00, 2),
            1 => self.write_bits(0b01, 2),
            2..34 => {
                self.write_bits(0b10, 2)?;
                self.write_bits(value - 2, 4) // 2 + u(4)
            }
            34..146 => {
                self.write_bits(0b11, 2)?;
                self.write_bits(value - 18, 4) // 18 + u(6)
            }
            _ => unreachable!(),
        }
    }

    // Implementation of U32(1 + u(9), 1 + u(13), 1 + u(18), 1 + u(30)) as in ISO/IEC 18181-1.
    fn wsz(&mut self, dim: u32) -> AvifResult<()> {
        match dim {
            0..=511 /* 1<<9 */ => {
                self.write_bits(0b00, 2)?;
                self.write_bits(dim, 9)
            }
            512..=8191 /* 1<<13 */ => {
                self.write_bits(0b01, 2)?;
                self.write_bits(dim, 13)
            }
            8192..=262144 /* 1<<18 */ => {
                self.write_bits(0b10, 2)?;
                self.write_bits(dim, 18)
            }
            _ => {
                self.write_bits(0b11, 2)?;
                self.write_bits(dim, 30)
            }
        }
    }

    // See libjxl's BitDepth::VisitFields().
    fn bit_depth_bundle(&mut self, float_sample: bool, bits_per_sample: u8) -> AvifResult<()> {
        self.write_bits(float_sample.into(), 1)?; // bit_depth.floating_point_sample
        if float_sample {
            // bit_depth.bits_per_sample
            match bits_per_sample {
                16 => self.write_bits(0b01, 2)?,
                32 => self.write_bits(0b00, 2)?,
                64 => {
                    self.write_bits(0b11, 2)?;
                    self.write_bits(64 - 1, 6)?; // 1 + u(6)
                }
                _ => return AvifError::unsupported_depth(),
            }
            // bit_depth.exp_bits
            match bits_per_sample {
                16 => self.write_bits(5 - 1, 4)?,  // 1 + u(4)
                32 => self.write_bits(8 - 1, 2)?,  // 1 + u(4)
                64 => self.write_bits(11 - 1, 4)?, // 1 + u(4)
                _ => return AvifError::unsupported_depth(),
            }
        } else {
            // bit_depth.bits_per_sample
            match bits_per_sample {
                8 => self.write_bits(0b00, 2)?,
                10 => self.write_bits(0b01, 2)?,
                12 => self.write_bits(0b10, 2)?,
                16 => {
                    self.write_bits(0b11, 2)?;
                    self.write_bits(16 - 1, 6)?; // 1 + u(6)
                }
                _ => return AvifError::unsupported_depth(),
            }
        }
        Ok(())
    }
}

// Image metadata that can be retrieved at the HEIF level.
struct ContainerFeatures<'a> {
    width: u32,
    height: u32,
    num_channels: u32,
    num_extra: u32,
    bit_depth: u8,
    float_sample: bool,
    premultiplied_alpha: bool,
    codec_config: &'a JpegXlCodecConfiguration, // hxlC
    colr_nclx_colour_primaries: u32,
    colr_nclx_transfer_characteristics: u32,
    intensity_target: u32,
}

fn reconstruct_jxl_header(container: ContainerFeatures) -> AvifResult<Vec<u8>> {
    let mut writer = OStreamLittleEndian::default();

    // JPEG XL compliant signature (ISO/IEC 18181-1, Table D.1) set to 0xFF0A (2815).

    writer.write_slice(&[0xff, 0x0a])?; // signature

    // JPEG XL compliant SizeHeader bundle with its fields set according to C.4.1.

    // See libjxl's SizeHeader::Set().
    let xsize = container.width;
    let ysize = container.height;
    let ratio = find_aspect_ratio(xsize, ysize);
    const BLOCK_DIM: u32 = 8; // kBlockDim
    let small = container.height <= 256
        && (container.height % BLOCK_DIM) == 0
        && (ratio != 0 || (container.width <= 256 && (container.width % BLOCK_DIM) == 0));

    // See libjxl's SizeHeader::VisitFields().
    writer.write_bits(if small { 1 } else { 0 }, 1)?;
    if small {
        writer.write_bits(ysize / 8 - 1, 5)?; // ysize_div8_minus_1_
    } else {
        writer.wsz(ysize - 1)?;
    }
    writer.write_bits(ratio, 3)?;
    if ratio == 0 && small {
        writer.write_bits(xsize / 8 - 1, 5)?; // xsize_div8_minus_1_
    } else if ratio == 0 {
        writer.wsz(xsize - 1)?;
    }

    // JPEG XL compliant ImageMetadata bundle.

    // See libjxl's ImageMetadata::VisitFields().
    writer.write_bits(0, 1)?; // all_default
    let extra_fields = container.codec_config.have_animation || container.intensity_target != 0;
    writer.write_bits(extra_fields.into(), 1)?; // extra_fields
    if extra_fields {
        writer.write_bits(0, 3)?; // orientation = 1 (minus one)
        writer.write_bits(0, 1)?; // have_intr_size
        writer.write_bits(0, 1)?; // have_preview
        writer.write_bits(container.codec_config.have_animation.into(), 1)?; // have_animation
        if container.codec_config.have_animation {
            // AnimationHeader bundle
            writer.write_bits(0b00, 2)?; // tps_numerator = 100
            writer.write_bits(0b00, 2)?; // tps_denominator = 1
            writer.write_bits(0b00, 2)?; // num_loops = 0
            writer.write_bits(0, 1)?; // have_timecodes
        }
    }
    writer.bit_depth_bundle(container.float_sample, container.bit_depth)?; // BitDepth bundle
    writer.write_bits(container.codec_config.modular_16bit_buffers.into(), 1)?; // modular_16bit_buffers
    match (container.num_extra, container.premultiplied_alpha) {
        (0, _) => writer.write_bits(0b00, 2)?, // num_extra
        (1, false) => {
            writer.write_bits(0b01, 2)?; // num_extra
            writer.write_bits(1, 1)?; // ec_info[0].d_alpha
        }
        (1, true) => {
            writer.write_bits(0b01, 2)?; // num_extra
            writer.write_bits(0, 1)?; // ec_info[0].d_alpha
            writer.enum_table(0)?; // ec_info[0].type = kAlpha

            // ec_info[0].bit_depth
            writer.bit_depth_bundle(container.float_sample, container.bit_depth)?;

            writer.write_bits(0b00, 2)?; // ec_info[0].dim_shift = 0
            writer.write_bits(0b00, 2)?; // ec_info[0].name_len = 0
            writer.write_bits(1, 1)?; // ec_info[0].alpha_associated
        }
        _ => unreachable!(),
    }
    writer.write_bits(container.codec_config.xyb_encoded.into(), 1)?; // xyb_encoded

    // ColourEncoding bundle
    writer.write_bits(0, 1)?; // color_encoding.all_default
    writer.write_bits(0, 1)?; // color_encoding.want_icc
    writer.enum_table(match container.num_channels {
        1 => 1, // color_encoding.colour_space = kGrey
        3 => 0, // color_encoding.colour_space = kRGB
        _ => unreachable!(),
    })?;
    writer.enum_table(1)?; // color_encoding.white_point = D65
    writer.enum_table(match container.colr_nclx_colour_primaries {
        1 | 9 | 11 => container.colr_nclx_colour_primaries, // color_encoding.primaries = kSRGB or k2100 or kP3
        _ => 1, // Unspecified or value not listed in table E.5 is mapped to color_encoding.primaries = kSRGB
    })?;
    {
        // CustomTransferFunction bundle
        writer.write_bits(0, 1)?; // color_encoding.tf.have_gamma
        writer.enum_table(match container.colr_nclx_transfer_characteristics {
            2 => 0, // color_encoding.tf = kUnknown
            13 | 16 | 17 | 18 => container.colr_nclx_transfer_characteristics, // color_encoding.tf = kSRGB or kPQ or kDCI or kHLG
            _ => 8, // color_encoding.tf = kLinear
        })?;
    }
    // TODO: b/456440247 - This should be kRelative but kPerceptual forces
    //                     color_encoding.all_default to be false.
    writer.enum_table(0)?; // color_encoding.rendering_intent = kPerceptual
    if extra_fields {
        // ToneMapping bundle
        match container.intensity_target {
            0 => writer.write_bits(1, 1)?,            // tone_mapping.all_default
            _ => return AvifError::not_implemented(), // TODO: b/456440247
        }
    }
    writer.write_bits(0b00, 2)?; // extensions.extensions = 0
    writer.write_bits(1, 1)?; // default_m = 1 // TODO: b/456440247

    // No ICC, no preview. Frame should start at byte boundary.
    writer.pad()?;

    Ok(writer.data)
}

#[test]
fn libjxl_enc_dec_test() -> Result<(), AvifError> {
    let mut encoder = Libjxl::default();
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
    let item = Item {
        width: image.width,
        height: image.height,
        properties: vec![
            ItemProperty::PixelInformation(PixelInformation {
                planes: vec![
                    PlanePixelInformation {
                        depth: image.depth,
                        channel_idc: Some(ChannelIdc::FirstColorChannel),
                        subsampling_type: Some(image.yuv_format),
                        subsampling_location: None,
                    },
                    PlanePixelInformation {
                        depth: image.depth,
                        channel_idc: Some(ChannelIdc::SecondColorChannel),
                        subsampling_type: Some(image.yuv_format),
                        subsampling_location: None,
                    },
                    PlanePixelInformation {
                        depth: image.depth,
                        channel_idc: Some(ChannelIdc::ThirdColorChannel),
                        subsampling_type: Some(image.yuv_format),
                        subsampling_location: None,
                    },
                ],
            }),
            ItemProperty::ColorInformation(ColorInformation::Nclx(Nclx {
                color_primaries: image.color_primaries,
                transfer_characteristics: image.transfer_characteristics,
                matrix_coefficients: image.matrix_coefficients,
                yuv_range: image.yuv_range,
            })),
            ItemProperty::CodecConfiguration(CodecConfiguration::JpegXl(
                encoder.get_codec_configuration(
                    &image,
                    config.is_single_image,
                    config.quality == 100.0,
                ),
            )),
        ],
        is_made_up: true,
        ..Default::default()
    };

    let mut output_samples = vec![];
    encoder.encode_image(&image, Category::Color, &config, &mut output_samples)?;
    encoder.finish(&mut output_samples)?;
    assert_eq!(output_samples.len(), 1);

    let mut decoder = Libjxl::default();
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
        codec_config: item.codec_config().unwrap().clone(),
        category: Category::Color,
        android_mediacodec_output_color_format: AndroidMediaCodecOutputColorFormat::default(),
    })?;
    decoder.get_next_image(
        &output_samples[0].data,
        0xff,
        &mut decoded,
        Category::Color,
        Some(&item),
    )?;

    assert!(are_images_equal(&image, &decoded)?);
    Ok(())
}
