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

use crate::*;

fn expand_coeffs(y: f32, v: f32) -> [f32; 3] {
    [y, 1.0 - y - v, v]
}

#[cfg(feature = "png")]
fn matches_f32(a: f32, b: f32) -> bool {
    (a - b).abs() <= 0.001
}

impl ColorPrimaries {
    pub(crate) fn y_coeffs(&self) -> [f32; 3] {
        // These values come from computations in Section 8 of
        // https://www.itu.int/rec/T-REC-H.273-201612-S
        match self {
            ColorPrimaries::Unknown | ColorPrimaries::Srgb | ColorPrimaries::Unspecified => {
                expand_coeffs(0.2126, 0.0722)
            }
            ColorPrimaries::Bt470m => expand_coeffs(0.299, 0.1146),
            ColorPrimaries::Bt470bg => expand_coeffs(0.222, 0.0713),
            ColorPrimaries::Bt601 | ColorPrimaries::Smpte240 => expand_coeffs(0.212, 0.087),
            ColorPrimaries::GenericFilm => expand_coeffs(0.2536, 0.06808),
            ColorPrimaries::Bt2020 => expand_coeffs(0.2627, 0.0593),
            ColorPrimaries::Xyz => expand_coeffs(0.0, 0.0),
            ColorPrimaries::Smpte431 => expand_coeffs(0.2095, 0.0689),
            ColorPrimaries::Smpte432 => expand_coeffs(0.229, 0.0793),
            ColorPrimaries::Ebu3213 => expand_coeffs(0.2318, 0.096),
        }
    }

    #[cfg(feature = "png")]
    pub(crate) fn values(&self) -> Option<[f32; 8]> {
        // return values in this order: rX, rY, gX, gY, bX, bY, wX, wY
        match self {
            Self::Srgb => Some([0.64, 0.33, 0.3, 0.6, 0.15, 0.06, 0.3127, 0.329]),
            Self::Bt470m => Some([0.67, 0.33, 0.21, 0.71, 0.14, 0.08, 0.310, 0.316]),
            Self::Bt470bg => Some([0.64, 0.33, 0.29, 0.60, 0.15, 0.06, 0.3127, 0.3290]),
            Self::Bt601 => Some([0.630, 0.340, 0.310, 0.595, 0.155, 0.070, 0.3127, 0.3290]),
            Self::Smpte240 => Some([0.630, 0.340, 0.310, 0.595, 0.155, 0.070, 0.3127, 0.3290]),
            Self::GenericFilm => Some([0.681, 0.319, 0.243, 0.692, 0.145, 0.049, 0.310, 0.316]),
            Self::Bt2020 => Some([0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290]),
            Self::Xyz => Some([1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.3333, 0.3333]),
            Self::Smpte431 => Some([0.680, 0.320, 0.265, 0.690, 0.150, 0.060, 0.314, 0.351]),
            Self::Smpte432 => Some([0.680, 0.320, 0.265, 0.690, 0.150, 0.060, 0.3127, 0.3290]),
            Self::Ebu3213 => Some([0.630, 0.340, 0.295, 0.605, 0.155, 0.077, 0.3127, 0.3290]),
            _ => None,
        }
    }

    #[cfg(feature = "png")]
    pub(crate) fn find(primaries: &[f32; 8]) -> Self {
        for i in 0u16..22 {
            let color_primary: Self = i.into();
            match color_primary.values() {
                Some(values) => {
                    if values
                        .iter()
                        .zip(primaries.iter())
                        .all(|(a, b)| matches_f32(*a, *b))
                    {
                        return color_primary;
                    }
                }
                None => continue,
            }
        }
        Self::Unknown
    }
}

impl TransferCharacteristics {
    #[cfg(feature = "png")]
    pub(crate) fn gamma(&self) -> Option<f32> {
        match self {
            Self::Bt470m => Some(2.2),
            Self::Bt470bg => Some(2.8),
            Self::Linear => Some(1.0),
            _ => None, // Not representable as a single gamma value.
        }
    }

    #[cfg(feature = "png")]
    pub(crate) fn from_gamma(gamma: f32) -> Self {
        if matches_f32(gamma, 2.2) {
            Self::Bt470m
        } else if matches_f32(gamma, 2.8) {
            Self::Bt470bg
        } else if matches_f32(gamma, 1.0) {
            Self::Linear
        } else {
            Self::Unknown
        }
    }
}

fn calculate_yuv_coefficients_from_cicp(
    color_primaries: ColorPrimaries,
    matrix_coefficients: MatrixCoefficients,
) -> Option<[f32; 3]> {
    match matrix_coefficients {
        MatrixCoefficients::ChromaDerivedNcl => Some(color_primaries.y_coeffs()),
        MatrixCoefficients::Bt709 => Some(expand_coeffs(0.2126, 0.0722)),
        MatrixCoefficients::Fcc => Some(expand_coeffs(0.30, 0.11)),
        MatrixCoefficients::Bt470bg | MatrixCoefficients::Bt601 => {
            Some(expand_coeffs(0.299, 0.114))
        }
        MatrixCoefficients::Smpte240 => Some(expand_coeffs(0.212, 0.087)),
        MatrixCoefficients::Bt2020Ncl => Some(expand_coeffs(0.2627, 0.0593)),
        _ => None,
    }
}

pub(crate) fn calculate_yuv_coefficients(
    color_primaries: ColorPrimaries,
    matrix_coefficients: MatrixCoefficients,
) -> [f32; 3] {
    // Return known coefficients or fall back to BT.601.
    calculate_yuv_coefficients_from_cicp(color_primaries, matrix_coefficients).unwrap_or(
        calculate_yuv_coefficients_from_cicp(color_primaries, MatrixCoefficients::Bt601).unwrap(),
    )
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::internal_utils::assert_eq_f32_array;

    #[test]
    fn yuv_coefficients() {
        assert_eq_f32_array(
            &calculate_yuv_coefficients(ColorPrimaries::Unknown, MatrixCoefficients::Bt601),
            &[0.299f32, 0.587f32, 0.114f32], // Kr,Kg,Kb as https://en.wikipedia.org/wiki/YCbCr#ITU-R_BT.601_conversion
        );
        assert_eq_f32_array(
            &calculate_yuv_coefficients(ColorPrimaries::Unknown, MatrixCoefficients::Unspecified),
            &[0.299f32, 0.587f32, 0.114f32], // Falls back to Bt601.
        );
        assert_eq_f32_array(
            &calculate_yuv_coefficients(ColorPrimaries::Unknown, MatrixCoefficients::Smpte240),
            &[0.212f32, 1f32 - 0.212 - 0.087, 0.087f32], // Kr,Kg,Kb as https://en.wikipedia.org/wiki/YCbCr#SMPTE_240M_conversion
        );
    }
}
