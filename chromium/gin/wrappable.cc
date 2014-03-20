// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/wrappable.h"

#include "base/logging.h"
#include "gin/per_isolate_data.h"

namespace gin {

WrappableBase::WrappableBase() {
}

WrappableBase::~WrappableBase() {
  wrapper_.Reset();
}

v8::Handle<v8::Object> WrappableBase::GetWrapperImpl(
    v8::Isolate* isolate, WrapperInfo* wrapper_info) {
    if (wrapper_.IsEmpty())
      CreateWrapper(isolate, wrapper_info);
    return v8::Local<v8::Object>::New(isolate, wrapper_);
}

void WrappableBase::WeakCallback(
    const v8::WeakCallbackData<v8::Object, WrappableBase>& data) {
  WrappableBase* wrappable = data.GetParameter();
  wrappable->wrapper_.Reset();
  delete wrappable;
}

v8::Handle<v8::Object> WrappableBase::CreateWrapper(v8::Isolate* isolate,
                                                    WrapperInfo* info) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  v8::Local<v8::ObjectTemplate> templ = data->GetObjectTemplate(info);
  CHECK(!templ.IsEmpty());  // Don't forget to register an object template.
  CHECK_EQ(kNumberOfInternalFields, templ->InternalFieldCount());
  v8::Handle<v8::Object> wrapper = templ->NewInstance();
  wrapper->SetAlignedPointerInInternalField(kWrapperInfoIndex, info);
  wrapper->SetAlignedPointerInInternalField(kEncodedValueIndex, this);
  wrapper_.Reset(isolate, wrapper);
  wrapper_.SetWeak(this, WeakCallback);
  return wrapper;
}

namespace internal {

void* FromV8Impl(v8::Isolate* isolate, v8::Handle<v8::Value> val,
                 WrapperInfo* wrapper_info) {
  if (!val->IsObject())
    return NULL;
  v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(val);
  WrapperInfo* info = WrapperInfo::From(obj);

  // If this fails, the object is not managed by Gin. It is either a normal JS
  // object that's not wrapping any external C++ object, or it is wrapping some
  // C++ object, but that object isn't managed by Gin (maybe Blink).
  if (!info)
    return NULL;

  // If this fails, the object is managed by Gin, but it's not wrapping an
  // instance of the C++ class associated with wrapper_info.
  if (info != wrapper_info)
    return NULL;

  return obj->GetAlignedPointerFromInternalField(kEncodedValueIndex);
}

}  // namespace internal

}  // namespace gin
