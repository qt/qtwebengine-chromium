/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "core/html/HTMLProgressElement.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ProgressShadowElement.h"
#include "core/rendering/RenderProgress.h"

namespace WebCore {

using namespace HTMLNames;

const double HTMLProgressElement::IndeterminatePosition = -1;
const double HTMLProgressElement::InvalidPosition = -2;

HTMLProgressElement::HTMLProgressElement(Document& document)
    : LabelableElement(progressTag, document)
    , m_value(0)
{
    ScriptWrappable::init(this);
}

HTMLProgressElement::~HTMLProgressElement()
{
}

PassRefPtr<HTMLProgressElement> HTMLProgressElement::create(Document& document)
{
    RefPtr<HTMLProgressElement> progress = adoptRef(new HTMLProgressElement(document));
    progress->ensureUserAgentShadowRoot();
    return progress.release();
}

RenderObject* HTMLProgressElement::createRenderer(RenderStyle* style)
{
    if (!style->hasAppearance() || hasAuthorShadowRoot())
        return RenderObject::createObject(this, style);

    return new RenderProgress(this);
}

RenderProgress* HTMLProgressElement::renderProgress() const
{
    if (renderer() && renderer()->isProgress())
        return toRenderProgress(renderer());

    RenderObject* renderObject = userAgentShadowRoot()->firstChild()->renderer();
    ASSERT_WITH_SECURITY_IMPLICATION(!renderObject || renderObject->isProgress());
    return toRenderProgress(renderObject);
}

void HTMLProgressElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == valueAttr)
        didElementStateChange();
    else if (name == maxAttr)
        didElementStateChange();
    else
        LabelableElement::parseAttribute(name, value);
}

void HTMLProgressElement::attach(const AttachContext& context)
{
    LabelableElement::attach(context);
    if (RenderProgress* render = renderProgress())
        render->updateFromElement();
}

double HTMLProgressElement::value() const
{
    double value = getFloatingPointAttribute(valueAttr);
    return !std::isfinite(value) || value < 0 ? 0 : std::min(value, max());
}

void HTMLProgressElement::setValue(double value, ExceptionState& exceptionState)
{
    if (!std::isfinite(value)) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(value));
        return;
    }
    setFloatingPointAttribute(valueAttr, std::max(value, 0.));
}

double HTMLProgressElement::max() const
{
    double max = getFloatingPointAttribute(maxAttr);
    return !std::isfinite(max) || max <= 0 ? 1 : max;
}

void HTMLProgressElement::setMax(double max, ExceptionState& exceptionState)
{
    if (!std::isfinite(max)) {
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::notAFiniteNumber(max));
        return;
    }
    // FIXME: The specification says we should ignore the input value if it is inferior or equal to 0.
    setFloatingPointAttribute(maxAttr, max > 0 ? max : 1);
}

double HTMLProgressElement::position() const
{
    if (!isDeterminate())
        return HTMLProgressElement::IndeterminatePosition;
    return value() / max();
}

bool HTMLProgressElement::isDeterminate() const
{
    return fastHasAttribute(valueAttr);
}

void HTMLProgressElement::didElementStateChange()
{
    m_value->setWidthPercentage(position() * 100);
    if (RenderProgress* render = renderProgress()) {
        bool wasDeterminate = render->isDeterminate();
        render->updateFromElement();
        if (wasDeterminate != isDeterminate())
            didAffectSelector(AffectedSelectorIndeterminate);
    }
}

void HTMLProgressElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    ASSERT(!m_value);

    RefPtr<ProgressInnerElement> inner = ProgressInnerElement::create(document());
    inner->setPseudo(AtomicString("-webkit-progress-inner-element", AtomicString::ConstructFromLiteral));
    root.appendChild(inner);

    RefPtr<ProgressBarElement> bar = ProgressBarElement::create(document());
    bar->setPseudo(AtomicString("-webkit-progress-bar", AtomicString::ConstructFromLiteral));
    RefPtr<ProgressValueElement> value = ProgressValueElement::create(document());
    m_value = value.get();
    m_value->setPseudo(AtomicString("-webkit-progress-value", AtomicString::ConstructFromLiteral));
    m_value->setWidthPercentage(HTMLProgressElement::IndeterminatePosition * 100);
    bar->appendChild(m_value);

    inner->appendChild(bar);
}

bool HTMLProgressElement::shouldAppearIndeterminate() const
{
    return !isDeterminate();
}

} // namespace
