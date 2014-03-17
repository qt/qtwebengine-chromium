/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008-2009 Torch Mobile, Inc.
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef GraphicsContext_h
#define GraphicsContext_h

#include "platform/PlatformExport.h"
#include "platform/TraceEvent.h"
#include "platform/fonts/Font.h"
#include "platform/geometry/FloatRect.h"
#include "platform/graphics/DashArray.h"
#include "platform/graphics/DrawLooper.h"
#include "platform/graphics/ImageBufferSurface.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/graphics/GraphicsContextAnnotation.h"
#include "platform/graphics/GraphicsContextState.h"
#include "platform/graphics/skia/OpaqueRegionSkia.h"
#include "platform/graphics/skia/SkiaUtils.h"
// TODO(robertphillips): replace this include with "class SkBaseDevice;"
#include "third_party/skia/include/core/SkDevice.h"
#include "wtf/FastAllocBase.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"

class SkBitmap;
class SkPaint;
class SkPath;
class SkRRect;
struct SkRect;

namespace WebCore {

class DisplayList;
class ImageBuffer;
class KURL;

class PLATFORM_EXPORT GraphicsContext {
    WTF_MAKE_NONCOPYABLE(GraphicsContext); WTF_MAKE_FAST_ALLOCATED;
public:
    enum AntiAliasingMode {
        NotAntiAliased,
        AntiAliased
    };
    enum AccessMode {
        ReadOnly,
        ReadWrite
    };

    explicit GraphicsContext(SkCanvas*);
    ~GraphicsContext();

    // Returns the canvas used for painting, NOT guaranteed to be non-null.
    // Accessing the backing canvas this way flushes all queued save ops,
    // so it should be avoided. Use the corresponding draw/matrix/clip methods instead.
    SkCanvas* canvas()
    {
        // Flush any pending saves.
        realizeSave(SkCanvas::kMatrixClip_SaveFlag);

        return m_canvas;
    }
    const SkCanvas* canvas() const { return m_canvas; }
    bool paintingDisabled() const { return !m_canvas; }

    const SkBitmap* bitmap() const;
    const SkBitmap& layerBitmap(AccessMode = ReadOnly) const;

    // ---------- State management methods -----------------
    void save();
    void restore();

    void saveLayer(const SkRect* bounds, const SkPaint*, SkCanvas::SaveFlags = SkCanvas::kARGB_ClipLayer_SaveFlag);
    void restoreLayer();

    float strokeThickness() const { return m_state->m_strokeData.thickness(); }
    void setStrokeThickness(float thickness) { m_state->m_strokeData.setThickness(thickness); }

    StrokeStyle strokeStyle() const { return m_state->m_strokeData.style(); }
    void setStrokeStyle(StrokeStyle style) { m_state->m_strokeData.setStyle(style); }

    Color strokeColor() const { return m_state->m_strokeData.color(); }
    void setStrokeColor(const Color&);

    Pattern* strokePattern() const { return m_state->m_strokeData.pattern(); }
    void setStrokePattern(PassRefPtr<Pattern>);

    Gradient* strokeGradient() const { return m_state->m_strokeData.gradient(); }
    void setStrokeGradient(PassRefPtr<Gradient>);

    void setLineCap(LineCap cap) { m_state->m_strokeData.setLineCap(cap); }
    void setLineDash(const DashArray& dashes, float dashOffset) { m_state->m_strokeData.setLineDash(dashes, dashOffset); }
    void setLineJoin(LineJoin join) { m_state->m_strokeData.setLineJoin(join); }
    void setMiterLimit(float limit) { m_state->m_strokeData.setMiterLimit(limit); }

    WindRule fillRule() const { return m_state->m_fillRule; }
    void setFillRule(WindRule fillRule) { m_state->m_fillRule = fillRule; }

    Color fillColor() const { return m_state->m_fillColor; }
    void setFillColor(const Color&);
    SkColor effectiveFillColor() const { return m_state->applyAlpha(m_state->m_fillColor.rgb()); }

    void setFillPattern(PassRefPtr<Pattern>);
    Pattern* fillPattern() const { return m_state->m_fillPattern.get(); }

    void setFillGradient(PassRefPtr<Gradient>);
    Gradient* fillGradient() const { return m_state->m_fillGradient.get(); }

    SkDrawLooper* drawLooper() const { return m_state->m_looper.get(); }
    SkColor effectiveStrokeColor() const { return m_state->applyAlpha(m_state->m_strokeData.color().rgb()); }

    int getNormalizedAlpha() const;

    bool getClipBounds(SkRect* bounds) const;
    bool getTransformedClipBounds(FloatRect* bounds) const;
    SkMatrix getTotalMatrix() const;
    bool isPrintingDevice() const;

    void setShouldAntialias(bool antialias) { m_state->m_shouldAntialias = antialias; }
    bool shouldAntialias() const { return m_state->m_shouldAntialias; }

    void setShouldClampToSourceRect(bool clampToSourceRect) { m_state->m_shouldClampToSourceRect = clampToSourceRect; }
    bool shouldClampToSourceRect() const { return m_state->m_shouldClampToSourceRect; }

    void setShouldSmoothFonts(bool smoothFonts) { m_state->m_shouldSmoothFonts = smoothFonts; }
    bool shouldSmoothFonts() const { return m_state->m_shouldSmoothFonts; }

    // Turn off LCD text for the paint if not supported on this context.
    void adjustTextRenderMode(SkPaint*);
    bool couldUseLCDRenderedText();

    TextDrawingModeFlags textDrawingMode() const { return m_state->m_textDrawingMode; }
    void setTextDrawingMode(TextDrawingModeFlags mode) { m_state->m_textDrawingMode = mode; }

    void setAlpha(float alpha) { m_state->m_alpha = alpha; }

    void setImageInterpolationQuality(InterpolationQuality quality) { m_state->m_interpolationQuality = quality; }
    InterpolationQuality imageInterpolationQuality() const { return m_state->m_interpolationQuality; }

    void setCompositeOperation(CompositeOperator, blink::WebBlendMode = blink::WebBlendModeNormal);
    CompositeOperator compositeOperation() const { return m_state->m_compositeOperator; }
    blink::WebBlendMode blendModeOperation() const { return m_state->m_blendMode; }

    // Change the way document markers are rendered.
    // Any deviceScaleFactor higher than 1.5 is enough to justify setting this flag.
    void setUseHighResMarkers(bool isHighRes) { m_useHighResMarker = isHighRes; }

    // If true we are (most likely) rendering to a web page and the
    // canvas has been prepared with an opaque background. If false,
    // the canvas may havbe transparency (as is the case when rendering
    // to a canvas object).
    void setCertainlyOpaque(bool isOpaque) { m_isCertainlyOpaque = isOpaque; }
    bool isCertainlyOpaque() const { return m_isCertainlyOpaque; }

    // Returns if the context is a printing context instead of a display
    // context. Bitmap shouldn't be resampled when printing to keep the best
    // possible quality.
    bool printing() const { return m_printing; }
    void setPrinting(bool printing) { m_printing = printing; }

    bool isAccelerated() const { return m_accelerated; }
    void setAccelerated(bool accelerated) { m_accelerated = accelerated; }

    // The opaque region is empty until tracking is turned on.
    // It is never clerared by the context.
    void setTrackOpaqueRegion(bool track) { m_trackOpaqueRegion = track; }
    const OpaqueRegionSkia& opaqueRegion() const { return m_opaqueRegion; }

    // The text region is empty until tracking is turned on.
    // It is never clerared by the context.
    void setTrackTextRegion(bool track) { m_trackTextRegion = track; }
    const SkRect& textRegion() const { return m_textRegion; }

    bool updatingControlTints() const { return m_updatingControlTints; }
    void setUpdatingControlTints(bool updatingTints) { m_updatingControlTints = updatingTints; }

    AnnotationModeFlags annotationMode() const { return m_annotationMode; }
    void setAnnotationMode(const AnnotationModeFlags mode) { m_annotationMode = mode; }

    SkColorFilter* colorFilter();
    void setColorFilter(ColorFilter);
    // ---------- End state management methods -----------------

    // Get the contents of the image buffer
    bool readPixels(SkBitmap*, int, int, SkCanvas::Config8888 = SkCanvas::kNative_Premul_Config8888);

    // Sets up the paint for the current fill style.
    void setupPaintForFilling(SkPaint*) const;

    // Sets up the paint for stroking. Returns a float representing the
    // effective width of the pen. If a non-zero length is provided, the
    // number of dashes/dots on a dashed/dotted line will be adjusted to
    // start and end that length with a dash/dot.
    float setupPaintForStroking(SkPaint*, int length = 0) const;

    // These draw methods will do both stroking and filling.
    // FIXME: ...except drawRect(), which fills properly but always strokes
    // using a 1-pixel stroke inset from the rect borders (of the correct
    // stroke color).
    void drawRect(const IntRect&);
    void drawLine(const IntPoint&, const IntPoint&);
    void drawEllipse(const IntRect&);
    void drawConvexPolygon(size_t numPoints, const FloatPoint*, bool shouldAntialias = false);

    void fillPath(const Path&);
    void strokePath(const Path&);

    void fillEllipse(const FloatRect&);
    void strokeEllipse(const FloatRect&);

    void fillRect(const FloatRect&);
    void fillRect(const FloatRect&, const Color&);
    void fillRect(const FloatRect&, const Color&, CompositeOperator);
    void fillRoundedRect(const IntRect&, const IntSize& topLeft, const IntSize& topRight, const IntSize& bottomLeft, const IntSize& bottomRight, const Color&);
    void fillRoundedRect(const RoundedRect&, const Color&);

    void clearRect(const FloatRect&);

    void strokeRect(const FloatRect&, float lineWidth);

    void drawDisplayList(DisplayList*);

    void drawImage(Image*, const IntPoint&, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation);
    void drawImage(Image*, const IntRect&, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);
    void drawImage(Image*, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation);
    void drawImage(Image*, const FloatRect& destRect);
    void drawImage(Image*, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator = CompositeSourceOver, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);
    void drawImage(Image*, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator, blink::WebBlendMode, RespectImageOrientationEnum = DoNotRespectImageOrientation, bool useLowQualityScale = false);

    void drawTiledImage(Image*, const IntRect& destRect, const IntPoint& srcPoint, const IntSize& tileSize,
        CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false, blink::WebBlendMode = blink::WebBlendModeNormal, const IntSize& repeatSpacing = IntSize());
    void drawTiledImage(Image*, const IntRect& destRect, const IntRect& srcRect,
        const FloatSize& tileScaleFactor, Image::TileRule hRule = Image::StretchTile, Image::TileRule vRule = Image::StretchTile,
        CompositeOperator = CompositeSourceOver, bool useLowQualityScale = false);

    void drawImageBuffer(ImageBuffer*, const IntPoint&, CompositeOperator = CompositeSourceOver, blink::WebBlendMode = blink::WebBlendModeNormal);
    void drawImageBuffer(ImageBuffer*, const IntRect&, CompositeOperator = CompositeSourceOver, blink::WebBlendMode = blink::WebBlendModeNormal, bool useLowQualityScale = false);
    void drawImageBuffer(ImageBuffer*, const IntPoint& destPoint, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, blink::WebBlendMode = blink::WebBlendModeNormal);
    void drawImageBuffer(ImageBuffer*, const IntRect& destRect, const IntRect& srcRect, CompositeOperator = CompositeSourceOver, blink::WebBlendMode = blink::WebBlendModeNormal, bool useLowQualityScale = false);
    void drawImageBuffer(ImageBuffer*, const FloatRect& destRect);
    void drawImageBuffer(ImageBuffer*, const FloatRect& destRect, const FloatRect& srcRect, CompositeOperator = CompositeSourceOver, blink::WebBlendMode = blink::WebBlendModeNormal, bool useLowQualityScale = false);

    // These methods write to the canvas and modify the opaque region, if tracked.
    // Also drawLine(const IntPoint& point1, const IntPoint& point2) and fillRoundedRect
    void writePixels(const SkBitmap&, int x, int y, SkCanvas::Config8888 = SkCanvas::kNative_Premul_Config8888);
    void drawBitmap(const SkBitmap&, SkScalar, SkScalar, const SkPaint* = 0);
    void drawBitmapRect(const SkBitmap&, const SkRect*, const SkRect&, const SkPaint* = 0);
    void drawOval(const SkRect&, const SkPaint&);
    void drawPath(const SkPath&, const SkPaint&);
    // After drawing directly to the context's canvas, use this function to notify the context so
    // it can track the opaque region.
    // FIXME: this is still needed only because ImageSkia::paintSkBitmap() may need to notify for a
    //        smaller rect than the one drawn to, due to its clipping logic.
    void didDrawRect(const SkRect&, const SkPaint&, const SkBitmap* = 0);
    void drawRect(const SkRect&, const SkPaint&);
    void drawPosText(const void* text, size_t byteLength, const SkPoint pos[], const SkRect& textRect, const SkPaint&);
    void drawPosTextH(const void* text, size_t byteLength, const SkScalar xpos[], SkScalar constY, const SkRect& textRect, const SkPaint&);
    void drawTextOnPath(const void* text, size_t byteLength, const SkPath&, const SkRect& textRect, const SkMatrix*, const SkPaint&);

    void clip(const IntRect& rect) { clip(FloatRect(rect)); }
    void clip(const FloatRect& rect) { clipRect(rect); }
    void clipRoundedRect(const RoundedRect&);
    void clipOut(const IntRect& rect) { clipRect(rect, NotAntiAliased, SkRegion::kDifference_Op); }
    void clipOutRoundedRect(const RoundedRect&);
    void clipPath(const Path&, WindRule = RULE_EVENODD);
    void clipConvexPolygon(size_t numPoints, const FloatPoint*, bool antialias = true);
    bool clipRect(const SkRect&, AntiAliasingMode = NotAntiAliased, SkRegion::Op = SkRegion::kIntersect_Op);

    void drawText(const Font&, const TextRunPaintInfo&, const FloatPoint&);
    void drawEmphasisMarks(const Font&, const TextRunPaintInfo&, const AtomicString& mark, const FloatPoint&);
    void drawBidiText(const Font&, const TextRunPaintInfo&, const FloatPoint&, Font::CustomFontNotReadyAction = Font::DoNotPaintIfFontNotReady);
    void drawHighlightForText(const Font&, const TextRun&, const FloatPoint&, int h, const Color& backgroundColor, int from = 0, int to = -1);

    void drawLineForText(const FloatPoint&, float width, bool printing);
    enum DocumentMarkerLineStyle {
        DocumentMarkerSpellingLineStyle,
        DocumentMarkerGrammarLineStyle
    };
    void drawLineForDocumentMarker(const FloatPoint&, float width, DocumentMarkerLineStyle);

    void beginTransparencyLayer(float opacity, const FloatRect* = 0);
    void beginLayer(float opacity, CompositeOperator, const FloatRect* = 0, ColorFilter = ColorFilterNone);
    void endLayer();

    // Instead of being dispatched to the active canvas, draw commands following beginRecording()
    // are stored in a display list that can be replayed at a later time.
    void beginRecording(const FloatRect& bounds);
    PassRefPtr<DisplayList> endRecording();

    bool hasShadow() const;
    void setShadow(const FloatSize& offset, float blur, const Color&,
        DrawLooper::ShadowTransformMode = DrawLooper::ShadowRespectsTransforms,
        DrawLooper::ShadowAlphaMode = DrawLooper::ShadowRespectsAlpha);
    void clearShadow() { clearDrawLooper(); }

    // It is assumed that this draw looper is used only for shadows
    // (i.e. a draw looper is set if and only if there is a shadow).
    void setDrawLooper(const DrawLooper&);
    void clearDrawLooper();

    void drawFocusRing(const Vector<IntRect>&, int width, int offset, const Color&);
    void drawFocusRing(const Path&, int width, int offset, const Color&);

    enum Edge {
        NoEdge = 0,
        TopEdge = 1 << 1,
        RightEdge = 1 << 2,
        BottomEdge = 1 << 3,
        LeftEdge = 1 << 4
    };
    typedef unsigned Edges;
    void drawInnerShadow(const RoundedRect&, const Color& shadowColor, const IntSize shadowOffset, int shadowBlur, int shadowSpread, Edges clippedEdges = NoEdge);

    // This clip function is used only by <canvas> code. It allows
    // implementations to handle clipping on the canvas differently since
    // the discipline is different.
    void canvasClip(const Path&, WindRule = RULE_EVENODD);
    void clipOut(const Path&);

    // ---------- Transformation methods -----------------
    enum IncludeDeviceScale { DefinitelyIncludeDeviceScale, PossiblyIncludeDeviceScale };
    AffineTransform getCTM(IncludeDeviceScale includeScale = PossiblyIncludeDeviceScale) const;
    void concatCTM(const AffineTransform& affine) { concat(affineTransformToSkMatrix(affine)); }
    void setCTM(const AffineTransform& affine) { setMatrix(affineTransformToSkMatrix(affine)); }
    void setMatrix(const SkMatrix&);

    void scale(const FloatSize&);
    void rotate(float angleInRadians);
    void translate(const FloatSize& size) { translate(size.width(), size.height()); }
    void translate(float x, float y);

    // This function applies the device scale factor to the context, making the context capable of
    // acting as a base-level context for a HiDPI environment.
    void applyDeviceScaleFactor(float deviceScaleFactor) { scale(FloatSize(deviceScaleFactor, deviceScaleFactor)); }
    // ---------- End transformation methods -----------------

    // URL drawing
    void setURLForRect(const KURL&, const IntRect&);
    void setURLFragmentForRect(const String& name, const IntRect&);
    void addURLTargetAtPoint(const String& name, const IntPoint&);
    bool supportsURLFragments() { return printing(); }

    // Create an image buffer compatible with this context, with suitable resolution
    // for drawing into the buffer and then into this context.
    PassOwnPtr<ImageBuffer> createCompatibleBuffer(const IntSize&, OpacityMode = NonOpaque) const;

    static void adjustLineToPixelBoundaries(FloatPoint& p1, FloatPoint& p2, float strokeWidth, StrokeStyle);

    void beginAnnotation(const char*, const char*, const String&, const String&, const String&);
    void endAnnotation();

private:
    static void addCornerArc(SkPath*, const SkRect&, const IntSize&, int);
    static void setPathFromConvexPoints(SkPath*, size_t, const FloatPoint*);
    static void setRadii(SkVector*, IntSize, IntSize, IntSize, IntSize);

    static PassRefPtr<SkColorFilter> WebCoreColorFilterToSkiaColorFilter(ColorFilter);

#if OS(MACOSX)
    static inline int getFocusRingOutset(int offset) { return offset + 2; }
#else
    static inline int getFocusRingOutset(int offset) { return 0; }
    static const SkPMColor lineColors(int);
    static const SkPMColor antiColors1(int);
    static const SkPMColor antiColors2(int);
    static void draw1xMarker(SkBitmap*, int);
    static void draw2xMarker(SkBitmap*, int);
#endif

    // Return value % max, but account for value possibly being negative.
    static int fastMod(int value, int max)
    {
        bool isNeg = false;
        if (value < 0) {
            value = -value;
            isNeg = true;
        }
        if (value >= max)
            value %= max;
        if (isNeg)
            value = -value;
        return value;
    }

    // Sets up the common flags on a paint for antialiasing, effects, etc.
    // This is implicitly called by setupPaintFill and setupPaintStroke, but
    // you may wish to call it directly sometimes if you don't want that other
    // behavior.
    void setupPaintCommon(SkPaint*) const;

    // Helpers for drawing a focus ring (drawFocusRing)
    void drawOuterPath(const SkPath&, SkPaint&, int);
    void drawInnerPath(const SkPath&, SkPaint&, int);

    // SkCanvas wrappers.
    bool isDrawingToLayer() const { return m_canvas->isDrawingToLayer(); }

    bool clipPath(const SkPath&, AntiAliasingMode = NotAntiAliased, SkRegion::Op = SkRegion::kIntersect_Op);
    bool clipRRect(const SkRRect&, AntiAliasingMode = NotAntiAliased, SkRegion::Op = SkRegion::kIntersect_Op);

    bool concat(const SkMatrix&);

    // common code between setupPaintFor[Filling,Stroking]
    void setupShader(SkPaint*, Gradient*, Pattern*, SkColor) const;

    // Apply deferred saves
    void realizeSave(SkCanvas::SaveFlags flags)
    {
        if (m_deferredSaveFlags & flags) {
            m_canvas->save((SkCanvas::SaveFlags)m_deferredSaveFlags);
            m_deferredSaveFlags = 0;
        }
    }

    void didDrawTextInRect(const SkRect& textRect);

    void fillRectWithRoundedHole(const IntRect&, const RoundedRect& roundedHoleRect, const Color&);

    bool isRecording() const;

    // null indicates painting is disabled. Never delete this object.
    SkCanvas* m_canvas;

    // Pointer to the current drawing state. This is a cached value of m_stateStack.last().
    GraphicsContextState* m_state;
    // States stack. Enables local drawing state change with save()/restore() calls.
    // Use OwnPtr to avoid copying the large state structure.
    Vector<OwnPtr<GraphicsContextState> > m_stateStack;

    // Currently pending save flags.
    // FIXME: While defined as a bitmask of SkCanvas::SaveFlags, this is mostly used as a bool.
    //        It will come in handy when adding granular save() support (clip vs. matrix vs. paint).
    // crbug.com/233713
    struct DeferredSaveState;
    unsigned m_deferredSaveFlags;
    Vector<DeferredSaveState> m_saveStateStack;

    AnnotationModeFlags m_annotationMode;

    struct RecordingState;
    Vector<RecordingState> m_recordingStateStack;

#if !ASSERT_DISABLED
    unsigned m_annotationCount;
    unsigned m_layerCount;
#endif
    // Tracks the region painted opaque via the GraphicsContext.
    OpaqueRegionSkia m_opaqueRegion;
    bool m_trackOpaqueRegion : 1;

    // Tracks the region where text is painted via the GraphicsContext.
    bool m_trackTextRegion : 1;
    SkRect m_textRegion;

    // Are we on a high DPI display? If so, spelling and grammar markers are larger.
    bool m_useHighResMarker : 1;
    // FIXME: Make this go away: crbug.com/236892
    bool m_updatingControlTints : 1;
    bool m_accelerated : 1;
    bool m_isCertainlyOpaque : 1;
    bool m_printing : 1;
};

} // namespace WebCore

#endif // GraphicsContext_h
