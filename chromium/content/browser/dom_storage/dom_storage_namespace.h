// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_
#define CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_

#include <map>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {

class DOMStorageArea;
class DOMStorageTaskRunner;
class SessionStorageDatabase;

// Container for the set of per-origin Areas.
// See class comments for DOMStorageContextImpl for a larger overview.
class CONTENT_EXPORT DOMStorageNamespace
    : public base::RefCountedThreadSafe<DOMStorageNamespace> {
 public:
  // Option for PurgeMemory.
  enum PurgeOption {
    // Purge unopened areas only.
    PURGE_UNOPENED,

    // Purge aggressively, i.e. discard cache even for areas that have
    // non-zero open count.
    PURGE_AGGRESSIVE,
  };

  // Constructor for a LocalStorage namespace with id of 0
  // and an optional backing directory on disk.
  DOMStorageNamespace(const base::FilePath& directory,  // may be empty
                      DOMStorageTaskRunner* task_runner);

  // Constructor for a SessionStorage namespace with a non-zero id and an
  // optional backing on disk via |session_storage_database| (may be NULL).
  DOMStorageNamespace(int64 namespace_id,
                      const std::string& persistent_namespace_id,
                      SessionStorageDatabase* session_storage_database,
                      DOMStorageTaskRunner* task_runner);

  int64 namespace_id() const { return namespace_id_; }
  const std::string& persistent_namespace_id() const {
    return persistent_namespace_id_;
  }

  // Returns the storage area for the given origin,
  // creating instance if needed. Each call to open
  // must be balanced with a call to CloseStorageArea.
  DOMStorageArea* OpenStorageArea(const GURL& origin);
  void CloseStorageArea(DOMStorageArea* area);

  // Returns the area for |origin| if it's open, otherwise NULL.
  DOMStorageArea* GetOpenStorageArea(const GURL& origin);

  // Creates a clone of |this| namespace including
  // shallow copies of all contained areas.
  // Should only be called for session storage namespaces.
  DOMStorageNamespace* Clone(int64 clone_namespace_id,
                             const std::string& clone_persistent_namespace_id);

  void DeleteLocalStorageOrigin(const GURL& origin);
  void DeleteSessionStorageOrigin(const GURL& origin);
  void PurgeMemory(PurgeOption purge);
  void Shutdown();

  unsigned int CountInMemoryAreas() const;

 private:
  friend class base::RefCountedThreadSafe<DOMStorageNamespace>;

  // Struct to hold references to our contained areas and
  // to keep track of how many tabs have a given area open.
  struct AreaHolder {
    scoped_refptr<DOMStorageArea> area_;
    int open_count_;
    AreaHolder();
    AreaHolder(DOMStorageArea* area, int count);
    ~AreaHolder();
  };
  typedef std::map<GURL, AreaHolder> AreaMap;

  ~DOMStorageNamespace();

  // Returns a pointer to the area holder in our map or NULL.
  AreaHolder* GetAreaHolder(const GURL& origin);

  int64 namespace_id_;
  std::string persistent_namespace_id_;
  base::FilePath directory_;
  AreaMap areas_;
  scoped_refptr<DOMStorageTaskRunner> task_runner_;
  scoped_refptr<SessionStorageDatabase> session_storage_database_;
};

}  // namespace content


#endif  // CONTENT_BROWSER_DOM_STORAGE_DOM_STORAGE_NAMESPACE_H_
