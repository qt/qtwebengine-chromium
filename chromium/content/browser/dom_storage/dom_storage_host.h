// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_HOST_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_HOST_H_

#include <map>

#include "base/memory/ref_counted.h"
#include "base/strings/nullable_string16.h"
#include "base/strings/string16.h"
#include "content/browser/dom_storage/dom_storage_namespace.h"
#include "content/common/content_export.h"
#include "content/common/dom_storage/dom_storage_types.h"

class GURL;

namespace content {

class DOMStorageContextImpl;
class DOMStorageHost;
class DOMStorageNamespace;
class DOMStorageArea;

// One instance is allocated in the main process for each client process.
// Used by DOMStorageMessageFilter in Chrome.
// This class is single threaded, and performs blocking file reads/writes,
// so it shouldn't be used on chrome's IO thread.
// See class comments for DOMStorageContextImpl for a larger overview.
class CONTENT_EXPORT DOMStorageHost {
 public:
  DOMStorageHost(DOMStorageContextImpl* context, int render_process_id);
  ~DOMStorageHost();

  bool OpenStorageArea(int connection_id, int namespace_id,
                       const GURL& origin);
  void CloseStorageArea(int connection_id);
  bool ExtractAreaValues(int connection_id, DOMStorageValuesMap* map,
                         bool* send_log_get_messages);
  unsigned GetAreaLength(int connection_id);
  base::NullableString16 GetAreaKey(int connection_id, unsigned index);
  base::NullableString16 GetAreaItem(int connection_id,
                                     const base::string16& key);
  bool SetAreaItem(int connection_id, const base::string16& key,
                   const base::string16& value, const GURL& page_url,
                   base::NullableString16* old_value);
  void LogGetAreaItem(int connection_id, const base::string16& key,
                      const base::NullableString16& value);
  bool RemoveAreaItem(int connection_id, const base::string16& key,
                  const GURL& page_url,
                  base::string16* old_value);
  bool ClearArea(int connection_id, const GURL& page_url);
  bool HasAreaOpen(int64 namespace_id, const GURL& origin,
                   int64* alias_namespace_id) const;
  // Resets all open areas for the namespace provided. Returns true
  // iff there were any areas to reset.
  bool ResetOpenAreasForNamespace(int64 namespace_id);

 private:
  // Struct to hold references needed for areas that are open
  // within our associated client process.
  struct NamespaceAndArea {
    scoped_refptr<DOMStorageNamespace> namespace_;
    scoped_refptr<DOMStorageArea> area_;
    NamespaceAndArea();
    ~NamespaceAndArea();
  };
  typedef std::map<int, NamespaceAndArea > AreaMap;

  DOMStorageArea* GetOpenArea(int connection_id);
  DOMStorageNamespace* GetNamespace(int connection_id);
  void MaybeLogTransaction(
      int connection_id,
      DOMStorageNamespace::LogType transaction_type,
      const GURL& origin,
      const GURL& page_url,
      const base::string16& key,
      const base::NullableString16& value);

  scoped_refptr<DOMStorageContextImpl> context_;
  AreaMap connections_;
  int render_process_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_HOST_H_
