// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/array_buffer.h"

#include <stdlib.h>

namespace gin {

COMPILE_ASSERT(V8_ARRAY_BUFFER_INTERNAL_FIELD_COUNT == 2,
               array_buffers_must_have_two_internal_fields);

static const int kBufferViewPrivateIndex = 0;

// ArrayBufferAllocator -------------------------------------------------------

void* ArrayBufferAllocator::Allocate(size_t length) {
  return calloc(1, length);
}

void* ArrayBufferAllocator::AllocateUninitialized(size_t length) {
  return malloc(length);
}

void ArrayBufferAllocator::Free(void* data, size_t length) {
  free(data);
}

ArrayBufferAllocator* ArrayBufferAllocator::SharedInstance() {
  static ArrayBufferAllocator* instance = new ArrayBufferAllocator();
  return instance;
}

// ArrayBuffer::Private -------------------------------------------------------

// This class exists to solve a tricky lifetime problem. The V8 API doesn't
// want to expose a direct view into the memory behind an array buffer because
// V8 might deallocate that memory during garbage collection. Instead, the V8
// API forces us to externalize the buffer and take ownership of the memory.
// In order to know when to free the memory, we need to figure out both when
// we're done with it and when V8 is done with it.
//
// To determine whether we're done with the memory, every view we have into
// the array buffer takes a reference to the ArrayBuffer::Private object that
// actually owns the memory. To determine when V8 is done with the memory, we
// open a weak handle to the ArrayBuffer object. When we receive the weak
// callback, we know the object is about to be garbage collected and we can
// drop V8's implied reference to the memory.
//
// The final subtlety is that we need every ArrayBuffer into the same array
// buffer to AddRef the same ArrayBuffer::Private. To make that work, we store
// a pointer to the ArrayBuffer::Private object in an internal field of the
// ArrayBuffer object.
//
class ArrayBuffer::Private : public base::RefCounted<ArrayBuffer::Private> {
 public:
  static scoped_refptr<Private> From(v8::Isolate* isolate,
                                     v8::Handle<v8::ArrayBuffer> array);

  void* buffer() const { return buffer_; }
  size_t length() const { return length_; }

 private:
  friend class base::RefCounted<Private>;

  Private(v8::Isolate* isolate, v8::Handle<v8::ArrayBuffer> array);
  ~Private();

  static void WeakCallback(
      const v8::WeakCallbackData<v8::ArrayBuffer, Private>& data);

  v8::Persistent<v8::ArrayBuffer> array_buffer_;
  scoped_refptr<Private> self_reference_;
  void* buffer_;
  size_t length_;
};

scoped_refptr<ArrayBuffer::Private> ArrayBuffer::Private::From(
    v8::Isolate* isolate, v8::Handle<v8::ArrayBuffer> array) {
  if (array->IsExternal()) {
    return make_scoped_refptr(static_cast<Private*>(
        array->GetAlignedPointerFromInternalField(kBufferViewPrivateIndex)));
  }
  return make_scoped_refptr(new Private(isolate, array));
}

ArrayBuffer::Private::Private(v8::Isolate* isolate,
                              v8::Handle<v8::ArrayBuffer> array)
    : array_buffer_(isolate, array) {
  // Take ownership of the array buffer.
  v8::ArrayBuffer::Contents contents = array->Externalize();
  buffer_ = contents.Data();
  length_ = contents.ByteLength();

  array->SetAlignedPointerInInternalField(kBufferViewPrivateIndex, this);

  self_reference_ = this;  // Cleared in WeakCallback.
  array_buffer_.SetWeak(this, WeakCallback);
}

ArrayBuffer::Private::~Private() {
  ArrayBufferAllocator::SharedInstance()->Free(buffer_, length_);
}

void ArrayBuffer::Private::WeakCallback(
    const v8::WeakCallbackData<v8::ArrayBuffer, Private>& data) {
  Private* parameter = data.GetParameter();
  parameter->array_buffer_.Reset();
  parameter->self_reference_ = NULL;
}

// ArrayBuffer ----------------------------------------------------------------

ArrayBuffer::ArrayBuffer()
    : bytes_(0),
      num_bytes_(0) {
}

ArrayBuffer::ArrayBuffer(v8::Isolate* isolate,
                         v8::Handle<v8::ArrayBuffer> array) {
  private_ = ArrayBuffer::Private::From(isolate, array);
  bytes_ = private_->buffer();
  num_bytes_ = private_->length();
}

ArrayBuffer::~ArrayBuffer() {
}

ArrayBuffer& ArrayBuffer::operator=(const ArrayBuffer& other) {
  private_ = other.private_;
  bytes_ = other.bytes_;
  num_bytes_ = other.num_bytes_;
  return *this;
}

// Converter<ArrayBuffer> -----------------------------------------------------

bool Converter<ArrayBuffer>::FromV8(v8::Isolate* isolate,
                                    v8::Handle<v8::Value> val,
                                    ArrayBuffer* out) {
  if (!val->IsArrayBuffer())
    return false;
  *out = ArrayBuffer(isolate, v8::Handle<v8::ArrayBuffer>::Cast(val));
  return true;
}

// ArrayBufferView ------------------------------------------------------------

ArrayBufferView::ArrayBufferView()
    : offset_(0),
      num_bytes_(0) {
}

ArrayBufferView::ArrayBufferView(v8::Isolate* isolate,
                                 v8::Handle<v8::ArrayBufferView> view)
    : array_buffer_(isolate, view->Buffer()),
      offset_(view->ByteOffset()),
      num_bytes_(view->ByteLength()) {
}

ArrayBufferView::~ArrayBufferView() {
}

// Converter<ArrayBufferView> -------------------------------------------------

bool Converter<ArrayBufferView>::FromV8(v8::Isolate* isolate,
                                        v8::Handle<v8::Value> val,
                                        ArrayBufferView* out) {
  if (!val->IsArrayBufferView())
    return false;
  *out = ArrayBufferView(isolate, v8::Handle<v8::ArrayBufferView>::Cast(val));
  return true;
}

}  // namespace gin
