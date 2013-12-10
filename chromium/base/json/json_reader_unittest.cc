// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(JSONReaderTest, Reading) {
  // some whitespace checking
  scoped_ptr<Value> root;
  root.reset(JSONReader().ReadToValue("   null   "));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_NULL));

  // Invalid JSON string
  root.reset(JSONReader().ReadToValue("nu"));
  EXPECT_FALSE(root.get());

  // Simple bool
  root.reset(JSONReader().ReadToValue("true  "));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_BOOLEAN));

  // Embedded comment
  root.reset(JSONReader().ReadToValue("/* comment */null"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_NULL));
  root.reset(JSONReader().ReadToValue("40 /* comment */"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_INTEGER));
  root.reset(JSONReader().ReadToValue("true // comment"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_BOOLEAN));
  root.reset(JSONReader().ReadToValue("/* comment */\"sample string\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_STRING));
  std::string value;
  EXPECT_TRUE(root->GetAsString(&value));
  EXPECT_EQ("sample string", value);
  root.reset(JSONReader().ReadToValue("[1, /* comment, 2 ] */ \n 3]"));
  ASSERT_TRUE(root.get());
  ListValue* list = static_cast<ListValue*>(root.get());
  EXPECT_EQ(2u, list->GetSize());
  int int_val = 0;
  EXPECT_TRUE(list->GetInteger(0, &int_val));
  EXPECT_EQ(1, int_val);
  EXPECT_TRUE(list->GetInteger(1, &int_val));
  EXPECT_EQ(3, int_val);
  root.reset(JSONReader().ReadToValue("[1, /*a*/2, 3]"));
  ASSERT_TRUE(root.get());
  list = static_cast<ListValue*>(root.get());
  EXPECT_EQ(3u, list->GetSize());
  root.reset(JSONReader().ReadToValue("/* comment **/42"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_INTEGER));
  EXPECT_TRUE(root->GetAsInteger(&int_val));
  EXPECT_EQ(42, int_val);
  root.reset(JSONReader().ReadToValue(
      "/* comment **/\n"
      "// */ 43\n"
      "44"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_INTEGER));
  EXPECT_TRUE(root->GetAsInteger(&int_val));
  EXPECT_EQ(44, int_val);

  // Test number formats
  root.reset(JSONReader().ReadToValue("43"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_INTEGER));
  EXPECT_TRUE(root->GetAsInteger(&int_val));
  EXPECT_EQ(43, int_val);

  // According to RFC4627, oct, hex, and leading zeros are invalid JSON.
  root.reset(JSONReader().ReadToValue("043"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("0x43"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("00"));
  EXPECT_FALSE(root.get());

  // Test 0 (which needs to be special cased because of the leading zero
  // clause).
  root.reset(JSONReader().ReadToValue("0"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_INTEGER));
  int_val = 1;
  EXPECT_TRUE(root->GetAsInteger(&int_val));
  EXPECT_EQ(0, int_val);

  // Numbers that overflow ints should succeed, being internally promoted to
  // storage as doubles
  root.reset(JSONReader().ReadToValue("2147483648"));
  ASSERT_TRUE(root.get());
  double double_val;
  EXPECT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(2147483648.0, double_val);
  root.reset(JSONReader().ReadToValue("-2147483649"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(-2147483649.0, double_val);

  // Parse a double
  root.reset(JSONReader().ReadToValue("43.1"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(43.1, double_val);

  root.reset(JSONReader().ReadToValue("4.3e-1"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(.43, double_val);

  root.reset(JSONReader().ReadToValue("2.1e0"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(2.1, double_val);

  root.reset(JSONReader().ReadToValue("2.1e+0001"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(21.0, double_val);

  root.reset(JSONReader().ReadToValue("0.01"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(0.01, double_val);

  root.reset(JSONReader().ReadToValue("1.00"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DOUBLE));
  double_val = 0.0;
  EXPECT_TRUE(root->GetAsDouble(&double_val));
  EXPECT_DOUBLE_EQ(1.0, double_val);

  // Fractional parts must have a digit before and after the decimal point.
  root.reset(JSONReader().ReadToValue("1."));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue(".1"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("1.e10"));
  EXPECT_FALSE(root.get());

  // Exponent must have a digit following the 'e'.
  root.reset(JSONReader().ReadToValue("1e"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("1E"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("1e1."));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("1e1.0"));
  EXPECT_FALSE(root.get());

  // INF/-INF/NaN are not valid
  root.reset(JSONReader().ReadToValue("1e1000"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("-1e1000"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("NaN"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("nan"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("inf"));
  EXPECT_FALSE(root.get());

  // Invalid number formats
  root.reset(JSONReader().ReadToValue("4.3.1"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("4e3.1"));
  EXPECT_FALSE(root.get());

  // Test string parser
  root.reset(JSONReader().ReadToValue("\"hello world\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_STRING));
  std::string str_val;
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("hello world", str_val);

  // Empty string
  root.reset(JSONReader().ReadToValue("\"\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("", str_val);

  // Test basic string escapes
  root.reset(JSONReader().ReadToValue("\" \\\"\\\\\\/\\b\\f\\n\\r\\t\\v\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ(" \"\\/\b\f\n\r\t\v", str_val);

  // Test hex and unicode escapes including the null character.
  root.reset(JSONReader().ReadToValue("\"\\x41\\x00\\u1234\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ(std::wstring(L"A\0\x1234", 3), UTF8ToWide(str_val));

  // Test invalid strings
  root.reset(JSONReader().ReadToValue("\"no closing quote"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("\"\\z invalid escape char\""));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("\"\\xAQ invalid hex code\""));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("not enough hex chars\\x1\""));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("\"not enough escape chars\\u123\""));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("\"extra backslash at end of input\\\""));
  EXPECT_FALSE(root.get());

  // Basic array
  root.reset(JSONReader::Read("[true, false, null]"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  EXPECT_EQ(3U, list->GetSize());

  // Test with trailing comma.  Should be parsed the same as above.
  scoped_ptr<Value> root2;
  root2.reset(JSONReader::Read("[true, false, null, ]",
                               JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_TRUE(root->Equals(root2.get()));

  // Empty array
  root.reset(JSONReader::Read("[]"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  EXPECT_EQ(0U, list->GetSize());

  // Nested arrays
  root.reset(JSONReader::Read("[[true], [], [false, [], [null]], null]"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  EXPECT_EQ(4U, list->GetSize());

  // Lots of trailing commas.
  root2.reset(JSONReader::Read("[[true], [], [false, [], [null, ]  , ], null,]",
                               JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_TRUE(root->Equals(root2.get()));

  // Invalid, missing close brace.
  root.reset(JSONReader::Read("[[true], [], [false, [], [null]], null"));
  EXPECT_FALSE(root.get());

  // Invalid, too many commas
  root.reset(JSONReader::Read("[true,, null]"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("[true,, null]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());

  // Invalid, no commas
  root.reset(JSONReader::Read("[true null]"));
  EXPECT_FALSE(root.get());

  // Invalid, trailing comma
  root.reset(JSONReader::Read("[true,]"));
  EXPECT_FALSE(root.get());

  // Valid if we set |allow_trailing_comma| to true.
  root.reset(JSONReader::Read("[true,]", JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  EXPECT_EQ(1U, list->GetSize());
  Value* tmp_value = NULL;
  ASSERT_TRUE(list->Get(0, &tmp_value));
  EXPECT_TRUE(tmp_value->IsType(Value::TYPE_BOOLEAN));
  bool bool_value = false;
  EXPECT_TRUE(tmp_value->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);

  // Don't allow empty elements, even if |allow_trailing_comma| is
  // true.
  root.reset(JSONReader::Read("[,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("[true,,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("[,true,]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("[true,,false]", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());

  // Test objects
  root.reset(JSONReader::Read("{}"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DICTIONARY));

  root.reset(JSONReader::Read(
      "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\" }"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  DictionaryValue* dict_val = static_cast<DictionaryValue*>(root.get());
  double_val = 0.0;
  EXPECT_TRUE(dict_val->GetDouble("number", &double_val));
  EXPECT_DOUBLE_EQ(9.87654321, double_val);
  Value* null_val = NULL;
  ASSERT_TRUE(dict_val->Get("null", &null_val));
  EXPECT_TRUE(null_val->IsType(Value::TYPE_NULL));
  str_val.clear();
  EXPECT_TRUE(dict_val->GetString("S", &str_val));
  EXPECT_EQ("str", str_val);

  root2.reset(JSONReader::Read(
      "{\"number\":9.87654321, \"null\":null , \"\\x53\" : \"str\", }",
      JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(root2.get());
  EXPECT_TRUE(root->Equals(root2.get()));

  // Test newline equivalence.
  root2.reset(JSONReader::Read(
      "{\n"
      "  \"number\":9.87654321,\n"
      "  \"null\":null,\n"
      "  \"\\x53\":\"str\",\n"
      "}\n", JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(root2.get());
  EXPECT_TRUE(root->Equals(root2.get()));

  root2.reset(JSONReader::Read(
      "{\r\n"
      "  \"number\":9.87654321,\r\n"
      "  \"null\":null,\r\n"
      "  \"\\x53\":\"str\",\r\n"
      "}\r\n", JSON_ALLOW_TRAILING_COMMAS));
  ASSERT_TRUE(root2.get());
  EXPECT_TRUE(root->Equals(root2.get()));

  // Test nesting
  root.reset(JSONReader::Read(
      "{\"inner\":{\"array\":[true]},\"false\":false,\"d\":{}}"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  dict_val = static_cast<DictionaryValue*>(root.get());
  DictionaryValue* inner_dict = NULL;
  ASSERT_TRUE(dict_val->GetDictionary("inner", &inner_dict));
  ListValue* inner_array = NULL;
  ASSERT_TRUE(inner_dict->GetList("array", &inner_array));
  EXPECT_EQ(1U, inner_array->GetSize());
  bool_value = true;
  EXPECT_TRUE(dict_val->GetBoolean("false", &bool_value));
  EXPECT_FALSE(bool_value);
  inner_dict = NULL;
  EXPECT_TRUE(dict_val->GetDictionary("d", &inner_dict));

  root2.reset(JSONReader::Read(
      "{\"inner\": {\"array\":[true] , },\"false\":false,\"d\":{},}",
      JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_TRUE(root->Equals(root2.get()));

  // Test keys with periods
  root.reset(JSONReader::Read(
      "{\"a.b\":3,\"c\":2,\"d.e.f\":{\"g.h.i.j\":1}}"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  dict_val = static_cast<DictionaryValue*>(root.get());
  int integer_value = 0;
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("a.b", &integer_value));
  EXPECT_EQ(3, integer_value);
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("c", &integer_value));
  EXPECT_EQ(2, integer_value);
  inner_dict = NULL;
  ASSERT_TRUE(dict_val->GetDictionaryWithoutPathExpansion("d.e.f",
                                                          &inner_dict));
  EXPECT_EQ(1U, inner_dict->size());
  EXPECT_TRUE(inner_dict->GetIntegerWithoutPathExpansion("g.h.i.j",
                                                         &integer_value));
  EXPECT_EQ(1, integer_value);

  root.reset(JSONReader::Read("{\"a\":{\"b\":2},\"a.b\":1}"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  dict_val = static_cast<DictionaryValue*>(root.get());
  EXPECT_TRUE(dict_val->GetInteger("a.b", &integer_value));
  EXPECT_EQ(2, integer_value);
  EXPECT_TRUE(dict_val->GetIntegerWithoutPathExpansion("a.b", &integer_value));
  EXPECT_EQ(1, integer_value);

  // Invalid, no closing brace
  root.reset(JSONReader::Read("{\"a\": true"));
  EXPECT_FALSE(root.get());

  // Invalid, keys must be quoted
  root.reset(JSONReader::Read("{foo:true}"));
  EXPECT_FALSE(root.get());

  // Invalid, trailing comma
  root.reset(JSONReader::Read("{\"a\":true,}"));
  EXPECT_FALSE(root.get());

  // Invalid, too many commas
  root.reset(JSONReader::Read("{\"a\":true,,\"b\":false}"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("{\"a\":true,,\"b\":false}",
                              JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());

  // Invalid, no separator
  root.reset(JSONReader::Read("{\"a\" \"b\"}"));
  EXPECT_FALSE(root.get());

  // Invalid, lone comma.
  root.reset(JSONReader::Read("{,}"));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("{,}", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("{\"a\":true,,}", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("{,\"a\":true}", JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader::Read("{\"a\":true,,\"b\":false}",
                              JSON_ALLOW_TRAILING_COMMAS));
  EXPECT_FALSE(root.get());

  // Test stack overflow
  std::string evil(1000000, '[');
  evil.append(std::string(1000000, ']'));
  root.reset(JSONReader::Read(evil));
  EXPECT_FALSE(root.get());

  // A few thousand adjacent lists is fine.
  std::string not_evil("[");
  not_evil.reserve(15010);
  for (int i = 0; i < 5000; ++i) {
    not_evil.append("[],");
  }
  not_evil.append("[]]");
  root.reset(JSONReader::Read(not_evil));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_LIST));
  list = static_cast<ListValue*>(root.get());
  EXPECT_EQ(5001U, list->GetSize());

  // Test utf8 encoded input
  root.reset(JSONReader().ReadToValue("\"\xe7\xbd\x91\xe9\xa1\xb5\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ(L"\x7f51\x9875", UTF8ToWide(str_val));

  root.reset(JSONReader().ReadToValue(
      "{\"path\": \"/tmp/\xc3\xa0\xc3\xa8\xc3\xb2.png\"}"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
  EXPECT_TRUE(root->GetAsDictionary(&dict_val));
  EXPECT_TRUE(dict_val->GetString("path", &str_val));
  EXPECT_EQ("/tmp/\xC3\xA0\xC3\xA8\xC3\xB2.png", str_val);

  // Test invalid utf8 encoded input
  root.reset(JSONReader().ReadToValue("\"345\xb0\xa1\xb0\xa2\""));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("\"123\xc0\x81\""));
  EXPECT_FALSE(root.get());
  root.reset(JSONReader().ReadToValue("\"abc\xc0\xae\""));
  EXPECT_FALSE(root.get());

  // Test utf16 encoded strings.
  root.reset(JSONReader().ReadToValue("\"\\u20ac3,14\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("\xe2\x82\xac""3,14", str_val);

  root.reset(JSONReader().ReadToValue("\"\\ud83d\\udca9\\ud83d\\udc6c\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->IsType(Value::TYPE_STRING));
  str_val.clear();
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("\xf0\x9f\x92\xa9\xf0\x9f\x91\xac", str_val);

  // Test invalid utf16 strings.
  const char* cases[] = {
    "\"\\u123\"",  // Invalid scalar.
    "\"\\ud83d\"",  // Invalid scalar.
    "\"\\u$%@!\"",  // Invalid scalar.
    "\"\\uzz89\"",  // Invalid scalar.
    "\"\\ud83d\\udca\"",  // Invalid lower surrogate.
    "\"\\ud83d\\ud83d\"",  // Invalid lower surrogate.
    "\"\\ud83foo\"",  // No lower surrogate.
    "\"\\ud83\\foo\""  // No lower surrogate.
  };
  for (size_t i = 0; i < arraysize(cases); ++i) {
    root.reset(JSONReader().ReadToValue(cases[i]));
    EXPECT_FALSE(root.get()) << cases[i];
  }

  // Test literal root objects.
  root.reset(JSONReader::Read("null"));
  EXPECT_TRUE(root->IsType(Value::TYPE_NULL));

  root.reset(JSONReader::Read("true"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->GetAsBoolean(&bool_value));
  EXPECT_TRUE(bool_value);

  root.reset(JSONReader::Read("10"));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->GetAsInteger(&integer_value));
  EXPECT_EQ(10, integer_value);

  root.reset(JSONReader::Read("\"root\""));
  ASSERT_TRUE(root.get());
  EXPECT_TRUE(root->GetAsString(&str_val));
  EXPECT_EQ("root", str_val);
}

TEST(JSONReaderTest, ReadFromFile) {
  FilePath path;
  ASSERT_TRUE(PathService::Get(base::DIR_TEST_DATA, &path));
  path = path.AppendASCII("json");
  ASSERT_TRUE(base::PathExists(path));

  std::string input;
  ASSERT_TRUE(ReadFileToString(
      path.Append(FILE_PATH_LITERAL("bom_feff.json")), &input));

  JSONReader reader;
  scoped_ptr<Value> root(reader.ReadToValue(input));
  ASSERT_TRUE(root.get()) << reader.GetErrorMessage();
  EXPECT_TRUE(root->IsType(Value::TYPE_DICTIONARY));
}

// Tests that the root of a JSON object can be deleted safely while its
// children outlive it.
TEST(JSONReaderTest, StringOptimizations) {
  scoped_ptr<Value> dict_literal_0;
  scoped_ptr<Value> dict_literal_1;
  scoped_ptr<Value> dict_string_0;
  scoped_ptr<Value> dict_string_1;
  scoped_ptr<Value> list_value_0;
  scoped_ptr<Value> list_value_1;

  {
    scoped_ptr<Value> root(JSONReader::Read(
        "{"
        "  \"test\": {"
        "    \"foo\": true,"
        "    \"bar\": 3.14,"
        "    \"baz\": \"bat\","
        "    \"moo\": \"cow\""
        "  },"
        "  \"list\": ["
        "    \"a\","
        "    \"b\""
        "  ]"
        "}", JSON_DETACHABLE_CHILDREN));
    ASSERT_TRUE(root.get());

    DictionaryValue* root_dict = NULL;
    ASSERT_TRUE(root->GetAsDictionary(&root_dict));

    DictionaryValue* dict = NULL;
    ListValue* list = NULL;

    ASSERT_TRUE(root_dict->GetDictionary("test", &dict));
    ASSERT_TRUE(root_dict->GetList("list", &list));

    EXPECT_TRUE(dict->Remove("foo", &dict_literal_0));
    EXPECT_TRUE(dict->Remove("bar", &dict_literal_1));
    EXPECT_TRUE(dict->Remove("baz", &dict_string_0));
    EXPECT_TRUE(dict->Remove("moo", &dict_string_1));

    ASSERT_EQ(2u, list->GetSize());
    EXPECT_TRUE(list->Remove(0, &list_value_0));
    EXPECT_TRUE(list->Remove(0, &list_value_1));
  }

  bool b = false;
  double d = 0;
  std::string s;

  EXPECT_TRUE(dict_literal_0->GetAsBoolean(&b));
  EXPECT_TRUE(b);

  EXPECT_TRUE(dict_literal_1->GetAsDouble(&d));
  EXPECT_EQ(3.14, d);

  EXPECT_TRUE(dict_string_0->GetAsString(&s));
  EXPECT_EQ("bat", s);

  EXPECT_TRUE(dict_string_1->GetAsString(&s));
  EXPECT_EQ("cow", s);

  EXPECT_TRUE(list_value_0->GetAsString(&s));
  EXPECT_EQ("a", s);
  EXPECT_TRUE(list_value_1->GetAsString(&s));
  EXPECT_EQ("b", s);
}

// A smattering of invalid JSON designed to test specific portions of the
// parser implementation against buffer overflow. Best run with DCHECKs so
// that the one in NextChar fires.
TEST(JSONReaderTest, InvalidSanity) {
  const char* invalid_json[] = {
      "/* test *",
      "{\"foo\"",
      "{\"foo\":",
      "  [",
      "\"\\u123g\"",
      "{\n\"eh:\n}",
  };

  for (size_t i = 0; i < arraysize(invalid_json); ++i) {
    JSONReader reader;
    LOG(INFO) << "Sanity test " << i << ": <" << invalid_json[i] << ">";
    EXPECT_FALSE(reader.ReadToValue(invalid_json[i]));
    EXPECT_NE(JSONReader::JSON_NO_ERROR, reader.error_code());
    EXPECT_NE("", reader.GetErrorMessage());
  }
}

TEST(JSONReaderTest, IllegalTrailingNull) {
  const char json[] = { '"', 'n', 'u', 'l', 'l', '"', '\0' };
  std::string json_string(json, sizeof(json));
  JSONReader reader;
  EXPECT_FALSE(reader.ReadToValue(json_string));
  EXPECT_EQ(JSONReader::JSON_UNEXPECTED_DATA_AFTER_ROOT, reader.error_code());
}

}  // namespace base
