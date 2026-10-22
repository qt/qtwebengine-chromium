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

use crate::utils::create_vec_exact;
use crate::AvifError;
use crate::AvifResult;
use crate::PixelFormat;

use md5::Digest;
use md5::Md5;

pub fn hex_string_to_bytes(hex_string: &[u8], num_expected_bytes: usize) -> AvifResult<Vec<u8>> {
    let mut bytes = create_vec_exact(num_expected_bytes)?;
    let mut hex_chars = hex_string.iter().filter(|&&b| b != b'\n');
    while bytes.len() < num_expected_bytes {
        let hi = hex_chars.next().ok_or(AvifError::UnknownError(
            "Unexpected end of hex string".into(),
        ))?;
        let lo = hex_chars
            .next()
            .ok_or(AvifError::UnknownError("Incomplete hex pair".into()))?;
        let byte = u8::from_str_radix(
            std::str::from_utf8(&[*hi, *lo])
                .map_err(|_| AvifError::UnknownError("Invalid UTF-8 in hex".into()))?,
            16,
        )
        .map_err(|_| {
            AvifError::UnknownError(format!("Invalid hex character at byte {}", bytes.len()))
        })?;
        bytes.push(byte);
    }
    Ok(bytes)
}

pub fn copy_raw_profile(profile: &[u8]) -> AvifResult<Vec<u8>> {
    if profile.is_empty() || profile[0] != b'\n' {
        return Err(AvifError::UnknownError(
            "Metadata extraction failed: truncated or malformed raw profile".into(),
        ));
    }
    let mut parts = profile.split(|&b| b == b'\n');
    parts.next(); // Skip prefix
    let _name = parts
        .next()
        .ok_or(AvifError::UnknownError("Missing profile name".into()))?;
    let length_bytes = parts
        .next()
        .ok_or(AvifError::UnknownError("Missing length bytes".into()))?;
    let length_str = std::str::from_utf8(length_bytes)
        .map_err(|_| AvifError::UnknownError("Length segment is not valid UTF-8".into()))?;
    let expected_length: usize = length_str
        .trim()
        .parse()
        .map_err(|_| AvifError::UnknownError("Invalid length format".into()))?;
    let hex_payload: Vec<u8> = parts.flatten().copied().collect();
    if expected_length == 0 || hex_payload.len() < expected_length * 2 {
        return Err(AvifError::UnknownError(
            "Invalid length or truncated hex payload".into(),
        ));
    }
    hex_string_to_bytes(&hex_payload, expected_length)
}

/**
 * Color Profile Structure
 *
 * Header:
 *  size         = 376 bytes (*1)
 *  CMM          = 'lcms' (*2)
 *  Version      = 2.2.0
 *  Device Class = Display
 *  Color Space  = RGB
 *  Conn. Space  = XYZ
 *  Date, Time   = 1 Jan 2000, 0:00:00
 *  Platform     = Microsoft
 *  Flags        = Not Embedded Profile, Use anywhere
 *  Dev. Mnfctr. = 0x0
 *  Dev. Model   = 0x0
 *  Dev. Attrbts = Reflective, Glossy, Positive, Color
 *  Rndrng Intnt = Perceptual
 *  Illuminant   = 0.96420288, 1.00000000, 0.82490540    [Lab 100.000000, 0.000000, 0.000000]
 *  Creator      = 'avif'
 *
 * Profile Tags:
 *                    Tag    ID      Offset         Size                 Value
 *                   ----  ------    ------         ----                 -----
 *  profileDescriptionTag  'desc'       240           95                  avif
 *     mediaWhitePointTag  'wtpt'       268 (*3)      20        (to be filled)
 *         redColorantTag  'rXYZ'       288           20        (to be filled)
 *       greenColorantTag  'gXYZ'       308           20        (to be filled)
 *        blueColorantTag  'bXYZ'       328           20        (to be filled)
 *              redTRCTag  'rTRC'       348 (*4)      16        (to be filled)
 *            greenTRCTag  'gTRC'       348           16        (to be filled)
 *             blueTRCTag  'bTRC'       348           16        (to be filled)
 *           copyrightTag  'cprt'       364           12                   CC0
 *
 * (*1): The template data is padded to 448 bytes according to MD5 specification, so that computation can be applied
 *       directly on it. The actual ICC profile data is the first 376 bytes.
 * (*2): 6.1.2 CMM Type: The signatures must be registered in order to avoid conflicts.
 *       The registry can be found at https://www.color.org/signatures2.xalter (Private and ICC tag and CMM registry)
 *       Therefore we are using the signature of Little CMS.
 * (*3): The profileDescriptionTag requires 95 bytes of data, but with some trick, the content of the last 67 bytes
 *       can be anything. Therefore we are placing the following tags in this region to reduce profile size.
 * (*4): The transfer characteristic (gamma) of the 3 channels are the same, so the data can be shared.
 */
static ICC_COLOR_TEMPLATE: [u8; 376] = [
    0x00, 0x00, 0x01, 0x78, 0x6c, 0x63, 0x6d, 0x73, 0x02, 0x20, 0x00, 0x00, 0x6d, 0x6e, 0x74, 0x72,
    0x52, 0x47, 0x42, 0x20, 0x58, 0x59, 0x5a, 0x20, 0x07, 0xd0, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x61, 0x63, 0x73, 0x70, 0x4d, 0x53, 0x46, 0x54, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0xd6, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x2d,
    0x61, 0x76, 0x69, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x09, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0x5f,
    0x77, 0x74, 0x70, 0x74, 0x00, 0x00, 0x01, 0x0c, 0x00, 0x00, 0x00, 0x14, 0x72, 0x58, 0x59, 0x5a,
    0x00, 0x00, 0x01, 0x20, 0x00, 0x00, 0x00, 0x14, 0x67, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0x34,
    0x00, 0x00, 0x00, 0x14, 0x62, 0x58, 0x59, 0x5a, 0x00, 0x00, 0x01, 0x48, 0x00, 0x00, 0x00, 0x14,
    0x72, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0x5c, 0x00, 0x00, 0x00, 0x10, 0x67, 0x54, 0x52, 0x43,
    0x00, 0x00, 0x01, 0x5c, 0x00, 0x00, 0x00, 0x10, 0x62, 0x54, 0x52, 0x43, 0x00, 0x00, 0x01, 0x5c,
    0x00, 0x00, 0x00, 0x10, 0x63, 0x70, 0x72, 0x74, 0x00, 0x00, 0x01, 0x6c, 0x00, 0x00, 0x00, 0x0c,
    0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x61, 0x76, 0x69, 0x66,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x58, 0x59, 0x5a, 0x20,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x54, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x16, 0xc9,
    0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x6f, 0xa0, 0x00, 0x00, 0x38, 0xf2,
    0x00, 0x00, 0x03, 0x8f, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x96,
    0x00, 0x00, 0xb7, 0x89, 0x00, 0x00, 0x18, 0xda, 0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x24, 0xa0, 0x00, 0x00, 0x0f, 0x85, 0x00, 0x00, 0xb6, 0xc4, 0x63, 0x75, 0x72, 0x76,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x74, 0x65, 0x78, 0x74,
    0x00, 0x00, 0x00, 0x00, 0x43, 0x43, 0x30, 0x00,
];
static COLOR_WHITE_OFFSET: usize = 0x114;
static COLOR_RED_OFFSET: usize = 0x128;
static COLOR_GREEN_OFFSET: usize = 0x13c;
static COLOR_BLUE_OFFSET: usize = 0x150;
static COLOR_GAMMA_OFFSET: usize = 0x168;

/**
 * Gray Profile Structure
 *
 * Header:
 *  size         = 275 bytes
 *  CMM          = 'lcms'
 *  Version      = 2.2.0
 *  Device Class = Display
 *  Color Space  = Gray
 *  Conn. Space  = XYZ
 *  Date, Time   = 1 Jan 2000, 0:00:00
 *  Platform     = Microsoft
 *  Flags        = Not Embedded Profile, Use anywhere
 *  Dev. Mnfctr. = 0x0
 *  Dev. Model   = 0x0
 *  Dev. Attrbts = Reflective, Glossy, Positive, Color
 *  Rndrng Intnt = Perceptual
 *  Illuminant   = 0.96420288, 1.00000000, 0.82490540    [Lab 100.000000, 0.000000, 0.000000]
 *  Creator      = 'avif'
 *
 * Profile Tags:
 *                    Tag    ID      Offset         Size                 Value
 *                   ----  ------    ------         ----                 -----
 *  profileDescriptionTag  'desc'       180           95                  avif
 *     mediaWhitePointTag  'wtpt'       208           20        (to be filled)
 *             grayTRCTag  'kTRC'       228           16        (to be filled)
 *           copyrightTag  'cprt'       244           12                   CC0
 */
static ICC_GRAY_TEMPLATE: [u8; 275] = [
    0x00, 0x00, 0x01, 0x13, 0x6c, 0x63, 0x6d, 0x73, 0x02, 0x20, 0x00, 0x00, 0x6d, 0x6e, 0x74, 0x72,
    0x47, 0x52, 0x41, 0x59, 0x58, 0x59, 0x5a, 0x20, 0x07, 0xd0, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x61, 0x63, 0x73, 0x70, 0x4d, 0x53, 0x46, 0x54, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0xd6, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0xd3, 0x2d,
    0x61, 0x76, 0x69, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x04, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0xb4, 0x00, 0x00, 0x00, 0x5f,
    0x77, 0x74, 0x70, 0x74, 0x00, 0x00, 0x00, 0xd0, 0x00, 0x00, 0x00, 0x14, 0x6b, 0x54, 0x52, 0x43,
    0x00, 0x00, 0x00, 0xe4, 0x00, 0x00, 0x00, 0x10, 0x63, 0x70, 0x72, 0x74, 0x00, 0x00, 0x00, 0xf4,
    0x00, 0x00, 0x00, 0x0c, 0x64, 0x65, 0x73, 0x63, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05,
    0x61, 0x76, 0x69, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x58, 0x59, 0x5a, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf3, 0x54, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x01, 0x16, 0xc9, 0x63, 0x75, 0x72, 0x76, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x74, 0x65, 0x78, 0x74, 0x00, 0x00, 0x00, 0x00, 0x43, 0x43, 0x30, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
];

static GRAY_WHITE_OFFSET: usize = 0xD8;
static GRAY_GAMMA_OFFSET: usize = 0xF0;
static CHECKSUM_OFFSET: usize = 0x54;

static SMALL: f64 = 1e-12;

// Bradford chromatic adaptation matrix
// from https://www.researchgate.net/publication/253799640_A_uniform_colour_space_based_upon_CIECAM97s
static BRADFORD: [[f64; 3]; 3] = [
    [0.8951, 0.2664, -0.1614],
    [-0.7502, 1.7135, 0.0367],
    [0.0389, -0.0685, 1.0296],
];

// LMS values for D50 whitepoint
static LMS_D50: [f64; 3] = [0.996284, 1.02043, 0.818644];

fn xy_to_xyz(x: f32, y: f32) -> AvifResult<[f64; 3]> {
    if (y.abs() as f64) < SMALL {
        return Err(AvifError::UnknownError("".into()));
    }
    let factor = 1.0 / y as f64;
    Ok([x as f64 * factor, 1.0, (1.0 - x as f64 - y as f64) * factor])
}

fn s15_fixed16(value: f64) -> AvifResult<[u8; 4]> {
    let value = (value * 65536.0).round();
    if value > i32::MAX as f64 || value < i32::MIN as f64 {
        return Err(AvifError::UnknownError(
            "Value out of range for s15.16".into(),
        ));
    }
    Ok((value as i32).to_be_bytes())
}

fn u8_fixed8(value: f32) -> AvifResult<[u8; 2]> {
    let value = (value * 256.0).round();
    if value > u16::MAX as f32 || value < 1.0 {
        return Err(AvifError::UnknownError(
            "Value out of range for u8.8".into(),
        ));
    }
    Ok((value as u16).to_be_bytes())
}

fn vec_offset_write(data: &mut [u8], offset: usize, data_to_write: &[u8]) {
    data[offset..offset + data_to_write.len()].copy_from_slice(data_to_write);
}

type Matrix3x3 = [[f64; 3]; 3];

fn matrix_inverse(m: &Matrix3x3) -> (Matrix3x3, bool) {
    let det = m[0][0] * (m[1][1] * m[2][2] - m[2][1] * m[1][2])
        - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0])
        + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    if det.abs() < SMALL {
        return ([[0.0; 3]; 3], false);
    }
    let inv_det = 1.0 / det;

    let mut inv = [[0.0; 3]; 3];
    inv[0][0] = (m[1][1] * m[2][2] - m[2][1] * m[1][2]) * inv_det;
    inv[0][1] = (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * inv_det;
    inv[0][2] = (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * inv_det;
    inv[1][0] = (m[1][2] * m[2][0] - m[1][0] * m[2][2]) * inv_det;
    inv[1][1] = (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * inv_det;
    inv[1][2] = (m[1][0] * m[0][2] - m[0][0] * m[1][2]) * inv_det;
    inv[2][0] = (m[1][0] * m[2][1] - m[2][0] * m[1][1]) * inv_det;
    inv[2][1] = (m[2][0] * m[0][1] - m[0][0] * m[2][1]) * inv_det;
    inv[2][2] = (m[0][0] * m[1][1] - m[1][0] * m[0][1]) * inv_det;

    (inv, true)
}

fn matrix_multiply(a: &Matrix3x3, b: &Matrix3x3) -> Matrix3x3 {
    let mut c = [[0.0; 3]; 3];
    for i in 0..3 {
        for j in 0..3 {
            c[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
        }
    }
    c
}

fn matrix_multiply_3x1(m: &Matrix3x3, v: &[f64; 3]) -> [f64; 3] {
    let mut result = [0.0; 3];
    for i in 0..3 {
        result[i] = m[i][0] * v[0] + m[i][1] * v[1] + m[i][2] * v[2];
    }
    result
}

fn matrix_diagonal(v: &[f64; 3]) -> Matrix3x3 {
    let mut m = [[0.0; 3]; 3];
    m[0][0] = v[0];
    m[1][1] = v[1];
    m[2][2] = v[2];
    m
}

fn matrix_transpose(m: &Matrix3x3) -> Matrix3x3 {
    let mut t = [[0.0; 3]; 3];
    for i in 0..3 {
        for j in 0..3 {
            t[i][j] = m[j][i];
        }
    }
    t
}

pub fn generate_icc(format: PixelFormat, gamma: f32, primaries: &[f32; 8]) -> AvifResult<Vec<u8>> {
    let mut icc;
    if format == PixelFormat::Yuv400 {
        icc = create_vec_exact(ICC_GRAY_TEMPLATE.len())?;
        icc.extend_from_slice(&ICC_GRAY_TEMPLATE);
        let mut offset = GRAY_WHITE_OFFSET;
        for white_xyz in xy_to_xyz(primaries[6], primaries[7])? {
            vec_offset_write(&mut icc, offset, &s15_fixed16(white_xyz)?);
            offset += 4;
        }
        vec_offset_write(&mut icc, GRAY_GAMMA_OFFSET, &u8_fixed8(gamma)?);
    } else {
        icc = create_vec_exact(ICC_COLOR_TEMPLATE.len())?;
        icc.extend_from_slice(&ICC_COLOR_TEMPLATE);
        let mut offset = COLOR_WHITE_OFFSET;
        let white_xyz = xy_to_xyz(primaries[6], primaries[7])?;
        for val in &white_xyz {
            vec_offset_write(&mut icc, offset, &s15_fixed16(*val)?);
            offset += 4;
        }

        let rgb_primaries: Matrix3x3 = [
            [
                primaries[0] as f64,
                primaries[2] as f64,
                primaries[4] as f64,
            ],
            [
                primaries[1] as f64,
                primaries[3] as f64,
                primaries[5] as f64,
            ],
            [
                1.0 - primaries[0] as f64 - primaries[1] as f64,
                1.0 - primaries[2] as f64 - primaries[3] as f64,
                1.0 - primaries[4] as f64 - primaries[5] as f64,
            ],
        ];

        let (rgb_primaries_inv, success) = matrix_inverse(&rgb_primaries);
        if !success {
            return Err(AvifError::UnknownError(
                "Matrix for primaries is not invertible".into(),
            ));
        }

        let rgb_coefficients = matrix_multiply_3x1(&rgb_primaries_inv, &white_xyz);
        let rgb_coefficients_mat = matrix_diagonal(&rgb_coefficients);
        let rgb_xyz = matrix_multiply(&rgb_primaries, &rgb_coefficients_mat);

        // ICC stores primaries XYZ under PCS.
        // Adapt using linear bradford transform
        let mut lms = matrix_multiply_3x1(&BRADFORD, &white_xyz);
        for i in 0..3 {
            if lms[i].abs() < SMALL {
                return Err(AvifError::UnknownError("LMS value too small".into()));
            }
            lms[i] = LMS_D50[i] / lms[i];
        }

        let adaptation_diag = matrix_diagonal(&lms);
        let tmp = matrix_multiply(&adaptation_diag, &BRADFORD);

        let (bradford_inv, success) = matrix_inverse(&BRADFORD);
        if !success {
            return Err(AvifError::UnknownError(
                "Bradford matrix is not invertible".into(),
            ));
        }
        let adaptation = matrix_multiply(&bradford_inv, &tmp);

        let rgb_xyz_d50 = matrix_multiply(&adaptation, &rgb_xyz);
        let rgb_xyz_d50_t = matrix_transpose(&rgb_xyz_d50);

        offset = COLOR_RED_OFFSET;
        for value in rgb_xyz_d50_t[0] {
            vec_offset_write(&mut icc, offset, &s15_fixed16(value)?);
            offset += 4;
        }

        offset = COLOR_GREEN_OFFSET;
        for value in rgb_xyz_d50_t[1] {
            vec_offset_write(&mut icc, offset, &s15_fixed16(value)?);
            offset += 4;
        }

        offset = COLOR_BLUE_OFFSET;
        for value in rgb_xyz_d50_t[2] {
            vec_offset_write(&mut icc, offset, &s15_fixed16(value)?);
            offset += 4;
        }

        vec_offset_write(&mut icc, COLOR_GAMMA_OFFSET, &u8_fixed8(gamma)?);
    }
    let hash = Md5::digest(&icc);
    vec_offset_write(&mut icc, CHECKSUM_OFFSET, &hash);
    Ok(icc)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hex_string_to_bytes() -> AvifResult<()> {
        let hex = b"01020304";
        let bytes = hex_string_to_bytes(hex, 4)?;
        assert_eq!(bytes, vec![1, 2, 3, 4]);

        let hex_with_newlines = b"0102\n0304\n";
        let bytes = hex_string_to_bytes(hex_with_newlines, 4)?;
        assert_eq!(bytes, vec![1, 2, 3, 4]);

        let truncated = b"010203";
        assert!(hex_string_to_bytes(truncated, 4).is_err());

        let invalid = b"01020G";
        assert!(hex_string_to_bytes(invalid, 3).is_err());

        Ok(())
    }

    #[test]
    fn test_copy_raw_profile() -> AvifResult<()> {
        // Valid profile: \n, name, \n, length, \n, hex
        let profile = b"\nICC\n4\n01020304";
        let bytes = copy_raw_profile(profile)?;
        assert_eq!(bytes, vec![1, 2, 3, 4]);

        // Malformed: missing leading \n
        let malformed = b"ICC\n1\n01";
        assert!(copy_raw_profile(malformed).is_err());

        // Malformed: invalid length
        let invalid_len = b"\nICC\nabc\n01";
        assert!(copy_raw_profile(invalid_len).is_err());

        Ok(())
    }

    #[test]
    fn test_xy_to_xyz() -> AvifResult<()> {
        let xyz = xy_to_xyz(0.3127, 0.3290)?; // D65
        assert!((xyz[0] - 0.95045).abs() < 0.001);
        assert!((xyz[1] - 1.0).abs() < 0.001);
        assert!((xyz[2] - 1.08905).abs() < 0.001);

        assert!(xy_to_xyz(0.3, 0.0).is_err());
        Ok(())
    }

    #[test]
    fn test_s15_fixed16() -> AvifResult<()> {
        assert_eq!(s15_fixed16(1.0)?, [0, 1, 0, 0]);
        assert_eq!(s15_fixed16(0.5)?, [0, 0, 128, 0]);
        assert_eq!(s15_fixed16(-1.0)?, [255, 255, 0, 0]);
        assert!(s15_fixed16(40000.0).is_err());
        Ok(())
    }

    #[test]
    fn test_u8_fixed8() -> AvifResult<()> {
        assert_eq!(u8_fixed8(1.0)?, [1, 0]);
        assert_eq!(u8_fixed8(2.2)?, [2, 51]);
        assert!(u8_fixed8(300.0).is_err());
        assert!(u8_fixed8(0.0).is_err());
        Ok(())
    }

    #[test]
    fn test_matrix_inverse() {
        let m: Matrix3x3 = [[1.0, 2.0, 3.0], [0.0, 1.0, 4.0], [5.0, 6.0, 0.0]];
        let (inv, success) = matrix_inverse(&m);
        assert!(success);
        let identity = matrix_multiply(&m, &inv);
        for (i, row) in identity.iter().enumerate() {
            for (j, value) in row.iter().enumerate() {
                let expected = if i == j { 1.0 } else { 0.0 };
                assert!((value - expected).abs() < 1e-10);
            }
        }

        let non_invertible: Matrix3x3 = [[1.0, 1.0, 1.0], [1.0, 1.0, 1.0], [1.0, 1.0, 1.0]];
        let (_, success) = matrix_inverse(&non_invertible);
        assert!(!success);
    }

    #[test]
    fn test_generate_icc() -> AvifResult<()> {
        let primaries = [
            0.64, 0.33, // R
            0.30, 0.60, // G
            0.15, 0.06, // B
            0.3127, 0.3290, // W (D65)
        ];

        let icc_color = generate_icc(PixelFormat::Yuv444, 2.2, &primaries)?;
        assert_eq!(icc_color.len(), 376);

        let icc_gray = generate_icc(PixelFormat::Yuv400, 2.2, &primaries)?;
        assert_eq!(icc_gray.len(), 275);

        Ok(())
    }
}
