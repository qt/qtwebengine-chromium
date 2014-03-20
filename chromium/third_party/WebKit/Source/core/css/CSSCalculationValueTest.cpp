/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
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

#include "config.h"
#include "core/css/CSSCalculationValue.h"

#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSToLengthConversionData.h"
#include "core/rendering/style/RenderStyle.h"
#include "core/rendering/style/StyleInheritedData.h"

#include <gtest/gtest.h>

using namespace WebCore;

namespace {

void testExpression(PassRefPtr<CSSCalcExpressionNode> expression, const RenderStyle* style)
{
    EXPECT_TRUE(
        expression->equals(
            *CSSCalcValue::createExpressionNode(
                expression->toCalcValue(CSSToLengthConversionData(style, style)).get(),
                style->effectiveZoom()).get()));
}

TEST(CSSCalculationValue, CreateExpressionNodeFromLength)
{
    RefPtr<RenderStyle> style = RenderStyle::create();
    RefPtr<CSSCalcExpressionNode> expected;
    RefPtr<CSSCalcExpressionNode> actual;

    expected = CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX), true);
    actual = CSSCalcValue::createExpressionNode(Length(10, WebCore::Fixed), style->effectiveZoom());
    EXPECT_TRUE(actual->equals(*expected.get()));

    expected = CSSCalcValue::createExpressionNode(
        CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX), true),
        CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(20, CSSPrimitiveValue::CSS_PX), true),
        CalcAdd);
    actual = CSSCalcValue::createExpressionNode(
        Length(CalculationValue::create(
            adoptPtr(new CalcExpressionBinaryOperation(
                adoptPtr(new CalcExpressionLength(Length(10, WebCore::Fixed))),
                adoptPtr(new CalcExpressionLength(Length(20, WebCore::Fixed))),
                CalcAdd)),
            ValueRangeAll)),
        style->effectiveZoom());
    EXPECT_TRUE(actual->equals(*expected.get()));

    expected = CSSCalcValue::createExpressionNode(
        CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(30, CSSPrimitiveValue::CSS_PX), true),
        CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(40, CSSPrimitiveValue::CSS_NUMBER), true),
        CalcMultiply);
    actual = CSSCalcValue::createExpressionNode(
        Length(CalculationValue::create(
            adoptPtr(new CalcExpressionBinaryOperation(
                adoptPtr(new CalcExpressionLength(Length(30, WebCore::Fixed))),
                adoptPtr(new CalcExpressionNumber(40)),
                CalcMultiply)),
            ValueRangeAll)),
        style->effectiveZoom());
    EXPECT_TRUE(actual->equals(*expected.get()));

    expected = CSSCalcValue::createExpressionNode(
        CSSCalcValue::createExpressionNode(
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(50, CSSPrimitiveValue::CSS_PX), true),
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(0.25, CSSPrimitiveValue::CSS_NUMBER), false),
            CalcMultiply),
        CSSCalcValue::createExpressionNode(
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(60, CSSPrimitiveValue::CSS_PX), true),
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(0.75, CSSPrimitiveValue::CSS_NUMBER), false),
            CalcMultiply),
        CalcAdd);
    actual = CSSCalcValue::createExpressionNode(
        Length(CalculationValue::create(
            adoptPtr(new CalcExpressionBlendLength(Length(50, WebCore::Fixed), Length(60, WebCore::Fixed), 0.75)),
            ValueRangeAll)),
        style->effectiveZoom());
    EXPECT_TRUE(actual->equals(*expected.get()));
}

TEST(CSSCalculationValue, CreateExpressionNodeFromLengthFromExpressionNode)
{
    RefPtr<CSSCalcExpressionNode> expression;
    RefPtr<RenderStyle> style = RenderStyle::createDefaultStyle();
    style->setEffectiveZoom(5);

    testExpression(
        CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX), true),
        style.get());

    testExpression(
        CSSCalcValue::createExpressionNode(
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(10, CSSPrimitiveValue::CSS_PX), true),
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(20, CSSPrimitiveValue::CSS_PX), true),
            CalcAdd),
        style.get());

    testExpression(
        CSSCalcValue::createExpressionNode(
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(30, CSSPrimitiveValue::CSS_PX), true),
            CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(40, CSSPrimitiveValue::CSS_NUMBER), true),
            CalcMultiply),
        style.get());

    testExpression(
        CSSCalcValue::createExpressionNode(
            CSSCalcValue::createExpressionNode(
                CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(50, CSSPrimitiveValue::CSS_PX), true),
                CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(0.25, CSSPrimitiveValue::CSS_NUMBER), false),
                CalcMultiply),
            CSSCalcValue::createExpressionNode(
                CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(60, CSSPrimitiveValue::CSS_PX), true),
                CSSCalcValue::createExpressionNode(CSSPrimitiveValue::create(0.75, CSSPrimitiveValue::CSS_NUMBER), false),
                CalcMultiply),
            CalcAdd),
        style.get());
}

}
