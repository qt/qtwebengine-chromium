// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/animation/interpolable_value.h"

#include <memory>

#include "third_party/blink/renderer/core/animation/css_color_interpolation_type.h"
#include "third_party/blink/renderer/core/animation/interpolable_style_color.h"

namespace blink {

bool InterpolableNumber::Equals(const InterpolableValue& other) const {
  return value_.Value() == To<InterpolableNumber>(other).value_.Value();
}

bool InterpolableList::Equals(const InterpolableValue& other) const {
  const auto& other_list = To<InterpolableList>(other);
  if (length() != other_list.length())
    return false;
  for (wtf_size_t i = 0; i < length(); i++) {
    if (!values_[i]->Equals(*other_list.values_[i]))
      return false;
  }
  return true;
}

double InlinedInterpolableNumber::Interpolate(double to,
                                              const double progress) const {
  if (progress == 0 || value_ == to) {
    return value_;
  } else if (progress == 1) {
    return to;
  } else {
    return value_ * (1 - progress) + to * progress;
  }
}

void InterpolableNumber::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsNumber());
}

void InterpolableNumber::Interpolate(const InterpolableValue& to,
                                     const double progress,
                                     InterpolableValue& result) const {
  const auto& to_number = To<InterpolableNumber>(to);
  auto& result_number = To<InterpolableNumber>(result);

  result_number.Set(value_.Interpolate(to_number.Value(), progress));
}

void InterpolableList::AssertCanInterpolateWith(
    const InterpolableValue& other) const {
  DCHECK(other.IsList());
  DCHECK_EQ(To<InterpolableList>(other).length(), length());
}

void InterpolableList::Interpolate(const InterpolableValue& to,
                                   const double progress,
                                   InterpolableValue& result) const {
  const auto& to_list = To<InterpolableList>(to);
  auto& result_list = To<InterpolableList>(result);

  for (wtf_size_t i = 0; i < length(); i++) {
    DCHECK(values_[i]);
    DCHECK(to_list.values_[i]);
    if (values_[i]->IsStyleColor() || to_list.values_[i]->IsStyleColor() ||
        result_list.values_[i]->IsStyleColor()) {
      CSSColorInterpolationType::EnsureInterpolableStyleColor(result_list, i);
      InterpolableStyleColor::Interpolate(*values_[i], *(to_list.values_[i]),
                                          progress, *(result_list.values_[i]));
      continue;
    }
    values_[i]->Interpolate(*(to_list.values_[i]), progress,
                            *(result_list.values_[i]));
  }
}

InterpolableList* InterpolableList::RawCloneAndZero() const {
  auto* result = MakeGarbageCollected<InterpolableList>(length());
  for (wtf_size_t i = 0; i < length(); i++) {
    result->Set(i, values_[i]->CloneAndZero());
  }
  return result;
}

void InterpolableNumber::Scale(double scale) {
  value_.Scale(scale);
}

void InterpolableList::Scale(double scale) {
  for (wtf_size_t i = 0; i < length(); i++)
    values_[i]->Scale(scale);
}

void InterpolableNumber::Add(const InterpolableValue& other) {
  value_.Add(To<InterpolableNumber>(other).value_.Value());
}

void InterpolableList::Add(const InterpolableValue& other) {
  const auto& other_list = To<InterpolableList>(other);
  DCHECK_EQ(other_list.length(), length());
  for (wtf_size_t i = 0; i < length(); i++)
    values_[i]->Add(*other_list.values_[i]);
}

void InterpolableList::ScaleAndAdd(double scale,
                                   const InterpolableValue& other) {
  const auto& other_list = To<InterpolableList>(other);
  DCHECK_EQ(other_list.length(), length());
  for (wtf_size_t i = 0; i < length(); i++)
    values_[i]->ScaleAndAdd(scale, *other_list.values_[i]);
}

}  // namespace blink
