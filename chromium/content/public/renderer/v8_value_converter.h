// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_V8_VALUE_CONVERTER_H_
#define CONTENT_PUBLIC_RENDERER_V8_VALUE_CONVERTER_H_

#include "content/common/content_export.h"
#include "v8/include/v8.h"

namespace base {
class Value;
}

namespace content {

// Converts between v8::Value (JavaScript values in the v8 heap) and Chrome's
// values (from base/values.h). Lists and dictionaries are converted
// recursively.
//
// The JSON types (null, boolean, string, number, array, and object) as well as
// binary values are supported. For binary values, we convert to WebKit
// ArrayBuffers, and support converting from an ArrayBuffer or any of the
// ArrayBufferView subclasses (Uint8Array, etc.).
class CONTENT_EXPORT V8ValueConverter {
 public:
  // Extends the default behaviour of V8ValueConverter.
  class CONTENT_EXPORT Strategy {
   public:
    virtual ~Strategy() {}
    // If false is returned, V8ValueConverter proceeds with the default
    // behavior.
    virtual bool FromV8Object(v8::Handle<v8::Object> value,
                              base::Value** out,
                              v8::Isolate* isolate) const = 0;
    // If false is returned, V8ValueConverter proceeds with the default
    // behavior.
    virtual bool FromV8Array(v8::Handle<v8::Array> value,
                             base::Value** out,
                             v8::Isolate* isolate) const = 0;
  };

  static V8ValueConverter* create();

  virtual ~V8ValueConverter() {}

  // If true, Date objects are converted into DoubleValues with the number of
  // seconds since Unix epoch.
  //
  // Otherwise they are converted into DictionaryValues with whatever additional
  // properties has been set on them.
  virtual void SetDateAllowed(bool val) = 0;

  // If true, RegExp objects are converted into StringValues with the regular
  // expression between / and /, for example "/ab?c/".
  //
  // Otherwise they are converted into DictionaryValues with whatever additional
  // properties has been set on them.
  virtual void SetRegExpAllowed(bool val) = 0;

  // If true, Function objects are converted into DictionaryValues with whatever
  // additional properties has been set on them.
  //
  // Otherwise they are treated as unsupported, see FromV8Value.
  virtual void SetFunctionAllowed(bool val) = 0;

  // If true, null values are stripped from objects. This is often useful when
  // converting arguments to extension APIs.
  virtual void SetStripNullFromObjects(bool val) = 0;

  // Extend default behavior of V8ValueConverter.
  virtual void SetStrategy(Strategy* strategy) = 0;

  // Converts a base::Value to a v8::Value.
  //
  // Unsupported types are replaced with null.  If an array or object throws
  // while setting a value, that property or item is skipped, leaving a hole in
  // the case of arrays.
  virtual v8::Handle<v8::Value> ToV8Value(
      const base::Value* value,
      v8::Handle<v8::Context> context) const = 0;

  // Converts a v8::Value to base::Value.
  //
  // Unsupported types (unless explicitly configured) are not converted, so
  // this method may return NULL -- the exception is when converting arrays,
  // where unsupported types are converted to Value(TYPE_NULL).
  //
  // Likewise, if an object throws while converting a property it will not be
  // converted, whereas if an array throws while converting an item it will be
  // converted to Value(TYPE_NULL).
  virtual base::Value* FromV8Value(v8::Handle<v8::Value> value,
                                   v8::Handle<v8::Context> context) const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_V8_VALUE_CONVERTER_H_
