/*
 * Copyright (C) 2011, 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CSSCalculationValue_h
#define CSSCalculationValue_h

#include "core/css/CSSParserValues.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSValue.h"
#include "core/platform/CalculationValue.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class CSSParserValueList;
class CSSValueList;
class CalculationValue;
class CalcExpressionNode;
class RenderStyle;
struct Length;

enum CalculationCategory {
    CalcNumber = 0,
    CalcLength,
    CalcPercent,
    CalcPercentNumber,
    CalcPercentLength,
    CalcVariable,
    CalcOther
};

class CSSCalcExpressionNode : public RefCounted<CSSCalcExpressionNode> {
public:
    enum Type {
        CssCalcPrimitiveValue = 1,
        CssCalcBinaryOperation
    };

    virtual ~CSSCalcExpressionNode() = 0;
    virtual bool isZero() const = 0;
    virtual PassOwnPtr<CalcExpressionNode> toCalcValue(const RenderStyle*, const RenderStyle* rootStyle, double zoom = 1.0) const = 0;
    virtual double doubleValue() const = 0;
    virtual double computeLengthPx(const RenderStyle* currentStyle, const RenderStyle* rootStyle, double multiplier = 1.0, bool computingFontSize = false) const = 0;
    virtual String customCssText() const = 0;
    virtual String serializeResolvingVariables(const HashMap<AtomicString, String>&) const = 0;
    virtual bool hasVariableReference() const = 0;
    virtual bool equals(const CSSCalcExpressionNode& other) const { return m_category == other.m_category && m_isInteger == other.m_isInteger; }
    virtual Type type() const = 0;

    CalculationCategory category() const { return m_category; }
    virtual CSSPrimitiveValue::UnitTypes primitiveType() const = 0;
    bool isInteger() const { return m_isInteger; }

protected:
    CSSCalcExpressionNode(CalculationCategory category, bool isInteger)
        : m_category(category)
        , m_isInteger(isInteger)
    {
    }

    CalculationCategory m_category;
    bool m_isInteger;
};

class CSSCalcValue : public CSSValue {
public:
    static PassRefPtr<CSSCalcValue> create(CSSParserString name, CSSParserValueList*, CalculationPermittedValueRange);
    static PassRefPtr<CSSCalcValue> create(PassRefPtr<CSSCalcExpressionNode>, CalculationPermittedValueRange = CalculationRangeAll);
    static PassRefPtr<CSSCalcValue> create(const CalculationValue* value, const RenderStyle* style) { return adoptRef(new CSSCalcValue(value, style)); }

    static PassRefPtr<CSSCalcExpressionNode> createExpressionNode(PassRefPtr<CSSPrimitiveValue>, bool isInteger = false);
    static PassRefPtr<CSSCalcExpressionNode> createExpressionNode(PassRefPtr<CSSCalcExpressionNode>, PassRefPtr<CSSCalcExpressionNode>, CalcOperator);
    static PassRefPtr<CSSCalcExpressionNode> createExpressionNode(const CalcExpressionNode*, const RenderStyle*);
    static PassRefPtr<CSSCalcExpressionNode> createExpressionNode(const Length&, const RenderStyle*);

    PassRefPtr<CalculationValue> toCalcValue(const RenderStyle* style, const RenderStyle* rootStyle, double zoom = 1.0) const
    {
        return CalculationValue::create(m_expression->toCalcValue(style, rootStyle, zoom), m_nonNegative ? CalculationRangeNonNegative : CalculationRangeAll);
    }
    CalculationCategory category() const { return m_expression->category(); }
    bool isInt() const { return m_expression->isInteger(); }
    double doubleValue() const;
    bool isNegative() const { return m_expression->doubleValue() < 0; }
    CalculationPermittedValueRange permittedValueRange() { return m_nonNegative ? CalculationRangeNonNegative : CalculationRangeAll; }
    double computeLengthPx(const RenderStyle* currentStyle, const RenderStyle* rootStyle, double multiplier = 1.0, bool computingFontSize = false) const;
    CSSCalcExpressionNode* expressionNode() const { return m_expression.get(); }

    String customCssText() const;
    bool equals(const CSSCalcValue&) const;
    String customSerializeResolvingVariables(const HashMap<AtomicString, String>&) const;
    bool hasVariableReference() const;

private:
    CSSCalcValue(PassRefPtr<CSSCalcExpressionNode> expression, CalculationPermittedValueRange range)
        : CSSValue(CalculationClass)
        , m_expression(expression)
        , m_nonNegative(range == CalculationRangeNonNegative)
    {
    }
    CSSCalcValue(const CalculationValue* value, const RenderStyle* style)
        : CSSValue(CalculationClass)
        , m_expression(createExpressionNode(value->expression(), style))
        , m_nonNegative(value->isNonNegative())
    {
    }

    double clampToPermittedRange(double) const;

    const RefPtr<CSSCalcExpressionNode> m_expression;
    const bool m_nonNegative;
};

inline CSSCalcValue* toCSSCalcValue(CSSValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!value || value->isCalculationValue());
    return static_cast<CSSCalcValue*>(value);
}

inline const CSSCalcValue* toCSSCalcValue(const CSSValue* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!value || value->isCalculationValue());
    return static_cast<const CSSCalcValue*>(value);
}

} // namespace WebCore


#endif // CSSCalculationValue_h
