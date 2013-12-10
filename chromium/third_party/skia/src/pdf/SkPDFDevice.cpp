/*
 * Copyright 2011 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkPDFDevice.h"

#include "SkAnnotation.h"
#include "SkColor.h"
#include "SkClipStack.h"
#include "SkData.h"
#include "SkDraw.h"
#include "SkFontHost.h"
#include "SkGlyphCache.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkPathOps.h"
#include "SkPDFFont.h"
#include "SkPDFFormXObject.h"
#include "SkPDFGraphicState.h"
#include "SkPDFImage.h"
#include "SkPDFResourceDict.h"
#include "SkPDFShader.h"
#include "SkPDFStream.h"
#include "SkPDFTypes.h"
#include "SkPDFUtils.h"
#include "SkRect.h"
#include "SkString.h"
#include "SkTextFormatParams.h"
#include "SkTemplates.h"
#include "SkTypefacePriv.h"
#include "SkTSet.h"

// Utility functions

static void emit_pdf_color(SkColor color, SkWStream* result) {
    SkASSERT(SkColorGetA(color) == 0xFF);  // We handle alpha elsewhere.
    SkScalar colorMax = SkIntToScalar(0xFF);
    SkPDFScalar::Append(
            SkScalarDiv(SkIntToScalar(SkColorGetR(color)), colorMax), result);
    result->writeText(" ");
    SkPDFScalar::Append(
            SkScalarDiv(SkIntToScalar(SkColorGetG(color)), colorMax), result);
    result->writeText(" ");
    SkPDFScalar::Append(
            SkScalarDiv(SkIntToScalar(SkColorGetB(color)), colorMax), result);
    result->writeText(" ");
}

static SkPaint calculate_text_paint(const SkPaint& paint) {
    SkPaint result = paint;
    if (result.isFakeBoldText()) {
        SkScalar fakeBoldScale = SkScalarInterpFunc(result.getTextSize(),
                                                    kStdFakeBoldInterpKeys,
                                                    kStdFakeBoldInterpValues,
                                                    kStdFakeBoldInterpLength);
        SkScalar width = SkScalarMul(result.getTextSize(), fakeBoldScale);
        if (result.getStyle() == SkPaint::kFill_Style) {
            result.setStyle(SkPaint::kStrokeAndFill_Style);
        } else {
            width += result.getStrokeWidth();
        }
        result.setStrokeWidth(width);
    }
    return result;
}

// Stolen from measure_text in SkDraw.cpp and then tweaked.
static void align_text(SkDrawCacheProc glyphCacheProc, const SkPaint& paint,
                       const uint16_t* glyphs, size_t len,
                       SkScalar* x, SkScalar* y) {
    if (paint.getTextAlign() == SkPaint::kLeft_Align) {
        return;
    }

    SkMatrix ident;
    ident.reset();
    SkAutoGlyphCache autoCache(paint, NULL, &ident);
    SkGlyphCache* cache = autoCache.getCache();

    const char* start = reinterpret_cast<const char*>(glyphs);
    const char* stop = reinterpret_cast<const char*>(glyphs + len);
    SkFixed xAdv = 0, yAdv = 0;

    // TODO(vandebo): This probably needs to take kerning into account.
    while (start < stop) {
        const SkGlyph& glyph = glyphCacheProc(cache, &start, 0, 0);
        xAdv += glyph.fAdvanceX;
        yAdv += glyph.fAdvanceY;
    };
    if (paint.getTextAlign() == SkPaint::kLeft_Align) {
        return;
    }

    SkScalar xAdj = SkFixedToScalar(xAdv);
    SkScalar yAdj = SkFixedToScalar(yAdv);
    if (paint.getTextAlign() == SkPaint::kCenter_Align) {
        xAdj = SkScalarHalf(xAdj);
        yAdj = SkScalarHalf(yAdj);
    }
    *x = *x - xAdj;
    *y = *y - yAdj;
}

static size_t max_glyphid_for_typeface(SkTypeface* typeface) {
    SkAutoResolveDefaultTypeface autoResolve(typeface);
    typeface = autoResolve.get();
    return typeface->countGlyphs() - 1;
}

typedef SkAutoSTMalloc<128, uint16_t> SkGlyphStorage;

static size_t force_glyph_encoding(const SkPaint& paint, const void* text,
                                   size_t len, SkGlyphStorage* storage,
                                   uint16_t** glyphIDs) {
    // Make sure we have a glyph id encoding.
    if (paint.getTextEncoding() != SkPaint::kGlyphID_TextEncoding) {
        size_t numGlyphs = paint.textToGlyphs(text, len, NULL);
        storage->reset(numGlyphs);
        paint.textToGlyphs(text, len, storage->get());
        *glyphIDs = storage->get();
        return numGlyphs;
    }

    // For user supplied glyph ids we need to validate them.
    SkASSERT((len & 1) == 0);
    size_t numGlyphs = len / 2;
    const uint16_t* input =
        reinterpret_cast<uint16_t*>(const_cast<void*>((text)));

    int maxGlyphID = max_glyphid_for_typeface(paint.getTypeface());
    size_t validated;
    for (validated = 0; validated < numGlyphs; ++validated) {
        if (input[validated] > maxGlyphID) {
            break;
        }
    }
    if (validated >= numGlyphs) {
        *glyphIDs = reinterpret_cast<uint16_t*>(const_cast<void*>((text)));
        return numGlyphs;
    }

    // Silently drop anything out of range.
    storage->reset(numGlyphs);
    if (validated > 0) {
        memcpy(storage->get(), input, validated * sizeof(uint16_t));
    }

    for (size_t i = validated; i < numGlyphs; ++i) {
        storage->get()[i] = input[i];
        if (input[i] > maxGlyphID) {
            storage->get()[i] = 0;
        }
    }
    *glyphIDs = storage->get();
    return numGlyphs;
}

static void set_text_transform(SkScalar x, SkScalar y, SkScalar textSkewX,
                               SkWStream* content) {
    // Flip the text about the x-axis to account for origin swap and include
    // the passed parameters.
    content->writeText("1 0 ");
    SkPDFScalar::Append(0 - textSkewX, content);
    content->writeText(" -1 ");
    SkPDFScalar::Append(x, content);
    content->writeText(" ");
    SkPDFScalar::Append(y, content);
    content->writeText(" Tm\n");
}

// It is important to not confuse GraphicStateEntry with SkPDFGraphicState, the
// later being our representation of an object in the PDF file.
struct GraphicStateEntry {
    GraphicStateEntry();

    // Compare the fields we care about when setting up a new content entry.
    bool compareInitialState(const GraphicStateEntry& b);

    SkMatrix fMatrix;
    // We can't do set operations on Paths, though PDF natively supports
    // intersect.  If the clip stack does anything other than intersect,
    // we have to fall back to the region.  Treat fClipStack as authoritative.
    // See http://code.google.com/p/skia/issues/detail?id=221
    SkClipStack fClipStack;
    SkRegion fClipRegion;

    // When emitting the content entry, we will ensure the graphic state
    // is set to these values first.
    SkColor fColor;
    SkScalar fTextScaleX;  // Zero means we don't care what the value is.
    SkPaint::Style fTextFill;  // Only if TextScaleX is non-zero.
    int fShaderIndex;
    int fGraphicStateIndex;

    // We may change the font (i.e. for Type1 support) within a
    // ContentEntry.  This is the one currently in effect, or NULL if none.
    SkPDFFont* fFont;
    // In PDF, text size has no default value. It is only valid if fFont is
    // not NULL.
    SkScalar fTextSize;
};

GraphicStateEntry::GraphicStateEntry() : fColor(SK_ColorBLACK),
                                         fTextScaleX(SK_Scalar1),
                                         fTextFill(SkPaint::kFill_Style),
                                         fShaderIndex(-1),
                                         fGraphicStateIndex(-1),
                                         fFont(NULL),
                                         fTextSize(SK_ScalarNaN) {
    fMatrix.reset();
}

bool GraphicStateEntry::compareInitialState(const GraphicStateEntry& b) {
    return fColor == b.fColor &&
           fShaderIndex == b.fShaderIndex &&
           fGraphicStateIndex == b.fGraphicStateIndex &&
           fMatrix == b.fMatrix &&
           fClipStack == b.fClipStack &&
               (fTextScaleX == 0 ||
                b.fTextScaleX == 0 ||
                (fTextScaleX == b.fTextScaleX && fTextFill == b.fTextFill));
}

class GraphicStackState {
public:
    GraphicStackState(const SkClipStack& existingClipStack,
                      const SkRegion& existingClipRegion,
                      SkWStream* contentStream)
            : fStackDepth(0),
              fContentStream(contentStream) {
        fEntries[0].fClipStack = existingClipStack;
        fEntries[0].fClipRegion = existingClipRegion;
    }

    void updateClip(const SkClipStack& clipStack, const SkRegion& clipRegion,
                    const SkPoint& translation);
    void updateMatrix(const SkMatrix& matrix);
    void updateDrawingState(const GraphicStateEntry& state);

    void drainStack();

private:
    void push();
    void pop();
    GraphicStateEntry* currentEntry() { return &fEntries[fStackDepth]; }

    // Conservative limit on save depth, see impl. notes in PDF 1.4 spec.
    static const int kMaxStackDepth = 12;
    GraphicStateEntry fEntries[kMaxStackDepth + 1];
    int fStackDepth;
    SkWStream* fContentStream;
};

void GraphicStackState::drainStack() {
    while (fStackDepth) {
        pop();
    }
}

void GraphicStackState::push() {
    SkASSERT(fStackDepth < kMaxStackDepth);
    fContentStream->writeText("q\n");
    fStackDepth++;
    fEntries[fStackDepth] = fEntries[fStackDepth - 1];
}

void GraphicStackState::pop() {
    SkASSERT(fStackDepth > 0);
    fContentStream->writeText("Q\n");
    fStackDepth--;
}

// This function initializes iter to be an iterator on the "stack" argument
// and then skips over the leading entries as specified in prefix.  It requires
// and asserts that "prefix" will be a prefix to "stack."
static void skip_clip_stack_prefix(const SkClipStack& prefix,
                                   const SkClipStack& stack,
                                   SkClipStack::Iter* iter) {
    SkClipStack::B2TIter prefixIter(prefix);
    iter->reset(stack, SkClipStack::Iter::kBottom_IterStart);

    const SkClipStack::Element* prefixEntry;
    const SkClipStack::Element* iterEntry;

    for (prefixEntry = prefixIter.next(); prefixEntry;
            prefixEntry = prefixIter.next()) {
        iterEntry = iter->next();
        SkASSERT(iterEntry);
        // Because of SkClipStack does internal intersection, the last clip
        // entry may differ.
        if (*prefixEntry != *iterEntry) {
            SkASSERT(prefixEntry->getOp() == SkRegion::kIntersect_Op);
            SkASSERT(iterEntry->getOp() == SkRegion::kIntersect_Op);
            SkASSERT(iterEntry->getType() == prefixEntry->getType());
            // back up the iterator by one
            iter->prev();
            prefixEntry = prefixIter.next();
            break;
        }
    }

    SkASSERT(prefixEntry == NULL);
}

static void emit_clip(SkPath* clipPath, SkRect* clipRect,
                      SkWStream* contentStream) {
    SkASSERT(clipPath || clipRect);

    SkPath::FillType clipFill;
    if (clipPath) {
        SkPDFUtils::EmitPath(*clipPath, SkPaint::kFill_Style, contentStream);
        clipFill = clipPath->getFillType();
    } else {
        SkPDFUtils::AppendRectangle(*clipRect, contentStream);
        clipFill = SkPath::kWinding_FillType;
    }

    NOT_IMPLEMENTED(clipFill == SkPath::kInverseEvenOdd_FillType, false);
    NOT_IMPLEMENTED(clipFill == SkPath::kInverseWinding_FillType, false);
    if (clipFill == SkPath::kEvenOdd_FillType) {
        contentStream->writeText("W* n\n");
    } else {
        contentStream->writeText("W n\n");
    }
}

#ifdef SK_PDF_USE_PATHOPS
/* Calculate an inverted path's equivalent non-inverted path, given the
 * canvas bounds.
 * outPath may alias with invPath (since this is supported by PathOps).
 */
static bool calculate_inverse_path(const SkRect& bounds, const SkPath& invPath,
                                   SkPath* outPath) {
    SkASSERT(invPath.isInverseFillType());

    SkPath clipPath;
    clipPath.addRect(bounds);

    return Op(clipPath, invPath, kIntersect_PathOp, outPath);
}

// Sanity check the numerical values of the SkRegion ops and PathOps ops
// enums so region_op_to_pathops_op can do a straight passthrough cast.
// If these are failing, it may be necessary to make region_op_to_pathops_op
// do more.
SK_COMPILE_ASSERT(SkRegion::kDifference_Op == (int)kDifference_PathOp,
                  region_pathop_mismatch);
SK_COMPILE_ASSERT(SkRegion::kIntersect_Op == (int)kIntersect_PathOp,
                  region_pathop_mismatch);
SK_COMPILE_ASSERT(SkRegion::kUnion_Op == (int)kUnion_PathOp,
                  region_pathop_mismatch);
SK_COMPILE_ASSERT(SkRegion::kXOR_Op == (int)kXOR_PathOp,
                  region_pathop_mismatch);
SK_COMPILE_ASSERT(SkRegion::kReverseDifference_Op ==
                  (int)kReverseDifference_PathOp,
                  region_pathop_mismatch);

static SkPathOp region_op_to_pathops_op(SkRegion::Op op) {
    SkASSERT(op >= 0);
    SkASSERT(op <= SkRegion::kReverseDifference_Op);
    return (SkPathOp)op;
}

/* Uses Path Ops to calculate a vector SkPath clip from a clip stack.
 * Returns true if successful, or false if not successful.
 * If successful, the resulting clip is stored in outClipPath.
 * If not successful, outClipPath is undefined, and a fallback method
 * should be used.
 */
static bool get_clip_stack_path(const SkMatrix& transform,
                                const SkClipStack& clipStack,
                                const SkRegion& clipRegion,
                                SkPath* outClipPath) {
    outClipPath->reset();
    outClipPath->setFillType(SkPath::kInverseWinding_FillType);

    const SkClipStack::Element* clipEntry;
    SkClipStack::Iter iter;
    iter.reset(clipStack, SkClipStack::Iter::kBottom_IterStart);
    for (clipEntry = iter.next(); clipEntry; clipEntry = iter.next()) {
        SkPath entryPath;
        if (SkClipStack::Element::kEmpty_Type == clipEntry->getType()) {
            outClipPath->reset();
            outClipPath->setFillType(SkPath::kInverseWinding_FillType);
            continue;
        } else if (SkClipStack::Element::kRect_Type == clipEntry->getType()) {
            entryPath.addRect(clipEntry->getRect());
        } else if (SkClipStack::Element::kPath_Type == clipEntry->getType()) {
            entryPath = clipEntry->getPath();
        }
        entryPath.transform(transform);

        if (SkRegion::kReplace_Op == clipEntry->getOp()) {
            *outClipPath = entryPath;
        } else {
            SkPathOp op = region_op_to_pathops_op(clipEntry->getOp());
            if (!Op(*outClipPath, entryPath, op, outClipPath)) {
                return false;
            }
        }
    }

    if (outClipPath->isInverseFillType()) {
        // The bounds are slightly outset to ensure this is correct in the
        // face of floating-point accuracy and possible SkRegion bitmap
        // approximations.
        SkRect clipBounds = SkRect::Make(clipRegion.getBounds());
        clipBounds.outset(SK_Scalar1, SK_Scalar1);
        if (!calculate_inverse_path(clipBounds, *outClipPath, outClipPath)) {
            return false;
        }
    }
    return true;
}
#endif

// TODO(vandebo): Take advantage of SkClipStack::getSaveCount(), the PDF
// graphic state stack, and the fact that we can know all the clips used
// on the page to optimize this.
void GraphicStackState::updateClip(const SkClipStack& clipStack,
                                   const SkRegion& clipRegion,
                                   const SkPoint& translation) {
    if (clipStack == currentEntry()->fClipStack) {
        return;
    }

    while (fStackDepth > 0) {
        pop();
        if (clipStack == currentEntry()->fClipStack) {
            return;
        }
    }
    push();

    currentEntry()->fClipStack = clipStack;
    currentEntry()->fClipRegion = clipRegion;

    SkMatrix transform;
    transform.setTranslate(translation.fX, translation.fY);

#ifdef SK_PDF_USE_PATHOPS
    SkPath clipPath;
    if (get_clip_stack_path(transform, clipStack, clipRegion, &clipPath)) {
        emit_clip(&clipPath, NULL, fContentStream);
        return;
    }
#endif
    // gsState->initialEntry()->fClipStack/Region specifies the clip that has
    // already been applied.  (If this is a top level device, then it specifies
    // a clip to the content area.  If this is a layer, then it specifies
    // the clip in effect when the layer was created.)  There's no need to
    // reapply that clip; SKCanvas's SkDrawIter will draw anything outside the
    // initial clip on the parent layer.  (This means there's a bug if the user
    // expands the clip and then uses any xfer mode that uses dst:
    // http://code.google.com/p/skia/issues/detail?id=228 )
    SkClipStack::Iter iter;
    skip_clip_stack_prefix(fEntries[0].fClipStack, clipStack, &iter);

    // If the clip stack does anything other than intersect or if it uses
    // an inverse fill type, we have to fall back to the clip region.
    bool needRegion = false;
    const SkClipStack::Element* clipEntry;
    for (clipEntry = iter.next(); clipEntry; clipEntry = iter.next()) {
        if (clipEntry->getOp() != SkRegion::kIntersect_Op || clipEntry->isInverseFilled()) {
            needRegion = true;
            break;
        }
    }

    if (needRegion) {
        SkPath clipPath;
        SkAssertResult(clipRegion.getBoundaryPath(&clipPath));
        emit_clip(&clipPath, NULL, fContentStream);
    } else {
        skip_clip_stack_prefix(fEntries[0].fClipStack, clipStack, &iter);
        const SkClipStack::Element* clipEntry;
        for (clipEntry = iter.next(); clipEntry; clipEntry = iter.next()) {
            SkASSERT(clipEntry->getOp() == SkRegion::kIntersect_Op);
            switch (clipEntry->getType()) {
                case SkClipStack::Element::kRect_Type: {
                    SkRect translatedClip;
                    transform.mapRect(&translatedClip, clipEntry->getRect());
                    emit_clip(NULL, &translatedClip, fContentStream);
                    break;
                }
                case SkClipStack::Element::kPath_Type: {
                    SkPath translatedPath;
                    clipEntry->getPath().transform(transform, &translatedPath);
                    emit_clip(&translatedPath, NULL, fContentStream);
                    break;
                }
                default:
                    SkASSERT(false);
            }
        }
    }
}

void GraphicStackState::updateMatrix(const SkMatrix& matrix) {
    if (matrix == currentEntry()->fMatrix) {
        return;
    }

    if (currentEntry()->fMatrix.getType() != SkMatrix::kIdentity_Mask) {
        SkASSERT(fStackDepth > 0);
        SkASSERT(fEntries[fStackDepth].fClipStack ==
                 fEntries[fStackDepth -1].fClipStack);
        pop();

        SkASSERT(currentEntry()->fMatrix.getType() == SkMatrix::kIdentity_Mask);
    }
    if (matrix.getType() == SkMatrix::kIdentity_Mask) {
        return;
    }

    push();
    SkPDFUtils::AppendTransform(matrix, fContentStream);
    currentEntry()->fMatrix = matrix;
}

void GraphicStackState::updateDrawingState(const GraphicStateEntry& state) {
    // PDF treats a shader as a color, so we only set one or the other.
    if (state.fShaderIndex >= 0) {
        if (state.fShaderIndex != currentEntry()->fShaderIndex) {
            SkPDFUtils::ApplyPattern(state.fShaderIndex, fContentStream);
            currentEntry()->fShaderIndex = state.fShaderIndex;
        }
    } else {
        if (state.fColor != currentEntry()->fColor ||
                currentEntry()->fShaderIndex >= 0) {
            emit_pdf_color(state.fColor, fContentStream);
            fContentStream->writeText("RG ");
            emit_pdf_color(state.fColor, fContentStream);
            fContentStream->writeText("rg\n");
            currentEntry()->fColor = state.fColor;
            currentEntry()->fShaderIndex = -1;
        }
    }

    if (state.fGraphicStateIndex != currentEntry()->fGraphicStateIndex) {
        SkPDFUtils::ApplyGraphicState(state.fGraphicStateIndex, fContentStream);
        currentEntry()->fGraphicStateIndex = state.fGraphicStateIndex;
    }

    if (state.fTextScaleX) {
        if (state.fTextScaleX != currentEntry()->fTextScaleX) {
            SkScalar pdfScale = SkScalarMul(state.fTextScaleX,
                                            SkIntToScalar(100));
            SkPDFScalar::Append(pdfScale, fContentStream);
            fContentStream->writeText(" Tz\n");
            currentEntry()->fTextScaleX = state.fTextScaleX;
        }
        if (state.fTextFill != currentEntry()->fTextFill) {
            SK_COMPILE_ASSERT(SkPaint::kFill_Style == 0, enum_must_match_value);
            SK_COMPILE_ASSERT(SkPaint::kStroke_Style == 1,
                              enum_must_match_value);
            SK_COMPILE_ASSERT(SkPaint::kStrokeAndFill_Style == 2,
                              enum_must_match_value);
            fContentStream->writeDecAsText(state.fTextFill);
            fContentStream->writeText(" Tr\n");
            currentEntry()->fTextFill = state.fTextFill;
        }
    }
}

SkBaseDevice* SkPDFDevice::onCreateCompatibleDevice(SkBitmap::Config config,
                                                    int width, int height,
                                                    bool isOpaque,
                                                    Usage usage) {
    SkMatrix initialTransform;
    initialTransform.reset();
    SkISize size = SkISize::Make(width, height);
    return SkNEW_ARGS(SkPDFDevice, (size, size, initialTransform));
}


struct ContentEntry {
    GraphicStateEntry fState;
    SkDynamicMemoryWStream fContent;
    SkAutoTDelete<ContentEntry> fNext;

    // If the stack is too deep we could get Stack Overflow.
    // So we manually destruct the object.
    ~ContentEntry() {
        ContentEntry* val = fNext.detach();
        while (val != NULL) {
            ContentEntry* valNext = val->fNext.detach();
            // When the destructor is called, fNext is NULL and exits.
            delete val;
            val = valNext;
        }
    }
};

// A helper class to automatically finish a ContentEntry at the end of a
// drawing method and maintain the state needed between set up and finish.
class ScopedContentEntry {
public:
    ScopedContentEntry(SkPDFDevice* device, const SkDraw& draw,
                       const SkPaint& paint, bool hasText = false)
        : fDevice(device),
          fContentEntry(NULL),
          fXfermode(SkXfermode::kSrcOver_Mode) {
        init(draw.fClipStack, *draw.fClip, *draw.fMatrix, paint, hasText);
    }
    ScopedContentEntry(SkPDFDevice* device, const SkClipStack* clipStack,
                       const SkRegion& clipRegion, const SkMatrix& matrix,
                       const SkPaint& paint, bool hasText = false)
        : fDevice(device),
          fContentEntry(NULL),
          fXfermode(SkXfermode::kSrcOver_Mode) {
        init(clipStack, clipRegion, matrix, paint, hasText);
    }

    ~ScopedContentEntry() {
        if (fContentEntry) {
            fDevice->finishContentEntry(fXfermode, fDstFormXObject);
        }
        SkSafeUnref(fDstFormXObject);
    }

    ContentEntry* entry() { return fContentEntry; }
private:
    SkPDFDevice* fDevice;
    ContentEntry* fContentEntry;
    SkXfermode::Mode fXfermode;
    SkPDFFormXObject* fDstFormXObject;

    void init(const SkClipStack* clipStack, const SkRegion& clipRegion,
              const SkMatrix& matrix, const SkPaint& paint, bool hasText) {
        fDstFormXObject = NULL;
        if (matrix.hasPerspective() ||
                (paint.getShader() &&
                 paint.getShader()->getLocalMatrix().hasPerspective())) {
            // Just report that PDF does not supports perspective
            // TODO(edisonn): update the shape when possible
            // or dump in an image otherwise
            NOT_IMPLEMENTED(true, false);
            return;
        }

        if (paint.getXfermode()) {
            paint.getXfermode()->asMode(&fXfermode);
        }
        fContentEntry = fDevice->setUpContentEntry(clipStack, clipRegion,
                                                   matrix, paint, hasText,
                                                   &fDstFormXObject);
    }
};

////////////////////////////////////////////////////////////////////////////////

static inline SkBitmap makeContentBitmap(const SkISize& contentSize,
                                         const SkMatrix* initialTransform) {
    SkBitmap bitmap;
    if (initialTransform) {
        // Compute the size of the drawing area.
        SkVector drawingSize;
        SkMatrix inverse;
        drawingSize.set(SkIntToScalar(contentSize.fWidth),
                        SkIntToScalar(contentSize.fHeight));
        if (!initialTransform->invert(&inverse)) {
            // This shouldn't happen, initial transform should be invertible.
            SkASSERT(false);
            inverse.reset();
        }
        inverse.mapVectors(&drawingSize, 1);
        SkISize size = SkSize::Make(drawingSize.fX, drawingSize.fY).toRound();
        bitmap.setConfig(SkBitmap::kNo_Config, abs(size.fWidth),
                         abs(size.fHeight));
    } else {
        bitmap.setConfig(SkBitmap::kNo_Config, abs(contentSize.fWidth),
                         abs(contentSize.fHeight));
    }

    return bitmap;
}

// TODO(vandebo) change pageSize to SkSize.
SkPDFDevice::SkPDFDevice(const SkISize& pageSize, const SkISize& contentSize,
                         const SkMatrix& initialTransform)
    : SkBitmapDevice(makeContentBitmap(contentSize, &initialTransform)),
      fPageSize(pageSize),
      fContentSize(contentSize),
      fLastContentEntry(NULL),
      fLastMarginContentEntry(NULL),
      fClipStack(NULL),
      fEncoder(NULL) {
    // just report that PDF does not supports perspective
    // TODO(edisonn): update the shape when possible
    // or dump in an image otherwise
    NOT_IMPLEMENTED(initialTransform.hasPerspective(), true);

    // Skia generally uses the top left as the origin but PDF natively has the
    // origin at the bottom left. This matrix corrects for that.  But that only
    // needs to be done once, we don't do it when layering.
    fInitialTransform.setTranslate(0, SkIntToScalar(pageSize.fHeight));
    fInitialTransform.preScale(SK_Scalar1, -SK_Scalar1);
    fInitialTransform.preConcat(initialTransform);

    SkIRect existingClip = SkIRect::MakeWH(this->width(), this->height());
    fExistingClipRegion.setRect(existingClip);

    this->init();
}

// TODO(vandebo) change layerSize to SkSize.
SkPDFDevice::SkPDFDevice(const SkISize& layerSize,
                         const SkClipStack& existingClipStack,
                         const SkRegion& existingClipRegion)
    : SkBitmapDevice(makeContentBitmap(layerSize, NULL)),
      fPageSize(layerSize),
      fContentSize(layerSize),
      fExistingClipStack(existingClipStack),
      fExistingClipRegion(existingClipRegion),
      fLastContentEntry(NULL),
      fLastMarginContentEntry(NULL),
      fClipStack(NULL) {
    fInitialTransform.reset();
    this->init();
}

SkPDFDevice::~SkPDFDevice() {
    this->cleanUp(true);
}

void SkPDFDevice::init() {
    fAnnotations = NULL;
    fResourceDict = NULL;
    fContentEntries.free();
    fLastContentEntry = NULL;
    fMarginContentEntries.free();
    fLastMarginContentEntry = NULL;
    fDrawingArea = kContent_DrawingArea;
    if (fFontGlyphUsage.get() == NULL) {
        fFontGlyphUsage.reset(new SkPDFGlyphSetMap());
    }
}

void SkPDFDevice::cleanUp(bool clearFontUsage) {
    fGraphicStateResources.unrefAll();
    fXObjectResources.unrefAll();
    fFontResources.unrefAll();
    fShaderResources.unrefAll();
    SkSafeUnref(fAnnotations);
    SkSafeUnref(fResourceDict);
    fNamedDestinations.deleteAll();

    if (clearFontUsage) {
        fFontGlyphUsage->reset();
    }
}

uint32_t SkPDFDevice::getDeviceCapabilities() {
    return kVector_Capability;
}

void SkPDFDevice::clear(SkColor color) {
    this->cleanUp(true);
    this->init();

    SkPaint paint;
    paint.setColor(color);
    paint.setStyle(SkPaint::kFill_Style);
    SkMatrix identity;
    identity.reset();
    ScopedContentEntry content(this, &fExistingClipStack, fExistingClipRegion,
                               identity, paint);
    internalDrawPaint(paint, content.entry());
}

void SkPDFDevice::drawPaint(const SkDraw& d, const SkPaint& paint) {
    SkPaint newPaint = paint;
    newPaint.setStyle(SkPaint::kFill_Style);
    ScopedContentEntry content(this, d, newPaint);
    internalDrawPaint(newPaint, content.entry());
}

void SkPDFDevice::internalDrawPaint(const SkPaint& paint,
                                    ContentEntry* contentEntry) {
    if (!contentEntry) {
        return;
    }
    SkRect bbox = SkRect::MakeWH(SkIntToScalar(this->width()),
                                 SkIntToScalar(this->height()));
    SkMatrix inverse;
    if (!contentEntry->fState.fMatrix.invert(&inverse)) {
        return;
    }
    inverse.mapRect(&bbox);

    SkPDFUtils::AppendRectangle(bbox, &contentEntry->fContent);
    SkPDFUtils::PaintPath(paint.getStyle(), SkPath::kWinding_FillType,
                          &contentEntry->fContent);
}

void SkPDFDevice::drawPoints(const SkDraw& d, SkCanvas::PointMode mode,
                             size_t count, const SkPoint* points,
                             const SkPaint& passedPaint) {
    if (count == 0) {
        return;
    }

    if (handlePointAnnotation(points, count, *d.fMatrix, passedPaint)) {
        return;
    }

    // SkDraw::drawPoints converts to multiple calls to fDevice->drawPath.
    // We only use this when there's a path effect because of the overhead
    // of multiple calls to setUpContentEntry it causes.
    if (passedPaint.getPathEffect()) {
        if (d.fClip->isEmpty()) {
            return;
        }
        SkDraw pointDraw(d);
        pointDraw.fDevice = this;
        pointDraw.drawPoints(mode, count, points, passedPaint, true);
        return;
    }

    const SkPaint* paint = &passedPaint;
    SkPaint modifiedPaint;

    if (mode == SkCanvas::kPoints_PointMode &&
            paint->getStrokeCap() != SkPaint::kRound_Cap) {
        modifiedPaint = *paint;
        paint = &modifiedPaint;
        if (paint->getStrokeWidth()) {
            // PDF won't draw a single point with square/butt caps because the
            // orientation is ambiguous.  Draw a rectangle instead.
            modifiedPaint.setStyle(SkPaint::kFill_Style);
            SkScalar strokeWidth = paint->getStrokeWidth();
            SkScalar halfStroke = SkScalarHalf(strokeWidth);
            for (size_t i = 0; i < count; i++) {
                SkRect r = SkRect::MakeXYWH(points[i].fX, points[i].fY, 0, 0);
                r.inset(-halfStroke, -halfStroke);
                drawRect(d, r, modifiedPaint);
            }
            return;
        } else {
            modifiedPaint.setStrokeCap(SkPaint::kRound_Cap);
        }
    }

    ScopedContentEntry content(this, d, *paint);
    if (!content.entry()) {
        return;
    }

    switch (mode) {
        case SkCanvas::kPolygon_PointMode:
            SkPDFUtils::MoveTo(points[0].fX, points[0].fY,
                               &content.entry()->fContent);
            for (size_t i = 1; i < count; i++) {
                SkPDFUtils::AppendLine(points[i].fX, points[i].fY,
                                       &content.entry()->fContent);
            }
            SkPDFUtils::StrokePath(&content.entry()->fContent);
            break;
        case SkCanvas::kLines_PointMode:
            for (size_t i = 0; i < count/2; i++) {
                SkPDFUtils::MoveTo(points[i * 2].fX, points[i * 2].fY,
                                   &content.entry()->fContent);
                SkPDFUtils::AppendLine(points[i * 2 + 1].fX,
                                       points[i * 2 + 1].fY,
                                       &content.entry()->fContent);
                SkPDFUtils::StrokePath(&content.entry()->fContent);
            }
            break;
        case SkCanvas::kPoints_PointMode:
            SkASSERT(paint->getStrokeCap() == SkPaint::kRound_Cap);
            for (size_t i = 0; i < count; i++) {
                SkPDFUtils::MoveTo(points[i].fX, points[i].fY,
                                   &content.entry()->fContent);
                SkPDFUtils::ClosePath(&content.entry()->fContent);
                SkPDFUtils::StrokePath(&content.entry()->fContent);
            }
            break;
        default:
            SkASSERT(false);
    }
}

void SkPDFDevice::drawRect(const SkDraw& d, const SkRect& rect,
                           const SkPaint& paint) {
    SkRect r = rect;
    r.sort();

    if (paint.getPathEffect()) {
        if (d.fClip->isEmpty()) {
            return;
        }
        SkPath path;
        path.addRect(r);
        drawPath(d, path, paint, NULL, true);
        return;
    }

    if (handleRectAnnotation(r, *d.fMatrix, paint)) {
        return;
    }

    ScopedContentEntry content(this, d, paint);
    if (!content.entry()) {
        return;
    }
    SkPDFUtils::AppendRectangle(r, &content.entry()->fContent);
    SkPDFUtils::PaintPath(paint.getStyle(), SkPath::kWinding_FillType,
                          &content.entry()->fContent);
}

void SkPDFDevice::drawPath(const SkDraw& d, const SkPath& origPath,
                           const SkPaint& paint, const SkMatrix* prePathMatrix,
                           bool pathIsMutable) {
    SkPath modifiedPath;
    SkPath* pathPtr = const_cast<SkPath*>(&origPath);

    SkMatrix matrix = *d.fMatrix;
    if (prePathMatrix) {
        if (paint.getPathEffect() || paint.getStyle() != SkPaint::kFill_Style) {
            if (!pathIsMutable) {
                pathPtr = &modifiedPath;
                pathIsMutable = true;
            }
            origPath.transform(*prePathMatrix, pathPtr);
        } else {
            if (!matrix.preConcat(*prePathMatrix)) {
                // TODO(edisonn): report somehow why we failed?
                return;
            }
        }
    }

    if (paint.getPathEffect()) {
        if (d.fClip->isEmpty()) {
            return;
        }
        if (!pathIsMutable) {
            pathPtr = &modifiedPath;
            pathIsMutable = true;
        }
        bool fill = paint.getFillPath(origPath, pathPtr);

        SkPaint noEffectPaint(paint);
        noEffectPaint.setPathEffect(NULL);
        if (fill) {
            noEffectPaint.setStyle(SkPaint::kFill_Style);
        } else {
            noEffectPaint.setStyle(SkPaint::kStroke_Style);
            noEffectPaint.setStrokeWidth(0);
        }
        drawPath(d, *pathPtr, noEffectPaint, NULL, true);
        return;
    }

#ifdef SK_PDF_USE_PATHOPS
    if (handleInversePath(d, origPath, paint, pathIsMutable)) {
        return;
    }
#endif

    if (handleRectAnnotation(pathPtr->getBounds(), *d.fMatrix, paint)) {
        return;
    }

    ScopedContentEntry content(this, d, paint);
    if (!content.entry()) {
        return;
    }
    SkPDFUtils::EmitPath(*pathPtr, paint.getStyle(),
                         &content.entry()->fContent);
    SkPDFUtils::PaintPath(paint.getStyle(), pathPtr->getFillType(),
                          &content.entry()->fContent);
}

void SkPDFDevice::drawBitmapRect(const SkDraw& draw, const SkBitmap& bitmap,
                                 const SkRect* src, const SkRect& dst,
                                 const SkPaint& paint,
                                 SkCanvas::DrawBitmapRectFlags flags) {
    // TODO: this code path must be updated to respect the flags parameter
    SkMatrix    matrix;
    SkRect      bitmapBounds, tmpSrc, tmpDst;
    SkBitmap    tmpBitmap;

    bitmapBounds.isetWH(bitmap.width(), bitmap.height());

    // Compute matrix from the two rectangles
    if (src) {
        tmpSrc = *src;
    } else {
        tmpSrc = bitmapBounds;
    }
    matrix.setRectToRect(tmpSrc, dst, SkMatrix::kFill_ScaleToFit);

    const SkBitmap* bitmapPtr = &bitmap;

    // clip the tmpSrc to the bounds of the bitmap, and recompute dstRect if
    // needed (if the src was clipped). No check needed if src==null.
    if (src) {
        if (!bitmapBounds.contains(*src)) {
            if (!tmpSrc.intersect(bitmapBounds)) {
                return; // nothing to draw
            }
            // recompute dst, based on the smaller tmpSrc
            matrix.mapRect(&tmpDst, tmpSrc);
        }

        // since we may need to clamp to the borders of the src rect within
        // the bitmap, we extract a subset.
        // TODO: make sure this is handled in drawBitmap and remove from here.
        SkIRect srcIR;
        tmpSrc.roundOut(&srcIR);
        if (!bitmap.extractSubset(&tmpBitmap, srcIR)) {
            return;
        }
        bitmapPtr = &tmpBitmap;

        // Since we did an extract, we need to adjust the matrix accordingly
        SkScalar dx = 0, dy = 0;
        if (srcIR.fLeft > 0) {
            dx = SkIntToScalar(srcIR.fLeft);
        }
        if (srcIR.fTop > 0) {
            dy = SkIntToScalar(srcIR.fTop);
        }
        if (dx || dy) {
            matrix.preTranslate(dx, dy);
        }
    }
    this->drawBitmap(draw, *bitmapPtr, matrix, paint);
}

void SkPDFDevice::drawBitmap(const SkDraw& d, const SkBitmap& bitmap,
                             const SkMatrix& matrix, const SkPaint& paint) {
    if (d.fClip->isEmpty()) {
        return;
    }

    SkMatrix transform = matrix;
    transform.postConcat(*d.fMatrix);
    this->internalDrawBitmap(transform, d.fClipStack, *d.fClip, bitmap, NULL, paint);
}

void SkPDFDevice::drawSprite(const SkDraw& d, const SkBitmap& bitmap,
                             int x, int y, const SkPaint& paint) {
    if (d.fClip->isEmpty()) {
        return;
    }

    SkMatrix matrix;
    matrix.setTranslate(SkIntToScalar(x), SkIntToScalar(y));
    this->internalDrawBitmap(matrix, d.fClipStack, *d.fClip, bitmap, NULL, paint);
}

void SkPDFDevice::drawText(const SkDraw& d, const void* text, size_t len,
                           SkScalar x, SkScalar y, const SkPaint& paint) {
    NOT_IMPLEMENTED(paint.getMaskFilter() != NULL, false);
    if (paint.getMaskFilter() != NULL) {
        // Don't pretend we support drawing MaskFilters, it makes for artifacts
        // making text unreadable (e.g. same text twice when using CSS shadows).
        return;
    }
    SkPaint textPaint = calculate_text_paint(paint);
    ScopedContentEntry content(this, d, textPaint, true);
    if (!content.entry()) {
        return;
    }

    SkGlyphStorage storage(0);
    uint16_t* glyphIDs = NULL;
    size_t numGlyphs = force_glyph_encoding(paint, text, len, &storage,
                                            &glyphIDs);
    textPaint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

    SkDrawCacheProc glyphCacheProc = textPaint.getDrawCacheProc();
    align_text(glyphCacheProc, textPaint, glyphIDs, numGlyphs, &x, &y);
    content.entry()->fContent.writeText("BT\n");
    set_text_transform(x, y, textPaint.getTextSkewX(),
                       &content.entry()->fContent);
    size_t consumedGlyphCount = 0;
    while (numGlyphs > consumedGlyphCount) {
        updateFont(textPaint, glyphIDs[consumedGlyphCount], content.entry());
        SkPDFFont* font = content.entry()->fState.fFont;
        size_t availableGlyphs =
            font->glyphsToPDFFontEncoding(glyphIDs + consumedGlyphCount,
                                          numGlyphs - consumedGlyphCount);
        fFontGlyphUsage->noteGlyphUsage(font, glyphIDs + consumedGlyphCount,
                                        availableGlyphs);
        SkString encodedString =
            SkPDFString::FormatString(glyphIDs + consumedGlyphCount,
                                      availableGlyphs, font->multiByteGlyphs());
        content.entry()->fContent.writeText(encodedString.c_str());
        consumedGlyphCount += availableGlyphs;
        content.entry()->fContent.writeText(" Tj\n");
    }
    content.entry()->fContent.writeText("ET\n");
}

void SkPDFDevice::drawPosText(const SkDraw& d, const void* text, size_t len,
                              const SkScalar pos[], SkScalar constY,
                              int scalarsPerPos, const SkPaint& paint) {
    NOT_IMPLEMENTED(paint.getMaskFilter() != NULL, false);
    if (paint.getMaskFilter() != NULL) {
        // Don't pretend we support drawing MaskFilters, it makes for artifacts
        // making text unreadable (e.g. same text twice when using CSS shadows).
        return;
    }
    SkASSERT(1 == scalarsPerPos || 2 == scalarsPerPos);
    SkPaint textPaint = calculate_text_paint(paint);
    ScopedContentEntry content(this, d, textPaint, true);
    if (!content.entry()) {
        return;
    }

    SkGlyphStorage storage(0);
    uint16_t* glyphIDs = NULL;
    size_t numGlyphs = force_glyph_encoding(paint, text, len, &storage,
                                            &glyphIDs);
    textPaint.setTextEncoding(SkPaint::kGlyphID_TextEncoding);

    SkDrawCacheProc glyphCacheProc = textPaint.getDrawCacheProc();
    content.entry()->fContent.writeText("BT\n");
    updateFont(textPaint, glyphIDs[0], content.entry());
    for (size_t i = 0; i < numGlyphs; i++) {
        SkPDFFont* font = content.entry()->fState.fFont;
        uint16_t encodedValue = glyphIDs[i];
        if (font->glyphsToPDFFontEncoding(&encodedValue, 1) != 1) {
            updateFont(textPaint, glyphIDs[i], content.entry());
            i--;
            continue;
        }
        fFontGlyphUsage->noteGlyphUsage(font, &encodedValue, 1);
        SkScalar x = pos[i * scalarsPerPos];
        SkScalar y = scalarsPerPos == 1 ? constY : pos[i * scalarsPerPos + 1];
        align_text(glyphCacheProc, textPaint, glyphIDs + i, 1, &x, &y);
        set_text_transform(x, y, textPaint.getTextSkewX(),
                           &content.entry()->fContent);
        SkString encodedString =
            SkPDFString::FormatString(&encodedValue, 1,
                                      font->multiByteGlyphs());
        content.entry()->fContent.writeText(encodedString.c_str());
        content.entry()->fContent.writeText(" Tj\n");
    }
    content.entry()->fContent.writeText("ET\n");
}

void SkPDFDevice::drawTextOnPath(const SkDraw& d, const void* text, size_t len,
                                 const SkPath& path, const SkMatrix* matrix,
                                 const SkPaint& paint) {
    if (d.fClip->isEmpty()) {
        return;
    }
    d.drawTextOnPath((const char*)text, len, path, matrix, paint);
}

void SkPDFDevice::drawVertices(const SkDraw& d, SkCanvas::VertexMode,
                               int vertexCount, const SkPoint verts[],
                               const SkPoint texs[], const SkColor colors[],
                               SkXfermode* xmode, const uint16_t indices[],
                               int indexCount, const SkPaint& paint) {
    if (d.fClip->isEmpty()) {
        return;
    }
    NOT_IMPLEMENTED("drawVerticies", true);
}

void SkPDFDevice::drawDevice(const SkDraw& d, SkBaseDevice* device, int x, int y,
                             const SkPaint& paint) {
    if ((device->getDeviceCapabilities() & kVector_Capability) == 0) {
        // If we somehow get a raster device, do what our parent would do.
        INHERITED::drawDevice(d, device, x, y, paint);
        return;
    }

    // Assume that a vector capable device means that it's a PDF Device.
    SkPDFDevice* pdfDevice = static_cast<SkPDFDevice*>(device);
    if (pdfDevice->isContentEmpty()) {
        return;
    }

    SkMatrix matrix;
    matrix.setTranslate(SkIntToScalar(x), SkIntToScalar(y));
    ScopedContentEntry content(this, d.fClipStack, *d.fClip, matrix, paint);
    if (!content.entry()) {
        return;
    }

    SkPDFFormXObject* xobject = new SkPDFFormXObject(pdfDevice);
    fXObjectResources.push(xobject);  // Transfer reference.
    SkPDFUtils::DrawFormXObject(fXObjectResources.count() - 1,
                                &content.entry()->fContent);

    // Merge glyph sets from the drawn device.
    fFontGlyphUsage->merge(pdfDevice->getFontGlyphUsage());
}

void SkPDFDevice::onAttachToCanvas(SkCanvas* canvas) {
    INHERITED::onAttachToCanvas(canvas);

    // Canvas promises that this ptr is valid until onDetachFromCanvas is called
    fClipStack = canvas->getClipStack();
}

void SkPDFDevice::onDetachFromCanvas() {
    INHERITED::onDetachFromCanvas();

    fClipStack = NULL;
}

ContentEntry* SkPDFDevice::getLastContentEntry() {
    if (fDrawingArea == kContent_DrawingArea) {
        return fLastContentEntry;
    } else {
        return fLastMarginContentEntry;
    }
}

SkAutoTDelete<ContentEntry>* SkPDFDevice::getContentEntries() {
    if (fDrawingArea == kContent_DrawingArea) {
        return &fContentEntries;
    } else {
        return &fMarginContentEntries;
    }
}

void SkPDFDevice::setLastContentEntry(ContentEntry* contentEntry) {
    if (fDrawingArea == kContent_DrawingArea) {
        fLastContentEntry = contentEntry;
    } else {
        fLastMarginContentEntry = contentEntry;
    }
}

void SkPDFDevice::setDrawingArea(DrawingArea drawingArea) {
    // A ScopedContentEntry only exists during the course of a draw call, so
    // this can't be called while a ScopedContentEntry exists.
    fDrawingArea = drawingArea;
}

SkPDFResourceDict* SkPDFDevice::getResourceDict() {
    if (NULL == fResourceDict) {
        fResourceDict = SkNEW(SkPDFResourceDict);

        if (fGraphicStateResources.count()) {
            for (int i = 0; i < fGraphicStateResources.count(); i++) {
                fResourceDict->insertResourceAsReference(
                        SkPDFResourceDict::kExtGState_ResourceType,
                        i, fGraphicStateResources[i]);
            }
        }

        if (fXObjectResources.count()) {
            for (int i = 0; i < fXObjectResources.count(); i++) {
                fResourceDict->insertResourceAsReference(
                        SkPDFResourceDict::kXObject_ResourceType,
                        i, fXObjectResources[i]);
            }
        }

        if (fFontResources.count()) {
            for (int i = 0; i < fFontResources.count(); i++) {
                fResourceDict->insertResourceAsReference(
                        SkPDFResourceDict::kFont_ResourceType,
                        i, fFontResources[i]);
            }
        }

        if (fShaderResources.count()) {
            SkAutoTUnref<SkPDFDict> patterns(new SkPDFDict());
            for (int i = 0; i < fShaderResources.count(); i++) {
                fResourceDict->insertResourceAsReference(
                        SkPDFResourceDict::kPattern_ResourceType,
                        i, fShaderResources[i]);
            }
        }
    }
    return fResourceDict;
}

const SkTDArray<SkPDFFont*>& SkPDFDevice::getFontResources() const {
    return fFontResources;
}

SkPDFArray* SkPDFDevice::copyMediaBox() const {
    // should this be a singleton?
    SkAutoTUnref<SkPDFInt> zero(SkNEW_ARGS(SkPDFInt, (0)));

    SkPDFArray* mediaBox = SkNEW(SkPDFArray);
    mediaBox->reserve(4);
    mediaBox->append(zero.get());
    mediaBox->append(zero.get());
    mediaBox->appendInt(fPageSize.fWidth);
    mediaBox->appendInt(fPageSize.fHeight);
    return mediaBox;
}

SkStream* SkPDFDevice::content() const {
    SkMemoryStream* result = new SkMemoryStream;
    result->setData(this->copyContentToData())->unref();
    return result;
}

void SkPDFDevice::copyContentEntriesToData(ContentEntry* entry,
        SkWStream* data) const {
    // TODO(ctguil): For margins, I'm not sure fExistingClipStack/Region is the
    // right thing to pass here.
    GraphicStackState gsState(fExistingClipStack, fExistingClipRegion, data);
    while (entry != NULL) {
        SkPoint translation;
        translation.iset(this->getOrigin());
        translation.negate();
        gsState.updateClip(entry->fState.fClipStack, entry->fState.fClipRegion,
                           translation);
        gsState.updateMatrix(entry->fState.fMatrix);
        gsState.updateDrawingState(entry->fState);

        SkAutoDataUnref copy(entry->fContent.copyToData());
        data->write(copy->data(), copy->size());
        entry = entry->fNext.get();
    }
    gsState.drainStack();
}

SkData* SkPDFDevice::copyContentToData() const {
    SkDynamicMemoryWStream data;
    if (fInitialTransform.getType() != SkMatrix::kIdentity_Mask) {
        SkPDFUtils::AppendTransform(fInitialTransform, &data);
    }

    // TODO(aayushkumar): Apply clip along the margins.  Currently, webkit
    // colors the contentArea white before it starts drawing into it and
    // that currently acts as our clip.
    // Also, think about adding a transform here (or assume that the values
    // sent across account for that)
    SkPDFDevice::copyContentEntriesToData(fMarginContentEntries.get(), &data);

    // If the content area is the entire page, then we don't need to clip
    // the content area (PDF area clips to the page size).  Otherwise,
    // we have to clip to the content area; we've already applied the
    // initial transform, so just clip to the device size.
    if (fPageSize != fContentSize) {
        SkRect r = SkRect::MakeWH(SkIntToScalar(this->width()),
                                  SkIntToScalar(this->height()));
        emit_clip(NULL, &r, &data);
    }

    SkPDFDevice::copyContentEntriesToData(fContentEntries.get(), &data);

    // potentially we could cache this SkData, and only rebuild it if we
    // see that our state has changed.
    return data.copyToData();
}

#ifdef SK_PDF_USE_PATHOPS
/* Draws an inverse filled path by using Path Ops to compute the positive
 * inverse using the current clip as the inverse bounds.
 * Return true if this was an inverse path and was properly handled,
 * otherwise returns false and the normal drawing routine should continue,
 * either as a (incorrect) fallback or because the path was not inverse
 * in the first place.
 */
bool SkPDFDevice::handleInversePath(const SkDraw& d, const SkPath& origPath,
                                    const SkPaint& paint, bool pathIsMutable) {
    if (!origPath.isInverseFillType()) {
        return false;
    }

    if (d.fClip->isEmpty()) {
        return false;
    }

    SkPath modifiedPath;
    SkPath* pathPtr = const_cast<SkPath*>(&origPath);
    SkPaint noInversePaint(paint);

    // Merge stroking operations into final path.
    if (SkPaint::kStroke_Style == paint.getStyle() ||
        SkPaint::kStrokeAndFill_Style == paint.getStyle()) {
        bool doFillPath = paint.getFillPath(origPath, &modifiedPath);
        if (doFillPath) {
            noInversePaint.setStyle(SkPaint::kFill_Style);
            noInversePaint.setStrokeWidth(0);
            pathPtr = &modifiedPath;
        } else {
            // To be consistent with the raster output, hairline strokes
            // are rendered as non-inverted.
            modifiedPath.toggleInverseFillType();
            drawPath(d, modifiedPath, paint, NULL, true);
            return true;
        }
    }

    // Get bounds of clip in current transform space
    // (clip bounds are given in device space).
    SkRect bounds;
    SkMatrix transformInverse;
    if (!d.fMatrix->invert(&transformInverse)) {
        return false;
    }
    bounds.set(d.fClip->getBounds());
    transformInverse.mapRect(&bounds);

    // Extend the bounds by the line width (plus some padding)
    // so the edge doesn't cause a visible stroke.
    bounds.outset(paint.getStrokeWidth() + SK_Scalar1,
                  paint.getStrokeWidth() + SK_Scalar1);

    if (!calculate_inverse_path(bounds, *pathPtr, &modifiedPath)) {
        return false;
    }

    drawPath(d, modifiedPath, noInversePaint, NULL, true);
    return true;
}
#endif

bool SkPDFDevice::handleRectAnnotation(const SkRect& r, const SkMatrix& matrix,
                                       const SkPaint& p) {
    SkAnnotation* annotationInfo = p.getAnnotation();
    if (!annotationInfo) {
        return false;
    }
    SkData* urlData = annotationInfo->find(SkAnnotationKeys::URL_Key());
    if (urlData) {
        handleLinkToURL(urlData, r, matrix);
        return p.isNoDrawAnnotation();
    }
    SkData* linkToName = annotationInfo->find(SkAnnotationKeys::Link_Named_Dest_Key());
    if (linkToName) {
        handleLinkToNamedDest(linkToName, r, matrix);
        return p.isNoDrawAnnotation();
    }
    return false;
}

bool SkPDFDevice::handlePointAnnotation(const SkPoint* points, size_t count,
                                        const SkMatrix& matrix,
                                        const SkPaint& paint) {
    SkAnnotation* annotationInfo = paint.getAnnotation();
    if (!annotationInfo) {
        return false;
    }
    SkData* nameData = annotationInfo->find(SkAnnotationKeys::Define_Named_Dest_Key());
    if (nameData) {
        for (size_t i = 0; i < count; i++) {
            defineNamedDestination(nameData, points[i], matrix);
        }
        return paint.isNoDrawAnnotation();
    }
    return false;
}

SkPDFDict* SkPDFDevice::createLinkAnnotation(const SkRect& r, const SkMatrix& matrix) {
    SkMatrix transform = matrix;
    transform.postConcat(fInitialTransform);
    SkRect translatedRect;
    transform.mapRect(&translatedRect, r);

    if (NULL == fAnnotations) {
        fAnnotations = SkNEW(SkPDFArray);
    }
    SkPDFDict* annotation(SkNEW_ARGS(SkPDFDict, ("Annot")));
    annotation->insertName("Subtype", "Link");
    fAnnotations->append(annotation);

    SkAutoTUnref<SkPDFArray> border(SkNEW(SkPDFArray));
    border->reserve(3);
    border->appendInt(0);  // Horizontal corner radius.
    border->appendInt(0);  // Vertical corner radius.
    border->appendInt(0);  // Width, 0 = no border.
    annotation->insert("Border", border.get());

    SkAutoTUnref<SkPDFArray> rect(SkNEW(SkPDFArray));
    rect->reserve(4);
    rect->appendScalar(translatedRect.fLeft);
    rect->appendScalar(translatedRect.fTop);
    rect->appendScalar(translatedRect.fRight);
    rect->appendScalar(translatedRect.fBottom);
    annotation->insert("Rect", rect.get());

    return annotation;
}

void SkPDFDevice::handleLinkToURL(SkData* urlData, const SkRect& r,
                                  const SkMatrix& matrix) {
    SkAutoTUnref<SkPDFDict> annotation(createLinkAnnotation(r, matrix));

    SkString url(static_cast<const char *>(urlData->data()),
                 urlData->size() - 1);
    SkAutoTUnref<SkPDFDict> action(SkNEW_ARGS(SkPDFDict, ("Action")));
    action->insertName("S", "URI");
    action->insert("URI", SkNEW_ARGS(SkPDFString, (url)))->unref();
    annotation->insert("A", action.get());
}

void SkPDFDevice::handleLinkToNamedDest(SkData* nameData, const SkRect& r,
                                        const SkMatrix& matrix) {
    SkAutoTUnref<SkPDFDict> annotation(createLinkAnnotation(r, matrix));
    SkString name(static_cast<const char *>(nameData->data()),
                  nameData->size() - 1);
    annotation->insert("Dest", SkNEW_ARGS(SkPDFName, (name)))->unref();
}

struct NamedDestination {
    const SkData* nameData;
    SkPoint point;

    NamedDestination(const SkData* nameData, const SkPoint& point)
        : nameData(nameData), point(point) {
        nameData->ref();
    }

    ~NamedDestination() {
        nameData->unref();
    }
};

void SkPDFDevice::defineNamedDestination(SkData* nameData, const SkPoint& point,
                                         const SkMatrix& matrix) {
    SkMatrix transform = matrix;
    transform.postConcat(fInitialTransform);
    SkPoint translatedPoint;
    transform.mapXY(point.x(), point.y(), &translatedPoint);
    fNamedDestinations.push(
        SkNEW_ARGS(NamedDestination, (nameData, translatedPoint)));
}

void SkPDFDevice::appendDestinations(SkPDFDict* dict, SkPDFObject* page) {
    int nDest = fNamedDestinations.count();
    for (int i = 0; i < nDest; i++) {
        NamedDestination* dest = fNamedDestinations[i];
        SkAutoTUnref<SkPDFArray> pdfDest(SkNEW(SkPDFArray));
        pdfDest->reserve(5);
        pdfDest->append(SkNEW_ARGS(SkPDFObjRef, (page)))->unref();
        pdfDest->appendName("XYZ");
        pdfDest->appendScalar(dest->point.x());
        pdfDest->appendScalar(dest->point.y());
        pdfDest->appendInt(0);  // Leave zoom unchanged
        dict->insert(static_cast<const char *>(dest->nameData->data()), pdfDest);
    }
}

SkPDFFormXObject* SkPDFDevice::createFormXObjectFromDevice() {
    SkPDFFormXObject* xobject = SkNEW_ARGS(SkPDFFormXObject, (this));
    // We always draw the form xobjects that we create back into the device, so
    // we simply preserve the font usage instead of pulling it out and merging
    // it back in later.
    cleanUp(false);  // Reset this device to have no content.
    init();
    return xobject;
}

void SkPDFDevice::clearClipFromContent(const SkClipStack* clipStack,
                                       const SkRegion& clipRegion) {
    if (clipRegion.isEmpty() || isContentEmpty()) {
        return;
    }
    SkAutoTUnref<SkPDFFormXObject> curContent(createFormXObjectFromDevice());

    // Redraw what we already had, but with the clip as a mask.
    drawFormXObjectWithClip(curContent, clipStack, clipRegion, true);
}

void SkPDFDevice::drawFormXObjectWithClip(SkPDFFormXObject* xobject,
                                          const SkClipStack* clipStack,
                                          const SkRegion& clipRegion,
                                          bool invertClip) {
    if (clipRegion.isEmpty() && !invertClip) {
        return;
    }

    // Create the mask.
    SkMatrix identity;
    identity.reset();
    SkDraw draw;
    draw.fMatrix = &identity;
    draw.fClip = &clipRegion;
    draw.fClipStack = clipStack;
    SkPaint stockPaint;
    this->drawPaint(draw, stockPaint);
    SkAutoTUnref<SkPDFFormXObject> maskFormXObject(createFormXObjectFromDevice());
    SkAutoTUnref<SkPDFGraphicState> sMaskGS(
        SkPDFGraphicState::GetSMaskGraphicState(maskFormXObject, invertClip,
                                                SkPDFGraphicState::kAlpha_SMaskMode));

    // Draw the xobject with the clip as a mask.
    ScopedContentEntry content(this, &fExistingClipStack, fExistingClipRegion,
                                 identity, stockPaint);
    if (!content.entry()) {
        return;
    }
    SkPDFUtils::ApplyGraphicState(addGraphicStateResource(sMaskGS.get()),
                                  &content.entry()->fContent);
    SkPDFUtils::DrawFormXObject(fXObjectResources.count(),
                                &content.entry()->fContent);
    fXObjectResources.push(xobject);
    xobject->ref();

    sMaskGS.reset(SkPDFGraphicState::GetNoSMaskGraphicState());
    SkPDFUtils::ApplyGraphicState(addGraphicStateResource(sMaskGS.get()),
                                  &content.entry()->fContent);
}

ContentEntry* SkPDFDevice::setUpContentEntry(const SkClipStack* clipStack,
                                             const SkRegion& clipRegion,
                                             const SkMatrix& matrix,
                                             const SkPaint& paint,
                                             bool hasText,
                                             SkPDFFormXObject** dst) {
    *dst = NULL;
    if (clipRegion.isEmpty()) {
        return NULL;
    }

    // The clip stack can come from an SkDraw where it is technically optional.
    SkClipStack synthesizedClipStack;
    if (clipStack == NULL) {
        if (clipRegion == fExistingClipRegion) {
            clipStack = &fExistingClipStack;
        } else {
            // GraphicStackState::updateClip expects the clip stack to have
            // fExistingClip as a prefix, so start there, then set the clip
            // to the passed region.
            synthesizedClipStack = fExistingClipStack;
            SkPath clipPath;
            clipRegion.getBoundaryPath(&clipPath);
            synthesizedClipStack.clipDevPath(clipPath, SkRegion::kReplace_Op,
                                             false);
            clipStack = &synthesizedClipStack;
        }
    }

    SkXfermode::Mode xfermode = SkXfermode::kSrcOver_Mode;
    if (paint.getXfermode()) {
        paint.getXfermode()->asMode(&xfermode);
    }

    if (xfermode == SkXfermode::kClear_Mode ||
            xfermode == SkXfermode::kSrc_Mode) {
        this->clearClipFromContent(clipStack, clipRegion);
    } else if (xfermode == SkXfermode::kSrcIn_Mode ||
               xfermode == SkXfermode::kDstIn_Mode ||
               xfermode == SkXfermode::kSrcOut_Mode ||
               xfermode == SkXfermode::kDstOut_Mode) {
        // For the following modes, we use both source and destination, but
        // we use one as a smask for the other, so we have to make form xobjects
        // out of both of them: SrcIn, DstIn, SrcOut, DstOut.
        if (isContentEmpty()) {
            return NULL;
        } else {
            *dst = createFormXObjectFromDevice();
        }
    }
    // TODO(vandebo): Figure out how/if we can handle the following modes:
    // SrcAtop, DestAtop, Xor, Plus.

    // These xfer modes don't draw source at all.
    if (xfermode == SkXfermode::kClear_Mode ||
            xfermode == SkXfermode::kDst_Mode) {
        return NULL;
    }

    ContentEntry* entry;
    SkAutoTDelete<ContentEntry> newEntry;

    ContentEntry* lastContentEntry = getLastContentEntry();
    if (lastContentEntry && lastContentEntry->fContent.getOffset() == 0) {
        entry = lastContentEntry;
    } else {
        newEntry.reset(new ContentEntry);
        entry = newEntry.get();
    }

    populateGraphicStateEntryFromPaint(matrix, *clipStack, clipRegion, paint,
                                       hasText, &entry->fState);
    if (lastContentEntry && xfermode != SkXfermode::kDstOver_Mode &&
            entry->fState.compareInitialState(lastContentEntry->fState)) {
        return lastContentEntry;
    }

    SkAutoTDelete<ContentEntry>* contentEntries = getContentEntries();
    if (!lastContentEntry) {
        contentEntries->reset(entry);
        setLastContentEntry(entry);
    } else if (xfermode == SkXfermode::kDstOver_Mode) {
        entry->fNext.reset(contentEntries->detach());
        contentEntries->reset(entry);
    } else {
        lastContentEntry->fNext.reset(entry);
        setLastContentEntry(entry);
    }
    newEntry.detach();
    return entry;
}

void SkPDFDevice::finishContentEntry(const SkXfermode::Mode xfermode,
                                     SkPDFFormXObject* dst) {
    if (xfermode != SkXfermode::kSrcIn_Mode &&
            xfermode != SkXfermode::kDstIn_Mode &&
            xfermode != SkXfermode::kSrcOut_Mode &&
            xfermode != SkXfermode::kDstOut_Mode) {
        SkASSERT(!dst);
        return;
    }

    ContentEntry* contentEntries = getContentEntries()->get();
    SkASSERT(dst);
    SkASSERT(!contentEntries->fNext.get());
    // We have to make a copy of these here because changing the current
    // content into a form xobject will destroy them.
    SkClipStack clipStack = contentEntries->fState.fClipStack;
    SkRegion clipRegion = contentEntries->fState.fClipRegion;

    SkAutoTUnref<SkPDFFormXObject> srcFormXObject;
    if (!isContentEmpty()) {
        srcFormXObject.reset(createFormXObjectFromDevice());
    }

    drawFormXObjectWithClip(dst, &clipStack, clipRegion, true);

    // We've redrawn dst minus the clip area, if there's no src, we're done.
    if (!srcFormXObject.get()) {
        return;
    }

    SkMatrix identity;
    identity.reset();
    SkPaint stockPaint;
    ScopedContentEntry inClipContentEntry(this, &fExistingClipStack,
                                          fExistingClipRegion, identity,
                                          stockPaint);
    if (!inClipContentEntry.entry()) {
        return;
    }

    SkAutoTUnref<SkPDFGraphicState> sMaskGS;
    if (xfermode == SkXfermode::kSrcIn_Mode ||
            xfermode == SkXfermode::kSrcOut_Mode) {
        sMaskGS.reset(SkPDFGraphicState::GetSMaskGraphicState(
                dst,
                xfermode == SkXfermode::kSrcOut_Mode,
                SkPDFGraphicState::kAlpha_SMaskMode));
        fXObjectResources.push(srcFormXObject.get());
        srcFormXObject.get()->ref();
    } else {
        sMaskGS.reset(SkPDFGraphicState::GetSMaskGraphicState(
                srcFormXObject.get(),
                xfermode == SkXfermode::kDstOut_Mode,
                SkPDFGraphicState::kAlpha_SMaskMode));
        // dst already added to fXObjectResources in drawFormXObjectWithClip.
    }
    SkPDFUtils::ApplyGraphicState(addGraphicStateResource(sMaskGS.get()),
                                  &inClipContentEntry.entry()->fContent);

    SkPDFUtils::DrawFormXObject(fXObjectResources.count() - 1,
                                &inClipContentEntry.entry()->fContent);

    sMaskGS.reset(SkPDFGraphicState::GetNoSMaskGraphicState());
    SkPDFUtils::ApplyGraphicState(addGraphicStateResource(sMaskGS.get()),
                                  &inClipContentEntry.entry()->fContent);
}

bool SkPDFDevice::isContentEmpty() {
    ContentEntry* contentEntries = getContentEntries()->get();
    if (!contentEntries || contentEntries->fContent.getOffset() == 0) {
        SkASSERT(!contentEntries || !contentEntries->fNext.get());
        return true;
    }
    return false;
}

void SkPDFDevice::populateGraphicStateEntryFromPaint(
        const SkMatrix& matrix,
        const SkClipStack& clipStack,
        const SkRegion& clipRegion,
        const SkPaint& paint,
        bool hasText,
        GraphicStateEntry* entry) {
    SkASSERT(paint.getPathEffect() == NULL);

    NOT_IMPLEMENTED(paint.getMaskFilter() != NULL, false);
    NOT_IMPLEMENTED(paint.getColorFilter() != NULL, false);

    entry->fMatrix = matrix;
    entry->fClipStack = clipStack;
    entry->fClipRegion = clipRegion;
    entry->fColor = SkColorSetA(paint.getColor(), 0xFF);
    entry->fShaderIndex = -1;

    // PDF treats a shader as a color, so we only set one or the other.
    SkAutoTUnref<SkPDFObject> pdfShader;
    const SkShader* shader = paint.getShader();
    SkColor color = paint.getColor();
    if (shader) {
        // PDF positions patterns relative to the initial transform, so
        // we need to apply the current transform to the shader parameters.
        SkMatrix transform = matrix;
        transform.postConcat(fInitialTransform);

        // PDF doesn't support kClamp_TileMode, so we simulate it by making
        // a pattern the size of the current clip.
        SkIRect bounds = clipRegion.getBounds();

        // We need to apply the initial transform to bounds in order to get
        // bounds in a consistent coordinate system.
        SkRect boundsTemp;
        boundsTemp.set(bounds);
        fInitialTransform.mapRect(&boundsTemp);
        boundsTemp.roundOut(&bounds);

        pdfShader.reset(SkPDFShader::GetPDFShader(*shader, transform, bounds));

        if (pdfShader.get()) {
            // pdfShader has been canonicalized so we can directly compare
            // pointers.
            int resourceIndex = fShaderResources.find(pdfShader.get());
            if (resourceIndex < 0) {
                resourceIndex = fShaderResources.count();
                fShaderResources.push(pdfShader.get());
                pdfShader.get()->ref();
            }
            entry->fShaderIndex = resourceIndex;
        } else {
            // A color shader is treated as an invalid shader so we don't have
            // to set a shader just for a color.
            SkShader::GradientInfo gradientInfo;
            SkColor gradientColor;
            gradientInfo.fColors = &gradientColor;
            gradientInfo.fColorOffsets = NULL;
            gradientInfo.fColorCount = 1;
            if (shader->asAGradient(&gradientInfo) ==
                    SkShader::kColor_GradientType) {
                entry->fColor = SkColorSetA(gradientColor, 0xFF);
                color = gradientColor;
            }
        }
    }

    SkAutoTUnref<SkPDFGraphicState> newGraphicState;
    if (color == paint.getColor()) {
        newGraphicState.reset(
                SkPDFGraphicState::GetGraphicStateForPaint(paint));
    } else {
        SkPaint newPaint = paint;
        newPaint.setColor(color);
        newGraphicState.reset(
                SkPDFGraphicState::GetGraphicStateForPaint(newPaint));
    }
    int resourceIndex = addGraphicStateResource(newGraphicState.get());
    entry->fGraphicStateIndex = resourceIndex;

    if (hasText) {
        entry->fTextScaleX = paint.getTextScaleX();
        entry->fTextFill = paint.getStyle();
    } else {
        entry->fTextScaleX = 0;
    }
}

int SkPDFDevice::addGraphicStateResource(SkPDFGraphicState* gs) {
    // Assumes that gs has been canonicalized (so we can directly compare
    // pointers).
    int result = fGraphicStateResources.find(gs);
    if (result < 0) {
        result = fGraphicStateResources.count();
        fGraphicStateResources.push(gs);
        gs->ref();
    }
    return result;
}

void SkPDFDevice::updateFont(const SkPaint& paint, uint16_t glyphID,
                             ContentEntry* contentEntry) {
    SkTypeface* typeface = paint.getTypeface();
    if (contentEntry->fState.fFont == NULL ||
            contentEntry->fState.fTextSize != paint.getTextSize() ||
            !contentEntry->fState.fFont->hasGlyph(glyphID)) {
        int fontIndex = getFontResourceIndex(typeface, glyphID);
        contentEntry->fContent.writeText("/");
        contentEntry->fContent.writeText(SkPDFResourceDict::getResourceName(
                SkPDFResourceDict::kFont_ResourceType,
                fontIndex).c_str());
        contentEntry->fContent.writeText(" ");
        SkPDFScalar::Append(paint.getTextSize(), &contentEntry->fContent);
        contentEntry->fContent.writeText(" Tf\n");
        contentEntry->fState.fFont = fFontResources[fontIndex];
    }
}

int SkPDFDevice::getFontResourceIndex(SkTypeface* typeface, uint16_t glyphID) {
    SkAutoTUnref<SkPDFFont> newFont(SkPDFFont::GetFontResource(typeface, glyphID));
    int resourceIndex = fFontResources.find(newFont.get());
    if (resourceIndex < 0) {
        resourceIndex = fFontResources.count();
        fFontResources.push(newFont.get());
        newFont.get()->ref();
    }
    return resourceIndex;
}

void SkPDFDevice::internalDrawBitmap(const SkMatrix& matrix,
                                     const SkClipStack* clipStack,
                                     const SkRegion& clipRegion,
                                     const SkBitmap& bitmap,
                                     const SkIRect* srcRect,
                                     const SkPaint& paint) {
    SkMatrix scaled;
    // Adjust for origin flip.
    scaled.setScale(SK_Scalar1, -SK_Scalar1);
    scaled.postTranslate(0, SK_Scalar1);
    // Scale the image up from 1x1 to WxH.
    SkIRect subset = SkIRect::MakeWH(bitmap.width(), bitmap.height());
    scaled.postScale(SkIntToScalar(subset.width()),
                     SkIntToScalar(subset.height()));
    scaled.postConcat(matrix);
    ScopedContentEntry content(this, clipStack, clipRegion, scaled, paint);
    if (!content.entry()) {
        return;
    }

    if (srcRect && !subset.intersect(*srcRect)) {
        return;
    }

    SkPDFImage* image = SkPDFImage::CreateImage(bitmap, subset, fEncoder);
    if (!image) {
        return;
    }

    fXObjectResources.push(image);  // Transfer reference.
    SkPDFUtils::DrawFormXObject(fXObjectResources.count() - 1,
                                &content.entry()->fContent);
}

bool SkPDFDevice::onReadPixels(const SkBitmap& bitmap, int x, int y,
                               SkCanvas::Config8888) {
    return false;
}

bool SkPDFDevice::allowImageFilter(SkImageFilter*) {
    return false;
}
