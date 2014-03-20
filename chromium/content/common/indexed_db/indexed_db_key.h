// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_H_
#define CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/platform/WebIDBTypes.h"

namespace blink {
class WebIDBKey;
}

namespace content {

class CONTENT_EXPORT IndexedDBKey {
 public:
  typedef std::vector<IndexedDBKey> KeyArray;

  IndexedDBKey();  // Defaults to blink::WebIDBKeyTypeInvalid.
  IndexedDBKey(blink::WebIDBKeyType);  // must be Null or Invalid
  explicit IndexedDBKey(const KeyArray& array);
  explicit IndexedDBKey(const std::string& binary);
  explicit IndexedDBKey(const base::string16& str);
  IndexedDBKey(double number,
               blink::WebIDBKeyType type);  // must be date or number
  ~IndexedDBKey();

  bool IsValid() const;

  int Compare(const IndexedDBKey& other) const;
  bool IsLessThan(const IndexedDBKey& other) const;
  bool IsEqual(const IndexedDBKey& other) const;

  blink::WebIDBKeyType type() const { return type_; }
  const std::vector<IndexedDBKey>& array() const { return array_; }
  const std::string& binary() const { return binary_; }
  const base::string16& string() const { return string_; }
  double date() const { return date_; }
  double number() const { return number_; }

  size_t size_estimate() const { return size_estimate_; }

 private:
  blink::WebIDBKeyType type_;
  std::vector<IndexedDBKey> array_;
  std::string binary_;
  base::string16 string_;
  double date_;
  double number_;

  size_t size_estimate_;
};

}  // namespace content

#endif  // CONTENT_COMMON_INDEXED_DB_INDEXED_DB_KEY_H_
