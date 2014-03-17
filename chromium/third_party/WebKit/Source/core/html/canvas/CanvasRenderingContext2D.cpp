/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012, 2013 Intel Corporation. All rights reserved.
 * Copyright (C) 2013 Adobe Systems Incorporated. All rights reserved.
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

#include "config.h"
#include "core/html/canvas/CanvasRenderingContext2D.h"

#include "CSSPropertyNames.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSParser.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/ExceptionCode.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLCanvasElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/ImageData.h"
#include "core/html/TextMetrics.h"
#include "core/html/canvas/CanvasGradient.h"
#include "core/html/canvas/CanvasPattern.h"
#include "core/html/canvas/CanvasStyle.h"
#include "core/html/canvas/DOMPath.h"
#include "core/frame/ImageBitmap.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderTheme.h"
#include "platform/fonts/FontCache.h"
#include "platform/geometry/FloatQuad.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/DrawLooper.h"
#include "platform/text/TextRun.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "wtf/CheckedArithmetic.h"
#include "wtf/MathExtras.h"
#include "wtf/OwnPtr.h"
#include "wtf/Uint8ClampedArray.h"
#include "wtf/text/StringBuilder.h"

using namespace std;

namespace WebCore {

static const int defaultFontSize = 10;
static const char defaultFontFamily[] = "sans-serif";
static const char defaultFont[] = "10px sans-serif";

CanvasRenderingContext2D::CanvasRenderingContext2D(HTMLCanvasElement* canvas, const Canvas2DContextAttributes* attrs, bool usesCSSCompatibilityParseMode)
    : CanvasRenderingContext(canvas)
    , m_stateStack(1)
    , m_unrealizedSaveCount(0)
    , m_usesCSSCompatibilityParseMode(usesCSSCompatibilityParseMode)
    , m_hasAlpha(!attrs || attrs->alpha())
{
    ScriptWrappable::init(this);
}

void CanvasRenderingContext2D::unwindStateStack()
{
    // Ensure that the state stack in the ImageBuffer's context
    // is cleared before destruction, to avoid assertions in the
    // GraphicsContext dtor.
    if (size_t stackSize = m_stateStack.size()) {
        if (GraphicsContext* context = canvas()->existingDrawingContext()) {
            while (--stackSize)
                context->restore();
        }
    }
}

CanvasRenderingContext2D::~CanvasRenderingContext2D()
{
#if !ASSERT_DISABLED
    unwindStateStack();
#endif
}

bool CanvasRenderingContext2D::isAccelerated() const
{
    if (!canvas()->hasImageBuffer())
        return false;
    GraphicsContext* context = drawingContext();
    return context && context->isAccelerated();
}

void CanvasRenderingContext2D::reset()
{
    unwindStateStack();
    m_stateStack.resize(1);
    m_stateStack.first() = State();
    m_path.clear();
    m_unrealizedSaveCount = 0;
}

// Important: Several of these properties are also stored in GraphicsContext's
// StrokeData. The default values that StrokeData uses may not the same values
// that the canvas 2d spec specifies. Make sure to sync the initial state of the
// GraphicsContext in HTMLCanvasElement::createImageBuffer()!
CanvasRenderingContext2D::State::State()
    : m_strokeStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_fillStyle(CanvasStyle::createFromRGBA(Color::black))
    , m_lineWidth(1)
    , m_lineCap(ButtCap)
    , m_lineJoin(MiterJoin)
    , m_miterLimit(10)
    , m_shadowBlur(0)
    , m_shadowColor(Color::transparent)
    , m_globalAlpha(1)
    , m_globalComposite(CompositeSourceOver)
    , m_globalBlend(blink::WebBlendModeNormal)
    , m_invertibleCTM(true)
    , m_lineDashOffset(0)
    , m_imageSmoothingEnabled(true)
    , m_textAlign(StartTextAlign)
    , m_textBaseline(AlphabeticTextBaseline)
    , m_unparsedFont(defaultFont)
    , m_realizedFont(false)
{
}

CanvasRenderingContext2D::State::State(const State& other)
    : FontSelectorClient()
    , m_unparsedStrokeColor(other.m_unparsedStrokeColor)
    , m_unparsedFillColor(other.m_unparsedFillColor)
    , m_strokeStyle(other.m_strokeStyle)
    , m_fillStyle(other.m_fillStyle)
    , m_lineWidth(other.m_lineWidth)
    , m_lineCap(other.m_lineCap)
    , m_lineJoin(other.m_lineJoin)
    , m_miterLimit(other.m_miterLimit)
    , m_shadowOffset(other.m_shadowOffset)
    , m_shadowBlur(other.m_shadowBlur)
    , m_shadowColor(other.m_shadowColor)
    , m_globalAlpha(other.m_globalAlpha)
    , m_globalComposite(other.m_globalComposite)
    , m_globalBlend(other.m_globalBlend)
    , m_transform(other.m_transform)
    , m_invertibleCTM(other.m_invertibleCTM)
    , m_lineDashOffset(other.m_lineDashOffset)
    , m_imageSmoothingEnabled(other.m_imageSmoothingEnabled)
    , m_textAlign(other.m_textAlign)
    , m_textBaseline(other.m_textBaseline)
    , m_unparsedFont(other.m_unparsedFont)
    , m_font(other.m_font)
    , m_realizedFont(other.m_realizedFont)
{
    if (m_realizedFont)
        m_font.fontSelector()->registerForInvalidationCallbacks(this);
}

CanvasRenderingContext2D::State& CanvasRenderingContext2D::State::operator=(const State& other)
{
    if (this == &other)
        return *this;

    if (m_realizedFont)
        m_font.fontSelector()->unregisterForInvalidationCallbacks(this);

    m_unparsedStrokeColor = other.m_unparsedStrokeColor;
    m_unparsedFillColor = other.m_unparsedFillColor;
    m_strokeStyle = other.m_strokeStyle;
    m_fillStyle = other.m_fillStyle;
    m_lineWidth = other.m_lineWidth;
    m_lineCap = other.m_lineCap;
    m_lineJoin = other.m_lineJoin;
    m_miterLimit = other.m_miterLimit;
    m_shadowOffset = other.m_shadowOffset;
    m_shadowBlur = other.m_shadowBlur;
    m_shadowColor = other.m_shadowColor;
    m_globalAlpha = other.m_globalAlpha;
    m_globalComposite = other.m_globalComposite;
    m_globalBlend = other.m_globalBlend;
    m_transform = other.m_transform;
    m_invertibleCTM = other.m_invertibleCTM;
    m_imageSmoothingEnabled = other.m_imageSmoothingEnabled;
    m_textAlign = other.m_textAlign;
    m_textBaseline = other.m_textBaseline;
    m_unparsedFont = other.m_unparsedFont;
    m_font = other.m_font;
    m_realizedFont = other.m_realizedFont;

    if (m_realizedFont)
        m_font.fontSelector()->registerForInvalidationCallbacks(this);

    return *this;
}

CanvasRenderingContext2D::State::~State()
{
    if (m_realizedFont)
        m_font.fontSelector()->unregisterForInvalidationCallbacks(this);
}

void CanvasRenderingContext2D::State::fontsNeedUpdate(FontSelector* fontSelector)
{
    ASSERT_ARG(fontSelector, fontSelector == m_font.fontSelector());
    ASSERT(m_realizedFont);

    m_font.update(fontSelector);
}

void CanvasRenderingContext2D::realizeSavesLoop()
{
    ASSERT(m_unrealizedSaveCount);
    ASSERT(m_stateStack.size() >= 1);
    GraphicsContext* context = drawingContext();
    do {
        m_stateStack.append(state());
        if (context)
            context->save();
    } while (--m_unrealizedSaveCount);
}

void CanvasRenderingContext2D::restore()
{
    if (m_unrealizedSaveCount) {
        --m_unrealizedSaveCount;
        return;
    }
    ASSERT(m_stateStack.size() >= 1);
    if (m_stateStack.size() <= 1)
        return;
    m_path.transform(state().m_transform);
    m_stateStack.removeLast();
    m_path.transform(state().m_transform.inverse());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->restore();
}

CanvasStyle* CanvasRenderingContext2D::strokeStyle() const
{
    return state().m_strokeStyle.get();
}

void CanvasRenderingContext2D::setStrokeStyle(PassRefPtr<CanvasStyle> prpStyle)
{
    RefPtr<CanvasStyle> style = prpStyle;

    if (!style)
        return;

    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentColor(*style))
        return;

    if (style->isCurrentColor()) {
        if (style->hasOverrideAlpha())
            style = CanvasStyle::createFromRGBA(colorWithOverrideAlpha(currentColor(canvas()), style->overrideAlpha()));
        else
            style = CanvasStyle::createFromRGBA(currentColor(canvas()));
    } else
        checkOrigin(style->canvasPattern());

    realizeSaves();
    modifiableState().m_strokeStyle = style.release();
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_strokeStyle->applyStrokeColor(c);
    modifiableState().m_unparsedStrokeColor = String();
}

CanvasStyle* CanvasRenderingContext2D::fillStyle() const
{
    return state().m_fillStyle.get();
}

void CanvasRenderingContext2D::setFillStyle(PassRefPtr<CanvasStyle> prpStyle)
{
    RefPtr<CanvasStyle> style = prpStyle;

    if (!style)
        return;

    if (state().m_fillStyle && state().m_fillStyle->isEquivalentColor(*style))
        return;

    if (style->isCurrentColor()) {
        if (style->hasOverrideAlpha())
            style = CanvasStyle::createFromRGBA(colorWithOverrideAlpha(currentColor(canvas()), style->overrideAlpha()));
        else
            style = CanvasStyle::createFromRGBA(currentColor(canvas()));
    } else
        checkOrigin(style->canvasPattern());

    realizeSaves();
    modifiableState().m_fillStyle = style.release();
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    state().m_fillStyle->applyFillColor(c);
    modifiableState().m_unparsedFillColor = String();
}

float CanvasRenderingContext2D::lineWidth() const
{
    return state().m_lineWidth;
}

void CanvasRenderingContext2D::setLineWidth(float width)
{
    if (!(std::isfinite(width) && width > 0))
        return;
    if (state().m_lineWidth == width)
        return;
    realizeSaves();
    modifiableState().m_lineWidth = width;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setStrokeThickness(width);
}

String CanvasRenderingContext2D::lineCap() const
{
    return lineCapName(state().m_lineCap);
}

void CanvasRenderingContext2D::setLineCap(const String& s)
{
    LineCap cap;
    if (!parseLineCap(s, cap))
        return;
    if (state().m_lineCap == cap)
        return;
    realizeSaves();
    modifiableState().m_lineCap = cap;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineCap(cap);
}

String CanvasRenderingContext2D::lineJoin() const
{
    return lineJoinName(state().m_lineJoin);
}

void CanvasRenderingContext2D::setLineJoin(const String& s)
{
    LineJoin join;
    if (!parseLineJoin(s, join))
        return;
    if (state().m_lineJoin == join)
        return;
    realizeSaves();
    modifiableState().m_lineJoin = join;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setLineJoin(join);
}

float CanvasRenderingContext2D::miterLimit() const
{
    return state().m_miterLimit;
}

void CanvasRenderingContext2D::setMiterLimit(float limit)
{
    if (!(std::isfinite(limit) && limit > 0))
        return;
    if (state().m_miterLimit == limit)
        return;
    realizeSaves();
    modifiableState().m_miterLimit = limit;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setMiterLimit(limit);
}

float CanvasRenderingContext2D::shadowOffsetX() const
{
    return state().m_shadowOffset.width();
}

void CanvasRenderingContext2D::setShadowOffsetX(float x)
{
    if (!std::isfinite(x))
        return;
    if (state().m_shadowOffset.width() == x)
        return;
    realizeSaves();
    modifiableState().m_shadowOffset.setWidth(x);
    applyShadow();
}

float CanvasRenderingContext2D::shadowOffsetY() const
{
    return state().m_shadowOffset.height();
}

void CanvasRenderingContext2D::setShadowOffsetY(float y)
{
    if (!std::isfinite(y))
        return;
    if (state().m_shadowOffset.height() == y)
        return;
    realizeSaves();
    modifiableState().m_shadowOffset.setHeight(y);
    applyShadow();
}

float CanvasRenderingContext2D::shadowBlur() const
{
    return state().m_shadowBlur;
}

void CanvasRenderingContext2D::setShadowBlur(float blur)
{
    if (!(std::isfinite(blur) && blur >= 0))
        return;
    if (state().m_shadowBlur == blur)
        return;
    realizeSaves();
    modifiableState().m_shadowBlur = blur;
    applyShadow();
}

String CanvasRenderingContext2D::shadowColor() const
{
    return Color(state().m_shadowColor).serialized();
}

void CanvasRenderingContext2D::setShadowColor(const String& color)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    if (state().m_shadowColor == rgba)
        return;
    realizeSaves();
    modifiableState().m_shadowColor = rgba;
    applyShadow();
}

const Vector<float>& CanvasRenderingContext2D::getLineDash() const
{
    return state().m_lineDash;
}

static bool lineDashSequenceIsValid(const Vector<float>& dash)
{
    for (size_t i = 0; i < dash.size(); i++) {
        if (!std::isfinite(dash[i]) || dash[i] < 0)
            return false;
    }
    return true;
}

void CanvasRenderingContext2D::setLineDash(const Vector<float>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;

    realizeSaves();
    modifiableState().m_lineDash = dash;
    // Spec requires the concatenation of two copies the dash list when the
    // number of elements is odd
    if (dash.size() % 2)
        modifiableState().m_lineDash.append(dash);

    applyLineDash();
}

void CanvasRenderingContext2D::setWebkitLineDash(const Vector<float>& dash)
{
    if (!lineDashSequenceIsValid(dash))
        return;

    realizeSaves();
    modifiableState().m_lineDash = dash;

    applyLineDash();
}

float CanvasRenderingContext2D::lineDashOffset() const
{
    return state().m_lineDashOffset;
}

void CanvasRenderingContext2D::setLineDashOffset(float offset)
{
    if (!std::isfinite(offset) || state().m_lineDashOffset == offset)
        return;

    realizeSaves();
    modifiableState().m_lineDashOffset = offset;
    applyLineDash();
}

float CanvasRenderingContext2D::webkitLineDashOffset() const
{
    return lineDashOffset();
}

void CanvasRenderingContext2D::setWebkitLineDashOffset(float offset)
{
    setLineDashOffset(offset);
}

void CanvasRenderingContext2D::applyLineDash() const
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    DashArray convertedLineDash(state().m_lineDash.size());
    for (size_t i = 0; i < state().m_lineDash.size(); ++i)
        convertedLineDash[i] = static_cast<DashArrayElement>(state().m_lineDash[i]);
    c->setLineDash(convertedLineDash, state().m_lineDashOffset);
}

float CanvasRenderingContext2D::globalAlpha() const
{
    return state().m_globalAlpha;
}

void CanvasRenderingContext2D::setGlobalAlpha(float alpha)
{
    if (!(alpha >= 0 && alpha <= 1))
        return;
    if (state().m_globalAlpha == alpha)
        return;
    realizeSaves();
    modifiableState().m_globalAlpha = alpha;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setAlpha(alpha);
}

String CanvasRenderingContext2D::globalCompositeOperation() const
{
    return compositeOperatorName(state().m_globalComposite, state().m_globalBlend);
}

void CanvasRenderingContext2D::setGlobalCompositeOperation(const String& operation)
{
    CompositeOperator op = CompositeSourceOver;
    blink::WebBlendMode blendMode = blink::WebBlendModeNormal;
    if (!parseCompositeAndBlendOperator(operation, op, blendMode))
        return;
    if ((state().m_globalComposite == op) && (state().m_globalBlend == blendMode))
        return;
    realizeSaves();
    modifiableState().m_globalComposite = op;
    modifiableState().m_globalBlend = blendMode;
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    c->setCompositeOperation(op, blendMode);
}

void CanvasRenderingContext2D::setCurrentTransform(const SVGMatrix& matrix)
{
    setTransform(matrix.a(), matrix.b(), matrix.c(), matrix.d(), matrix.e(), matrix.f());
}

void CanvasRenderingContext2D::scale(float sx, float sy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(sx) | !std::isfinite(sy))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.scaleNonUniform(sx, sy);
    if (state().m_transform == newTransform)
        return;

    realizeSaves();

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->scale(FloatSize(sx, sy));
    m_path.transform(AffineTransform().scaleNonUniform(1.0 / sx, 1.0 / sy));
}

void CanvasRenderingContext2D::rotate(float angleInRadians)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(angleInRadians))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.rotate(angleInRadians / piDouble * 180.0);
    if (state().m_transform == newTransform)
        return;

    realizeSaves();

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->rotate(angleInRadians);
    m_path.transform(AffineTransform().rotate(-angleInRadians / piDouble * 180.0));
}

void CanvasRenderingContext2D::translate(float tx, float ty)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(tx) | !std::isfinite(ty))
        return;

    AffineTransform newTransform = state().m_transform;
    newTransform.translate(tx, ty);
    if (state().m_transform == newTransform)
        return;

    realizeSaves();

    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    modifiableState().m_transform = newTransform;
    c->translate(tx, ty);
    m_path.transform(AffineTransform().translate(-tx, -ty));
}

void CanvasRenderingContext2D::transform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    if (!std::isfinite(m11) | !std::isfinite(m21) | !std::isfinite(dx) | !std::isfinite(m12) | !std::isfinite(m22) | !std::isfinite(dy))
        return;

    AffineTransform transform(m11, m12, m21, m22, dx, dy);
    AffineTransform newTransform = state().m_transform * transform;
    if (state().m_transform == newTransform)
        return;

    realizeSaves();

    modifiableState().m_transform = newTransform;
    if (!newTransform.isInvertible()) {
        modifiableState().m_invertibleCTM = false;
        return;
    }

    c->concatCTM(transform);
    m_path.transform(transform.inverse());
}

void CanvasRenderingContext2D::resetTransform()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    AffineTransform ctm = state().m_transform;
    bool invertibleCTM = state().m_invertibleCTM;
    // It is possible that CTM is identity while CTM is not invertible.
    // When CTM becomes non-invertible, realizeSaves() can make CTM identity.
    if (ctm.isIdentity() && invertibleCTM)
        return;

    realizeSaves();
    // resetTransform() resolves the non-invertible CTM state.
    modifiableState().m_transform.makeIdentity();
    modifiableState().m_invertibleCTM = true;
    c->setCTM(canvas()->baseTransform());

    if (invertibleCTM)
        m_path.transform(ctm);
    // When else, do nothing because all transform methods didn't update m_path when CTM became non-invertible.
    // It means that resetTransform() restores m_path just before CTM became non-invertible.
}

void CanvasRenderingContext2D::setTransform(float m11, float m12, float m21, float m22, float dx, float dy)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (!std::isfinite(m11) | !std::isfinite(m21) | !std::isfinite(dx) | !std::isfinite(m12) | !std::isfinite(m22) | !std::isfinite(dy))
        return;

    resetTransform();
    transform(m11, m12, m21, m22, dx, dy);
}

void CanvasRenderingContext2D::setStrokeColor(const String& color)
{
    if (color == state().m_unparsedStrokeColor)
        return;
    realizeSaves();
    setStrokeStyle(CanvasStyle::createFromString(color, &canvas()->document()));
    modifiableState().m_unparsedStrokeColor = color;
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, 1.0f))
        return;
    setStrokeStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, 1.0f));
}

void CanvasRenderingContext2D::setStrokeColor(const String& color, float alpha)
{
    setStrokeStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float grayLevel, float alpha)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, alpha))
        return;
    setStrokeStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, alpha));
}

void CanvasRenderingContext2D::setStrokeColor(float r, float g, float b, float a)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentRGBA(r, g, b, a))
        return;
    setStrokeStyle(CanvasStyle::createFromRGBAChannels(r, g, b, a));
}

void CanvasRenderingContext2D::setStrokeColor(float c, float m, float y, float k, float a)
{
    if (state().m_strokeStyle && state().m_strokeStyle->isEquivalentCMYKA(c, m, y, k, a))
        return;
    setStrokeStyle(CanvasStyle::createFromCMYKAChannels(c, m, y, k, a));
}

void CanvasRenderingContext2D::setFillColor(const String& color)
{
    if (color == state().m_unparsedFillColor)
        return;
    realizeSaves();
    setFillStyle(CanvasStyle::createFromString(color, &canvas()->document()));
    modifiableState().m_unparsedFillColor = color;
}

void CanvasRenderingContext2D::setFillColor(float grayLevel)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, 1.0f))
        return;
    setFillStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, 1.0f));
}

void CanvasRenderingContext2D::setFillColor(const String& color, float alpha)
{
    setFillStyle(CanvasStyle::createFromStringWithOverrideAlpha(color, alpha));
}

void CanvasRenderingContext2D::setFillColor(float grayLevel, float alpha)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(grayLevel, grayLevel, grayLevel, alpha))
        return;
    setFillStyle(CanvasStyle::createFromGrayLevelWithAlpha(grayLevel, alpha));
}

void CanvasRenderingContext2D::setFillColor(float r, float g, float b, float a)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentRGBA(r, g, b, a))
        return;
    setFillStyle(CanvasStyle::createFromRGBAChannels(r, g, b, a));
}

void CanvasRenderingContext2D::setFillColor(float c, float m, float y, float k, float a)
{
    if (state().m_fillStyle && state().m_fillStyle->isEquivalentCMYKA(c, m, y, k, a))
        return;
    setFillStyle(CanvasStyle::createFromCMYKAChannels(c, m, y, k, a));
}

void CanvasRenderingContext2D::beginPath()
{
    m_path.clear();
}

PassRefPtr<DOMPath> CanvasRenderingContext2D::currentPath()
{
    return DOMPath::create(m_path);
}

void CanvasRenderingContext2D::setCurrentPath(DOMPath* path)
{
    if (!path)
        return;
    m_path = path->path();
}

static bool validateRectForCanvas(float& x, float& y, float& width, float& height)
{
    if (!std::isfinite(x) | !std::isfinite(y) | !std::isfinite(width) | !std::isfinite(height))
        return false;

    if (!width && !height)
        return false;

    if (width < 0) {
        width = -width;
        x -= width;
    }

    if (height < 0) {
        height = -height;
        y -= height;
    }

    return true;
}

static bool isFullCanvasCompositeMode(CompositeOperator op)
{
    // See 4.8.11.1.3 Compositing
    // CompositeSourceAtop and CompositeDestinationOut are not listed here as the platforms already
    // implement the specification's behavior.
    return op == CompositeSourceIn || op == CompositeSourceOut || op == CompositeDestinationIn || op == CompositeDestinationAtop;
}

static bool parseWinding(const String& windingRuleString, WindRule& windRule)
{
    if (windingRuleString == "nonzero")
        windRule = RULE_NONZERO;
    else if (windingRuleString == "evenodd")
        windRule = RULE_EVENODD;
    else
        return false;

    return true;
}

void CanvasRenderingContext2D::fill(const String& windingRuleString)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds))
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    if (!m_path.isEmpty()) {
        WindRule windRule = c->fillRule();
        WindRule newWindRule = RULE_NONZERO;
        if (!parseWinding(windingRuleString, newWindRule))
            return;
        c->setFillRule(newWindRule);

        if (isFullCanvasCompositeMode(state().m_globalComposite)) {
            fullCanvasCompositedFill(m_path);
            didDraw(clipBounds);
        } else if (state().m_globalComposite == CompositeCopy) {
            clearCanvas();
            c->fillPath(m_path);
            didDraw(clipBounds);
        } else {
            FloatRect dirtyRect;
            if (computeDirtyRect(m_path.boundingRect(), clipBounds, &dirtyRect)) {
                c->fillPath(m_path);
                didDraw(dirtyRect);
            }
        }

        c->setFillRule(windRule);
    }
}

void CanvasRenderingContext2D::stroke()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize())
        return;

    if (!m_path.isEmpty()) {
        FloatRect bounds = m_path.boundingRect();
        inflateStrokeRect(bounds);
        FloatRect dirtyRect;
        if (computeDirtyRect(bounds, &dirtyRect)) {
            c->strokePath(m_path);
            didDraw(dirtyRect);
        }
    }
}

void CanvasRenderingContext2D::clip(const String& windingRuleString)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    WindRule newWindRule = RULE_NONZERO;
    if (!parseWinding(windingRuleString, newWindRule))
        return;

    realizeSaves();
    c->canvasClip(m_path, newWindRule);
}

bool CanvasRenderingContext2D::isPointInPath(const float x, const float y, const String& windingRuleString)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    if (!state().m_invertibleCTM)
        return false;

    FloatPoint point(x, y);
    AffineTransform ctm = state().m_transform;
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);
    if (!std::isfinite(transformedPoint.x()) || !std::isfinite(transformedPoint.y()))
        return false;

    WindRule windRule = RULE_NONZERO;
    if (!parseWinding(windingRuleString, windRule))
        return false;

    return m_path.contains(transformedPoint, windRule);
}


bool CanvasRenderingContext2D::isPointInStroke(const float x, const float y)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return false;
    if (!state().m_invertibleCTM)
        return false;

    FloatPoint point(x, y);
    AffineTransform ctm = state().m_transform;
    FloatPoint transformedPoint = ctm.inverse().mapPoint(point);
    if (!std::isfinite(transformedPoint.x()) || !std::isfinite(transformedPoint.y()))
        return false;

    StrokeData strokeData;
    strokeData.setThickness(lineWidth());
    strokeData.setLineCap(getLineCap());
    strokeData.setLineJoin(getLineJoin());
    strokeData.setMiterLimit(miterLimit());
    strokeData.setLineDash(getLineDash(), lineDashOffset());
    return m_path.strokeContains(transformedPoint, strokeData);
}

void CanvasRenderingContext2D::clearRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;
    GraphicsContext* context = drawingContext();
    if (!context)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect rect(x, y, width, height);

    FloatRect dirtyRect;
    if (!computeDirtyRect(rect, &dirtyRect))
        return;

    bool saved = false;
    if (shouldDrawShadows()) {
        context->save();
        saved = true;
        context->clearShadow();
    }
    if (state().m_globalAlpha != 1) {
        if (!saved) {
            context->save();
            saved = true;
        }
        context->setAlpha(1);
    }
    if (state().m_globalComposite != CompositeSourceOver) {
        if (!saved) {
            context->save();
            saved = true;
        }
        context->setCompositeOperation(CompositeSourceOver);
    }
    context->clearRect(rect);
    if (saved)
        context->restore();

    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::fillRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds))
        return;

    // from the HTML5 Canvas spec:
    // If x0 = x1 and y0 = y1, then the linear gradient must paint nothing
    // If x0 = x1 and y0 = y1 and r0 = r1, then the radial gradient must paint nothing
    Gradient* gradient = c->fillGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);
    if (rectContainsTransformedRect(rect, clipBounds)) {
        c->fillRect(rect);
        didDraw(clipBounds);
    } else if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedFill(rect);
        didDraw(clipBounds);
    } else if (state().m_globalComposite == CompositeCopy) {
        clearCanvas();
        c->fillRect(rect);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(rect, clipBounds, &dirtyRect)) {
            c->fillRect(rect);
            didDraw(dirtyRect);
        }
    }
}

void CanvasRenderingContext2D::strokeRect(float x, float y, float width, float height)
{
    if (!validateRectForCanvas(x, y, width, height))
        return;

    if (!(state().m_lineWidth >= 0))
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (gradient && gradient->isZeroSize())
        return;

    FloatRect rect(x, y, width, height);

    FloatRect boundingRect = rect;
    boundingRect.inflate(state().m_lineWidth / 2);
    FloatRect dirtyRect;
    if (computeDirtyRect(boundingRect, &dirtyRect)) {
        c->strokeRect(rect, state().m_lineWidth);
        didDraw(dirtyRect);
    }
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur)
{
    setShadow(FloatSize(width, height), blur, Color::transparent);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    setShadow(FloatSize(width, height), blur, rgba);
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, 1));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, const String& color, float alpha)
{
    RGBA32 rgba;
    if (!parseColorOrCurrentColor(rgba, color, canvas()))
        return;
    setShadow(FloatSize(width, height), blur, colorWithOverrideAlpha(rgba, alpha));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float grayLevel, float alpha)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(grayLevel, grayLevel, grayLevel, alpha));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float r, float g, float b, float a)
{
    setShadow(FloatSize(width, height), blur, makeRGBA32FromFloats(r, g, b, a));
}

void CanvasRenderingContext2D::setShadow(float width, float height, float blur, float c, float m, float y, float k, float a)
{
    setShadow(FloatSize(width, height), blur, makeRGBAFromCMYKA(c, m, y, k, a));
}

void CanvasRenderingContext2D::clearShadow()
{
    setShadow(FloatSize(), 0, Color::transparent);
}

void CanvasRenderingContext2D::setShadow(const FloatSize& offset, float blur, RGBA32 color)
{
    if (state().m_shadowOffset == offset && state().m_shadowBlur == blur && state().m_shadowColor == color)
        return;
    bool wasDrawingShadows = shouldDrawShadows();
    realizeSaves();
    modifiableState().m_shadowOffset = offset;
    modifiableState().m_shadowBlur = blur;
    modifiableState().m_shadowColor = color;
    if (!wasDrawingShadows && !shouldDrawShadows())
        return;
    applyShadow();
}

void CanvasRenderingContext2D::applyShadow()
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    if (shouldDrawShadows()) {
        c->setShadow(state().m_shadowOffset, state().m_shadowBlur, state().m_shadowColor,
            DrawLooper::ShadowIgnoresTransforms);
    } else {
        c->clearShadow();
    }
}

bool CanvasRenderingContext2D::shouldDrawShadows() const
{
    return alphaChannel(state().m_shadowColor) && (state().m_shadowBlur || !state().m_shadowOffset.isZero());
}

enum ImageSizeType {
    ImageSizeAfterDevicePixelRatio,
    ImageSizeBeforeDevicePixelRatio
};

static LayoutSize sizeFor(HTMLImageElement* image, ImageSizeType sizeType)
{
    LayoutSize size;
    ImageResource* cachedImage = image->cachedImage();
    if (cachedImage) {
        size = cachedImage->imageSizeForRenderer(image->renderer(), 1.0f); // FIXME: Not sure about this.

        if (sizeType == ImageSizeAfterDevicePixelRatio && image->renderer() && image->renderer()->isRenderImage() && cachedImage->image() && !cachedImage->image()->hasRelativeWidth())
            size.scale(toRenderImage(image->renderer())->imageDevicePixelRatio());
    }
    return size;
}

static IntSize sizeFor(HTMLVideoElement* video)
{
    if (MediaPlayer* player = video->player())
        return player->naturalSize();
    return IntSize();
}

static inline FloatRect normalizeRect(const FloatRect& rect)
{
    return FloatRect(min(rect.x(), rect.maxX()),
        min(rect.y(), rect.maxY()),
        max(rect.width(), -rect.width()),
        max(rect.height(), -rect.height()));
}

static inline void clipRectsToImageRect(const FloatRect& imageRect, FloatRect* srcRect, FloatRect* dstRect)
{
    if (imageRect.contains(*srcRect))
        return;

    // Compute the src to dst transform
    FloatSize scale(dstRect->size().width() / srcRect->size().width(), dstRect->size().height() / srcRect->size().height());
    FloatPoint scaledSrcLocation = srcRect->location();
    scaledSrcLocation.scale(scale.width(), scale.height());
    FloatSize offset = dstRect->location() - scaledSrcLocation;

    srcRect->intersect(imageRect);

    // To clip the destination rectangle in the same proportion, transform the clipped src rect
    *dstRect = *srcRect;
    dstRect->scale(scale.width(), scale.height());
    dstRect->move(offset);
}

void CanvasRenderingContext2D::drawImageInternal(Image* image, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator& op, const blink::WebBlendMode& blendMode)
{
    if (!image)
        return;

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds))
        return;

    if (rectContainsTransformedRect(dstRect, clipBounds)) {
        c->drawImage(image, dstRect, srcRect, op, blendMode);
        didDraw(clipBounds);
    } else if (isFullCanvasCompositeMode(op)) {
        fullCanvasCompositedDrawImage(image, dstRect, srcRect, op);
        didDraw(clipBounds);
    } else if (op == CompositeCopy) {
        clearCanvas();
        c->drawImage(image, dstRect, srcRect, op, blendMode);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(dstRect, &dirtyRect)) {
            c->drawImage(image, dstRect, srcRect, op, blendMode);
            didDraw(dirtyRect);
        }
    }
}

void CanvasRenderingContext2D::drawImage(ImageBitmap* bitmap, float x, float y, ExceptionState& exceptionState)
{
    if (!bitmap) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }
    drawImage(bitmap, x, y, bitmap->width(), bitmap->height(), exceptionState);
}

void CanvasRenderingContext2D::drawImage(ImageBitmap* bitmap,
    float x, float y, float width, float height, ExceptionState& exceptionState)
{
    if (!bitmap) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }
    if (!bitmap->bitmapRect().width() || !bitmap->bitmapRect().height())
        return;

    drawImage(bitmap, 0, 0, bitmap->width(), bitmap->height(), x, y, width, height, exceptionState);
}

void CanvasRenderingContext2D::drawImage(ImageBitmap* bitmap,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionState& exceptionState)
{
    if (!bitmap) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }

    FloatRect srcRect(sx, sy, sw, sh);
    FloatRect dstRect(dx, dy, dw, dh);
    FloatRect bitmapRect = bitmap->bitmapRect();

    if (!std::isfinite(dstRect.x()) || !std::isfinite(dstRect.y()) || !std::isfinite(dstRect.width()) || !std::isfinite(dstRect.height())
        || !std::isfinite(srcRect.x()) || !std::isfinite(srcRect.y()) || !std::isfinite(srcRect.width()) || !std::isfinite(srcRect.height()))
        return;

    if (!dstRect.width() || !dstRect.height())
        return;
    if (!srcRect.width() || !srcRect.height()) {
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError);
        return;
    }

    ASSERT(bitmap->height() && bitmap->width());
    FloatRect normalizedSrcRect = normalizeRect(srcRect);
    FloatRect normalizedDstRect = normalizeRect(dstRect);

    // Clip the rects to where the user thinks that the image is situated.
    clipRectsToImageRect(IntRect(IntPoint(), bitmap->size()), &normalizedSrcRect, &normalizedDstRect);

    FloatRect intersectRect = intersection(bitmapRect, normalizedSrcRect);
    FloatRect actualSrcRect(intersectRect);

    IntPoint bitmapOffset = bitmap->bitmapOffset();
    actualSrcRect.move(bitmapOffset - bitmapRect.location());
    FloatRect imageRect = FloatRect(bitmapOffset, bitmapRect.size());

    FloatRect actualDstRect(FloatPoint(intersectRect.location() - normalizedSrcRect.location()), bitmapRect.size());
    actualDstRect.scale(normalizedDstRect.width() / normalizedSrcRect.width() * intersectRect.width() / bitmapRect.width(),
        normalizedDstRect.height() / normalizedSrcRect.height() * intersectRect.height() / bitmapRect.height());
    actualDstRect.moveBy(normalizedDstRect.location());

    if (!imageRect.intersects(actualSrcRect))
        return;

    RefPtr<Image> imageForRendering = bitmap->bitmapImage();
    if (!imageForRendering)
        return;

    drawImageInternal(imageForRendering.get(), actualSrcRect, actualDstRect, state().m_globalComposite, state().m_globalBlend);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, float x, float y, ExceptionState& exceptionState)
{
    if (!image) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }
    LayoutSize destRectSize = sizeFor(image, ImageSizeAfterDevicePixelRatio);
    drawImage(image, x, y, destRectSize.width(), destRectSize.height(), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image,
    float x, float y, float width, float height, ExceptionState& exceptionState)
{
    if (!image) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }
    LayoutSize sourceRectSize = sizeFor(image, ImageSizeBeforeDevicePixelRatio);
    drawImage(image, FloatRect(0, 0, sourceRectSize.width(), sourceRectSize.height()), FloatRect(x, y, width, height), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionState& exceptionState)
{
    drawImage(image, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, const FloatRect& srcRect, const FloatRect& dstRect, ExceptionState& exceptionState)
{
    drawImage(image, srcRect, dstRect, state().m_globalComposite, state().m_globalBlend, exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLImageElement* image, const FloatRect& srcRect, const FloatRect& dstRect, const CompositeOperator& op, const blink::WebBlendMode& blendMode, ExceptionState& exceptionState)
{
    if (!image) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }

    if (!std::isfinite(dstRect.x()) || !std::isfinite(dstRect.y()) || !std::isfinite(dstRect.width()) || !std::isfinite(dstRect.height())
        || !std::isfinite(srcRect.x()) || !std::isfinite(srcRect.y()) || !std::isfinite(srcRect.width()) || !std::isfinite(srcRect.height()))
        return;

    ImageResource* cachedImage = image->cachedImage();
    if (!cachedImage || !image->complete())
        return;

    LayoutSize size = sizeFor(image, ImageSizeBeforeDevicePixelRatio);
    if (!size.width() || !size.height()) {
        exceptionState.throwUninformativeAndGenericDOMException(InvalidStateError);
        return;
    }

    if (!dstRect.width() || !dstRect.height())
        return;

    FloatRect normalizedSrcRect = normalizeRect(srcRect);
    FloatRect normalizedDstRect = normalizeRect(dstRect);

    FloatRect imageRect = FloatRect(FloatPoint(), size);
    if (!srcRect.width() || !srcRect.height()) {
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError);
        return;
    }
    if (!imageRect.intersects(normalizedSrcRect))
        return;

    clipRectsToImageRect(imageRect, &normalizedSrcRect, &normalizedDstRect);

    checkOrigin(image);

    Image* imageForRendering = cachedImage->imageForRenderer(image->renderer());

    // For images that depend on an unavailable container size, we need to fall back to the intrinsic
    // object size. http://www.w3.org/TR/2dcontext2/#dom-context-2d-drawimage
    // FIXME: Without a specified image size this should resolve against the canvas element's size, see: crbug.com/230163.
    if (!image->renderer() && imageForRendering->usesContainerSize())
        imageForRendering->setContainerSize(imageForRendering->size());

    drawImageInternal(imageForRendering, normalizedSrcRect, normalizedDstRect, op, blendMode);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas, float x, float y, ExceptionState& exceptionState)
{
    drawImage(sourceCanvas, 0, 0, sourceCanvas->width(), sourceCanvas->height(), x, y, sourceCanvas->width(), sourceCanvas->height(), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas,
    float x, float y, float width, float height, ExceptionState& exceptionState)
{
    drawImage(sourceCanvas, FloatRect(0, 0, sourceCanvas->width(), sourceCanvas->height()), FloatRect(x, y, width, height), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionState& exceptionState)
{
    drawImage(sourceCanvas, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLCanvasElement* sourceCanvas, const FloatRect& srcRect,
    const FloatRect& dstRect, ExceptionState& exceptionState)
{
    if (!sourceCanvas) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }

    FloatRect srcCanvasRect = FloatRect(FloatPoint(), sourceCanvas->size());

    if (!srcCanvasRect.width() || !srcCanvasRect.height()) {
        exceptionState.throwUninformativeAndGenericDOMException(InvalidStateError);
        return;
    }

    if (!srcRect.width() || !srcRect.height()) {
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError);
        return;
    }

    FloatRect normalizedSrcRect = normalizeRect(srcRect);
    FloatRect normalizedDstRect = normalizeRect(dstRect);

    if (!srcCanvasRect.intersects(normalizedSrcRect) || !normalizedDstRect.width() || !normalizedDstRect.height())
        return;

    clipRectsToImageRect(srcCanvasRect, &normalizedSrcRect, &normalizedDstRect);

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    // FIXME: Do this through platform-independent GraphicsContext API.
    ImageBuffer* buffer = sourceCanvas->buffer();
    if (!buffer)
        return;

    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds))
        return;

    checkOrigin(sourceCanvas);

    // If we're drawing from one accelerated canvas 2d to another, avoid calling sourceCanvas->makeRenderingResultsAvailable()
    // as that will do a readback to software.
    CanvasRenderingContext* sourceContext = sourceCanvas->renderingContext();
    // FIXME: Implement an accelerated path for drawing from a WebGL canvas to a 2d canvas when possible.
    if (sourceContext && sourceContext->is3d())
        sourceContext->paintRenderingResultsToCanvas();

    if (rectContainsTransformedRect(normalizedDstRect, clipBounds)) {
        c->drawImageBuffer(buffer, normalizedDstRect, normalizedSrcRect, state().m_globalComposite, state().m_globalBlend);
        didDraw(clipBounds);
    } else if (isFullCanvasCompositeMode(state().m_globalComposite)) {
        fullCanvasCompositedDrawImage(buffer, normalizedDstRect, normalizedSrcRect, state().m_globalComposite);
        didDraw(clipBounds);
    } else if (state().m_globalComposite == CompositeCopy) {
        clearCanvas();
        c->drawImageBuffer(buffer, normalizedDstRect, normalizedSrcRect, state().m_globalComposite, state().m_globalBlend);
        didDraw(clipBounds);
    } else {
        FloatRect dirtyRect;
        if (computeDirtyRect(normalizedDstRect, clipBounds, &dirtyRect)) {
            c->drawImageBuffer(buffer, normalizedDstRect, normalizedSrcRect, state().m_globalComposite, state().m_globalBlend);
            didDraw(dirtyRect);
        }
    }

    // Flush canvas's ImageBuffer when drawImage from WebGL to HW accelerated 2d canvas
    if (sourceContext && sourceContext->is3d() && is2d() && isAccelerated() && canvas()->buffer())
        canvas()->buffer()->flush();
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video, float x, float y, ExceptionState& exceptionState)
{
    if (!video) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }
    IntSize size = sizeFor(video);
    drawImage(video, x, y, size.width(), size.height(), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video,
    float x, float y, float width, float height, ExceptionState& exceptionState)
{
    if (!video) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }
    IntSize size = sizeFor(video);
    drawImage(video, FloatRect(0, 0, size.width(), size.height()), FloatRect(x, y, width, height), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh, ExceptionState& exceptionState)
{
    drawImage(video, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), exceptionState);
}

void CanvasRenderingContext2D::drawImage(HTMLVideoElement* video, const FloatRect& srcRect, const FloatRect& dstRect, ExceptionState& exceptionState)
{
    if (!video) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }

    if (video->readyState() == HTMLMediaElement::HAVE_NOTHING || video->readyState() == HTMLMediaElement::HAVE_METADATA)
        return;

    FloatRect videoRect = FloatRect(FloatPoint(), sizeFor(video));
    if (!srcRect.width() || !srcRect.height()) {
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError);
        return;
    }

    FloatRect normalizedSrcRect = normalizeRect(srcRect);
    FloatRect normalizedDstRect = normalizeRect(dstRect);

    if (!videoRect.intersects(normalizedSrcRect) || !normalizedDstRect.width() || !normalizedDstRect.height())
        return;

    clipRectsToImageRect(videoRect, &normalizedSrcRect, &normalizedDstRect);

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;

    checkOrigin(video);

    FloatRect dirtyRect;
    if (!computeDirtyRect(normalizedDstRect, &dirtyRect))
        return;

    GraphicsContextStateSaver stateSaver(*c);
    c->clip(normalizedDstRect);
    c->translate(normalizedDstRect.x(), normalizedDstRect.y());
    c->scale(FloatSize(normalizedDstRect.width() / normalizedSrcRect.width(), normalizedDstRect.height() / normalizedSrcRect.height()));
    c->translate(-normalizedSrcRect.x(), -normalizedSrcRect.y());
    video->paintCurrentFrameInContext(c, IntRect(IntPoint(), sizeFor(video)));
    stateSaver.restore();

    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::drawImageFromRect(HTMLImageElement* image,
    float sx, float sy, float sw, float sh,
    float dx, float dy, float dw, float dh,
    const String& compositeOperation)
{
    CompositeOperator op;
    blink::WebBlendMode blendOp = blink::WebBlendModeNormal;
    if (!parseCompositeAndBlendOperator(compositeOperation, op, blendOp) || blendOp != blink::WebBlendModeNormal)
        op = CompositeSourceOver;

    drawImage(image, FloatRect(sx, sy, sw, sh), FloatRect(dx, dy, dw, dh), op, blink::WebBlendModeNormal, IGNORE_EXCEPTION);
}

void CanvasRenderingContext2D::setAlpha(float alpha)
{
    setGlobalAlpha(alpha);
}

void CanvasRenderingContext2D::setCompositeOperation(const String& operation)
{
    setGlobalCompositeOperation(operation);
}

void CanvasRenderingContext2D::clearCanvas()
{
    FloatRect canvasRect(0, 0, canvas()->width(), canvas()->height());
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    c->save();
    c->setCTM(canvas()->baseTransform());
    c->clearRect(canvasRect);
    c->restore();
}

bool CanvasRenderingContext2D::rectContainsTransformedRect(const FloatRect& rect, const FloatRect& transformedRect) const
{
    FloatQuad quad(rect);
    FloatQuad transformedQuad(transformedRect);
    return state().m_transform.mapQuad(quad).containsQuad(transformedQuad);
}

static void drawImageToContext(Image* image, GraphicsContext* context, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    context->drawImage(image, dest, src, op);
}

static void drawImageToContext(ImageBuffer* imageBuffer, GraphicsContext* context, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    context->drawImageBuffer(imageBuffer, dest, src, op);
}

template<class T> void  CanvasRenderingContext2D::fullCanvasCompositedDrawImage(T* image, const FloatRect& dest, const FloatRect& src, CompositeOperator op)
{
    ASSERT(isFullCanvasCompositeMode(op));

    drawingContext()->beginLayer(1, op);
    drawImageToContext(image, drawingContext(), dest, src, CompositeSourceOver);
    drawingContext()->endLayer();
}

static void fillPrimitive(const FloatRect& rect, GraphicsContext* context)
{
    context->fillRect(rect);
}

static void fillPrimitive(const Path& path, GraphicsContext* context)
{
    context->fillPath(path);
}

template<class T> void CanvasRenderingContext2D::fullCanvasCompositedFill(const T& area)
{
    ASSERT(isFullCanvasCompositeMode(state().m_globalComposite));

    GraphicsContext* c = drawingContext();
    ASSERT(c);
    c->beginLayer(1, state().m_globalComposite);
    CompositeOperator previousOperator = c->compositeOperation();
    c->setCompositeOperation(CompositeSourceOver);
    fillPrimitive(area, c);
    c->setCompositeOperation(previousOperator);
    c->endLayer();
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createLinearGradient(float x0, float y0, float x1, float y1, ExceptionState& exceptionState)
{
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(x1) || !std::isfinite(y1)) {
        exceptionState.throwUninformativeAndGenericDOMException(NotSupportedError);
        return 0;
    }

    RefPtr<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), FloatPoint(x1, y1));
    return gradient.release();
}

PassRefPtr<CanvasGradient> CanvasRenderingContext2D::createRadialGradient(float x0, float y0, float r0, float x1, float y1, float r1, ExceptionState& exceptionState)
{
    if (!std::isfinite(x0) || !std::isfinite(y0) || !std::isfinite(r0) || !std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(r1)) {
        exceptionState.throwUninformativeAndGenericDOMException(NotSupportedError);
        return 0;
    }

    if (r0 < 0 || r1 < 0) {
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError);
        return 0;
    }

    RefPtr<CanvasGradient> gradient = CanvasGradient::create(FloatPoint(x0, y0), r0, FloatPoint(x1, y1), r1);
    return gradient.release();
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLImageElement* image,
    const String& repetitionType, ExceptionState& exceptionState)
{
    if (!image) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return 0;
    }
    bool repeatX, repeatY;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, exceptionState);
    if (exceptionState.hadException())
        return 0;

    if (!image->complete())
        return 0;

    ImageResource* cachedImage = image->cachedImage();
    Image* imageForRendering = cachedImage ? cachedImage->imageForRenderer(image->renderer()) : 0;
    if (!imageForRendering)
        return CanvasPattern::create(Image::nullImage(), repeatX, repeatY, true);

    // We need to synthesize a container size if a renderer is not available to provide one.
    if (!image->renderer() && imageForRendering->usesContainerSize())
        imageForRendering->setContainerSize(imageForRendering->size());

    bool originClean = cachedImage->isAccessAllowed(canvas()->securityOrigin());
    return CanvasPattern::create(imageForRendering, repeatX, repeatY, originClean);
}

PassRefPtr<CanvasPattern> CanvasRenderingContext2D::createPattern(HTMLCanvasElement* canvas,
    const String& repetitionType, ExceptionState& exceptionState)
{
    if (!canvas) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return 0;
    }
    if (!canvas->width() || !canvas->height()) {
        exceptionState.throwUninformativeAndGenericDOMException(InvalidStateError);
        return 0;
    }

    bool repeatX, repeatY;
    CanvasPattern::parseRepetitionType(repetitionType, repeatX, repeatY, exceptionState);
    if (exceptionState.hadException())
        return 0;
    return CanvasPattern::create(canvas->copiedImage(), repeatX, repeatY, canvas->originClean());
}

bool CanvasRenderingContext2D::computeDirtyRect(const FloatRect& localRect, FloatRect* dirtyRect)
{
    FloatRect clipBounds;
    if (!drawingContext()->getTransformedClipBounds(&clipBounds))
        return false;
    return computeDirtyRect(localRect, clipBounds, dirtyRect);
}

bool CanvasRenderingContext2D::computeDirtyRect(const FloatRect& localRect, const FloatRect& transformedClipBounds, FloatRect* dirtyRect)
{
    FloatRect canvasRect = state().m_transform.mapRect(localRect);

    if (alphaChannel(state().m_shadowColor)) {
        FloatRect shadowRect(canvasRect);
        shadowRect.move(state().m_shadowOffset);
        shadowRect.inflate(state().m_shadowBlur);
        canvasRect.unite(shadowRect);
    }

    canvasRect.intersect(transformedClipBounds);
    if (canvasRect.isEmpty())
        return false;

    if (dirtyRect)
        *dirtyRect = canvasRect;

    return true;
}

void CanvasRenderingContext2D::didDraw(const FloatRect& dirtyRect)
{
    if (dirtyRect.isEmpty())
        return;

    // If we are drawing to hardware and we have a composited layer, just call contentChanged().
    if (isAccelerated()) {
        RenderBox* renderBox = canvas()->renderBox();
        if (renderBox && renderBox->hasAcceleratedCompositing()) {
            renderBox->contentChanged(CanvasPixelsChanged);
            canvas()->clearCopiedImage();
            canvas()->notifyObserversCanvasChanged(dirtyRect);
            return;
        }
    }

    canvas()->didDraw(dirtyRect);
}

GraphicsContext* CanvasRenderingContext2D::drawingContext() const
{
    return canvas()->drawingContext();
}

static PassRefPtr<ImageData> createEmptyImageData(const IntSize& size)
{
    Checked<int, RecordOverflow> dataSize = 4;
    dataSize *= size.width();
    dataSize *= size.height();
    if (dataSize.hasOverflowed())
        return 0;

    RefPtr<ImageData> data = ImageData::create(size);
    data->data()->zeroFill();
    return data.release();
}

PassRefPtr<ImageData> CanvasRenderingContext2D::createImageData(PassRefPtr<ImageData> imageData, ExceptionState& exceptionState) const
{
    if (!imageData) {
        exceptionState.throwUninformativeAndGenericDOMException(NotSupportedError);
        return 0;
    }

    return createEmptyImageData(imageData->size());
}

PassRefPtr<ImageData> CanvasRenderingContext2D::createImageData(float sw, float sh, ExceptionState& exceptionState) const
{
    if (!sw || !sh) {
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError);
        return 0;
    }
    if (!std::isfinite(sw) || !std::isfinite(sh)) {
        exceptionState.throwUninformativeAndGenericDOMException(NotSupportedError);
        return 0;
    }

    FloatSize logicalSize(fabs(sw), fabs(sh));
    if (!logicalSize.isExpressibleAsIntSize())
        return 0;

    IntSize size = expandedIntSize(logicalSize);
    if (size.width() < 1)
        size.setWidth(1);
    if (size.height() < 1)
        size.setHeight(1);

    return createEmptyImageData(size);
}

PassRefPtr<ImageData> CanvasRenderingContext2D::webkitGetImageDataHD(float sx, float sy, float sw, float sh, ExceptionState& exceptionState) const
{
    return getImageData(sx, sy, sw, sh, exceptionState);
}

PassRefPtr<ImageData> CanvasRenderingContext2D::getImageData(float sx, float sy, float sw, float sh, ExceptionState& exceptionState) const
{
    if (!canvas()->originClean()) {
        exceptionState.throwSecurityError("The canvas has been tainted by cross-origin data.");
        return 0;
    }

    if (!sw || !sh) {
        exceptionState.throwUninformativeAndGenericDOMException(IndexSizeError);
        return 0;
    }
    if (!std::isfinite(sx) || !std::isfinite(sy) || !std::isfinite(sw) || !std::isfinite(sh)) {
        exceptionState.throwUninformativeAndGenericDOMException(NotSupportedError);
        return 0;
    }

    if (sw < 0) {
        sx += sw;
        sw = -sw;
    }
    if (sh < 0) {
        sy += sh;
        sh = -sh;
    }

    FloatRect logicalRect(sx, sy, sw, sh);
    if (logicalRect.width() < 1)
        logicalRect.setWidth(1);
    if (logicalRect.height() < 1)
        logicalRect.setHeight(1);
    if (!logicalRect.isExpressibleAsIntRect())
        return 0;

    IntRect imageDataRect = enclosingIntRect(logicalRect);
    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return createEmptyImageData(imageDataRect.size());

    RefPtr<Uint8ClampedArray> byteArray = buffer->getUnmultipliedImageData(imageDataRect);
    if (!byteArray)
        return 0;

    return ImageData::create(imageDataRect.size(), byteArray.release());
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, ExceptionState& exceptionState)
{
    if (!data) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }
    putImageData(data, dx, dy, 0, 0, data->width(), data->height(), exceptionState);
}

void CanvasRenderingContext2D::putImageData(ImageData* data, float dx, float dy, float dirtyX, float dirtyY,
    float dirtyWidth, float dirtyHeight, ExceptionState& exceptionState)
{
    if (!data) {
        exceptionState.throwUninformativeAndGenericDOMException(TypeMismatchError);
        return;
    }
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dirtyX) || !std::isfinite(dirtyY) || !std::isfinite(dirtyWidth) || !std::isfinite(dirtyHeight)) {
        exceptionState.throwUninformativeAndGenericDOMException(NotSupportedError);
        return;
    }

    ImageBuffer* buffer = canvas()->buffer();
    if (!buffer)
        return;

    if (dirtyWidth < 0) {
        dirtyX += dirtyWidth;
        dirtyWidth = -dirtyWidth;
    }

    if (dirtyHeight < 0) {
        dirtyY += dirtyHeight;
        dirtyHeight = -dirtyHeight;
    }

    FloatRect clipRect(dirtyX, dirtyY, dirtyWidth, dirtyHeight);
    clipRect.intersect(IntRect(0, 0, data->width(), data->height()));
    IntSize destOffset(static_cast<int>(dx), static_cast<int>(dy));
    IntRect destRect = enclosingIntRect(clipRect);
    destRect.move(destOffset);
    destRect.intersect(IntRect(IntPoint(), buffer->size()));
    if (destRect.isEmpty())
        return;
    IntRect sourceRect(destRect);
    sourceRect.move(-destOffset);

    buffer->putByteArray(Unmultiplied, data->data(), IntSize(data->width(), data->height()), sourceRect, IntPoint(destOffset));

    didDraw(destRect);
}

String CanvasRenderingContext2D::font() const
{
    if (!state().m_realizedFont)
        return defaultFont;

    StringBuilder serializedFont;
    const FontDescription& fontDescription = state().m_font.fontDescription();

    if (fontDescription.italic())
        serializedFont.appendLiteral("italic ");
    if (fontDescription.weight() == FontWeightBold)
        serializedFont.appendLiteral("bold ");
    if (fontDescription.smallCaps() == FontSmallCapsOn)
        serializedFont.appendLiteral("small-caps ");

    serializedFont.appendNumber(fontDescription.computedPixelSize());
    serializedFont.appendLiteral("px");

    const FontFamily& firstFontFamily = fontDescription.family();
    for (const FontFamily* fontFamily = &firstFontFamily; fontFamily; fontFamily = fontFamily->next()) {
        if (fontFamily != &firstFontFamily)
            serializedFont.append(',');

        // FIXME: We should append family directly to serializedFont rather than building a temporary string.
        String family = fontFamily->family();
        if (family.startsWith("-webkit-"))
            family = family.substring(8);
        if (family.contains(' '))
            family = "\"" + family + "\"";

        serializedFont.append(' ');
        serializedFont.append(family);
    }

    return serializedFont.toString();
}

void CanvasRenderingContext2D::setFont(const String& newFont)
{
    MutableStylePropertyMap::iterator i = m_fetchedFonts.find(newFont);
    RefPtr<MutableStylePropertySet> parsedStyle = i != m_fetchedFonts.end() ? i->value : 0;

    if (!parsedStyle) {
        parsedStyle = MutableStylePropertySet::create();
        CSSParserMode mode = m_usesCSSCompatibilityParseMode ? HTMLQuirksMode : HTMLStandardMode;
        CSSParser::parseValue(parsedStyle.get(), CSSPropertyFont, newFont, true, mode, 0);
        m_fetchedFonts.add(newFont, parsedStyle);
    }
    if (parsedStyle->isEmpty())
        return;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);

    // According to http://lists.w3.org/Archives/Public/public-html/2009Jul/0947.html,
    // the "inherit" and "initial" values must be ignored.
    if (fontValue == "inherit" || fontValue == "initial")
        return;

    // The parse succeeded.
    String newFontSafeCopy(newFont); // Create a string copy since newFont can be deleted inside realizeSaves.
    realizeSaves();
    modifiableState().m_unparsedFont = newFontSafeCopy;

    // Map the <canvas> font into the text style. If the font uses keywords like larger/smaller, these will work
    // relative to the canvas.
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    if (RenderStyle* computedStyle = canvas()->computedStyle())
        newStyle->setFontDescription(computedStyle->fontDescription());
    else {
        FontFamily fontFamily;
        fontFamily.setFamily(defaultFontFamily);

        FontDescription defaultFontDescription;
        defaultFontDescription.setFamily(fontFamily);
        defaultFontDescription.setSpecifiedSize(defaultFontSize);
        defaultFontDescription.setComputedSize(defaultFontSize);

        newStyle->setFontDescription(defaultFontDescription);
    }

    newStyle->font().update(newStyle->font().fontSelector());

    // Now map the font property longhands into the style.
    CSSPropertyValue properties[] = {
        CSSPropertyValue(CSSPropertyFontFamily, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontStyle, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontVariant, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontWeight, *parsedStyle),
        CSSPropertyValue(CSSPropertyFontSize, *parsedStyle),
        CSSPropertyValue(CSSPropertyLineHeight, *parsedStyle),
    };

    StyleResolver& styleResolver = canvas()->document().ensureStyleResolver();
    styleResolver.applyPropertiesToStyle(properties, WTF_ARRAY_LENGTH(properties), newStyle.get());

    if (state().m_realizedFont)
        state().m_font.fontSelector()->unregisterForInvalidationCallbacks(&modifiableState());
    modifiableState().m_font = newStyle->font();
    modifiableState().m_font.update(canvas()->document().styleEngine()->fontSelector());
    modifiableState().m_realizedFont = true;
    canvas()->document().styleEngine()->fontSelector()->registerForInvalidationCallbacks(&modifiableState());
}

String CanvasRenderingContext2D::textAlign() const
{
    return textAlignName(state().m_textAlign);
}

void CanvasRenderingContext2D::setTextAlign(const String& s)
{
    TextAlign align;
    if (!parseTextAlign(s, align))
        return;
    if (state().m_textAlign == align)
        return;
    realizeSaves();
    modifiableState().m_textAlign = align;
}

String CanvasRenderingContext2D::textBaseline() const
{
    return textBaselineName(state().m_textBaseline);
}

void CanvasRenderingContext2D::setTextBaseline(const String& s)
{
    TextBaseline baseline;
    if (!parseTextBaseline(s, baseline))
        return;
    if (state().m_textBaseline == baseline)
        return;
    realizeSaves();
    modifiableState().m_textBaseline = baseline;
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, true);
}

void CanvasRenderingContext2D::fillText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, true, maxWidth, true);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y)
{
    drawTextInternal(text, x, y, false);
}

void CanvasRenderingContext2D::strokeText(const String& text, float x, float y, float maxWidth)
{
    drawTextInternal(text, x, y, false, maxWidth, true);
}

PassRefPtr<TextMetrics> CanvasRenderingContext2D::measureText(const String& text)
{
    FontCachePurgePreventer fontCachePurgePreventer;
    RefPtr<TextMetrics> metrics = TextMetrics::create();
    canvas()->document().updateStyleIfNeeded();
    metrics->setWidth(accessFont().width(TextRun(text)));
    return metrics.release();
}

static void replaceCharacterInString(String& text, WTF::CharacterMatchFunctionPtr matchFunction, const String& replacement)
{
    const size_t replacementLength = replacement.length();
    size_t index = 0;
    while ((index = text.find(matchFunction, index)) != kNotFound) {
        text.replace(index, 1, replacement);
        index += replacementLength;
    }
}

void CanvasRenderingContext2D::drawTextInternal(const String& text, float x, float y, bool fill, float maxWidth, bool useMaxWidth)
{
    // accessFont needs the style to be up to date, but updating style can cause script to run,
    // (e.g. due to autofocus) which can free the GraphicsContext, so update style before grabbing
    // the GraphicsContext.
    canvas()->document().updateStyleIfNeeded();

    GraphicsContext* c = drawingContext();
    if (!c)
        return;
    if (!state().m_invertibleCTM)
        return;
    if (!std::isfinite(x) | !std::isfinite(y))
        return;
    if (useMaxWidth && (!std::isfinite(maxWidth) || maxWidth <= 0))
        return;

    // If gradient size is zero, then paint nothing.
    Gradient* gradient = c->strokeGradient();
    if (!fill && gradient && gradient->isZeroSize())
        return;

    gradient = c->fillGradient();
    if (fill && gradient && gradient->isZeroSize())
        return;

    FontCachePurgePreventer fontCachePurgePreventer;

    const Font& font = accessFont();
    const FontMetrics& fontMetrics = font.fontMetrics();
    // According to spec, all the space characters must be replaced with U+0020 SPACE characters.
    String normalizedText = text;
    replaceCharacterInString(normalizedText, isSpaceOrNewline, " ");

    // FIXME: Need to turn off font smoothing.

    RenderStyle* computedStyle = canvas()->computedStyle();
    TextDirection direction = computedStyle ? computedStyle->direction() : LTR;
    bool isRTL = direction == RTL;
    bool override = computedStyle ? isOverride(computedStyle->unicodeBidi()) : false;

    TextRun textRun(normalizedText, 0, 0, TextRun::AllowTrailingExpansion, direction, override, true, TextRun::NoRounding);
    // Draw the item text at the correct point.
    FloatPoint location(x, y);
    switch (state().m_textBaseline) {
    case TopTextBaseline:
    case HangingTextBaseline:
        location.setY(y + fontMetrics.ascent());
        break;
    case BottomTextBaseline:
    case IdeographicTextBaseline:
        location.setY(y - fontMetrics.descent());
        break;
    case MiddleTextBaseline:
        location.setY(y - fontMetrics.descent() + fontMetrics.height() / 2);
        break;
    case AlphabeticTextBaseline:
    default:
         // Do nothing.
        break;
    }

    float fontWidth = font.width(TextRun(normalizedText, 0, 0, TextRun::AllowTrailingExpansion, direction, override));

    useMaxWidth = (useMaxWidth && maxWidth < fontWidth);
    float width = useMaxWidth ? maxWidth : fontWidth;

    TextAlign align = state().m_textAlign;
    if (align == StartTextAlign)
        align = isRTL ? RightTextAlign : LeftTextAlign;
    else if (align == EndTextAlign)
        align = isRTL ? LeftTextAlign : RightTextAlign;

    switch (align) {
    case CenterTextAlign:
        location.setX(location.x() - width / 2);
        break;
    case RightTextAlign:
        location.setX(location.x() - width);
        break;
    default:
        break;
    }

    // The slop built in to this mask rect matches the heuristic used in FontCGWin.cpp for GDI text.
    TextRunPaintInfo textRunPaintInfo(textRun);
    textRunPaintInfo.bounds = FloatRect(location.x() - fontMetrics.height() / 2,
                                        location.y() - fontMetrics.ascent() - fontMetrics.lineGap(),
                                        width + fontMetrics.height(),
                                        fontMetrics.lineSpacing());
    if (!fill)
        inflateStrokeRect(textRunPaintInfo.bounds);

    FloatRect dirtyRect;
    if (!computeDirtyRect(textRunPaintInfo.bounds, &dirtyRect))
        return;

    c->setTextDrawingMode(fill ? TextModeFill : TextModeStroke);
    if (useMaxWidth) {
        GraphicsContextStateSaver stateSaver(*c);
        c->translate(location.x(), location.y());
        // We draw when fontWidth is 0 so compositing operations (eg, a "copy" op) still work.
        c->scale(FloatSize((fontWidth > 0 ? (width / fontWidth) : 0), 1));
        c->drawBidiText(font, textRunPaintInfo, FloatPoint(0, 0), Font::UseFallbackIfFontNotReady);
    } else
        c->drawBidiText(font, textRunPaintInfo, location, Font::UseFallbackIfFontNotReady);

    didDraw(dirtyRect);
}

void CanvasRenderingContext2D::inflateStrokeRect(FloatRect& rect) const
{
    // Fast approximation of the stroke's bounding rect.
    // This yields a slightly oversized rect but is very fast
    // compared to Path::strokeBoundingRect().
    static const float root2 = sqrtf(2);
    float delta = state().m_lineWidth / 2;
    if (state().m_lineJoin == MiterJoin)
        delta *= state().m_miterLimit;
    else if (state().m_lineCap == SquareCap)
        delta *= root2;

    rect.inflate(delta);
}

const Font& CanvasRenderingContext2D::accessFont()
{
    // This needs style to be up to date, but can't assert so because drawTextInternal
    // can invalidate style before this is called (e.g. drawingContext invalidates style).
    if (!state().m_realizedFont)
        setFont(state().m_unparsedFont);
    return state().m_font;
}

blink::WebLayer* CanvasRenderingContext2D::platformLayer() const
{
    return canvas()->buffer() ? canvas()->buffer()->platformLayer() : 0;
}

bool CanvasRenderingContext2D::imageSmoothingEnabled() const
{
    return state().m_imageSmoothingEnabled;
}

void CanvasRenderingContext2D::setImageSmoothingEnabled(bool enabled)
{
    if (enabled == state().m_imageSmoothingEnabled)
        return;

    realizeSaves();
    modifiableState().m_imageSmoothingEnabled = enabled;
    GraphicsContext* c = drawingContext();
    if (c)
        c->setImageInterpolationQuality(enabled ? DefaultInterpolationQuality : InterpolationNone);
}

PassRefPtr<Canvas2DContextAttributes> CanvasRenderingContext2D::getContextAttributes() const
{
    RefPtr<Canvas2DContextAttributes> attributes = Canvas2DContextAttributes::create();
    attributes->setAlpha(m_hasAlpha);
    return attributes.release();
}

void CanvasRenderingContext2D::drawSystemFocusRing(Element* element)
{
    if (!focusRingCallIsValid(m_path, element))
        return;

    updateFocusRingAccessibility(m_path, element);
    // Note: we need to check document->focusedElement() rather than just calling
    // element->focused(), because element->focused() isn't updated until after
    // focus events fire.
    if (element->document().focusedElement() == element)
        drawFocusRing(m_path);
}

bool CanvasRenderingContext2D::drawCustomFocusRing(Element* element)
{
    if (!focusRingCallIsValid(m_path, element))
        return false;

    updateFocusRingAccessibility(m_path, element);

    // Return true if the application should draw the focus ring. The spec allows us to
    // override this for accessibility, but currently Blink doesn't take advantage of this.
    return element->focused();
}

bool CanvasRenderingContext2D::focusRingCallIsValid(const Path& path, Element* element)
{
    if (!state().m_invertibleCTM)
        return false;
    if (path.isEmpty())
        return false;
    if (!element->isDescendantOf(canvas()))
        return false;

    return true;
}

void CanvasRenderingContext2D::updateFocusRingAccessibility(const Path& path, Element* element)
{
    if (!canvas()->renderer())
        return;

    // If accessibility is already enabled in this frame, associate this path's
    // bounding box with the accessible object. Do this even if the element
    // isn't focused because assistive technology might try to explore the object's
    // location before it gets focus.
    if (AXObjectCache* axObjectCache = element->document().existingAXObjectCache()) {
        if (AXObject* obj = axObjectCache->getOrCreate(element)) {
            // Get the bounding rect and apply transformations.
            FloatRect bounds = m_path.boundingRect();
            AffineTransform ctm = state().m_transform;
            FloatRect transformedBounds = ctm.mapRect(bounds);
            LayoutRect elementRect = LayoutRect(transformedBounds);

            // Offset by the canvas rect and set the bounds of the accessible element.
            IntRect canvasRect = canvas()->renderer()->absoluteBoundingBoxRect();
            elementRect.moveBy(canvasRect.location());
            obj->setElementRect(elementRect);

            // Set the bounds of any ancestor accessible elements, up to the canvas element,
            // otherwise this element will appear to not be within its parent element.
            obj = obj->parentObject();
            while (obj && obj->node() != canvas()) {
                obj->setElementRect(elementRect);
                obj = obj->parentObject();
            }
        }
    }
}

void CanvasRenderingContext2D::drawFocusRing(const Path& path)
{
    GraphicsContext* c = drawingContext();
    if (!c)
        return;

    FloatRect dirtyRect;
    if (!computeDirtyRect(path.boundingRect(), &dirtyRect))
        return;

    c->save();
    c->setAlpha(1.0);
    c->clearShadow();
    c->setCompositeOperation(CompositeSourceOver, blink::WebBlendModeNormal);

    // These should match the style defined in html.css.
    Color focusRingColor = RenderTheme::focusRingColor();
    const int focusRingWidth = 5;
    const int focusRingOutline = 0;
    c->drawFocusRing(path, focusRingWidth, focusRingOutline, focusRingColor);

    c->restore();

    didDraw(dirtyRect);
}

} // namespace WebCore
