// Copyright 2018 The PDFium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fxjs/cfx_isolate_wrapper.h"

#include <math.h>

#include <memory>

#include "testing/fxv8_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8-container.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-date.h"
#include "v8/include/v8-isolate.h"

namespace {
bool getter_sentinel = false;
bool setter_sentinel = false;
}  // namespace

class CFXIsolateWrapperUnitTest : public FXV8UnitTest {
 public:
  CFXIsolateWrapperUnitTest() = default;
  ~CFXIsolateWrapperUnitTest() override = default;

  // FXV8UnitTest:
  void SetUp() override {
    FXV8UnitTest::SetUp();
    cfx_v8_ = std::make_unique<CFX_IsolateWrapper>(isolate());
  }

  CFX_IsolateWrapper* cfx_v8() const { return cfx_v8_.get(); }

 protected:
  std::unique_ptr<CFX_IsolateWrapper> cfx_v8_;
};

TEST_F(CFXIsolateWrapperUnitTest, EmptyLocal) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  v8::Local<v8::Value> empty;
  EXPECT_FALSE(cfx_v8()->ToBooleanReentrant(empty));
  EXPECT_EQ(0, cfx_v8()->ToInt32Reentrant(empty));
  EXPECT_EQ(0.0, cfx_v8()->ToDoubleReentrant(empty));
  EXPECT_EQ("", cfx_v8()->ToByteStringReentrant(empty));
  EXPECT_EQ(L"", cfx_v8()->ToWideStringReentrant(empty));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(empty).IsEmpty());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(empty).IsEmpty());

  // Can't set properties on empty objects, but does not fault.
  v8::Local<v8::Value> marker = cfx_v8()->NewNumber(2);
  v8::Local<v8::Object> empty_object;
  cfx_v8()->PutObjectPropertyReentrant(empty_object, "clams", marker);
  EXPECT_TRUE(
      cfx_v8()->GetObjectPropertyReentrant(empty_object, "clams").IsEmpty());
  EXPECT_EQ(0u, cfx_v8()->GetObjectPropertyNamesReentrant(empty_object).size());

  // Can't set elements in empty arrays, but does not fault.
  v8::Local<v8::Array> empty_array;
  cfx_v8()->PutArrayElementReentrant(empty_array, 0, marker);
  EXPECT_TRUE(cfx_v8()->GetArrayElementReentrant(empty_array, 0).IsEmpty());
  EXPECT_EQ(0u, cfx_v8()->GetArrayLength(empty_array));
}

TEST_F(CFXIsolateWrapperUnitTest, NewNull) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  auto nullz = cfx_v8()->NewNull();
  EXPECT_FALSE(cfx_v8()->ToBooleanReentrant(nullz));
  EXPECT_EQ(0, cfx_v8()->ToInt32Reentrant(nullz));
  EXPECT_EQ(0.0, cfx_v8()->ToDoubleReentrant(nullz));
  EXPECT_EQ("null", cfx_v8()->ToByteStringReentrant(nullz));
  EXPECT_EQ(L"null", cfx_v8()->ToWideStringReentrant(nullz));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(nullz).IsEmpty());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(nullz).IsEmpty());
}

TEST_F(CFXIsolateWrapperUnitTest, NewUndefined) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  auto undef = cfx_v8()->NewUndefined();
  EXPECT_FALSE(cfx_v8()->ToBooleanReentrant(undef));
  EXPECT_EQ(0, cfx_v8()->ToInt32Reentrant(undef));
  EXPECT_TRUE(isnan(cfx_v8()->ToDoubleReentrant(undef)));
  EXPECT_EQ("undefined", cfx_v8()->ToByteStringReentrant(undef));
  EXPECT_EQ(L"undefined", cfx_v8()->ToWideStringReentrant(undef));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(undef).IsEmpty());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(undef).IsEmpty());
}

TEST_F(CFXIsolateWrapperUnitTest, NewBoolean) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  auto boolz = cfx_v8()->NewBoolean(true);
  EXPECT_TRUE(cfx_v8()->ToBooleanReentrant(boolz));
  EXPECT_EQ(1, cfx_v8()->ToInt32Reentrant(boolz));
  EXPECT_EQ(1.0, cfx_v8()->ToDoubleReentrant(boolz));
  EXPECT_EQ("true", cfx_v8()->ToByteStringReentrant(boolz));
  EXPECT_EQ(L"true", cfx_v8()->ToWideStringReentrant(boolz));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(boolz).IsEmpty());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(boolz).IsEmpty());

  boolz = cfx_v8()->NewBoolean(false);
  EXPECT_FALSE(cfx_v8()->ToBooleanReentrant(boolz));
  EXPECT_EQ(0, cfx_v8()->ToInt32Reentrant(boolz));
  EXPECT_EQ(0.0, cfx_v8()->ToDoubleReentrant(boolz));
  EXPECT_EQ("false", cfx_v8()->ToByteStringReentrant(boolz));
  EXPECT_EQ(L"false", cfx_v8()->ToWideStringReentrant(boolz));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(boolz).IsEmpty());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(boolz).IsEmpty());
}

TEST_F(CFXIsolateWrapperUnitTest, NewNumber) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  auto num = cfx_v8()->NewNumber(42.1);
  EXPECT_TRUE(cfx_v8()->ToBooleanReentrant(num));
  EXPECT_EQ(42, cfx_v8()->ToInt32Reentrant(num));
  EXPECT_EQ(42.1, cfx_v8()->ToDoubleReentrant(num));
  EXPECT_EQ("42.1", cfx_v8()->ToByteStringReentrant(num));
  EXPECT_EQ(L"42.1", cfx_v8()->ToWideStringReentrant(num));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(num).IsEmpty());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(num).IsEmpty());
}

TEST_F(CFXIsolateWrapperUnitTest, NewString) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  auto str = cfx_v8()->NewString("123");
  EXPECT_TRUE(cfx_v8()->ToBooleanReentrant(str));
  EXPECT_EQ(123, cfx_v8()->ToInt32Reentrant(str));
  EXPECT_EQ(123, cfx_v8()->ToDoubleReentrant(str));
  EXPECT_EQ("123", cfx_v8()->ToByteStringReentrant(str));
  EXPECT_EQ(L"123", cfx_v8()->ToWideStringReentrant(str));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(str).IsEmpty());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(str).IsEmpty());

  auto str2 = cfx_v8()->NewString(L"123");
  EXPECT_TRUE(cfx_v8()->ToBooleanReentrant(str2));
  EXPECT_EQ(123, cfx_v8()->ToInt32Reentrant(str2));
  EXPECT_EQ(123, cfx_v8()->ToDoubleReentrant(str2));
  EXPECT_EQ("123", cfx_v8()->ToByteStringReentrant(str2));
  EXPECT_EQ(L"123", cfx_v8()->ToWideStringReentrant(str2));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(str2).IsEmpty());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(str2).IsEmpty());
}

TEST_F(CFXIsolateWrapperUnitTest, NewDate) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  auto date = cfx_v8()->NewDate(1111111111);
  EXPECT_TRUE(cfx_v8()->ToBooleanReentrant(date));
  EXPECT_EQ(1111111111, cfx_v8()->ToInt32Reentrant(date));
  EXPECT_EQ(1111111111.0, cfx_v8()->ToDoubleReentrant(date));
  EXPECT_NE("", cfx_v8()->ToByteStringReentrant(date));  // exact format varies.
  EXPECT_NE(L"",
            cfx_v8()->ToWideStringReentrant(date));  // exact format varies.
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(date)->IsObject());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(date).IsEmpty());
}

TEST_F(CFXIsolateWrapperUnitTest, NewArray) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  auto array = cfx_v8()->NewArray();
  EXPECT_EQ(0u, cfx_v8()->GetArrayLength(array));
  EXPECT_FALSE(cfx_v8()->GetArrayElementReentrant(array, 2).IsEmpty());
  EXPECT_TRUE(cfx_v8()->GetArrayElementReentrant(array, 2)->IsUndefined());
  EXPECT_EQ(0u, cfx_v8()->GetArrayLength(array));

  cfx_v8()->PutArrayElementReentrant(array, 3, cfx_v8()->NewNumber(12));
  EXPECT_FALSE(cfx_v8()->GetArrayElementReentrant(array, 2).IsEmpty());
  EXPECT_TRUE(cfx_v8()->GetArrayElementReentrant(array, 2)->IsUndefined());
  EXPECT_FALSE(cfx_v8()->GetArrayElementReentrant(array, 3).IsEmpty());
  EXPECT_TRUE(cfx_v8()->GetArrayElementReentrant(array, 3)->IsNumber());
  EXPECT_EQ(4u, cfx_v8()->GetArrayLength(array));

  EXPECT_TRUE(cfx_v8()->ToBooleanReentrant(array));
  EXPECT_EQ(0, cfx_v8()->ToInt32Reentrant(array));
  double d = cfx_v8()->ToDoubleReentrant(array);
  EXPECT_NE(d, d);  // i.e. NaN.
  EXPECT_EQ(L",,,12", cfx_v8()->ToWideStringReentrant(array));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(array)->IsObject());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(array)->IsArray());
}

TEST_F(CFXIsolateWrapperUnitTest, NewObject) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Context::Scope context_scope(v8::Context::New(isolate()));

  auto object = cfx_v8()->NewObject();
  ASSERT_FALSE(object.IsEmpty());
  EXPECT_EQ(0u, cfx_v8()->GetObjectPropertyNamesReentrant(object).size());
  EXPECT_FALSE(cfx_v8()->GetObjectPropertyReentrant(object, "clams").IsEmpty());
  EXPECT_TRUE(
      cfx_v8()->GetObjectPropertyReentrant(object, "clams")->IsUndefined());
  EXPECT_EQ(0u, cfx_v8()->GetObjectPropertyNamesReentrant(object).size());

  cfx_v8()->PutObjectPropertyReentrant(object, "clams",
                                       cfx_v8()->NewNumber(12));
  EXPECT_FALSE(cfx_v8()->GetObjectPropertyReentrant(object, "clams").IsEmpty());
  EXPECT_TRUE(
      cfx_v8()->GetObjectPropertyReentrant(object, "clams")->IsNumber());
  EXPECT_EQ(1u, cfx_v8()->GetObjectPropertyNamesReentrant(object).size());
  EXPECT_EQ(L"clams", cfx_v8()->GetObjectPropertyNamesReentrant(object)[0]);

  EXPECT_TRUE(cfx_v8()->ToBooleanReentrant(object));
  EXPECT_EQ(0, cfx_v8()->ToInt32Reentrant(object));
  double d = cfx_v8()->ToDoubleReentrant(object);
  EXPECT_NE(d, d);  // i.e. NaN.
  EXPECT_EQ(L"[object Object]", cfx_v8()->ToWideStringReentrant(object));
  EXPECT_TRUE(cfx_v8()->ToObjectReentrant(object)->IsObject());
  EXPECT_TRUE(cfx_v8()->ToArrayReentrant(object).IsEmpty());
}

TEST_F(CFXIsolateWrapperUnitTest, ThrowFromGetter) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = v8::Context::New(isolate());
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> object = cfx_v8()->NewObject();
  v8::Local<v8::String> name = cfx_v8()->NewString("clams");
  EXPECT_TRUE(object
                  ->SetNativeDataProperty(
                      context, name,
                      [](v8::Local<v8::Name> property,
                         const v8::PropertyCallbackInfo<v8::Value>& info) {
                        getter_sentinel = true;
                        info.GetIsolate()->ThrowException(property);
                      })
                  .FromJust());
  getter_sentinel = false;
  EXPECT_TRUE(cfx_v8()->GetObjectPropertyReentrant(object, "clams").IsEmpty());
  EXPECT_TRUE(getter_sentinel);
}

TEST_F(CFXIsolateWrapperUnitTest, ThrowFromSetter) {
  v8::Isolate::Scope isolate_scope(isolate());
  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = v8::Context::New(isolate());
  v8::Context::Scope context_scope(context);

  v8::Local<v8::Object> object = cfx_v8()->NewObject();
  v8::Local<v8::String> name = cfx_v8()->NewString("clams");
  EXPECT_TRUE(
      object
          ->SetNativeDataProperty(
              context, name, nullptr,
              [](v8::Local<v8::Name> property, v8::Local<v8::Value> value,
                 const v8::PropertyCallbackInfo<v8::Boolean>& info) {
                setter_sentinel = true;
                info.GetIsolate()->ThrowException(property);
              })
          .FromJust());
  setter_sentinel = false;
  cfx_v8()->PutObjectPropertyReentrant(object, "clams", name);
  EXPECT_TRUE(setter_sentinel);
}
