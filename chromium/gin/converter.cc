// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/converter.h"

#include "v8/include/v8.h"

using v8::ArrayBuffer;
using v8::Boolean;
using v8::External;
using v8::Function;
using v8::Handle;
using v8::Integer;
using v8::Isolate;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

namespace gin {

Handle<Value> Converter<bool>::ToV8(Isolate* isolate, bool val) {
  return Boolean::New(isolate, val).As<Value>();
}

bool Converter<bool>::FromV8(Isolate* isolate, Handle<Value> val, bool* out) {
  *out = val->BooleanValue();
  return true;
}

Handle<Value> Converter<int32_t>::ToV8(Isolate* isolate, int32_t val) {
  return Integer::New(isolate, val).As<Value>();
}

bool Converter<int32_t>::FromV8(Isolate* isolate, Handle<Value> val,
                                int32_t* out) {
  if (!val->IsInt32())
    return false;
  *out = val->Int32Value();
  return true;
}

Handle<Value> Converter<uint32_t>::ToV8(Isolate* isolate, uint32_t val) {
  return Integer::NewFromUnsigned(isolate, val).As<Value>();
}

bool Converter<uint32_t>::FromV8(Isolate* isolate, Handle<Value> val,
                                 uint32_t* out) {
  if (!val->IsUint32())
    return false;
  *out = val->Uint32Value();
  return true;
}

Handle<Value> Converter<int64_t>::ToV8(Isolate* isolate, int64_t val) {
  return Number::New(isolate, static_cast<double>(val)).As<Value>();
}

bool Converter<int64_t>::FromV8(Isolate* isolate, Handle<Value> val,
                                int64_t* out) {
  if (!val->IsNumber())
    return false;
  // Even though IntegerValue returns int64_t, JavaScript cannot represent
  // the full precision of int64_t, which means some rounding might occur.
  *out = val->IntegerValue();
  return true;
}

Handle<Value> Converter<uint64_t>::ToV8(Isolate* isolate, uint64_t val) {
  return Number::New(isolate, static_cast<double>(val)).As<Value>();
}

bool Converter<uint64_t>::FromV8(Isolate* isolate, Handle<Value> val,
                                 uint64_t* out) {
  if (!val->IsNumber())
    return false;
  *out = static_cast<uint64_t>(val->IntegerValue());
  return true;
}

Handle<Value> Converter<double>::ToV8(Isolate* isolate, double val) {
  return Number::New(isolate, val).As<Value>();
}

bool Converter<double>::FromV8(Isolate* isolate, Handle<Value> val,
                               double* out) {
  if (!val->IsNumber())
    return false;
  *out = val->NumberValue();
  return true;
}

Handle<Value> Converter<base::StringPiece>::ToV8(
    Isolate* isolate, const base::StringPiece& val) {
  return String::NewFromUtf8(isolate, val.data(), String::kNormalString,
                             static_cast<uint32_t>(val.length()));
}

Handle<Value> Converter<std::string>::ToV8(Isolate* isolate,
                                           const std::string& val) {
  return Converter<base::StringPiece>::ToV8(isolate, val);
}

bool Converter<std::string>::FromV8(Isolate* isolate, Handle<Value> val,
                                    std::string* out) {
  if (!val->IsString())
    return false;
  Handle<String> str = Handle<String>::Cast(val);
  int length = str->Utf8Length();
  out->resize(length);
  str->WriteUtf8(&(*out)[0], length, NULL, String::NO_NULL_TERMINATION);
  return true;
}

bool Converter<Handle<Function> >::FromV8(Isolate* isolate, Handle<Value> val,
                                          Handle<Function>* out) {
  if (!val->IsFunction())
    return false;
  *out = Handle<Function>::Cast(val);
  return true;
}

Handle<Value> Converter<Handle<Object> >::ToV8(Isolate* isolate,
                                               Handle<Object> val) {
  return val.As<Value>();
}

bool Converter<Handle<Object> >::FromV8(Isolate* isolate, Handle<Value> val,
                                        Handle<Object>* out) {
  if (!val->IsObject())
    return false;
  *out = Handle<Object>::Cast(val);
  return true;
}

Handle<Value> Converter<Handle<ArrayBuffer> >::ToV8(Isolate* isolate,
                                                    Handle<ArrayBuffer> val) {
  return val.As<Value>();
}

bool Converter<Handle<ArrayBuffer> >::FromV8(Isolate* isolate,
                                             Handle<Value> val,
                                             Handle<ArrayBuffer>* out) {
  if (!val->IsArrayBuffer())
    return false;
  *out = Handle<ArrayBuffer>::Cast(val);
  return true;
}

Handle<Value> Converter<Handle<External> >::ToV8(Isolate* isolate,
                                                 Handle<External> val) {
  return val.As<Value>();
}

bool Converter<Handle<External> >::FromV8(Isolate* isolate,
                                          v8::Handle<Value> val,
                                          Handle<External>* out) {
  if (!val->IsExternal())
    return false;
  *out = Handle<External>::Cast(val);
  return true;
}

Handle<Value> Converter<Handle<Value> >::ToV8(Isolate* isolate,
                                              Handle<Value> val) {
  return val;
}

bool Converter<Handle<Value> >::FromV8(Isolate* isolate, Handle<Value> val,
                                       Handle<Value>* out) {
  *out = val;
  return true;
}

v8::Handle<v8::String> StringToSymbol(v8::Isolate* isolate,
                                      const base::StringPiece& val) {
  return String::NewFromUtf8(isolate,
                             val.data(),
                             String::kInternalizedString,
                             static_cast<uint32_t>(val.length()));
}

std::string V8ToString(v8::Handle<v8::Value> value) {
  if (value.IsEmpty())
    return std::string();
  std::string result;
  if (!ConvertFromV8(NULL, value, &result))
    return std::string();
  return result;
}

}  // namespace gin
