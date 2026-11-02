// Copyright 2026 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
};
use skrifa::{
    charmap::Charmap,
    instance::{LocationRef, Size},
    metrics::Metrics,
    outline::OutlineGlyphFormat,
    string::StringId,
    FontRef, GlyphNameSource, GlyphNames, MetadataProvider, OutlineGlyphCollection,
};

#[cxx::bridge(namespace = "skrifa")]
mod skrifa_ffi {
    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub enum FaceFormat {
        Unknown = 0,
        TrueType = 1,
        Type1 = 2,
        Cff = 3,
    }

    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub enum FsType {
        RestrictedLicenseEmbedding = 0x0002,
        BitmapEmbeddingOnly = 0x0200,
    }

    #[derive(Copy, Clone, PartialEq, Eq, Debug)]
    pub enum PsEncodingKind {
        None = 0,
        Standard = 1,
        Expert = 2,
        IsoLatin1 = 3,
        Custom = 4,
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
        type SkrifaFont<'a>;
        unsafe fn new_font<'a>(data: &'a [u8], index: u32) -> Box<SkrifaFont<'a>>;
        fn is_ok(&self) -> bool;
        fn font_type(&self) -> FaceFormat;
        unsafe fn postscript_name<'a>(&'a self) -> &'a str;
        unsafe fn family_name<'a>(&'a self) -> &'a str;
        fn style_name(&self) -> String;
        fn units_per_em(&self) -> i32;
        fn ascent(&self) -> f32;
        fn descent(&self) -> f32;
        fn num_glyphs(&self) -> u32;
        fn is_fixed_pitch(&self) -> bool;
        fn is_tricky(&self) -> bool;
        fn is_scalable(&self) -> bool;
        fn is_cid(&self) -> bool;
        fn cid_to_gid(&self, cid: u16) -> u32;
        fn unicode_to_gid(&self, unicode: u32) -> u32;
        fn encoding(&self) -> PsEncodingKind;
        fn code_to_gid(&self, code: u8) -> u32;
        fn has_glyph_names(&self) -> bool;
        fn glyph_name(&self, gid: u32) -> String;
        fn scaled_outline(&self, gid: u32, ppem: f32, outline: &mut Outline) -> bool;
        fn unscaled_outline(&self, gid: u32, outline: &mut Outline) -> bool;
        fn has_outline(&self, gid: u32) -> bool;

        fn get_os2_code_page_range(&self, range: &mut CodePageRange) -> bool;
        fn get_os2_unicode_range(&self, range: &mut UnicodeRange) -> bool;
        fn get_os2_panose(&self, panose: &mut Os2Panose) -> bool;
        fn get_os2_fs_type(&self, fs_type: &mut u16) -> bool;
        fn get_char_codes_and_indices(&self, max_char: u32) -> Vec<CharCodeAndIndex>;
        fn name_index(&self, name: &str) -> u32;
        fn glyph_bounds(&self, glyph_index: u32) -> BoundingBox;

        fn agl_name_to_unicode(name: &str, unicode: &mut u32) -> bool;
        fn agl_unicode_to_name(unicode: u32, name: &mut [u8]) -> bool;
        fn get_num_faces(data: &[u8]) -> u32;
    }

    unsafe extern "C++" {
        include!("outlines.h");

        fn run(font_path: &str);
    }
}

use skrifa_ffi::{
    BoundingBox, CharCodeAndIndex, CodePageRange, FaceFormat, Os2Panose, Outline, PathVerb, Point,
    PsEncodingKind, UnicodeRange,
};

pub enum SkrifaFont<'a> {
    Sfnt(Sfnt<'a>),
    Type1(Type1Font),
    Cff(CffFont<'a>),
    Error,
}

pub struct Sfnt<'a> {
    font: FontRef<'a>,
    metrics: Metrics,
    ps_name: Option<String>,
    family_name: Option<String>,
    style_name: Option<String>,
    glyph_names: GlyphNames<'a>,
    charmap: Charmap<'a>,
    outlines: OutlineGlyphCollection<'a>,
    is_tricky: bool,
}

impl<'a> Sfnt<'a> {
    fn new(data: &'a [u8], index: u32) -> Option<Self> {
        let font = FontRef::from_index(data, index).ok()?;
        let metrics = font.metrics(Size::unscaled(), LocationRef::default());
        let get_name = |id| font.localized_strings(id).english_or_first().map(|s| s.to_string());
        let ps_name = get_name(StringId::POSTSCRIPT_NAME);
        let family_name =
            get_name(StringId::TYPOGRAPHIC_FAMILY_NAME).or_else(|| get_name(StringId::FAMILY_NAME));
        let style_name = get_name(StringId::TYPOGRAPHIC_SUBFAMILY_NAME)
            .or_else(|| get_name(StringId::SUBFAMILY_NAME));
        let glyph_names = font.glyph_names();
        let charmap = font.charmap();
        let outlines = font.outline_glyphs();
        let is_tricky = outlines.require_interpreter();
        Some(Self {
            font,
            ps_name,
            metrics,
            family_name,
            style_name,
            glyph_names,
            charmap,
            outlines,
            is_tricky,
        })
    }
}

pub struct CffFont<'a> {
    font: CffFontRef<'a>,
    meta: Option<CffMetadata<'a>>,
    charset: Option<CffCharset<'a>>,
    encoding: Option<CffEncoding<'a>>,
    unicode_cmap: Option<PsCharmap>,
    subfonts: Vec<Option<CffSubfont>>,
    cid_count: Option<u32>,
}

pub fn new_font<'a>(data: &'a [u8], index: u32) -> Box<SkrifaFont<'a>> {
    if let Some(sfnt) = Sfnt::new(data, index) {
        return Box::new(SkrifaFont::Sfnt(sfnt));
    }
    let font = if let Ok(cff) = CffFontRef::new(data, index, None) {
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
        let cid_count = if cff.is_cid() {
            charset.as_ref().and_then(|cs| {
                cs.iter()
                    .filter(|(gid, _)| gid.to_u32() < cff.num_glyphs())
                    .map(|(_, cid)| cid.to_u16() as u32)
                    .max()
                    .map(|max_cid| max_cid + 1)
            })
        } else {
            None
        };
        SkrifaFont::Cff(CffFont {
            font: cff,
            meta,
            charset,
            encoding,
            unicode_cmap,
            subfonts,
            cid_count,
        })
    } else if let Ok(type1) = Type1Font::new(data) {
        SkrifaFont::Type1(type1)
    } else {
        SkrifaFont::Error
    };
    Box::new(font)
}

impl SkrifaFont<'_> {
    fn map_gid(&self, gid: u32) -> Option<u32> {
        match self {
            Self::Cff(cff) => {
                if cff.font.is_cid() {
                    let charset = cff.charset.as_ref()?;
                    let actual_gid = charset.glyph_id(Sid::new(gid as u16)).ok()?;
                    Some(actual_gid.to_u32())
                } else {
                    Some(gid)
                }
            }
            _ => Some(gid),
        }
    }

    fn is_ok(&self) -> bool {
        !matches!(self, Self::Error)
    }

    fn font_type(&self) -> FaceFormat {
        match self {
            Self::Sfnt(sfnt) => match sfnt.outlines.format() {
                Some(OutlineGlyphFormat::Glyf) => FaceFormat::TrueType,
                Some(OutlineGlyphFormat::Cff) | Some(OutlineGlyphFormat::Cff2) => FaceFormat::Cff,
                _ => FaceFormat::Unknown,
            },
            Self::Type1(_) => FaceFormat::Type1,
            Self::Cff(_) => FaceFormat::Cff,
            Self::Error => FaceFormat::Unknown,
        }
    }

    fn postscript_name(&self) -> &str {
        match self {
            Self::Sfnt(sfnt) => sfnt.ps_name.as_deref().unwrap_or_default(),
            Self::Type1(type1) => type1.name().unwrap_or_default(),
            Self::Cff(cff) => cff.meta.as_ref().and_then(|meta| meta.name()).unwrap_or_default(),
            Self::Error => "",
        }
    }

    fn family_name(&self) -> &str {
        match self {
            Self::Sfnt(sfnt) => sfnt.family_name.as_deref().unwrap_or_default(),
            Self::Type1(type1) => type1.family_name().unwrap_or_default(),
            Self::Cff(cff) => {
                let name =
                    cff.meta.as_ref().and_then(|meta| meta.family_name()).unwrap_or_default();
                if name.is_empty() {
                    self.postscript_name()
                } else {
                    name
                }
            }
            Self::Error => "",
        }
    }

    fn units_per_em(&self) -> i32 {
        match self {
            Self::Sfnt(sfnt) => sfnt.metrics.units_per_em as i32,
            Self::Type1(type1) => type1.upem(),
            Self::Cff(cff) => cff.font.upem(),
            Self::Error => 0,
        }
    }

    fn ascent(&self) -> f32 {
        let bbox = match self {
            Self::Sfnt(sfnt) => return sfnt.metrics.ascent,
            Self::Type1(type1) => type1.bbox(),
            Self::Cff(cff) => cff.meta.as_ref().map(|meta| meta.bbox()).unwrap_or_default(),
            Self::Error => return 0.0,
        };
        bbox.y_max.to_f32()
    }

    fn descent(&self) -> f32 {
        let bbox = match self {
            Self::Sfnt(sfnt) => return sfnt.metrics.descent,
            Self::Type1(type1) => type1.bbox(),
            Self::Cff(cff) => cff.meta.as_ref().map(|meta| meta.bbox()).unwrap_or_default(),
            Self::Error => return 0.0,
        };
        bbox.y_min.to_f32()
    }

    fn num_glyphs(&self) -> u32 {
        match self {
            Self::Sfnt(sfnt) => sfnt.metrics.glyph_count as u32,
            Self::Type1(type1) => type1.num_glyphs(),
            Self::Cff(cff) => cff.cid_count.unwrap_or_else(|| cff.font.num_glyphs()),
            Self::Error => 0,
        }
    }

    fn is_tricky(&self) -> bool {
        let Self::Sfnt(sfnt) = self else {
            return false;
        };
        sfnt.is_tricky
    }
    fn unicode_to_gid(&self, unicode: u32) -> u32 {
        let gid = match self {
            Self::Sfnt(sfnt) => sfnt.charmap.map(unicode),
            Self::Type1(type1) => type1.unicode_charmap().map(unicode),
            Self::Cff(cff) => cff.unicode_cmap.as_ref().and_then(|cmap| cmap.map(unicode)),
            Self::Error => return 0,
        };
        gid.unwrap_or_default().to_u32()
    }

    fn encoding(&self) -> PsEncodingKind {
        let maybe_predefined = match self {
            Self::Sfnt(_sfnt) => return PsEncodingKind::None,
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
            Self::Sfnt(_sfnt) => return 0,
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
        outline.clear();
        if let Some(width) = self.outline_impl(gid, Some(ppem), outline) {
            outline.advance_width = width.unwrap_or_default();
            true
        } else {
            false
        }
    }

    fn unscaled_outline(&self, gid: u32, outline: &mut Outline) -> bool {
        outline.clear();
        if let Some(width) = self.outline_impl(gid, None, outline) {
            outline.advance_width = width.unwrap_or_default();
            true
        } else {
            false
        }
    }

    fn has_outline(&self, gid: u32) -> bool {
        match self {
            Self::Sfnt(sfnt) => sfnt.outlines.get(GlyphId::new(gid)).is_some(),
            Self::Type1(type1) => gid < type1.num_glyphs(),
            Self::Cff(cff) => self
                .map_gid(gid)
                .map(|mapped_gid| mapped_gid < cff.font.num_glyphs())
                .unwrap_or(false),
            Self::Error => false,
        }
    }

    fn get_glyph_bounds(&self, gid: u32) -> BoundingBox {
        let mut pen = BoundsPen::new();
        if self.outline_impl(gid, None, &mut pen).is_some() {
            pen.to_bounding_box()
        } else {
            BoundingBox { x_min: 0.0, y_min: 0.0, x_max: 0.0, y_max: 0.0 }
        }
    }

    fn outline_impl(
        &self,
        gid: u32,
        ppem: Option<f32>,
        outline: &mut impl OutlinePen,
    ) -> Option<Option<f32>> {
        match self {
            Self::Sfnt(sfnt) => {
                let gid = GlyphId::new(gid);
                let size = ppem.map(Size::new).unwrap_or(Size::unscaled());
                let glyph = sfnt.outlines.get(gid)?;
                let metrics = glyph.draw(size, outline).ok()?;
                Some(metrics.advance_width.or_else(|| {
                    sfnt.font.glyph_metrics(size, LocationRef::default()).advance_width(gid)
                }))
            }
            Self::Type1(type1) => type1.draw(gid.into(), ppem, outline).ok(),
            Self::Cff(cff) => {
                let mapped_gid = self.map_gid(gid)?;
                let gid = GlyphId::new(mapped_gid);
                let subfont = cff.subfonts.get(cff.font.subfont_index(gid)? as usize)?.as_ref()?;
                cff.font.draw(subfont, gid, &[], ppem, outline).ok()
            }
            Self::Error => None,
        }
    }
}

impl Point {
    fn new(x: f32, y: f32) -> Self {
        Self { x, y }
    }
}

impl Outline {
    fn clear(&mut self) {
        self.verbs.clear();
        self.points.clear();
        self.advance_width = 0.0;
    }

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

#[derive(Default)]
struct BoundsPen {
    x_min: f32,
    y_min: f32,
    x_max: f32,
    y_max: f32,
    has_points: bool,
}

impl BoundsPen {
    fn new() -> Self {
        Self {
            x_min: f32::MAX,
            y_min: f32::MAX,
            x_max: f32::MIN,
            y_max: f32::MIN,
            has_points: false,
        }
    }

    fn update(&mut self, x: f32, y: f32) {
        self.x_min = self.x_min.min(x);
        self.y_min = self.y_min.min(y);
        self.x_max = self.x_max.max(x);
        self.y_max = self.y_max.max(y);
        self.has_points = true;
    }

    fn to_bounding_box(&self) -> BoundingBox {
        if self.has_points {
            BoundingBox {
                x_min: self.x_min,
                y_min: self.y_min,
                x_max: self.x_max,
                y_max: self.y_max,
            }
        } else {
            BoundingBox { x_min: 0.0, y_min: 0.0, x_max: 0.0, y_max: 0.0 }
        }
    }
}

impl OutlinePen for BoundsPen {
    fn move_to(&mut self, x: f32, y: f32) {
        self.update(x, y);
    }

    fn line_to(&mut self, x: f32, y: f32) {
        self.update(x, y);
    }

    fn quad_to(&mut self, cx0: f32, cy0: f32, x: f32, y: f32) {
        self.update(cx0, cy0);
        self.update(x, y);
    }

    fn curve_to(&mut self, cx0: f32, cy0: f32, cx1: f32, cy1: f32, x: f32, y: f32) {
        self.update(cx0, cy0);
        self.update(cx1, cy1);
        self.update(x, y);
    }

    fn close(&mut self) {}
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

impl SkrifaFont<'_> {
    fn style_name(&self) -> String {
        match self {
            Self::Sfnt(sfnt) => sfnt
                .font
                .localized_strings(StringId::SUBFAMILY_NAME)
                .english_or_first()
                .map(|s| s.to_string())
                .unwrap_or_default(),
            _ => String::new(),
        }
    }

    fn is_scalable(&self) -> bool {
        self.font_type() != FaceFormat::Unknown
    }

    fn get_os2_unicode_range(&self, range: &mut UnicodeRange) -> bool {
        let Self::Sfnt(sfnt) = self else {
            return false;
        };
        use read_fonts::TableProvider;
        if let Ok(os2) = sfnt.font.os2() {
            range.range1 = os2.ul_unicode_range_1();
            range.range2 = os2.ul_unicode_range_2();
            range.range3 = os2.ul_unicode_range_3();
            range.range4 = os2.ul_unicode_range_4();
            return true;
        }
        false
    }

    fn name_index(&self, name: &str) -> u32 {
        match self {
            Self::Sfnt(sfnt) => sfnt
                .glyph_names
                .iter()
                .find(|(_id, n)| n.as_str() == name)
                .map(|(id, _n)| id.to_u32())
                .unwrap_or_default(),
            Self::Cff(cff) => cff
                .charset
                .as_ref()
                .and_then(|charset| {
                    charset
                        .iter()
                        .find(|(_gid, sid)| cff.font.string(*sid) == Some(name.as_bytes()))
                        .map(|(gid, _sid)| gid.to_u32())
                })
                .unwrap_or_default(),
            Self::Type1(type1) => type1
                .glyph_names()
                .find(|(_, gname)| *gname == name)
                .map(|(gid, _)| gid.to_u32())
                .unwrap_or_default(),
            _ => 0,
        }
    }

    fn glyph_bounds(&self, glyph_index: u32) -> BoundingBox {
        self.get_glyph_bounds(glyph_index)
    }

    fn is_fixed_pitch(&self) -> bool {
        match self {
            Self::Sfnt(sfnt) => sfnt.metrics.is_monospace,
            Self::Type1(type1) => type1.is_fixed_pitch(),
            Self::Cff(cff) => cff.meta.as_ref().map(|meta| meta.is_fixed_pitch()).unwrap_or(false),
            Self::Error => false,
        }
    }

    fn has_glyph_names(&self) -> bool {
        match self {
            Self::Sfnt(sfnt) => sfnt.glyph_names.source() != GlyphNameSource::Synthesized,
            Self::Type1(_type1) => true,
            Self::Cff(cff) => cff.charset.is_some() && !cff.font.is_cid(),
            Self::Error => false,
        }
    }

    fn glyph_name(&self, gid: u32) -> String {
        match self {
            Self::Sfnt(sfnt) => {
                sfnt.glyph_names.get(GlyphId::new(gid)).map(|s| s.to_string()).unwrap_or_default()
            }
            Self::Type1(type1) => type1.glyph_name(gid.into()).unwrap_or_default().to_string(),
            Self::Cff(cff) if cff.charset.is_some() && !cff.font.is_cid() => cff
                .charset
                .as_ref()
                .and_then(|charset| charset.string_id(gid.into()).ok())
                .and_then(|sid| cff.font.string(sid))
                .and_then(|s| core::str::from_utf8(s).ok())
                .map(|s| s.to_string())
                .unwrap_or_default(),
            _ => String::new(),
        }
    }

    fn get_os2_code_page_range(&self, range: &mut CodePageRange) -> bool {
        let Self::Sfnt(sfnt) = self else {
            return false;
        };
        use read_fonts::TableProvider;
        if let Ok(os2) = sfnt.font.os2() {
            range.range1 = os2.ul_code_page_range_1().unwrap_or(0);
            range.range2 = os2.ul_code_page_range_2().unwrap_or(0);
            return true;
        }
        false
    }

    fn get_os2_panose(&self, panose: &mut Os2Panose) -> bool {
        let Self::Sfnt(sfnt) = self else {
            return false;
        };
        use read_fonts::TableProvider;
        if let Ok(os2) = sfnt.font.os2() {
            let p = os2.panose_10();
            panose.b0 = p[0];
            panose.b1 = p[1];
            return true;
        }
        false
    }

    fn get_os2_fs_type(&self, fs_type: &mut u16) -> bool {
        let Self::Sfnt(sfnt) = self else {
            return false;
        };
        use read_fonts::TableProvider;
        if let Ok(os2) = sfnt.font.os2() {
            *fs_type = os2.fs_type();
            return true;
        }
        false
    }

    fn get_char_codes_and_indices(&self, max_char: u32) -> Vec<CharCodeAndIndex> {
        let mut results = Vec::new();
        let Self::Sfnt(sfnt) = self else {
            return results;
        };
        let charmap = sfnt.font.charmap();
        if charmap.has_map() {
            let mut has_any = false;
            for (char_code, glyph_id) in charmap.mappings() {
                has_any = true;
                if char_code > max_char {
                    break;
                }
                results.push(CharCodeAndIndex { char_code, glyph_index: glyph_id.to_u32() });
            }
            if !has_any && results.is_empty() {
                results.push(CharCodeAndIndex { char_code: 0, glyph_index: 0 });
            }
            return results;
        }

        use read_fonts::TableProvider;
        if let Ok(cmap) = sfnt.font.cmap() {
            let mut has_any = false;
            for record in cmap.encoding_records() {
                if let Ok(read_fonts::tables::cmap::CmapSubtable::Format0(format0)) =
                    record.subtable(cmap.offset_data())
                {
                    for (code, &gid) in format0.glyph_id_array().iter().enumerate() {
                        if gid != 0 {
                            has_any = true;
                            let char_code = code as u32;
                            if char_code <= max_char {
                                results
                                    .push(CharCodeAndIndex { char_code, glyph_index: gid as u32 });
                            }
                        }
                    }
                    if !has_any && results.is_empty() {
                        results.push(CharCodeAndIndex { char_code: 0, glyph_index: 0 });
                    }
                    return results;
                }
            }

            if let Some((_, _, subtable)) = cmap.best_subtable() {
                for (char_code, glyph_id) in subtable.iter() {
                    has_any = true;
                    if char_code > max_char {
                        continue;
                    }
                    results.push(CharCodeAndIndex { char_code, glyph_index: glyph_id.to_u32() });
                }
                results.sort_by_key(|r| r.char_code);
                if let Some(pos) = results.iter().position(|r| r.char_code > max_char) {
                    results.truncate(pos);
                }
                if !has_any && results.is_empty() {
                    results.push(CharCodeAndIndex { char_code: 0, glyph_index: 0 });
                }
                return results;
            }
        }
        results
    }
}

fn get_num_faces(data: &[u8]) -> u32 {
    if let Ok(collection) = read_fonts::CollectionRef::new(data) {
        return collection.len();
    }
    if read_fonts::FontRef::new(data).is_ok() {
        return 1;
    }
    0
}
fn main() {
    skrifa_ffi::run("");
}
