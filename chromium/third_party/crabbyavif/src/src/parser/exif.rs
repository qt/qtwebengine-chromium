use crate::internal_utils::stream::*;
use crate::*;

fn parse_exif_tiff_header_offset(stream: &mut IStream) -> AvifResult<u32> {
    const TIFF_HEADER_BE: u32 = 0x4D4D002A; // MM0* (read as a big endian u32)
    const TIFF_HEADER_LE: u32 = 0x49492A00; // II*0 (read as a big endian u32)
    let mut expected_offset: u32 = 0;
    let mut size = u32::try_from(stream.bytes_left()?).unwrap_or(u32::MAX);
    while size > 0 {
        let value = stream.read_u32().or(Err(AvifError::InvalidExifPayload))?;
        if value == TIFF_HEADER_BE || value == TIFF_HEADER_LE {
            stream.rewind(4)?;
            return Ok(expected_offset);
        }
        size -= 4;
        expected_offset += 4;
    }
    // Could not find the TIFF header.
    Err(AvifError::InvalidExifPayload)
}

pub fn parse(stream: &mut IStream) -> AvifResult<()> {
    // unsigned int(32) exif_tiff_header_offset;
    let offset = stream.read_u32().or(Err(AvifError::InvalidExifPayload))?;

    let expected_offset = parse_exif_tiff_header_offset(stream)?;
    if offset != expected_offset {
        return Err(AvifError::InvalidExifPayload);
    }
    Ok(())
}
