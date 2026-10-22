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

use crate::gainmap::GainMapMetadata;
use crate::utils::Fraction;
use crate::utils::UFraction;
use crate::AvifResult;
use std::io::Cursor;
use xml::reader::{EventReader, XmlEvent as ReadXmlEvent};

const XML_NAME_SPACE_RDF: &str = "http://www.w3.org/1999/02/22-rdf-syntax-ns#";
const XML_NAME_SPACE_GAIN_MAP: &str = "http://ns.adobe.com/hdr-gain-map/1.0/";
const XML_NAME_SPACE_APPLE_GAIN_MAP: &str = "http://ns.apple.com/HDRGainMap/1.0/";

/// Return value is a tuple of the form (GainMapMetadata, is_apple_gainmap).
pub(crate) fn parse_gainmap_metadata(xmp_data: &[u8]) -> AvifResult<(GainMapMetadata, bool)> {
    let reader = EventReader::new(Cursor::new(xmp_data));

    // Default values for Adobe HDR gainmap.
    let mut metadata = GainMapMetadata {
        min: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
        max: [Fraction(1, 1), Fraction(1, 1), Fraction(1, 1)],
        gamma: [UFraction(1, 1), UFraction(1, 1), UFraction(1, 1)],
        base_offset: [Fraction(1, 64), Fraction(1, 64), Fraction(1, 64)],
        alternate_offset: [Fraction(1, 64), Fraction(1, 64), Fraction(1, 64)],
        base_hdr_headroom: UFraction(0, 1),
        alternate_hdr_headroom: UFraction(1, 1),
        use_base_color_space: true,
    };
    let apple_default = GainMapMetadata {
        base_offset: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
        alternate_offset: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
        alternate_hdr_headroom: UFraction(0, 1),
        ..metadata
    };

    let mut inside_rdf = false;
    let mut is_apple = false;
    let mut base_rendition_is_hdr = false;
    let mut current_prop: Option<String> = None;
    let mut in_li = false;
    let mut li_index = 0;
    for event in reader {
        match event {
            Ok(ReadXmlEvent::StartElement {
                name, attributes, ..
            }) => {
                if name.local_name == "RDF" && name.namespace.as_deref() == Some(XML_NAME_SPACE_RDF)
                {
                    inside_rdf = true;
                    continue;
                }
                if !inside_rdf {
                    continue;
                }
                if name.local_name == "Description"
                    && name.namespace.as_deref() == Some(XML_NAME_SPACE_RDF)
                {
                    for attr in &attributes {
                        if attr.name.namespace.as_deref() == Some(XML_NAME_SPACE_GAIN_MAP) {
                            match attr.name.local_name.as_str() {
                                "HDRCapacityMin" => {
                                    metadata.base_hdr_headroom =
                                        UFraction::from(attr.value.parse::<f64>().unwrap_or(0.0));
                                }
                                "HDRCapacityMax" => {
                                    metadata.alternate_hdr_headroom =
                                        UFraction::from(attr.value.parse::<f64>().unwrap_or(1.0));
                                }
                                "GainMapMin" => {
                                    let val = attr.value.parse::<f64>().unwrap_or(0.0);
                                    for i in 0..3 {
                                        metadata.min[i] = Fraction::from(val);
                                    }
                                }
                                "GainMapMax" => {
                                    let val = attr.value.parse::<f64>().unwrap_or(1.0);
                                    for i in 0..3 {
                                        metadata.max[i] = Fraction::from(val);
                                    }
                                }
                                "Gamma" => {
                                    let val = attr.value.parse::<f64>().unwrap_or(1.0);
                                    for i in 0..3 {
                                        metadata.gamma[i] = UFraction::from(val);
                                    }
                                }
                                "OffsetSDR" => {
                                    let val = attr.value.parse::<f64>().unwrap_or(1.0 / 64.0);
                                    for i in 0..3 {
                                        metadata.base_offset[i] = Fraction::from(val);
                                    }
                                }
                                "OffsetHDR" => {
                                    let val = attr.value.parse::<f64>().unwrap_or(1.0 / 64.0);
                                    for i in 0..3 {
                                        metadata.alternate_offset[i] = Fraction::from(val);
                                    }
                                }
                                "BaseRenditionIsHDR" => {
                                    base_rendition_is_hdr = attr.value.to_lowercase() == "true";
                                }
                                _ => {}
                            }
                        } else if attr.name.namespace.as_deref()
                            == Some(XML_NAME_SPACE_APPLE_GAIN_MAP)
                        {
                            is_apple = true;
                            metadata = apple_default.clone();
                        }
                    }
                } else if name.namespace.as_deref() == Some(XML_NAME_SPACE_GAIN_MAP)
                    || name.namespace.as_deref() == Some(XML_NAME_SPACE_APPLE_GAIN_MAP)
                {
                    current_prop = Some(name.local_name.clone());
                    if name.namespace.as_deref() == Some(XML_NAME_SPACE_APPLE_GAIN_MAP) {
                        if !is_apple {
                            metadata = apple_default.clone();
                        }
                        is_apple = true;
                    }
                    li_index = 0;
                } else if name.local_name == "li"
                    && name.namespace.as_deref() == Some(XML_NAME_SPACE_RDF)
                {
                    in_li = true;
                }
            }
            Ok(ReadXmlEvent::Characters(s)) => {
                if let Some(ref prop) = current_prop {
                    let val = s.parse::<f64>().unwrap_or(0.0);
                    match prop.as_str() {
                        "HDRCapacityMin" => {
                            metadata.base_hdr_headroom = UFraction::from(val);
                        }
                        "HDRCapacityMax" | "HDRGainMapHeadroom" => {
                            let headroom = if val > 0.0 { val.log2() } else { 0.0 };
                            let fraction = Fraction::from(headroom);
                            if prop == "HDRGainMapHeadroom" {
                                for i in 0..3 {
                                    metadata.min[i] = Fraction(0, 1);
                                    metadata.max[i] = fraction;
                                    metadata.gamma[i] = UFraction(1, 1);
                                    metadata.base_offset[i] = Fraction(0, 1);
                                    metadata.alternate_offset[i] = Fraction(0, 1);
                                }
                                metadata.base_hdr_headroom = UFraction(0, 1);
                                metadata.alternate_hdr_headroom = UFraction::from(headroom);
                            } else {
                                metadata.alternate_hdr_headroom = UFraction::from(val);
                            }
                        }
                        "GainMapMin" if in_li && li_index < 3 => {
                            metadata.min[li_index] = Fraction::from(val);
                        }
                        "GainMapMin" if !in_li => {
                            for i in 0..3 {
                                metadata.min[i] = Fraction::from(val);
                            }
                        }
                        "GainMapMax" if in_li && li_index < 3 => {
                            metadata.max[li_index] = Fraction::from(val);
                        }
                        "GainMapMax" if !in_li => {
                            for i in 0..3 {
                                metadata.max[i] = Fraction::from(val);
                            }
                        }
                        "Gamma" if in_li && li_index < 3 => {
                            metadata.gamma[li_index] = UFraction::from(val);
                        }
                        "Gamma" if !in_li => {
                            for i in 0..3 {
                                metadata.gamma[i] = UFraction::from(val);
                            }
                        }
                        "OffsetSDR" if in_li && li_index < 3 => {
                            metadata.base_offset[li_index] = Fraction::from(val);
                        }
                        "OffsetSDR" if !in_li => {
                            for i in 0..3 {
                                metadata.base_offset[i] = Fraction::from(val);
                            }
                        }
                        "OffsetHDR" if in_li && li_index < 3 => {
                            metadata.alternate_offset[li_index] = Fraction::from(val);
                        }
                        "OffsetHDR" if !in_li => {
                            for i in 0..3 {
                                metadata.alternate_offset[i] = Fraction::from(val);
                            }
                        }
                        "BaseRenditionIsHDR" => {
                            base_rendition_is_hdr = s.to_lowercase() == "true";
                        }
                        _ => {}
                    }
                }
            }
            Ok(ReadXmlEvent::EndElement { name }) => {
                if name.local_name == "RDF" && name.namespace.as_deref() == Some(XML_NAME_SPACE_RDF)
                {
                    inside_rdf = false;
                } else if name.namespace.as_deref() == Some(XML_NAME_SPACE_GAIN_MAP)
                    || name.namespace.as_deref() == Some(XML_NAME_SPACE_APPLE_GAIN_MAP)
                {
                    current_prop = None;
                } else if name.local_name == "li"
                    && name.namespace.as_deref() == Some(XML_NAME_SPACE_RDF)
                {
                    in_li = false;
                    li_index += 1;
                }
            }
            _ => {}
        }
    }
    if base_rendition_is_hdr && !is_apple {
        std::mem::swap(
            &mut metadata.base_hdr_headroom,
            &mut metadata.alternate_hdr_headroom,
        );
        for i in 0..3 {
            std::mem::swap(
                &mut metadata.base_offset[i],
                &mut metadata.alternate_offset[i],
            );
        }
    }
    Ok((metadata, is_apple))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn test_data(index: usize) -> (String, GainMapMetadata) {
        let data = [
            // Apple gainmap.
            (
                String::from(
                    r#"
<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 6.0.0">
   <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
      <rdf:Description rdf:about=""
            xmlns:apdi="http://ns.apple.com/pixeldatainfo/1.0/"
            xmlns:HDRGainMap="http://ns.apple.com/HDRGainMap/1.0/">
         <apdi:NativeFormat>1278226488</apdi:NativeFormat>
         <apdi:AuxiliaryImageType>urn:com:apple:photo:2020:aux:hdrgainmap</apdi:AuxiliaryImageType>
         <apdi:StoredFormat>1278226488</apdi:StoredFormat>
         <HDRGainMap:HDRGainMapVersion>131072</HDRGainMap:HDRGainMapVersion>
         <HDRGainMap:HDRGainMapHeadroom>4.532783</HDRGainMap:HDRGainMapHeadroom>
      </rdf:Description>
   </rdf:RDF>
</x:xmpmeta>"#,
                ),
                GainMapMetadata {
                    min: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
                    max: [
                        Fraction::from(4.532783f64.log2()),
                        Fraction::from(4.532783f64.log2()),
                        Fraction::from(4.532783f64.log2()),
                    ],
                    gamma: [UFraction(1, 1), UFraction(1, 1), UFraction(1, 1)],
                    base_offset: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
                    alternate_offset: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
                    base_hdr_headroom: UFraction(0, 1),
                    alternate_hdr_headroom: UFraction::from(4.532783f64.log2()),
                    use_base_color_space: true,
                },
            ),
            // Adobe gainmap.
            (
                String::from(
                    r#"
<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="XMP Core 5.5.0">
   <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
      <rdf:Description xmlns:hdrgm="http://ns.adobe.com/hdr-gain-map/1.0/"
            hdrgm:Version="1.0"
            hdrgm:HDRCapacityMin="0"
            hdrgm:HDRCapacityMax="3.5"
            hdrgm:OffsetHDR="0"
            hdrgm:OffsetSDR="0">
         <hdrgm:GainMapMax>
            <rdf:Seq>
               <rdf:li>3.5</rdf:li>
               <rdf:li>3.6</rdf:li>
               <rdf:li>3.7</rdf:li>
            </rdf:Seq>
         </hdrgm:GainMapMax>
      </rdf:Description>
   </rdf:RDF>
</x:xmpmeta>"#,
                ),
                GainMapMetadata {
                    min: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
                    max: [
                        Fraction::from(3.5),
                        Fraction::from(3.6),
                        Fraction::from(3.7),
                    ],
                    gamma: [UFraction(1, 1), UFraction(1, 1), UFraction(1, 1)],
                    base_offset: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
                    alternate_offset: [Fraction(0, 1), Fraction(0, 1), Fraction(0, 1)],
                    base_hdr_headroom: UFraction(0, 1),
                    alternate_hdr_headroom: UFraction::from(3.5),
                    use_base_color_space: true,
                },
            ),
            // Adobe gainmap with Seq.
            (
                String::from(
                    r#"
<x:xmpmeta xmlns:x="adobe:ns:meta/" x:xmptk="Adobe XMP Core 7.0-c000 1.000000, 0000/00/00-00:00:00        ">
 <rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">
  <rdf:Description rdf:about=""
    xmlns:hdrgm="http://ns.adobe.com/hdr-gain-map/1.0/"
   hdrgm:Version="1.0"
   hdrgm:BaseRenditionIsHDR="False"
   hdrgm:OffsetSDR="0.015625"
   hdrgm:OffsetHDR="0.015625"
   hdrgm:HDRCapacityMin="0"
   hdrgm:HDRCapacityMax="1.3">
   <hdrgm:GainMapMin>
    <rdf:Seq>
     <rdf:li>-0.256907</rdf:li>
     <rdf:li>-0.261365</rdf:li>
     <rdf:li>-0.280284</rdf:li>
    </rdf:Seq>
   </hdrgm:GainMapMin>
   <hdrgm:GainMapMax>
    <rdf:Seq>
     <rdf:li>1.277177</rdf:li>
     <rdf:li>1.277203</rdf:li>
     <rdf:li>1.277969</rdf:li>
    </rdf:Seq>
   </hdrgm:GainMapMax>
   <hdrgm:Gamma>
    <rdf:Seq>
     <rdf:li>0.953784</rdf:li>
     <rdf:li>0.941095</rdf:li>
     <rdf:li>0.919422</rdf:li>
    </rdf:Seq>
   </hdrgm:Gamma>
  </rdf:Description>
 </rdf:RDF>
</x:xmpmeta>"#,
                ),
                GainMapMetadata {
                    min: [
                        Fraction::from(-0.256907),
                        Fraction::from(-0.261365),
                        Fraction::from(-0.280284),
                    ],
                    max: [
                        Fraction::from(1.277177),
                        Fraction::from(1.277203),
                        Fraction::from(1.277969),
                    ],
                    gamma: [
                        UFraction::from(0.953784),
                        UFraction::from(0.941095),
                        UFraction::from(0.919422),
                    ],
                    base_offset: [
                        Fraction::from(0.015625),
                        Fraction::from(0.015625),
                        Fraction::from(0.015625),
                    ],
                    alternate_offset: [Fraction(1, 64), Fraction(1, 64), Fraction(1, 64)],
                    base_hdr_headroom: UFraction(0, 1),
                    alternate_hdr_headroom: UFraction::from(1.3),
                    use_base_color_space: true,
                },
            ),
        ];
        data[index].clone()
    }

    #[allow(clippy::zero_prefixed_literal)]
    #[test_case::test_matrix(0usize..3)]
    fn parse_gainmap_metadata(index: usize) -> AvifResult<()> {
        let (xmp_data, expected_metadata) = test_data(index);
        assert_eq!(
            super::parse_gainmap_metadata(xmp_data.as_bytes())?.0,
            expected_metadata
        );
        Ok(())
    }
}
