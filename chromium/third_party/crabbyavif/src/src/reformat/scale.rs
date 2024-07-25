use crate::decoder::Category;
use crate::image::*;
use crate::internal_utils::*;
use crate::*;

use libyuv_sys::bindings::*;

impl Image {
    pub fn scale(&mut self, width: u32, height: u32, category: Category) -> AvifResult<()> {
        if self.width == width && self.height == height {
            return Ok(());
        }
        if width == 0 || height == 0 {
            return Err(AvifError::InvalidArgument);
        }
        let planes: &[Plane] = match category {
            Category::Color | Category::Gainmap => &YUV_PLANES,
            Category::Alpha => &A_PLANE,
        };
        for plane in planes {
            if self.planes[plane.to_usize()].is_some()
                && !self.planes[plane.to_usize()].unwrap_ref().is_pointer()
            {
                // TODO: implement this function for non-pointer inputs.
                return Err(AvifError::NotImplemented);
            }
        }
        let src = image::Image {
            width: self.width,
            height: self.height,
            depth: self.depth,
            yuv_format: self.yuv_format,
            planes: [
                if self.planes[0].is_some() {
                    self.planes[0].unwrap_ref().clone_pointer()
                } else {
                    None
                },
                if self.planes[1].is_some() {
                    self.planes[1].unwrap_ref().clone_pointer()
                } else {
                    None
                },
                if self.planes[2].is_some() {
                    self.planes[2].unwrap_ref().clone_pointer()
                } else {
                    None
                },
                if self.planes[3].is_some() {
                    self.planes[3].unwrap_ref().clone_pointer()
                } else {
                    None
                },
            ],
            row_bytes: self.row_bytes,
            ..image::Image::default()
        };

        self.width = width;
        self.height = height;
        if src.has_plane(Plane::Y) || src.has_plane(Plane::A) {
            if src.width > 16384 || src.height > 16384 {
                return Err(AvifError::NotImplemented);
            }
            if src.has_plane(Plane::Y) && category != Category::Alpha {
                self.allocate_planes(Category::Color)?;
            }
            if src.has_plane(Plane::A) && category == Category::Alpha {
                self.allocate_planes(Category::Alpha)?;
            }
        }
        for plane in planes {
            if !src.has_plane(*plane) {
                continue;
            }
            let src_pd = src.plane_data(*plane).unwrap();
            let dst_pd = self.plane_data(*plane).unwrap();
            // libyuv versions >= 1880 reports a return value here. Older versions do not. Ignore
            // the return value for now.
            #[allow(clippy::let_unit_value)]
            let _ret = unsafe {
                if src.depth > 8 {
                    let source_ptr = src.planes[plane.to_usize()].unwrap_ref().ptr16();
                    let dst_ptr = self.planes[plane.to_usize()].unwrap_mut().ptr16_mut();
                    ScalePlane_12(
                        source_ptr,
                        i32_from_u32(src_pd.row_bytes / 2)?,
                        i32_from_u32(src_pd.width)?,
                        i32_from_u32(src_pd.height)?,
                        dst_ptr,
                        i32_from_u32(dst_pd.row_bytes / 2)?,
                        i32_from_u32(dst_pd.width)?,
                        i32_from_u32(dst_pd.height)?,
                        FilterMode_kFilterBox,
                    )
                } else {
                    let source_ptr = src.planes[plane.to_usize()].unwrap_ref().ptr();
                    let dst_ptr = self.planes[plane.to_usize()].unwrap_mut().ptr_mut();
                    ScalePlane(
                        source_ptr,
                        i32_from_u32(src_pd.row_bytes)?,
                        i32_from_u32(src_pd.width)?,
                        i32_from_u32(src_pd.height)?,
                        dst_ptr,
                        i32_from_u32(dst_pd.row_bytes)?,
                        i32_from_u32(dst_pd.width)?,
                        i32_from_u32(dst_pd.height)?,
                        FilterMode_kFilterBox,
                    )
                }
            };
        }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::internal_utils::pixels::Pixels;
    use test_case::test_matrix;

    #[test_matrix([PixelFormat::Yuv444, PixelFormat::Yuv422, PixelFormat::Yuv420, PixelFormat::Monochrome], [false, true])]
    fn scale_pointer(yuv_format: PixelFormat, use_alpha: bool) {
        let mut yuv = image::Image {
            width: 2,
            height: 2,
            depth: 8,
            yuv_format,
            ..Default::default()
        };

        let planes: &[Plane] = match (yuv_format, use_alpha) {
            (PixelFormat::Monochrome, false) => &[Plane::Y],
            (PixelFormat::Monochrome, true) => &[Plane::Y, Plane::A],
            (_, false) => &YUV_PLANES,
            (_, true) => &ALL_PLANES,
        };
        let mut values = [
            10, 20, //
            30, 40,
        ];
        for plane in planes {
            yuv.planes[plane.to_usize()] = Some(Pixels::Pointer(values.as_mut_ptr()));
            yuv.row_bytes[plane.to_usize()] = 2;
            yuv.image_owns_planes[plane.to_usize()] = true;
        }
        let categories: &[Category] =
            if use_alpha { &[Category::Color, Category::Alpha] } else { &[Category::Color] };
        for category in categories {
            // Scale will update the width and height when scaling YUV planes. Reset it back before
            // calling it again.
            yuv.width = 2;
            yuv.height = 2;
            assert!(yuv.scale(4, 4, *category).is_ok());
        }
        for plane in planes {
            let expected_samples: &[u8] = match (yuv_format, plane) {
                (PixelFormat::Yuv422, Plane::U | Plane::V) => &[
                    10, 10, //
                    10, 10, //
                    30, 30, //
                    30, 30,
                ],
                (PixelFormat::Yuv420, Plane::U | Plane::V) => &[
                    10, 10, //
                    10, 10,
                ],
                (_, _) => &[
                    10, 13, 18, 20, //
                    15, 18, 23, 25, //
                    25, 28, 33, 35, //
                    30, 33, 38, 40,
                ],
            };
            match &yuv.planes[plane.to_usize()] {
                Some(Pixels::Buffer(samples)) => {
                    assert_eq!(*samples, expected_samples)
                }
                _ => panic!(),
            }
        }
    }

    #[test]
    fn scale_buffer() {
        let mut yuv = image::Image {
            width: 2,
            height: 2,
            depth: 8,
            yuv_format: PixelFormat::Monochrome,
            ..Default::default()
        };

        yuv.planes[0] = Some(Pixels::Buffer(vec![
            1, 2, //
            3, 4,
        ]));
        assert_eq!(
            yuv.scale(4, 4, Category::Color),
            Err(AvifError::NotImplemented)
        );
    }
}
