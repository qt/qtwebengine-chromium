// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Based on https://github.com/googlefonts/fontations/pull/1820

#![allow(dead_code)]
use read_fonts::{
    model::pen::OutlinePen,
    ps::{
        cff::{
            charset::Charset as CffCharset, CffFontRef, Encoding as CffEncoding,
            Metadata as CffMetadata, Subfont as CffSubfont,
        },
        charmap::Charmap as PsCharmap,
        encoding::PredefinedEncoding,
        string::Sid,
        type1::Type1Font,
    },
    types::GlyphId,
    TableProvider,
};
use skrifa::MetadataProvider;

#[cxx::bridge(namespace = "skrifa")]
mod skrifa_ffi {
    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub enum PsEncodingKind {
        None = 0,
        Standard = 1,
        Expert = 2,
        IsoLatin1 = 3,
        Custom = 4,
    }

    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub enum FsType {
        RestrictedLicenseEmbedding = 0x0002,
        BitmapEmbeddingOnly = 0x0200,
    }

    // Should match SkPathVerb
    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    #[repr(u8)]
    pub enum PathVerb {
        MoveTo = 0,
        LineTo = 1,
        QuadTo = 2,
        CurveTo = 4,
        Close = 5,
    }

    #[derive(Copy, Clone, PartialEq, Debug)]
    pub struct Point {
        pub x: f32,
        pub y: f32,
    }

    #[derive(Copy, Clone, PartialEq, Debug)]
    pub struct BoundingBox {
        pub x_min: f32,
        pub y_min: f32,
        pub x_max: f32,
        pub y_max: f32,
    }

    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub struct CodePageRange {
        pub range1: u32,
        pub range2: u32,
    }

    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub struct Os2Panose {
        pub b0: u8,
        pub b1: u8,
    }

    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub struct UnicodeRange {
        pub range1: u32,
        pub range2: u32,
        pub range3: u32,
        pub range4: u32,
    }

    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub struct CharCodeAndIndex {
        pub char_code: u32,
        pub glyph_index: u32,
    }

    #[derive(Clone, Debug)]
    pub struct Outline {
        pub verbs: Vec<PathVerb>,
        pub points: Vec<Point>,
        pub advance_width: f32,
    }

    extern "Rust" {
        type PsFont<'a>;
        unsafe fn new_ps_font<'a>(data: &'a [u8]) -> Box<PsFont<'a>>;
        fn is_ok(&self) -> bool;
        unsafe fn name<'a>(&'a self) -> &'a str;
        unsafe fn family_name<'a>(&'a self) -> &'a str;
        fn units_per_em(&self) -> i32;
        fn ascent(&self) -> f32;
        fn descent(&self) -> f32;
        fn num_glyphs(&self) -> u32;
        fn is_cid(&self) -> bool;
        fn cid_to_gid(&self, cid: u16) -> u32;
        fn unicode_to_gid(&self, unicode: u32) -> u32;
        fn encoding(&self) -> PsEncodingKind;
        fn code_to_gid(&self, code: u8) -> u32;
        fn scaled_outline(&self, gid: u32, ppem: f32, outline: &mut Outline) -> bool;
        fn unscaled_outline(&self, gid: u32, outline: &mut Outline) -> bool;
        fn get_os2_code_page_range(data: &[u8], range: &mut CodePageRange) -> bool;
        fn get_os2_panose(data: &[u8], panose: &mut Os2Panose) -> bool;
        fn get_os2_fs_type(data: &[u8], fs_type: &mut u16) -> bool;
        fn get_os2_unicode_range(data: &[u8], range: &mut UnicodeRange) -> bool;
        fn get_style_name(data: &[u8]) -> String;
        fn get_glyph_name(data: &[u8], gid: u32) -> String;
        fn get_name_index(data: &[u8], name: &str) -> u32;

        fn agl_name_to_unicode(name: &str, unicode: &mut u32) -> bool;
        fn agl_unicode_to_name(unicode: u32, name: &mut [u8]) -> bool;

        fn get_char_codes_and_indices(data: &[u8], max_char: u32) -> Vec<CharCodeAndIndex>;
        fn has_glyph_names(data: &[u8]) -> bool;
        fn is_fixed_pitch(data: &[u8]) -> bool;
        fn is_scalable(data: &[u8]) -> bool;
        fn get_font_format(data: &[u8]) -> String;
        fn get_glyph_bounds(data: &[u8], glyph_index: u32) -> BoundingBox;
    }

    unsafe extern "C++" {
        include!("outlines.h");

        fn run(font_path: &str);
    }
}

use skrifa_ffi::{Outline, PathVerb, Point, PsEncodingKind};

pub enum PsFont<'a> {
    Type1(Type1Font),
    Cff(CffFont<'a>),
    Error,
}

pub struct CffFont<'a> {
    font: CffFontRef<'a>,
    meta: Option<CffMetadata<'a>>,
    charset: Option<CffCharset<'a>>,
    encoding: Option<CffEncoding<'a>>,
    unicode_cmap: Option<PsCharmap>,
    subfonts: Vec<Option<CffSubfont>>,
}

pub fn new_ps_font(data: &[u8]) -> Box<PsFont<'_>> {
    let font = if let Ok(cff) = CffFontRef::new(data, 0, None) {
        let meta = cff.metadata();
        let charset = cff.charset();
        let encoding = cff.encoding();
        let subfonts = (0..cff.num_subfonts()).map(|i| cff.subfont(i, &[]).ok()).collect();
        let unicode_cmap = if let Some(charset) = charset.as_ref() {
            Some(PsCharmap::from_glyph_names(charset.iter().filter_map(|(gid, sid)| {
                Some((gid, core::str::from_utf8(cff.string(sid)?).ok()?))
            })))
        } else {
            None
        };
        PsFont::Cff(CffFont { font: cff, meta, charset, encoding, unicode_cmap, subfonts })
    } else if let Ok(type1) = Type1Font::new(data) {
        PsFont::Type1(type1)
    } else {
        PsFont::Error
    };
    Box::new(font)
}

impl PsFont<'_> {
    fn is_ok(&self) -> bool {
        !matches!(self, Self::Error)
    }

    fn name(&self) -> &str {
        match self {
            Self::Type1(type1) => type1.name().unwrap_or_default(),
            Self::Cff(cff) => cff.meta.as_ref().and_then(|meta| meta.name()).unwrap_or_default(),
            Self::Error => "",
        }
    }

    fn family_name(&self) -> &str {
        match self {
            Self::Type1(type1) => type1.family_name().unwrap_or_default(),
            Self::Cff(cff) => {
                cff.meta.as_ref().and_then(|meta| meta.family_name()).unwrap_or_default()
            }
            Self::Error => "",
        }
    }

    fn units_per_em(&self) -> i32 {
        match self {
            Self::Type1(type1) => type1.upem(),
            Self::Cff(cff) => cff.font.upem(),
            Self::Error => 0,
        }
    }

    fn ascent(&self) -> f32 {
        let bbox = match self {
            Self::Type1(type1) => type1.bbox(),
            Self::Cff(cff) => cff.meta.as_ref().map(|meta| meta.bbox()).unwrap_or_default(),
            Self::Error => return 0.0,
        };
        bbox.y_max.to_f32()
    }

    fn descent(&self) -> f32 {
        let bbox = match self {
            Self::Type1(type1) => type1.bbox(),
            Self::Cff(cff) => cff.meta.as_ref().map(|meta| meta.bbox()).unwrap_or_default(),
            Self::Error => return 0.0,
        };
        bbox.y_min.to_f32()
    }

    fn num_glyphs(&self) -> u32 {
        match self {
            Self::Type1(type1) => type1.num_glyphs(),
            Self::Cff(cff) => cff.font.num_glyphs(),
            Self::Error => 0,
        }
    }

    fn unicode_to_gid(&self, unicode: u32) -> u32 {
        let gid = match self {
            Self::Type1(type1) => type1.unicode_charmap().map(unicode),
            Self::Cff(cff) => cff.unicode_cmap.as_ref().and_then(|cmap| cmap.map(unicode)),
            Self::Error => return 0,
        };
        gid.unwrap_or_default().to_u32()
    }

    fn encoding(&self) -> PsEncodingKind {
        let maybe_predefined = match self {
            Self::Type1(type1) => type1.encoding().map(|encoding| encoding.predefined()),
            Self::Cff(cff) => cff.encoding.as_ref().map(|encoding| encoding.predefined()),
            Self::Error => return PsEncodingKind::None,
        };
        let Some(predefined) = maybe_predefined else {
            return PsEncodingKind::None;
        };
        match predefined {
            Some(PredefinedEncoding::Standard) => PsEncodingKind::Standard,
            Some(PredefinedEncoding::Expert) => PsEncodingKind::Custom,
            Some(PredefinedEncoding::IsoLatin1) => PsEncodingKind::IsoLatin1,
            None => PsEncodingKind::Custom,
        }
    }

    fn code_to_gid(&self, code: u8) -> u32 {
        let gid = match self {
            Self::Type1(type1) => type1.encoding().and_then(|encoding| encoding.map(code)),
            Self::Cff(cff) => cff.encoding.as_ref().and_then(|encoding| encoding.map(code)),
            Self::Error => return 0,
        };
        let gid = gid.unwrap_or_default().to_u32();
        if gid < self.num_glyphs() {
            gid
        } else {
            0
        }
    }

    fn is_cid(&self) -> bool {
        match self {
            Self::Cff(cff) => cff.font.is_cid(),
            _ => false,
        }
    }

    fn cid_to_gid(&self, cid: u16) -> u32 {
        match self {
            Self::Cff(cff) if cff.font.is_cid() => cff
                .charset
                .as_ref()
                .and_then(|charset| charset.glyph_id(Sid::new(cid)).ok())
                .unwrap_or_default()
                .to_u32(),
            _ => 0,
        }
    }

    fn scaled_outline(&self, gid: u32, ppem: f32, outline: &mut Outline) -> bool {
        self.outline_impl(gid, Some(ppem), outline).is_some()
    }

    fn unscaled_outline(&self, gid: u32, outline: &mut Outline) -> bool {
        self.outline_impl(gid, None, outline).is_some()
    }

    fn outline_impl(&self, gid: u32, ppem: Option<f32>, outline: &mut Outline) -> Option<()> {
        outline.verbs.clear();
        outline.points.clear();
        outline.advance_width = 0.0;
        let width = match self {
            Self::Type1(type1) => type1.draw(gid.into(), ppem, outline).ok()??,
            Self::Cff(cff) => {
                let gid = GlyphId::new(gid);
                let subfont = cff.subfonts.get(cff.font.subfont_index(gid)? as usize)?.as_ref()?;
                cff.font.draw(subfont, gid, &[], ppem, outline).ok()??
            }
            Self::Error => return None,
        };
        outline.advance_width = width;
        Some(())
    }
}

impl Point {
    fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }
}

impl Outline {
    fn push<const N: usize>(&mut self, verb: PathVerb, points: [(f32, f32); N]) {
        self.verbs.push(verb);
        self.points.extend(points.into_iter().map(|(x, y)| Point::new(x, y)));
    }
}

impl OutlinePen for Outline {
    fn move_to(&mut self, x: f32, y: f32) {
        self.push(PathVerb::MoveTo, [(x, y)]);
    }

    fn line_to(&mut self, x: f32, y: f32) {
        self.push(PathVerb::LineTo, [(x, y)]);
    }

    fn quad_to(&mut self, cx0: f32, cy0: f32, x: f32, y: f32) {
        self.push(PathVerb::QuadTo, [(cx0, cy0), (x, y)]);
    }

    fn curve_to(&mut self, cx0: f32, cy0: f32, cx1: f32, cy1: f32, x: f32, y: f32) {
        self.push(PathVerb::CurveTo, [(cx0, cy0), (cx1, cy1), (x, y)]);
    }

    fn close(&mut self) {
        self.push(PathVerb::Close, []);
    }
}

fn agl_name_to_unicode(name: &str, unicode: &mut u32) -> bool {
    if let Some(uni) = read_fonts::ps::agl::name_to_char(name) {
        *unicode = uni as u32;
        true
    } else {
        false
    }
}

fn agl_unicode_to_name(unicode: u32, name: &mut [u8]) -> bool {
    read_fonts::ps::agl::char_to_name(unicode, name).is_some()
}

pub fn get_os2_code_page_range(data: &[u8], range: &mut skrifa_ffi::CodePageRange) -> bool {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        use read_fonts::TableProvider;
        if let Ok(os2) = font.os2() {
            range.range1 = os2.ul_code_page_range_1().unwrap_or(0);
            range.range2 = os2.ul_code_page_range_2().unwrap_or(0);
            return true;
        }
    }
    false
}

pub fn get_os2_panose(data: &[u8], panose: &mut skrifa_ffi::Os2Panose) -> bool {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        use read_fonts::TableProvider;
        if let Ok(os2) = font.os2() {
            let p = os2.panose_10();
            panose.b0 = p[0];
            panose.b1 = p[1];
            return true;
        }
    }
    false
}

pub fn get_os2_fs_type(data: &[u8], fs_type: &mut u16) -> bool {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        use read_fonts::TableProvider;
        if let Ok(os2) = font.os2() {
            *fs_type = os2.fs_type();
            return true;
        }
    }
    false
}

pub fn get_os2_unicode_range(data: &[u8], range: &mut skrifa_ffi::UnicodeRange) -> bool {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        use read_fonts::TableProvider;
        if let Ok(os2) = font.os2() {
            range.range1 = os2.ul_unicode_range_1();
            range.range2 = os2.ul_unicode_range_2();
            range.range3 = os2.ul_unicode_range_3();
            range.range4 = os2.ul_unicode_range_4();
            return true;
        }
    }
    false
}

pub fn get_style_name(data: &[u8]) -> String {
    if let Ok(font) = skrifa::FontRef::new(data) {
        use skrifa::string::StringId;
        use skrifa::MetadataProvider;
        if let Some(name) = font.localized_strings(StringId::SUBFAMILY_NAME).english_or_first() {
            return name.to_string();
        }
    }
    String::new()
}

pub fn get_glyph_name(data: &[u8], gid: u32) -> String {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        let glyph_names = skrifa::GlyphNames::new(&font);
        if let Some(name) = glyph_names.get(skrifa::GlyphId::new(gid)) {
            return name.to_string();
        }
    }
    String::new()
}

pub fn get_name_index(data: &[u8], name: &str) -> u32 {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        let glyph_names = skrifa::GlyphNames::new(&font);
        if let Some(gid) = glyph_names.iter().find(|(_id, n)| n.as_str() == name).map(|(id, _n)| id)
        {
            return gid.to_u32();
        }
    }
    0
}

pub fn get_char_codes_and_indices(data: &[u8], max_char: u32) -> Vec<skrifa_ffi::CharCodeAndIndex> {
    let mut results = Vec::new();
    if let Ok(font) = read_fonts::FontRef::new(data) {
        let charmap = font.charmap();
        if charmap.has_map() {
            for (char_code, glyph_id) in charmap.mappings() {
                if char_code > max_char {
                    break;
                }
                results.push(skrifa_ffi::CharCodeAndIndex {
                    char_code,
                    glyph_index: glyph_id.to_u32(),
                });
            }
            return results;
        }

        if let Ok(cmap) = font.cmap() {
            for record in cmap.encoding_records() {
                if let Ok(read_fonts::tables::cmap::CmapSubtable::Format0(format0)) =
                    record.subtable(cmap.offset_data())
                {
                    for (code, &gid) in format0.glyph_id_array().iter().enumerate() {
                        if gid != 0 {
                            let char_code = code as u32;
                            if char_code <= max_char {
                                results.push(skrifa_ffi::CharCodeAndIndex {
                                    char_code,
                                    glyph_index: gid as u32,
                                });
                            }
                        }
                    }
                    return results;
                }
            }

            if let Some((_, _, subtable)) = cmap.best_subtable() {
                for (char_code, glyph_id) in subtable.iter() {
                    if char_code > max_char {
                        continue;
                    }
                    results.push(skrifa_ffi::CharCodeAndIndex {
                        char_code,
                        glyph_index: glyph_id.to_u32(),
                    });
                }
                results.sort_by_key(|r| r.char_code);
                if let Some(pos) = results.iter().position(|r| r.char_code > max_char) {
                    results.truncate(pos);
                }
                return results;
            }
        }
    }
    results
}

pub fn has_glyph_names(data: &[u8]) -> bool {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        let glyph_names = skrifa::GlyphNames::new(&font);
        return glyph_names.source() != skrifa::GlyphNameSource::Synthesized;
    }
    false
}

pub fn is_fixed_pitch(data: &[u8]) -> bool {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        use read_fonts::TableProvider;
        if let Ok(post) = font.post() {
            return post.is_fixed_pitch() != 0;
        }
    }
    false
}

pub fn is_scalable(data: &[u8]) -> bool {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        use read_fonts::TableProvider;
        return font.glyf().is_ok() || font.cff().is_ok() || font.cff2().is_ok();
    }
    if read_fonts::ps::cff::CffFontRef::new(data, 0, None).is_ok() {
        return true;
    }
    if read_fonts::ps::type1::Type1Font::new(data).is_ok() {
        return true;
    }
    false
}

pub fn get_font_format(data: &[u8]) -> String {
    if read_fonts::ps::type1::Type1Font::new(data).is_ok() {
        return "Type 1".to_string();
    }
    if let Ok(font) = read_fonts::FontRef::new(data) {
        use read_fonts::TableProvider;
        if font.cff().is_ok() || font.cff2().is_ok() {
            return "CFF".to_string();
        }
        if font.glyf().is_ok() {
            return "TrueType".to_string();
        }
    }
    if read_fonts::ps::cff::CffFontRef::new(data, 0, None).is_ok() {
        return "CFF".to_string();
    }
    String::new()
}

pub fn get_glyph_bounds(data: &[u8], glyph_index: u32) -> skrifa_ffi::BoundingBox {
    if let Ok(font) = read_fonts::FontRef::new(data) {
        let metrics = skrifa::metrics::GlyphMetrics::new(
            &font,
            skrifa::instance::Size::unscaled(),
            skrifa::instance::LocationRef::default(),
        );
        if let Some(bbox) = metrics.bounds(skrifa::GlyphId::new(glyph_index)) {
            return skrifa_ffi::BoundingBox {
                x_min: bbox.x_min,
                y_min: bbox.y_min,
                x_max: bbox.x_max,
                y_max: bbox.y_max,
            };
        }
    }
    skrifa_ffi::BoundingBox { x_min: 0.0, y_min: 0.0, x_max: 0.0, y_max: 0.0 }
}

fn main() {
    skrifa_ffi::run("");
}
