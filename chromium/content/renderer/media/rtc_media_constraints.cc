// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "content/renderer/media/rtc_media_constraints.h"

#include <string>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/common/media/media_stream_options.h"
#include "third_party/WebKit/public/platform/WebMediaConstraints.h"
#include "third_party/WebKit/public/platform/WebCString.h"
#include "third_party/WebKit/public/platform/WebString.h"

namespace content {
namespace {

void GetNativeMediaConstraints(
    const blink::WebVector<blink::WebMediaConstraint>& constraints,
    webrtc::MediaConstraintsInterface::Constraints* native_constraints) {
  DCHECK(native_constraints);
  for (size_t i = 0; i < constraints.size(); ++i) {
    webrtc::MediaConstraintsInterface::Constraint new_constraint;
    new_constraint.key = constraints[i].m_name.utf8();
    new_constraint.value = constraints[i].m_value.utf8();

    // Ignore Chrome specific Tab capture constraints.
    if (new_constraint.key == kMediaStreamSource ||
        new_constraint.key == kMediaStreamSourceId)
      continue;

    // Ignore sourceId constraint since that has nothing to do with webrtc.
    if (new_constraint.key == kMediaStreamSourceInfoId)
      continue;

    // Ignore internal constraints set by JS.
    if (StartsWithASCII(
        new_constraint.key,
        webrtc::MediaConstraintsInterface::kInternalConstraintPrefix,
        true)) {
      continue;
    }

    DVLOG(3) << "MediaStreamConstraints:" << new_constraint.key
             << " : " <<  new_constraint.value;
    native_constraints->push_back(new_constraint);
  }
}

}  // namespace

RTCMediaConstraints::RTCMediaConstraints() {}

RTCMediaConstraints::RTCMediaConstraints(
      const blink::WebMediaConstraints& constraints) {
  if (constraints.isNull())
    return;  // Will happen in unit tests.
  blink::WebVector<blink::WebMediaConstraint> mandatory;
  constraints.getMandatoryConstraints(mandatory);
  GetNativeMediaConstraints(mandatory, &mandatory_);
  blink::WebVector<blink::WebMediaConstraint> optional;
  constraints.getOptionalConstraints(optional);
  GetNativeMediaConstraints(optional, &optional_);
}

RTCMediaConstraints::~RTCMediaConstraints() {}

const webrtc::MediaConstraintsInterface::Constraints&
RTCMediaConstraints::GetMandatory() const  {
  return mandatory_;
}

const webrtc::MediaConstraintsInterface::Constraints&
RTCMediaConstraints::GetOptional() const {
  return optional_;
}

void RTCMediaConstraints::AddOptional(const std::string& key,
                                      const std::string& value) {
  optional_.push_back(Constraint(key, value));
}

bool RTCMediaConstraints::AddMandatory(const std::string& key,
                                       const std::string& value,
                                       bool override_if_exists) {
  for (Constraints::iterator iter = mandatory_.begin();
       iter != mandatory_.end();
       ++iter) {
    if (iter->key == key) {
      if (override_if_exists)
        iter->value = value;
      return override_if_exists;
    }
  }
  // The key wasn't found, add it.
  mandatory_.push_back(Constraint(key, value));
  return true;
}

}  // namespace content
