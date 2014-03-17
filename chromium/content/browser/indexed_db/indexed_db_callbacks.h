// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "content/browser/indexed_db/indexed_db_database_error.h"
#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"
#include "content/common/indexed_db/indexed_db_key.h"
#include "content/common/indexed_db/indexed_db_key_path.h"
#include "url/gurl.h"

namespace content {
class IndexedDBConnection;
class IndexedDBCursor;
class IndexedDBDatabase;
class IndexedDBDatabaseCallbacks;
struct IndexedDBDatabaseMetadata;

class CONTENT_EXPORT IndexedDBCallbacks
    : public base::RefCounted<IndexedDBCallbacks> {
 public:
  // Simple payload responses
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32 ipc_thread_id,
                     int32 ipc_callbacks_id);

  // IndexedDBCursor responses
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32 ipc_thread_id,
                     int32 ipc_callbacks_id,
                     int32 ipc_cursor_id);

  // IndexedDBDatabase responses
  IndexedDBCallbacks(IndexedDBDispatcherHost* dispatcher_host,
                     int32 ipc_thread_id,
                     int32 ipc_callbacks_id,
                     int32 ipc_database_callbacks_id,
                     int64 host_transaction_id,
                     const GURL& origin_url);

  virtual void OnError(const IndexedDBDatabaseError& error);

  // IndexedDBFactory::GetDatabaseNames
  virtual void OnSuccess(const std::vector<base::string16>& string);

  // IndexedDBFactory::Open / DeleteDatabase
  virtual void OnBlocked(int64 existing_version);

  // IndexedDBFactory::Open
  virtual void OnUpgradeNeeded(
      int64 old_version,
      scoped_ptr<IndexedDBConnection> connection,
      const content::IndexedDBDatabaseMetadata& metadata,
      blink::WebIDBDataLoss data_loss,
      std::string data_loss_message);
  virtual void OnSuccess(scoped_ptr<IndexedDBConnection> connection,
                         const content::IndexedDBDatabaseMetadata& metadata);

  // IndexedDBDatabase::OpenCursor
  virtual void OnSuccess(scoped_refptr<IndexedDBCursor> cursor,
                         const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         std::string* value);

  // IndexedDBCursor::Continue / Advance
  virtual void OnSuccess(const IndexedDBKey& key,
                         const IndexedDBKey& primary_key,
                         std::string* value);

  // IndexedDBCursor::PrefetchContinue
  virtual void OnSuccessWithPrefetch(
      const std::vector<IndexedDBKey>& keys,
      const std::vector<IndexedDBKey>& primary_keys,
      const std::vector<std::string>& values);

  // IndexedDBDatabase::Get (with key injection)
  virtual void OnSuccess(std::string* data,
                         const IndexedDBKey& key,
                         const IndexedDBKeyPath& key_path);

  // IndexedDBDatabase::Get
  virtual void OnSuccess(std::string* value);

  // IndexedDBDatabase::Put / IndexedDBCursor::Update
  virtual void OnSuccess(const IndexedDBKey& value);

  // IndexedDBDatabase::Count
  virtual void OnSuccess(int64 value);

  // IndexedDBDatabase::Delete
  // IndexedDBCursor::Continue / Advance (when complete)
  virtual void OnSuccess();

 protected:
  virtual ~IndexedDBCallbacks();

 private:
  friend class base::RefCounted<IndexedDBCallbacks>;

  // Originally from IndexedDBCallbacks:
  scoped_refptr<IndexedDBDispatcherHost> dispatcher_host_;
  int32 ipc_callbacks_id_;
  int32 ipc_thread_id_;

  // IndexedDBCursor callbacks ------------------------
  int32 ipc_cursor_id_;

  // IndexedDBDatabase callbacks ------------------------
  int64 host_transaction_id_;
  GURL origin_url_;
  int32 ipc_database_id_;
  int32 ipc_database_callbacks_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_CALLBACKS_H_
