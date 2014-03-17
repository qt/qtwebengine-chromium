// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/id_map.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_message_filter.h"
#include "url/gurl.h"

struct IndexedDBDatabaseMetadata;
struct IndexedDBHostMsg_DatabaseCount_Params;
struct IndexedDBHostMsg_DatabaseCreateIndex_Params;
struct IndexedDBHostMsg_DatabaseCreateObjectStore_Params;
struct IndexedDBHostMsg_DatabaseCreateTransaction_Params;
struct IndexedDBHostMsg_DatabaseDeleteRange_Params;
struct IndexedDBHostMsg_DatabaseGet_Params;
struct IndexedDBHostMsg_DatabaseOpenCursor_Params;
struct IndexedDBHostMsg_DatabasePut_Params;
struct IndexedDBHostMsg_DatabaseSetIndexKeys_Params;
struct IndexedDBHostMsg_FactoryDeleteDatabase_Params;
struct IndexedDBHostMsg_FactoryGetDatabaseNames_Params;
struct IndexedDBHostMsg_FactoryOpen_Params;

namespace content {
class IndexedDBConnection;
class IndexedDBContextImpl;
class IndexedDBCursor;
class IndexedDBKey;
class IndexedDBKeyPath;
class IndexedDBKeyRange;
struct IndexedDBDatabaseMetadata;

// Handles all IndexedDB related messages from a particular renderer process.
class IndexedDBDispatcherHost : public BrowserMessageFilter {
 public:
  // Only call the constructor from the UI thread.
  explicit IndexedDBDispatcherHost(IndexedDBContextImpl* indexed_db_context);

  static ::IndexedDBDatabaseMetadata ConvertMetadata(
      const content::IndexedDBDatabaseMetadata& metadata);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;
  virtual base::TaskRunner* OverrideTaskRunnerForMessage(
      const IPC::Message& message) OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;

  void FinishTransaction(int64 host_transaction_id, bool committed);

  // A shortcut for accessing our context.
  IndexedDBContextImpl* Context() { return indexed_db_context_; }

  // IndexedDBCallbacks call these methods to add the results into the
  // applicable map.  See below for more details.
  int32 Add(IndexedDBCursor* cursor);
  int32 Add(IndexedDBConnection* connection,
            int32 ipc_thread_id,
            const GURL& origin_url);

  void RegisterTransactionId(int64 host_transaction_id, const GURL& origin_url);

  IndexedDBCursor* GetCursorFromId(int32 ipc_cursor_id);

  // These are called to map a 32-bit front-end (renderer-specific) transaction
  // id to and from a back-end ("host") transaction id that encodes the process
  // id in the high 32 bits. The mapping is host-specific and ids are validated.
  int64 HostTransactionId(int64 transaction_id);
  int64 RendererTransactionId(int64 host_transaction_id);

  // These are called to decode a host transaction ID, for diagnostic purposes.
  static uint32 TransactionIdToRendererTransactionId(int64 host_transaction_id);
  static uint32 TransactionIdToProcessId(int64 host_transaction_id);

 private:
  // Friends to enable OnDestruct() delegation.
  friend class BrowserThread;
  friend class base::DeleteHelper<IndexedDBDispatcherHost>;

  virtual ~IndexedDBDispatcherHost();

  // Message processing. Most of the work is delegated to the dispatcher hosts
  // below.
  void OnIDBFactoryGetDatabaseNames(
      const IndexedDBHostMsg_FactoryGetDatabaseNames_Params& p);
  void OnIDBFactoryOpen(const IndexedDBHostMsg_FactoryOpen_Params& p);

  void OnIDBFactoryDeleteDatabase(
      const IndexedDBHostMsg_FactoryDeleteDatabase_Params& p);

  void ResetDispatcherHosts();

  // IDMap for RefCounted types
  template <typename RefCountedType>
  class RefIDMap {
   private:
    typedef int32 KeyType;

   public:
    RefIDMap() {}
    ~RefIDMap() {}

    KeyType Add(RefCountedType* data) {
      return map_.Add(new scoped_refptr<RefCountedType>(data));
    }

    RefCountedType* Lookup(KeyType id) {
      scoped_refptr<RefCountedType>* ptr = map_.Lookup(id);
      if (ptr == NULL)
        return NULL;
      return ptr->get();
    }

    void Remove(KeyType id) { map_.Remove(id); }

    void set_check_on_null_data(bool value) {
      map_.set_check_on_null_data(value);
    }

   private:
    IDMap<scoped_refptr<RefCountedType>, IDMapOwnPointer> map_;
  };

  // Helper templates.
  template <class ReturnType>
  ReturnType* GetOrTerminateProcess(IDMap<ReturnType, IDMapOwnPointer>* map,
                                    int32 ipc_return_object_id);
  template <class ReturnType>
  ReturnType* GetOrTerminateProcess(RefIDMap<ReturnType>* map,
                                    int32 ipc_return_object_id);

  template <typename MapType>
  void DestroyObject(MapType* map, int32 ipc_object_id);

  // Used in nested classes.
  typedef std::map<int32, GURL> WebIDBObjectIDToURLMap;

  typedef std::map<int64, GURL> TransactionIDToURLMap;
  typedef std::map<int64, uint64> TransactionIDToSizeMap;
  typedef std::map<int64, int64> TransactionIDToDatabaseIDMap;

  class DatabaseDispatcherHost {
   public:
    explicit DatabaseDispatcherHost(IndexedDBDispatcherHost* parent);
    ~DatabaseDispatcherHost();

    void CloseAll();
    bool OnMessageReceived(const IPC::Message& message, bool* msg_is_ok);

    void OnCreateObjectStore(
        const IndexedDBHostMsg_DatabaseCreateObjectStore_Params& params);
    void OnDeleteObjectStore(int32 ipc_database_id,
                             int64 transaction_id,
                             int64 object_store_id);
    void OnCreateTransaction(
        const IndexedDBHostMsg_DatabaseCreateTransaction_Params&);
    void OnClose(int32 ipc_database_id);
    void OnDestroyed(int32 ipc_database_id);

    void OnGet(const IndexedDBHostMsg_DatabaseGet_Params& params);
    void OnPut(const IndexedDBHostMsg_DatabasePut_Params& params);
    void OnSetIndexKeys(
        const IndexedDBHostMsg_DatabaseSetIndexKeys_Params& params);
    void OnSetIndexesReady(int32 ipc_database_id,
                           int64 transaction_id,
                           int64 object_store_id,
                           const std::vector<int64>& ids);
    void OnOpenCursor(const IndexedDBHostMsg_DatabaseOpenCursor_Params& params);
    void OnCount(const IndexedDBHostMsg_DatabaseCount_Params& params);
    void OnDeleteRange(
        const IndexedDBHostMsg_DatabaseDeleteRange_Params& params);
    void OnClear(int32 ipc_thread_id,
                 int32 ipc_callbacks_id,
                 int32 ipc_database_id,
                 int64 transaction_id,
                 int64 object_store_id);
    void OnCreateIndex(
        const IndexedDBHostMsg_DatabaseCreateIndex_Params& params);
    void OnDeleteIndex(int32 ipc_database_id,
                       int64 transaction_id,
                       int64 object_store_id,
                       int64 index_id);

    void OnAbort(int32 ipc_database_id, int64 transaction_id);
    void OnCommit(int32 ipc_database_id, int64 transaction_id);
    IndexedDBDispatcherHost* parent_;
    IDMap<IndexedDBConnection, IDMapOwnPointer> map_;
    WebIDBObjectIDToURLMap database_url_map_;
    TransactionIDToSizeMap transaction_size_map_;
    TransactionIDToURLMap transaction_url_map_;
    TransactionIDToDatabaseIDMap transaction_database_map_;
  };

  class CursorDispatcherHost {
   public:
    explicit CursorDispatcherHost(IndexedDBDispatcherHost* parent);
    ~CursorDispatcherHost();

    bool OnMessageReceived(const IPC::Message& message, bool* msg_is_ok);

    void OnAdvance(int32 ipc_object_store_id,
                   int32 ipc_thread_id,
                   int32 ipc_callbacks_id,
                   unsigned long count);
    void OnContinue(int32 ipc_object_store_id,
                    int32 ipc_thread_id,
                    int32 ipc_callbacks_id,
                    const IndexedDBKey& key,
                    const IndexedDBKey& primary_key);
    void OnPrefetch(int32 ipc_cursor_id,
                    int32 ipc_thread_id,
                    int32 ipc_callbacks_id,
                    int n);
    void OnPrefetchReset(int32 ipc_cursor_id,
                         int used_prefetches,
                         int unused_prefetches);
    void OnDestroyed(int32 ipc_cursor_id);

    IndexedDBDispatcherHost* parent_;
    RefIDMap<IndexedDBCursor> map_;
  };

  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;

  // Only access on IndexedDB thread.
  scoped_ptr<DatabaseDispatcherHost> database_dispatcher_host_;
  scoped_ptr<CursorDispatcherHost> cursor_dispatcher_host_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(IndexedDBDispatcherHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_DISPATCHER_HOST_H_
