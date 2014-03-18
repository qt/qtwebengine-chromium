// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_INDEXED_DB_INDEXED_DB_KEY_BUILDERS_H_
#define CONTENT_CHILD_INDEXED_DB_INDEXED_DB_KEY_BUILDERS_H_

#include "content/common/content_export.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "content/common/indexed_db/indexed_db_key_range.h"
#include "third_party/WebKit/public/platform/WebIDBKey.h"
#include "third_party/WebKit/public/platform/WebIDBKeyPath.h"
#include "third_party/WebKit/public/platform/WebIDBKeyRange.h"

namespace blink {
class WebIDBKey;
}

namespace content {

class CONTENT_EXPORT IndexedDBKeyBuilder {
 public:
  static IndexedDBKey Build(const blink::WebIDBKey& key);
};

class CONTENT_EXPORT WebIDBKeyBuilder {
 public:
  static blink::WebIDBKey Build(const content::IndexedDBKey& key);
};

class CONTENT_EXPORT IndexedDBKeyRangeBuilder {
 public:
  static IndexedDBKeyRange Build(const blink::WebIDBKeyRange& key_range);
};

class CONTENT_EXPORT IndexedDBKeyPathBuilder {
 public:
  static IndexedDBKeyPath Build(const blink::WebIDBKeyPath& key_path);
};

class CONTENT_EXPORT WebIDBKeyPathBuilder {
 public:
  static blink::WebIDBKeyPath Build(const IndexedDBKeyPath& key_path);
};

}  // namespace content

#endif  // CONTENT_CHILD_INDEXED_DB_INDEXED_DB_KEY_BUILDERS_H_
