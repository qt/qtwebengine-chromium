// Copyright 2017 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "fxjs/cfx_isolate_wrapper.h"

#include "fxjs/fxv8.h"
#include "v8/include/v8-isolate.h"

CFX_IsolateWrapper::CFX_IsolateWrapper(v8::Isolate* isolate)
    : isolate_(isolate) {}

CFX_IsolateWrapper::~CFX_IsolateWrapper() = default;

v8::Local<v8::Value> CFX_IsolateWrapper::GetObjectPropertyReentrant(
    v8::Local<v8::Object> pObj,
    ByteStringView bsUTF8PropertyName) {
  return fxv8::ReentrantGetObjectPropertyHelper(GetIsolate(), pObj,
                                                bsUTF8PropertyName);
}

std::vector<WideString> CFX_IsolateWrapper::GetObjectPropertyNamesReentrant(
    v8::Local<v8::Object> pObj) {
  return fxv8::ReentrantGetObjectPropertyNamesHelper(GetIsolate(), pObj);
}

void CFX_IsolateWrapper::PutObjectPropertyReentrant(
    v8::Local<v8::Object> pObj,
    ByteStringView bsUTF8PropertyName,
    v8::Local<v8::Value> pPut) {
  fxv8::ReentrantPutObjectPropertyHelper(GetIsolate(), pObj, bsUTF8PropertyName,
                                         pPut);
}

void CFX_IsolateWrapper::DisposeIsolate() {
  if (isolate_) {
    isolate_.ExtractAsDangling()->Dispose();
  }
}

v8::Local<v8::Array> CFX_IsolateWrapper::NewArray() {
  return fxv8::NewArrayHelper(GetIsolate());
}

v8::Local<v8::Object> CFX_IsolateWrapper::NewObject() {
  return fxv8::NewObjectHelper(GetIsolate());
}

void CFX_IsolateWrapper::PutArrayElementReentrant(v8::Local<v8::Array> pArray,
                                                  size_t index,
                                                  v8::Local<v8::Value> pValue) {
  fxv8::ReentrantPutArrayElementHelper(GetIsolate(), pArray, index, pValue);
}

v8::Local<v8::Value> CFX_IsolateWrapper::GetArrayElementReentrant(
    v8::Local<v8::Array> pArray,
    size_t index) {
  return fxv8::ReentrantGetArrayElementHelper(GetIsolate(), pArray, index);
}

size_t CFX_IsolateWrapper::GetArrayLength(v8::Local<v8::Array> pArray) {
  return fxv8::GetArrayLengthHelper(pArray);
}

v8::Local<v8::Number> CFX_IsolateWrapper::NewNumber(int number) {
  return fxv8::NewNumberHelper(GetIsolate(), number);
}

v8::Local<v8::Number> CFX_IsolateWrapper::NewNumber(double number) {
  return fxv8::NewNumberHelper(GetIsolate(), number);
}

v8::Local<v8::Number> CFX_IsolateWrapper::NewNumber(float number) {
  return fxv8::NewNumberHelper(GetIsolate(), number);
}

v8::Local<v8::Boolean> CFX_IsolateWrapper::NewBoolean(bool b) {
  return fxv8::NewBooleanHelper(GetIsolate(), b);
}

v8::Local<v8::String> CFX_IsolateWrapper::NewString(ByteStringView str) {
  return fxv8::NewStringHelper(GetIsolate(), str);
}

v8::Local<v8::String> CFX_IsolateWrapper::NewString(WideStringView str) {
  // Conversion from pdfium's wchar_t wide-strings to v8's uint16_t
  // wide-strings isn't handled by v8, so use UTF8 as a common
  // intermediate format.
  return NewString(FX_UTF8Encode(str).AsStringView());
}

v8::Local<v8::Value> CFX_IsolateWrapper::NewNull() {
  return fxv8::NewNullHelper(GetIsolate());
}

v8::Local<v8::Value> CFX_IsolateWrapper::NewUndefined() {
  return fxv8::NewUndefinedHelper(GetIsolate());
}

v8::Local<v8::Date> CFX_IsolateWrapper::NewDate(double d) {
  return fxv8::NewDateHelper(GetIsolate(), d);
}

int CFX_IsolateWrapper::ToInt32Reentrant(v8::Local<v8::Value> pValue) {
  return fxv8::ReentrantToInt32Helper(GetIsolate(), pValue);
}

bool CFX_IsolateWrapper::ToBooleanReentrant(v8::Local<v8::Value> pValue) {
  return fxv8::ReentrantToBooleanHelper(GetIsolate(), pValue);
}

double CFX_IsolateWrapper::ToDoubleReentrant(v8::Local<v8::Value> pValue) {
  return fxv8::ReentrantToDoubleHelper(GetIsolate(), pValue);
}

WideString CFX_IsolateWrapper::ToWideStringReentrant(
    v8::Local<v8::Value> pValue) {
  return fxv8::ReentrantToWideStringHelper(GetIsolate(), pValue);
}

ByteString CFX_IsolateWrapper::ToByteStringReentrant(
    v8::Local<v8::Value> pValue) {
  return fxv8::ReentrantToByteStringHelper(GetIsolate(), pValue);
}

v8::Local<v8::Object> CFX_IsolateWrapper::ToObjectReentrant(
    v8::Local<v8::Value> pValue) {
  return fxv8::ReentrantToObjectHelper(GetIsolate(), pValue);
}

v8::Local<v8::Array> CFX_IsolateWrapper::ToArrayReentrant(
    v8::Local<v8::Value> pValue) {
  return fxv8::ReentrantToArrayHelper(GetIsolate(), pValue);
}

void CFX_V8IsolateDeleter::operator()(v8::Isolate* ptr) {
  ptr->Dispose();
}
