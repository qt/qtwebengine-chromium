#[cfg(feature = "libyuv")]
pub mod libyuv;
#[cfg(feature = "libyuv")]
pub mod scale;

pub mod alpha;
pub mod coeffs;
pub mod rgb;
pub mod rgb_impl;

// If libyuv is not present, add placeholder functions so that the library will build successfully
// without it.
#[cfg(not(feature = "libyuv"))]
pub mod libyuv {
    use crate::decoder::Category;
    use crate::reformat::*;
    use crate::*;

    pub fn yuv_to_rgb(_image: &image::Image, _rgb: &mut rgb::Image) -> AvifResult<bool> {
        Err(AvifError::NotImplemented)
    }

    pub fn convert_to_half_float(_rgb: &mut rgb::Image, _scale: f32) -> AvifResult<()> {
        Err(AvifError::NotImplemented)
    }

    impl image::Image {
        pub fn scale(&mut self, width: u32, height: u32, _category: Category) -> AvifResult<()> {
            if self.width == width && self.height == height {
                return Ok(());
            }
            Err(AvifError::NotImplemented)
        }
    }
}
