use crate::decoder::Image;
use crate::internal_utils::*;
use crate::parser::mp4box::ContentLightLevelInformation;
use crate::*;

#[derive(Debug, Default)]
pub struct GainMapMetadata {
    pub min: [Fraction; 3],
    pub max: [Fraction; 3],
    pub gamma: [UFraction; 3],
    pub base_offset: [Fraction; 3],
    pub alternate_offset: [Fraction; 3],
    pub base_hdr_headroom: UFraction,
    pub alternate_hdr_headroom: UFraction,
    pub backward_direction: bool,
    pub use_base_color_space: bool,
}

#[derive(Default)]
pub struct GainMap {
    pub image: Image,
    pub metadata: GainMapMetadata,

    pub alt_icc: Vec<u8>,
    pub alt_color_primaries: ColorPrimaries,
    pub alt_transfer_characteristics: TransferCharacteristics,
    pub alt_matrix_coefficients: MatrixCoefficients,
    pub alt_full_range: bool,

    pub alt_plane_count: u8,
    pub alt_plane_depth: u8,

    pub alt_clli: ContentLightLevelInformation,
}
