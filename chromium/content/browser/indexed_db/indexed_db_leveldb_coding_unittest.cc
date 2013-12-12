// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_leveldb_coding.h"

#include <limits>
#include <vector>

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using WebKit::WebIDBKeyTypeDate;
using WebKit::WebIDBKeyTypeNumber;

namespace content {

namespace {

static IndexedDBKey CreateArrayIDBKey() {
  return IndexedDBKey(IndexedDBKey::KeyArray());
}

static IndexedDBKey CreateArrayIDBKey(const IndexedDBKey& key1) {
  IndexedDBKey::KeyArray array;
  array.push_back(key1);
  return IndexedDBKey(array);
}

static IndexedDBKey CreateArrayIDBKey(const IndexedDBKey& key1,
                                      const IndexedDBKey& key2) {
  IndexedDBKey::KeyArray array;
  array.push_back(key1);
  array.push_back(key2);
  return IndexedDBKey(array);
}

static std::string WrappedEncodeByte(char value) {
  std::string buffer;
  EncodeByte(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeByte) {
  std::string expected;
  expected.push_back(0);
  unsigned char c;

  c = 0;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));

  c = 1;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));

  c = 255;
  expected[0] = c;
  EXPECT_EQ(expected, WrappedEncodeByte(c));
}

TEST(IndexedDBLevelDBCodingTest, DecodeByte) {
  std::vector<unsigned char> test_cases;
  test_cases.push_back(0);
  test_cases.push_back(1);
  test_cases.push_back(255);

  for (size_t i = 0; i < test_cases.size(); ++i) {
    unsigned char n = test_cases[i];
    std::string v;
    EncodeByte(n, &v);

    unsigned char res;
    ASSERT_GT(v.size(), static_cast<size_t>(0));
    StringPiece slice(v);
    EXPECT_TRUE(DecodeByte(&slice, &res));
    EXPECT_EQ(n, res);
    EXPECT_TRUE(slice.empty());
  }

  {
    StringPiece slice;
    unsigned char value;
    EXPECT_FALSE(DecodeByte(&slice, &value));
  }
}

static std::string WrappedEncodeBool(bool value) {
  std::string buffer;
  EncodeBool(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeBool) {
  {
    std::string expected;
    expected.push_back(1);
    EXPECT_EQ(expected, WrappedEncodeBool(true));
  }
  {
    std::string expected;
    expected.push_back(0);
    EXPECT_EQ(expected, WrappedEncodeBool(false));
  }
}

static int CompareKeys(const std::string& a, const std::string& b) {
  bool ok;
  int result = CompareEncodedIDBKeys(a, b, &ok);
  EXPECT_TRUE(ok);
  return result;
}

TEST(IndexedDBLevelDBCodingTest, MaxIDBKey) {
  std::string max_key = MaxIDBKey();

  std::string min_key = MinIDBKey();
  std::string array_key;
  EncodeIDBKey(IndexedDBKey(IndexedDBKey::KeyArray()), &array_key);
  std::string string_key;
  EncodeIDBKey(IndexedDBKey(ASCIIToUTF16("Hello world")), &string_key);
  std::string number_key;
  EncodeIDBKey(IndexedDBKey(3.14, WebIDBKeyTypeNumber), &number_key);
  std::string date_key;
  EncodeIDBKey(IndexedDBKey(1000000, WebIDBKeyTypeDate), &date_key);

  EXPECT_GT(CompareKeys(max_key, min_key), 0);
  EXPECT_GT(CompareKeys(max_key, array_key), 0);
  EXPECT_GT(CompareKeys(max_key, string_key), 0);
  EXPECT_GT(CompareKeys(max_key, number_key), 0);
  EXPECT_GT(CompareKeys(max_key, date_key), 0);
}

TEST(IndexedDBLevelDBCodingTest, MinIDBKey) {
  std::string min_key = MinIDBKey();

  std::string max_key = MaxIDBKey();
  std::string array_key;
  EncodeIDBKey(IndexedDBKey(IndexedDBKey::KeyArray()), &array_key);
  std::string string_key;
  EncodeIDBKey(IndexedDBKey(ASCIIToUTF16("Hello world")), &string_key);
  std::string number_key;
  EncodeIDBKey(IndexedDBKey(3.14, WebIDBKeyTypeNumber), &number_key);
  std::string date_key;
  EncodeIDBKey(IndexedDBKey(1000000, WebIDBKeyTypeDate), &date_key);

  EXPECT_LT(CompareKeys(min_key, max_key), 0);
  EXPECT_LT(CompareKeys(min_key, array_key), 0);
  EXPECT_LT(CompareKeys(min_key, string_key), 0);
  EXPECT_LT(CompareKeys(min_key, number_key), 0);
  EXPECT_LT(CompareKeys(min_key, date_key), 0);
}

static std::string WrappedEncodeInt(int64 value) {
  std::string buffer;
  EncodeInt(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeInt) {
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeInt(0).size());
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeInt(1).size());
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeInt(255).size());
  EXPECT_EQ(static_cast<size_t>(2), WrappedEncodeInt(256).size());
  EXPECT_EQ(static_cast<size_t>(4), WrappedEncodeInt(0xffffffff).size());
#ifdef NDEBUG
  EXPECT_EQ(static_cast<size_t>(8), WrappedEncodeInt(-1).size());
#endif
}

TEST(IndexedDBLevelDBCodingTest, DecodeBool) {
  {
    std::string encoded;
    encoded.push_back(1);
    StringPiece slice(encoded);
    bool value;
    EXPECT_TRUE(DecodeBool(&slice, &value));
    EXPECT_TRUE(value);
    EXPECT_TRUE(slice.empty());
  }
  {
    std::string encoded;
    encoded.push_back(0);
    StringPiece slice(encoded);
    bool value;
    EXPECT_TRUE(DecodeBool(&slice, &value));
    EXPECT_FALSE(value);
    EXPECT_TRUE(slice.empty());
  }
  {
    StringPiece slice;
    bool value;
    EXPECT_FALSE(DecodeBool(&slice, &value));
  }
}

TEST(IndexedDBLevelDBCodingTest, DecodeInt) {
  std::vector<int64> test_cases;
  test_cases.push_back(0);
  test_cases.push_back(1);
  test_cases.push_back(255);
  test_cases.push_back(256);
  test_cases.push_back(65535);
  test_cases.push_back(655536);
  test_cases.push_back(7711192431755665792ll);
  test_cases.push_back(0x7fffffffffffffffll);
#ifdef NDEBUG
  test_cases.push_back(-3);
#endif

  for (size_t i = 0; i < test_cases.size(); ++i) {
    int64 n = test_cases[i];
    std::string v = WrappedEncodeInt(n);
    ASSERT_GT(v.size(), static_cast<size_t>(0));
    StringPiece slice(v);
    int64 value;
    EXPECT_TRUE(DecodeInt(&slice, &value));
    EXPECT_EQ(n, value);
    EXPECT_TRUE(slice.empty());

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), static_cast<size_t>(1), static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeInt(&slice, &value));
    EXPECT_EQ(n, value);
    EXPECT_TRUE(slice.empty());
  }
  {
    StringPiece slice;
    int64 value;
    EXPECT_FALSE(DecodeInt(&slice, &value));
  }
}

static std::string WrappedEncodeVarInt(int64 value) {
  std::string buffer;
  EncodeVarInt(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeVarInt) {
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeVarInt(0).size());
  EXPECT_EQ(static_cast<size_t>(1), WrappedEncodeVarInt(1).size());
  EXPECT_EQ(static_cast<size_t>(2), WrappedEncodeVarInt(255).size());
  EXPECT_EQ(static_cast<size_t>(2), WrappedEncodeVarInt(256).size());
  EXPECT_EQ(static_cast<size_t>(5), WrappedEncodeVarInt(0xffffffff).size());
  EXPECT_EQ(static_cast<size_t>(8),
            WrappedEncodeVarInt(0xfffffffffffffLL).size());
  EXPECT_EQ(static_cast<size_t>(9),
            WrappedEncodeVarInt(0x7fffffffffffffffLL).size());
#ifdef NDEBUG
  EXPECT_EQ(static_cast<size_t>(10), WrappedEncodeVarInt(-100).size());
#endif
}

TEST(IndexedDBLevelDBCodingTest, DecodeVarInt) {
  std::vector<int64> test_cases;
  test_cases.push_back(0);
  test_cases.push_back(1);
  test_cases.push_back(255);
  test_cases.push_back(256);
  test_cases.push_back(65535);
  test_cases.push_back(655536);
  test_cases.push_back(7711192431755665792ll);
  test_cases.push_back(0x7fffffffffffffffll);
#ifdef NDEBUG
  test_cases.push_back(-3);
#endif

  for (size_t i = 0; i < test_cases.size(); ++i) {
    int64 n = test_cases[i];
    std::string v = WrappedEncodeVarInt(n);
    ASSERT_GT(v.size(), static_cast<size_t>(0));
    StringPiece slice(v);
    int64 res;
    EXPECT_TRUE(DecodeVarInt(&slice, &res));
    EXPECT_EQ(n, res);
    EXPECT_TRUE(slice.empty());

    slice = StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeVarInt(&slice, &res));

    slice = StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeVarInt(&slice, &res));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), static_cast<size_t>(1), static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeVarInt(&slice, &res));
    EXPECT_EQ(n, res);
    EXPECT_TRUE(slice.empty());
  }
}

static std::string WrappedEncodeString(string16 value) {
  std::string buffer;
  EncodeString(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeString) {
  const char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  EXPECT_EQ(static_cast<size_t>(0),
            WrappedEncodeString(ASCIIToUTF16("")).size());
  EXPECT_EQ(static_cast<size_t>(2),
            WrappedEncodeString(ASCIIToUTF16("a")).size());
  EXPECT_EQ(static_cast<size_t>(6),
            WrappedEncodeString(ASCIIToUTF16("foo")).size());
  EXPECT_EQ(static_cast<size_t>(6),
            WrappedEncodeString(string16(test_string_a)).size());
  EXPECT_EQ(static_cast<size_t>(4),
            WrappedEncodeString(string16(test_string_b)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeString) {
  const char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  std::vector<string16> test_cases;
  test_cases.push_back(string16());
  test_cases.push_back(ASCIIToUTF16("a"));
  test_cases.push_back(ASCIIToUTF16("foo"));
  test_cases.push_back(test_string_a);
  test_cases.push_back(test_string_b);

  for (size_t i = 0; i < test_cases.size(); ++i) {
    const string16& test_case = test_cases[i];
    std::string v = WrappedEncodeString(test_case);

    StringPiece slice;
    if (v.size()) {
      slice = StringPiece(&*v.begin(), v.size());
    }

    string16 result;
    EXPECT_TRUE(DecodeString(&slice, &result));
    EXPECT_EQ(test_case, result);
    EXPECT_TRUE(slice.empty());

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), static_cast<size_t>(1), static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeString(&slice, &result));
    EXPECT_EQ(test_case, result);
    EXPECT_TRUE(slice.empty());
  }
}

static std::string WrappedEncodeStringWithLength(string16 value) {
  std::string buffer;
  EncodeStringWithLength(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeStringWithLength) {
  const char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  EXPECT_EQ(static_cast<size_t>(1),
            WrappedEncodeStringWithLength(string16()).size());
  EXPECT_EQ(static_cast<size_t>(3),
            WrappedEncodeStringWithLength(ASCIIToUTF16("a")).size());
  EXPECT_EQ(static_cast<size_t>(7),
            WrappedEncodeStringWithLength(string16(test_string_a)).size());
  EXPECT_EQ(static_cast<size_t>(5),
            WrappedEncodeStringWithLength(string16(test_string_b)).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeStringWithLength) {
  const char16 test_string_a[] = {'f', 'o', 'o', '\0'};
  const char16 test_string_b[] = {0xdead, 0xbeef, '\0'};

  const int kLongStringLen = 1234;
  char16 long_string[kLongStringLen + 1];
  for (int i = 0; i < kLongStringLen; ++i)
    long_string[i] = i;
  long_string[kLongStringLen] = 0;

  std::vector<string16> test_cases;
  test_cases.push_back(ASCIIToUTF16(""));
  test_cases.push_back(ASCIIToUTF16("a"));
  test_cases.push_back(ASCIIToUTF16("foo"));
  test_cases.push_back(string16(test_string_a));
  test_cases.push_back(string16(test_string_b));
  test_cases.push_back(string16(long_string));

  for (size_t i = 0; i < test_cases.size(); ++i) {
    string16 s = test_cases[i];
    std::string v = WrappedEncodeStringWithLength(s);
    ASSERT_GT(v.size(), static_cast<size_t>(0));
    StringPiece slice(v);
    string16 res;
    EXPECT_TRUE(DecodeStringWithLength(&slice, &res));
    EXPECT_EQ(s, res);
    EXPECT_TRUE(slice.empty());

    slice = StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeStringWithLength(&slice, &res));

    slice = StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeStringWithLength(&slice, &res));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), static_cast<size_t>(1), static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeStringWithLength(&slice, &res));
    EXPECT_EQ(s, res);
    EXPECT_TRUE(slice.empty());
  }
}

static int CompareStrings(const std::string& p, const std::string& q) {
  bool ok;
  DCHECK(!p.empty());
  DCHECK(!q.empty());
  StringPiece slice_p(p);
  StringPiece slice_q(q);
  int result = CompareEncodedStringsWithLength(&slice_p, &slice_q, &ok);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(slice_p.empty());
  EXPECT_TRUE(slice_q.empty());
  return result;
}

TEST(IndexedDBLevelDBCodingTest, CompareEncodedStringsWithLength) {
  const char16 test_string_a[] = {0x1000, 0x1000, '\0'};
  const char16 test_string_b[] = {0x1000, 0x1000, 0x1000, '\0'};
  const char16 test_string_c[] = {0x1000, 0x1000, 0x1001, '\0'};
  const char16 test_string_d[] = {0x1001, 0x1000, 0x1000, '\0'};
  const char16 test_string_e[] = {0xd834, 0xdd1e, '\0'};
  const char16 test_string_f[] = {0xfffd, '\0'};

  std::vector<string16> test_cases;
  test_cases.push_back(ASCIIToUTF16(""));
  test_cases.push_back(ASCIIToUTF16("a"));
  test_cases.push_back(ASCIIToUTF16("b"));
  test_cases.push_back(ASCIIToUTF16("baaa"));
  test_cases.push_back(ASCIIToUTF16("baab"));
  test_cases.push_back(ASCIIToUTF16("c"));
  test_cases.push_back(string16(test_string_a));
  test_cases.push_back(string16(test_string_b));
  test_cases.push_back(string16(test_string_c));
  test_cases.push_back(string16(test_string_d));
  test_cases.push_back(string16(test_string_e));
  test_cases.push_back(string16(test_string_f));

  for (size_t i = 0; i < test_cases.size() - 1; ++i) {
    string16 a = test_cases[i];
    string16 b = test_cases[i + 1];

    EXPECT_LT(a.compare(b), 0);
    EXPECT_GT(b.compare(a), 0);
    EXPECT_EQ(a.compare(a), 0);
    EXPECT_EQ(b.compare(b), 0);

    std::string encoded_a = WrappedEncodeStringWithLength(a);
    EXPECT_TRUE(encoded_a.size());
    std::string encoded_b = WrappedEncodeStringWithLength(b);
    EXPECT_TRUE(encoded_a.size());

    EXPECT_LT(CompareStrings(encoded_a, encoded_b), 0);
    EXPECT_GT(CompareStrings(encoded_b, encoded_a), 0);
    EXPECT_EQ(CompareStrings(encoded_a, encoded_a), 0);
    EXPECT_EQ(CompareStrings(encoded_b, encoded_b), 0);
  }
}

static std::string WrappedEncodeDouble(double value) {
  std::string buffer;
  EncodeDouble(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeDouble) {
  EXPECT_EQ(static_cast<size_t>(8), WrappedEncodeDouble(0).size());
  EXPECT_EQ(static_cast<size_t>(8), WrappedEncodeDouble(3.14).size());
}

TEST(IndexedDBLevelDBCodingTest, DecodeDouble) {
  std::vector<double> test_cases;
  test_cases.push_back(3.14);
  test_cases.push_back(-3.14);

  for (size_t i = 0; i < test_cases.size(); ++i) {
    double value = test_cases[i];
    std::string v = WrappedEncodeDouble(value);
    ASSERT_GT(v.size(), static_cast<size_t>(0));
    StringPiece slice(v);
    double result;
    EXPECT_TRUE(DecodeDouble(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());

    slice = StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeDouble(&slice, &result));

    slice = StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeDouble(&slice, &result));

    // Verify decoding at an offset, to detect unaligned memory access.
    v.insert(v.begin(), static_cast<size_t>(1), static_cast<char>(0));
    slice = StringPiece(&*v.begin() + 1, v.size() - 1);
    EXPECT_TRUE(DecodeDouble(&slice, &result));
    EXPECT_EQ(value, result);
    EXPECT_TRUE(slice.empty());
  }
}

TEST(IndexedDBLevelDBCodingTest, EncodeDecodeIDBKey) {
  IndexedDBKey expected_key;
  scoped_ptr<IndexedDBKey> decoded_key;
  std::string v;
  StringPiece slice;

  std::vector<IndexedDBKey> test_cases;
  test_cases.push_back(IndexedDBKey(1234, WebIDBKeyTypeNumber));
  test_cases.push_back(IndexedDBKey(7890, WebIDBKeyTypeDate));
  test_cases.push_back(IndexedDBKey(ASCIIToUTF16("Hello World!")));
  test_cases.push_back(IndexedDBKey(IndexedDBKey::KeyArray()));

  IndexedDBKey::KeyArray array;
  array.push_back(IndexedDBKey(1234, WebIDBKeyTypeNumber));
  array.push_back(IndexedDBKey(7890, WebIDBKeyTypeDate));
  array.push_back(IndexedDBKey(ASCIIToUTF16("Hello World!")));
  array.push_back(IndexedDBKey(IndexedDBKey::KeyArray()));
  test_cases.push_back(IndexedDBKey(array));

  for (size_t i = 0; i < test_cases.size(); ++i) {
    expected_key = test_cases[i];
    v.clear();
    EncodeIDBKey(expected_key, &v);
    slice = StringPiece(&*v.begin(), v.size());
    EXPECT_TRUE(DecodeIDBKey(&slice, &decoded_key));
    EXPECT_TRUE(decoded_key->IsEqual(expected_key));
    EXPECT_TRUE(slice.empty());

    slice = StringPiece(&*v.begin(), v.size() - 1);
    EXPECT_FALSE(DecodeIDBKey(&slice, &decoded_key));

    slice = StringPiece(&*v.begin(), static_cast<size_t>(0));
    EXPECT_FALSE(DecodeIDBKey(&slice, &decoded_key));
  }
}

static std::string WrappedEncodeIDBKeyPath(const IndexedDBKeyPath& value) {
  std::string buffer;
  EncodeIDBKeyPath(value, &buffer);
  return buffer;
}

TEST(IndexedDBLevelDBCodingTest, EncodeDecodeIDBKeyPath) {
  std::vector<IndexedDBKeyPath> key_paths;
  std::vector<std::string> encoded_paths;

  {
    key_paths.push_back(IndexedDBKeyPath());
    char expected[] = {0, 0,  // Header
                       0      // Type is null
    };
    encoded_paths.push_back(
        std::string(expected, expected + arraysize(expected)));
  }

  {
    key_paths.push_back(IndexedDBKeyPath(string16()));
    char expected[] = {0, 0,  // Header
                       1,     // Type is string
                       0      // Length is 0
    };
    encoded_paths.push_back(
        std::string(expected, expected + arraysize(expected)));
  }

  {
    key_paths.push_back(IndexedDBKeyPath(ASCIIToUTF16("foo")));
    char expected[] = {0, 0,                      // Header
                       1,                         // Type is string
                       3, 0, 'f', 0, 'o', 0, 'o'  // String length 3, UTF-16BE
    };
    encoded_paths.push_back(
        std::string(expected, expected + arraysize(expected)));
  }

  {
    key_paths.push_back(IndexedDBKeyPath(ASCIIToUTF16("foo.bar")));
    char expected[] = {0, 0,  // Header
                       1,     // Type is string
                       7, 0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0,
                       'r'  // String length 7, UTF-16BE
    };
    encoded_paths.push_back(
        std::string(expected, expected + arraysize(expected)));
  }

  {
    std::vector<string16> array;
    array.push_back(string16());
    array.push_back(ASCIIToUTF16("foo"));
    array.push_back(ASCIIToUTF16("foo.bar"));

    key_paths.push_back(IndexedDBKeyPath(array));
    char expected[] = {0, 0,                       // Header
                       2, 3,                       // Type is array, length is 3
                       0,                          // Member 1 (String length 0)
                       3, 0, 'f', 0, 'o', 0, 'o',  // Member 2 (String length 3)
                       7, 0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0,
                       'r'  // Member 3 (String length 7)
    };
    encoded_paths.push_back(
        std::string(expected, expected + arraysize(expected)));
  }

  ASSERT_EQ(key_paths.size(), encoded_paths.size());
  for (size_t i = 0; i < key_paths.size(); ++i) {
    IndexedDBKeyPath key_path = key_paths[i];
    std::string encoded = encoded_paths[i];

    std::string v = WrappedEncodeIDBKeyPath(key_path);
    EXPECT_EQ(encoded, v);

    StringPiece slice(encoded);
    IndexedDBKeyPath decoded;
    EXPECT_TRUE(DecodeIDBKeyPath(&slice, &decoded));
    EXPECT_EQ(key_path, decoded);
    EXPECT_TRUE(slice.empty());
  }
}

TEST(IndexedDBLevelDBCodingTest, DecodeLegacyIDBKeyPath) {
  // Legacy encoding of string key paths.
  std::vector<IndexedDBKeyPath> key_paths;
  std::vector<std::string> encoded_paths;

  {
    key_paths.push_back(IndexedDBKeyPath(string16()));
    encoded_paths.push_back(std::string());
  }
  {
    key_paths.push_back(IndexedDBKeyPath(ASCIIToUTF16("foo")));
    char expected[] = {0, 'f', 0, 'o', 0, 'o'};
    encoded_paths.push_back(std::string(expected, arraysize(expected)));
  }
  {
    key_paths.push_back(IndexedDBKeyPath(ASCIIToUTF16("foo.bar")));
    char expected[] = {0, 'f', 0, 'o', 0, 'o', 0, '.', 0, 'b', 0, 'a', 0, 'r'};
    encoded_paths.push_back(std::string(expected, arraysize(expected)));
  }

  ASSERT_EQ(key_paths.size(), encoded_paths.size());
  for (size_t i = 0; i < key_paths.size(); ++i) {
    IndexedDBKeyPath key_path = key_paths[i];
    std::string encoded = encoded_paths[i];

    StringPiece slice(encoded);
    IndexedDBKeyPath decoded;
    EXPECT_TRUE(DecodeIDBKeyPath(&slice, &decoded));
    EXPECT_EQ(key_path, decoded);
    EXPECT_TRUE(slice.empty());
  }
}

TEST(IndexedDBLevelDBCodingTest, ExtractAndCompareIDBKeys) {
  std::vector<IndexedDBKey> keys;

  keys.push_back(IndexedDBKey(-10, WebIDBKeyTypeNumber));
  keys.push_back(IndexedDBKey(0, WebIDBKeyTypeNumber));
  keys.push_back(IndexedDBKey(3.14, WebIDBKeyTypeNumber));

  keys.push_back(IndexedDBKey(0, WebIDBKeyTypeDate));
  keys.push_back(IndexedDBKey(100, WebIDBKeyTypeDate));
  keys.push_back(IndexedDBKey(100000, WebIDBKeyTypeDate));

  keys.push_back(IndexedDBKey(ASCIIToUTF16("")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("a")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("b")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("baaa")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("baab")));
  keys.push_back(IndexedDBKey(ASCIIToUTF16("c")));

  keys.push_back(CreateArrayIDBKey());
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(0, WebIDBKeyTypeNumber)));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(0, WebIDBKeyTypeNumber),
                                   IndexedDBKey(3.14, WebIDBKeyTypeNumber)));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(0, WebIDBKeyTypeDate)));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(0, WebIDBKeyTypeDate),
                                   IndexedDBKey(0, WebIDBKeyTypeDate)));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(ASCIIToUTF16(""))));
  keys.push_back(CreateArrayIDBKey(IndexedDBKey(ASCIIToUTF16("")),
                                   IndexedDBKey(ASCIIToUTF16("a"))));
  keys.push_back(CreateArrayIDBKey(CreateArrayIDBKey()));
  keys.push_back(CreateArrayIDBKey(CreateArrayIDBKey(), CreateArrayIDBKey()));
  keys.push_back(CreateArrayIDBKey(CreateArrayIDBKey(CreateArrayIDBKey())));
  keys.push_back(CreateArrayIDBKey(
      CreateArrayIDBKey(CreateArrayIDBKey(CreateArrayIDBKey()))));

  for (size_t i = 0; i < keys.size() - 1; ++i) {
    const IndexedDBKey& key_a = keys[i];
    const IndexedDBKey& key_b = keys[i + 1];

    EXPECT_TRUE(key_a.IsLessThan(key_b));

    std::string encoded_a;
    EncodeIDBKey(key_a, &encoded_a);
    EXPECT_TRUE(encoded_a.size());
    std::string encoded_b;
    EncodeIDBKey(key_b, &encoded_b);
    EXPECT_TRUE(encoded_b.size());

    std::string extracted_a;
    std::string extracted_b;
    StringPiece slice;

    slice = StringPiece(encoded_a);
    EXPECT_TRUE(ExtractEncodedIDBKey(&slice, &extracted_a));
    EXPECT_TRUE(slice.empty());
    EXPECT_EQ(encoded_a, extracted_a);

    slice = StringPiece(encoded_b);
    EXPECT_TRUE(ExtractEncodedIDBKey(&slice, &extracted_b));
    EXPECT_TRUE(slice.empty());
    EXPECT_EQ(encoded_b, extracted_b);

    EXPECT_LT(CompareKeys(extracted_a, extracted_b), 0);
    EXPECT_GT(CompareKeys(extracted_b, extracted_a), 0);
    EXPECT_EQ(CompareKeys(extracted_a, extracted_a), 0);
    EXPECT_EQ(CompareKeys(extracted_b, extracted_b), 0);

    slice = StringPiece(&*encoded_a.begin(), encoded_a.size() - 1);
    EXPECT_FALSE(ExtractEncodedIDBKey(&slice, &extracted_a));
  }
}

TEST(IndexedDBLevelDBCodingTest, ComparisonTest) {
  std::vector<std::string> keys;
  keys.push_back(SchemaVersionKey::Encode());
  keys.push_back(MaxDatabaseIdKey::Encode());
  keys.push_back(DatabaseFreeListKey::Encode(0));
  keys.push_back(DatabaseFreeListKey::EncodeMaxKey());
  keys.push_back(DatabaseNameKey::Encode("", ASCIIToUTF16("")));
  keys.push_back(DatabaseNameKey::Encode("", ASCIIToUTF16("a")));
  keys.push_back(DatabaseNameKey::Encode("a", ASCIIToUTF16("a")));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::ORIGIN_NAME));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::DATABASE_NAME));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::USER_VERSION));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::MAX_OBJECT_STORE_ID));
  keys.push_back(
      DatabaseMetaDataKey::Encode(1, DatabaseMetaDataKey::USER_INT_VERSION));
  keys.push_back(
      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::NAME));
  keys.push_back(
      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::KEY_PATH));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::AUTO_INCREMENT));
  keys.push_back(
      ObjectStoreMetaDataKey::Encode(1, 1, ObjectStoreMetaDataKey::EVICTABLE));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::LAST_VERSION));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::MAX_INDEX_ID));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::HAS_KEY_PATH));
  keys.push_back(ObjectStoreMetaDataKey::Encode(
      1, 1, ObjectStoreMetaDataKey::KEY_GENERATOR_CURRENT_NUMBER));
  keys.push_back(ObjectStoreMetaDataKey::EncodeMaxKey(1, 1));
  keys.push_back(ObjectStoreMetaDataKey::EncodeMaxKey(1, 2));
  keys.push_back(ObjectStoreMetaDataKey::EncodeMaxKey(1));
  keys.push_back(IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::NAME));
  keys.push_back(IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::UNIQUE));
  keys.push_back(
      IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::KEY_PATH));
  keys.push_back(
      IndexMetaDataKey::Encode(1, 1, 30, IndexMetaDataKey::MULTI_ENTRY));
  keys.push_back(IndexMetaDataKey::Encode(1, 1, 31, 0));
  keys.push_back(IndexMetaDataKey::Encode(1, 1, 31, 1));
  keys.push_back(IndexMetaDataKey::EncodeMaxKey(1, 1, 31));
  keys.push_back(IndexMetaDataKey::EncodeMaxKey(1, 1, 32));
  keys.push_back(IndexMetaDataKey::EncodeMaxKey(1, 1));
  keys.push_back(IndexMetaDataKey::EncodeMaxKey(1, 2));
  keys.push_back(ObjectStoreFreeListKey::Encode(1, 1));
  keys.push_back(ObjectStoreFreeListKey::EncodeMaxKey(1));
  keys.push_back(IndexFreeListKey::Encode(1, 1, kMinimumIndexId));
  keys.push_back(IndexFreeListKey::EncodeMaxKey(1, 1));
  keys.push_back(IndexFreeListKey::Encode(1, 2, kMinimumIndexId));
  keys.push_back(IndexFreeListKey::EncodeMaxKey(1, 2));
  keys.push_back(ObjectStoreNamesKey::Encode(1, ASCIIToUTF16("")));
  keys.push_back(ObjectStoreNamesKey::Encode(1, ASCIIToUTF16("a")));
  keys.push_back(IndexNamesKey::Encode(1, 1, ASCIIToUTF16("")));
  keys.push_back(IndexNamesKey::Encode(1, 1, ASCIIToUTF16("a")));
  keys.push_back(IndexNamesKey::Encode(1, 2, ASCIIToUTF16("a")));
  keys.push_back(ObjectStoreDataKey::Encode(1, 1, std::string()));
  keys.push_back(ObjectStoreDataKey::Encode(1, 1, MinIDBKey()));
  keys.push_back(ObjectStoreDataKey::Encode(1, 1, MaxIDBKey()));
  keys.push_back(ExistsEntryKey::Encode(1, 1, std::string()));
  keys.push_back(ExistsEntryKey::Encode(1, 1, MinIDBKey()));
  keys.push_back(ExistsEntryKey::Encode(1, 1, MaxIDBKey()));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), std::string(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MinIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MinIDBKey(), 1));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MaxIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MinIDBKey(), MaxIDBKey(), 1));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MinIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MinIDBKey(), 1));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MaxIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 1, 30, MaxIDBKey(), MaxIDBKey(), 1));
  keys.push_back(IndexDataKey::Encode(1, 1, 31, MinIDBKey(), MinIDBKey(), 0));
  keys.push_back(IndexDataKey::Encode(1, 2, 30, MinIDBKey(), MinIDBKey(), 0));
  keys.push_back(
      IndexDataKey::EncodeMaxKey(1, 2, std::numeric_limits<int32>::max() - 1));

  for (size_t i = 0; i < keys.size(); ++i) {
    EXPECT_EQ(Compare(keys[i], keys[i], false), 0);

    for (size_t j = i + 1; j < keys.size(); ++j) {
      EXPECT_LT(Compare(keys[i], keys[j], false), 0);
      EXPECT_GT(Compare(keys[j], keys[i], false), 0);
    }
  }
}

TEST(IndexedDBLevelDBCodingTest, EncodeVarIntVSEncodeByteTest) {
  std::vector<unsigned char> test_cases;
  test_cases.push_back(0);
  test_cases.push_back(1);
  test_cases.push_back(127);

  for (size_t i = 0; i < test_cases.size(); ++i) {
    unsigned char n = test_cases[i];

    std::string vA = WrappedEncodeByte(n);
    std::string vB = WrappedEncodeVarInt(static_cast<int64>(n));

    EXPECT_EQ(vA.size(), vB.size());
    EXPECT_EQ(*vA.begin(), *vB.begin());
  }
}

}  // namespace

}  // namespace content
