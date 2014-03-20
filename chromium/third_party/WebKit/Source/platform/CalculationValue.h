/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef CalculationValue_h
#define CalculationValue_h

#include "platform/Length.h"
#include "platform/LengthFunctions.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

enum CalcOperator {
    CalcAdd = '+',
    CalcSubtract = '-',
    CalcMultiply = '*',
    CalcDivide = '/'
};

enum CalcExpressionNodeType {
    CalcExpressionNodeUndefined,
    CalcExpressionNodeNumber,
    CalcExpressionNodeLength,
    CalcExpressionNodeBinaryOperation,
    CalcExpressionNodeBlendLength,
};

class PLATFORM_EXPORT CalcExpressionNode {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CalcExpressionNode()
        : m_type(CalcExpressionNodeUndefined)
    {
    }

    virtual ~CalcExpressionNode()
    {
    }

    virtual float evaluate(float maxValue) const = 0;
    virtual bool operator==(const CalcExpressionNode&) const = 0;

    CalcExpressionNodeType type() const { return m_type; }

protected:
    CalcExpressionNodeType m_type;
};

class PLATFORM_EXPORT CalculationValue : public RefCounted<CalculationValue> {
public:
    static PassRefPtr<CalculationValue> create(PassOwnPtr<CalcExpressionNode> value, ValueRange);
    float evaluate(float maxValue) const;

    bool operator==(const CalculationValue& o) const
    {
        return *(m_value.get()) == *(o.m_value.get());
    }

    bool isNonNegative() const { return m_isNonNegative; }
    const CalcExpressionNode* expression() const { return m_value.get(); }

private:
    CalculationValue(PassOwnPtr<CalcExpressionNode> value, ValueRange range)
        : m_value(value)
        , m_isNonNegative(range == ValueRangeNonNegative)
    {
    }

    OwnPtr<CalcExpressionNode> m_value;
    bool m_isNonNegative;
};

class PLATFORM_EXPORT CalcExpressionNumber : public CalcExpressionNode {
public:
    explicit CalcExpressionNumber(float value)
        : m_value(value)
    {
        m_type = CalcExpressionNodeNumber;
    }

    bool operator==(const CalcExpressionNumber& o) const
    {
        return m_value == o.m_value;
    }

    virtual bool operator==(const CalcExpressionNode& o) const OVERRIDE
    {
        return type() == o.type() && *this == static_cast<const CalcExpressionNumber&>(o);
    }

    virtual float evaluate(float) const OVERRIDE
    {
        return m_value;
    }

    float value() const { return m_value; }

private:
    float m_value;
};

inline const CalcExpressionNumber* toCalcExpressionNumber(const CalcExpressionNode* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!value || value->type() == CalcExpressionNodeNumber);
    return static_cast<const CalcExpressionNumber*>(value);
}

class PLATFORM_EXPORT CalcExpressionLength : public CalcExpressionNode {
public:
    explicit CalcExpressionLength(Length length)
        : m_length(length)
    {
        m_type = CalcExpressionNodeLength;
    }

    bool operator==(const CalcExpressionLength& o) const
    {
        return m_length == o.m_length;
    }

    virtual bool operator==(const CalcExpressionNode& o) const OVERRIDE
    {
        return type() == o.type() && *this == static_cast<const CalcExpressionLength&>(o);
    }

    virtual float evaluate(float maxValue) const OVERRIDE
    {
        return floatValueForLength(m_length, maxValue);
    }

    const Length& length() const { return m_length; }

private:
    Length m_length;
};

inline const CalcExpressionLength* toCalcExpressionLength(const CalcExpressionNode* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!value || value->type() == CalcExpressionNodeLength);
    return static_cast<const CalcExpressionLength*>(value);
}

class PLATFORM_EXPORT CalcExpressionBinaryOperation : public CalcExpressionNode {
public:
    CalcExpressionBinaryOperation(PassOwnPtr<CalcExpressionNode> leftSide, PassOwnPtr<CalcExpressionNode> rightSide, CalcOperator op)
        : m_leftSide(leftSide)
        , m_rightSide(rightSide)
        , m_operator(op)
    {
        m_type = CalcExpressionNodeBinaryOperation;
    }

    bool operator==(const CalcExpressionBinaryOperation& o) const
    {
        return m_operator == o.m_operator && *m_leftSide == *o.m_leftSide && *m_rightSide == *o.m_rightSide;
    }

    virtual bool operator==(const CalcExpressionNode& o) const OVERRIDE
    {
        return type() == o.type() && *this == static_cast<const CalcExpressionBinaryOperation&>(o);
    }

    virtual float evaluate(float) const OVERRIDE;

    const CalcExpressionNode* leftSide() const { return m_leftSide.get(); }
    const CalcExpressionNode* rightSide() const { return m_rightSide.get(); }
    CalcOperator getOperator() const { return m_operator; }

private:
    // Disallow the copy constructor. Resolves Windows link errors.
    CalcExpressionBinaryOperation(const CalcExpressionBinaryOperation&);

    OwnPtr<CalcExpressionNode> m_leftSide;
    OwnPtr<CalcExpressionNode> m_rightSide;
    CalcOperator m_operator;
};

inline const CalcExpressionBinaryOperation* toCalcExpressionBinaryOperation(const CalcExpressionNode* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!value || value->type() == CalcExpressionNodeBinaryOperation);
    return static_cast<const CalcExpressionBinaryOperation*>(value);
}

class PLATFORM_EXPORT CalcExpressionBlendLength : public CalcExpressionNode {
public:
    CalcExpressionBlendLength(Length from, Length to, float progress)
        : m_from(from)
        , m_to(to)
        , m_progress(progress)
    {
        m_type = CalcExpressionNodeBlendLength;
    }

    bool operator==(const CalcExpressionBlendLength& o) const
    {
        return m_progress == o.m_progress && m_from == o.m_from && m_to == o.m_to;
    }

    virtual bool operator==(const CalcExpressionNode& o) const OVERRIDE
    {
        return type() == o.type() && *this == static_cast<const CalcExpressionBlendLength&>(o);
    }

    virtual float evaluate(float maxValue) const OVERRIDE
    {
        return (1.0f - m_progress) * floatValueForLength(m_from, maxValue) + m_progress * floatValueForLength(m_to, maxValue);
    }

    const Length& from() const { return m_from; }
    const Length& to() const { return m_to; }
    float progress() const { return m_progress; }

private:
    Length m_from;
    Length m_to;
    float m_progress;
};

inline const CalcExpressionBlendLength* toCalcExpressionBlendLength(const CalcExpressionNode* value)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!value || value->type() == CalcExpressionNodeBlendLength);
    return static_cast<const CalcExpressionBlendLength*>(value);
}

} // namespace WebCore

#endif // CalculationValue_h
