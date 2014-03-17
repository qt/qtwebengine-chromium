// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_dispatcher_host.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/process.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/indexed_db/indexed_db_callbacks.h"
#include "content/browser/indexed_db/indexed_db_connection.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/indexed_db/indexed_db_cursor.h"
#include "content/browser/indexed_db/indexed_db_database_callbacks.h"
#include "content/browser/indexed_db/indexed_db_metadata.h"
#include "content/browser/renderer_host/render_message_filter.h"
#include "content/common/indexed_db/indexed_db_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/result_codes.h"
#include "third_party/WebKit/public/platform/WebIDBDatabaseException.h"
#include "url/gurl.h"
#include "webkit/browser/database/database_util.h"
#include "webkit/common/database/database_identifier.h"

using webkit_database::DatabaseUtil;
using blink::WebIDBKey;

namespace content {

IndexedDBDispatcherHost::IndexedDBDispatcherHost(
    IndexedDBContextImpl* indexed_db_context)
    : indexed_db_context_(indexed_db_context),
      database_dispatcher_host_(new DatabaseDispatcherHost(this)),
      cursor_dispatcher_host_(new CursorDispatcherHost(this)) {
  DCHECK(indexed_db_context_);
}

IndexedDBDispatcherHost::~IndexedDBDispatcherHost() {}

void IndexedDBDispatcherHost::OnChannelClosing() {
  bool success = indexed_db_context_->TaskRunner()->PostTask(
      FROM_HERE,
      base::Bind(&IndexedDBDispatcherHost::ResetDispatcherHosts, this));

  if (!success)
    ResetDispatcherHosts();
}

void IndexedDBDispatcherHost::OnDestruct() const {
  // The last reference to the dispatcher may be a posted task, which would
  // be destructed on the IndexedDB thread. Without this override, that would
  // take the dispatcher with it. Since the dispatcher may be keeping the
  // IndexedDBContext alive, it might be destructed to on its own thread,
  // which is not supported. Ensure destruction runs on the IO thread instead.
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

void IndexedDBDispatcherHost::ResetDispatcherHosts() {
  // It is important that the various *_dispatcher_host_ members are reset
  // on the IndexedDB thread, since there might be incoming messages on that
  // thread, and we must not reset the dispatcher hosts until after those
  // messages are processed.
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  // Note that we explicitly separate CloseAll() from destruction of the
  // DatabaseDispatcherHost, since CloseAll() can invoke callbacks which need to
  // be dispatched through database_dispatcher_host_.
  database_dispatcher_host_->CloseAll();
  database_dispatcher_host_.reset();
  cursor_dispatcher_host_.reset();
}

base::TaskRunner* IndexedDBDispatcherHost::OverrideTaskRunnerForMessage(
    const IPC::Message& message) {
  if (IPC_MESSAGE_CLASS(message) == IndexedDBMsgStart)
    return indexed_db_context_->TaskRunner();
  return NULL;
}

bool IndexedDBDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  if (IPC_MESSAGE_CLASS(message) != IndexedDBMsgStart)
    return false;

  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  bool handled =
      database_dispatcher_host_->OnMessageReceived(message, message_was_ok) ||
      cursor_dispatcher_host_->OnMessageReceived(message, message_was_ok);

  if (!handled) {
    handled = true;
    IPC_BEGIN_MESSAGE_MAP_EX(IndexedDBDispatcherHost, message, *message_was_ok)
      IPC_MESSAGE_HANDLER(IndexedDBHostMsg_FactoryGetDatabaseNames,
                          OnIDBFactoryGetDatabaseNames)
      IPC_MESSAGE_HANDLER(IndexedDBHostMsg_FactoryOpen, OnIDBFactoryOpen)
      IPC_MESSAGE_HANDLER(IndexedDBHostMsg_FactoryDeleteDatabase,
                          OnIDBFactoryDeleteDatabase)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
  }
  return handled;
}

int32 IndexedDBDispatcherHost::Add(IndexedDBCursor* cursor) {
  if (!cursor_dispatcher_host_) {
    return 0;
  }
  return cursor_dispatcher_host_->map_.Add(cursor);
}

int32 IndexedDBDispatcherHost::Add(IndexedDBConnection* connection,
                                   int32 ipc_thread_id,
                                   const GURL& origin_url) {
  if (!database_dispatcher_host_) {
    connection->Close();
    delete connection;
    return -1;
  }
  int32 ipc_database_id = database_dispatcher_host_->map_.Add(connection);
  Context()->ConnectionOpened(origin_url, connection);
  database_dispatcher_host_->database_url_map_[ipc_database_id] = origin_url;
  return ipc_database_id;
}

void IndexedDBDispatcherHost::RegisterTransactionId(int64 host_transaction_id,
                                                    const GURL& url) {
  if (!database_dispatcher_host_)
    return;
  database_dispatcher_host_->transaction_url_map_[host_transaction_id] = url;
}

int64 IndexedDBDispatcherHost::HostTransactionId(int64 transaction_id) {
  // Inject the renderer process id into the transaction id, to
  // uniquely identify this transaction, and effectively bind it to
  // the renderer that initiated it. The lower 32 bits of
  // transaction_id are guaranteed to be unique within that renderer.
  base::ProcessId pid = peer_pid();
  DCHECK(!(transaction_id >> 32)) << "Transaction ids can only be 32 bits";
  COMPILE_ASSERT(sizeof(base::ProcessId) <= sizeof(int32),
                 Process_ID_must_fit_in_32_bits);

  return transaction_id | (static_cast<uint64>(pid) << 32);
}

int64 IndexedDBDispatcherHost::RendererTransactionId(
    int64 host_transaction_id) {
  DCHECK(host_transaction_id >> 32 == peer_pid())
      << "Invalid renderer target for transaction id";
  return host_transaction_id & 0xffffffff;
}

// static
uint32 IndexedDBDispatcherHost::TransactionIdToRendererTransactionId(
    int64 host_transaction_id) {
  return host_transaction_id & 0xffffffff;
}

// static
uint32 IndexedDBDispatcherHost::TransactionIdToProcessId(
    int64 host_transaction_id) {
  return (host_transaction_id >> 32) & 0xffffffff;
}


IndexedDBCursor* IndexedDBDispatcherHost::GetCursorFromId(int32 ipc_cursor_id) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  return cursor_dispatcher_host_->map_.Lookup(ipc_cursor_id);
}

::IndexedDBDatabaseMetadata IndexedDBDispatcherHost::ConvertMetadata(
    const content::IndexedDBDatabaseMetadata& web_metadata) {
  ::IndexedDBDatabaseMetadata metadata;
  metadata.id = web_metadata.id;
  metadata.name = web_metadata.name;
  metadata.version = web_metadata.version;
  metadata.int_version = web_metadata.int_version;
  metadata.max_object_store_id = web_metadata.max_object_store_id;

  for (content::IndexedDBDatabaseMetadata::ObjectStoreMap::const_iterator iter =
           web_metadata.object_stores.begin();
       iter != web_metadata.object_stores.end();
       ++iter) {

    const content::IndexedDBObjectStoreMetadata& web_store_metadata =
        iter->second;
    ::IndexedDBObjectStoreMetadata idb_store_metadata;
    idb_store_metadata.id = web_store_metadata.id;
    idb_store_metadata.name = web_store_metadata.name;
    idb_store_metadata.keyPath = web_store_metadata.key_path;
    idb_store_metadata.autoIncrement = web_store_metadata.auto_increment;
    idb_store_metadata.max_index_id = web_store_metadata.max_index_id;

    for (content::IndexedDBObjectStoreMetadata::IndexMap::const_iterator
             index_iter = web_store_metadata.indexes.begin();
         index_iter != web_store_metadata.indexes.end();
         ++index_iter) {
      const content::IndexedDBIndexMetadata& web_index_metadata =
          index_iter->second;
      ::IndexedDBIndexMetadata idb_index_metadata;
      idb_index_metadata.id = web_index_metadata.id;
      idb_index_metadata.name = web_index_metadata.name;
      idb_index_metadata.keyPath = web_index_metadata.key_path;
      idb_index_metadata.unique = web_index_metadata.unique;
      idb_index_metadata.multiEntry = web_index_metadata.multi_entry;
      idb_store_metadata.indexes.push_back(idb_index_metadata);
    }
    metadata.object_stores.push_back(idb_store_metadata);
  }
  return metadata;
}

void IndexedDBDispatcherHost::OnIDBFactoryGetDatabaseNames(
    const IndexedDBHostMsg_FactoryGetDatabaseNames_Params& params) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  base::FilePath indexed_db_path = indexed_db_context_->data_path();

  GURL origin_url =
      webkit_database::GetOriginFromIdentifier(params.database_identifier);

  Context()->GetIDBFactory()->GetDatabaseNames(
      new IndexedDBCallbacks(
          this, params.ipc_thread_id, params.ipc_callbacks_id),
      origin_url,
      indexed_db_path);
}

void IndexedDBDispatcherHost::OnIDBFactoryOpen(
    const IndexedDBHostMsg_FactoryOpen_Params& params) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  base::FilePath indexed_db_path = indexed_db_context_->data_path();

  GURL origin_url =
      webkit_database::GetOriginFromIdentifier(params.database_identifier);

  int64 host_transaction_id = HostTransactionId(params.transaction_id);

  // TODO(dgrogan): Don't let a non-existing database be opened (and therefore
  // created) if this origin is already over quota.
  scoped_refptr<IndexedDBCallbacks> callbacks =
      new IndexedDBCallbacks(this,
                             params.ipc_thread_id,
                             params.ipc_callbacks_id,
                             params.ipc_database_callbacks_id,
                             host_transaction_id,
                             origin_url);
  scoped_refptr<IndexedDBDatabaseCallbacks> database_callbacks =
      new IndexedDBDatabaseCallbacks(
          this, params.ipc_thread_id, params.ipc_database_callbacks_id);
  Context()->GetIDBFactory()->Open(params.name,
                                   params.version,
                                   host_transaction_id,
                                   callbacks,
                                   database_callbacks,
                                   origin_url,
                                   indexed_db_path);
}

void IndexedDBDispatcherHost::OnIDBFactoryDeleteDatabase(
    const IndexedDBHostMsg_FactoryDeleteDatabase_Params& params) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  GURL origin_url =
      webkit_database::GetOriginFromIdentifier(params.database_identifier);
  base::FilePath indexed_db_path = indexed_db_context_->data_path();
  Context()->GetIDBFactory()->DeleteDatabase(
      params.name,
      new IndexedDBCallbacks(
          this, params.ipc_thread_id, params.ipc_callbacks_id),
      origin_url,
      indexed_db_path);
}

void IndexedDBDispatcherHost::FinishTransaction(int64 host_transaction_id,
                                                bool committed) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  if (!database_dispatcher_host_)
    return;
  TransactionIDToURLMap& transaction_url_map =
      database_dispatcher_host_->transaction_url_map_;
  TransactionIDToSizeMap& transaction_size_map =
      database_dispatcher_host_->transaction_size_map_;
  TransactionIDToDatabaseIDMap& transaction_database_map =
      database_dispatcher_host_->transaction_database_map_;
  if (committed)
    Context()->TransactionComplete(transaction_url_map[host_transaction_id]);
  transaction_url_map.erase(host_transaction_id);
  transaction_size_map.erase(host_transaction_id);
  transaction_database_map.erase(host_transaction_id);
}

//////////////////////////////////////////////////////////////////////
// Helper templates.
//

template <typename ObjectType>
ObjectType* IndexedDBDispatcherHost::GetOrTerminateProcess(
    IDMap<ObjectType, IDMapOwnPointer>* map,
    int32 ipc_return_object_id) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  ObjectType* return_object = map->Lookup(ipc_return_object_id);
  if (!return_object) {
    NOTREACHED() << "Uh oh, couldn't find object with id "
                 << ipc_return_object_id;
    RecordAction(UserMetricsAction("BadMessageTerminate_IDBMF"));
    BadMessageReceived();
  }
  return return_object;
}

template <typename ObjectType>
ObjectType* IndexedDBDispatcherHost::GetOrTerminateProcess(
    RefIDMap<ObjectType>* map,
    int32 ipc_return_object_id) {
  DCHECK(indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  ObjectType* return_object = map->Lookup(ipc_return_object_id);
  if (!return_object) {
    NOTREACHED() << "Uh oh, couldn't find object with id "
                 << ipc_return_object_id;
    RecordAction(UserMetricsAction("BadMessageTerminate_IDBMF"));
    BadMessageReceived();
  }
  return return_object;
}

template <typename MapType>
void IndexedDBDispatcherHost::DestroyObject(MapType* map, int32 ipc_object_id) {
  GetOrTerminateProcess(map, ipc_object_id);
  map->Remove(ipc_object_id);
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::DatabaseDispatcherHost
//

IndexedDBDispatcherHost::DatabaseDispatcherHost::DatabaseDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
  map_.set_check_on_null_data(true);
}

IndexedDBDispatcherHost::DatabaseDispatcherHost::~DatabaseDispatcherHost() {
  // TODO(alecflett): uncomment these when we find the source of these leaks.
  // DCHECK(transaction_size_map_.empty());
  // DCHECK(transaction_url_map_.empty());
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::CloseAll() {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  // Abort outstanding transactions started by connections in the associated
  // front-end to unblock later transactions. This should only occur on unclean
  // (crash) or abrupt (process-kill) shutdowns.
  for (TransactionIDToDatabaseIDMap::iterator iter =
           transaction_database_map_.begin();
       iter != transaction_database_map_.end();) {
    int64 transaction_id = iter->first;
    int32 ipc_database_id = iter->second;
    ++iter;
    IndexedDBConnection* connection = map_.Lookup(ipc_database_id);
    if (connection && connection->IsConnected()) {
      connection->database()->Abort(
          transaction_id,
          IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionUnknownError));
    }
  }
  DCHECK(transaction_database_map_.empty());

  for (WebIDBObjectIDToURLMap::iterator iter = database_url_map_.begin();
       iter != database_url_map_.end();
       iter++) {
    IndexedDBConnection* connection = map_.Lookup(iter->first);
    if (connection && connection->IsConnected()) {
      connection->Close();
      parent_->Context()->ConnectionClosed(iter->second, connection);
    }
  }
}

bool IndexedDBDispatcherHost::DatabaseDispatcherHost::OnMessageReceived(
    const IPC::Message& message,
    bool* msg_is_ok) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(
      IndexedDBDispatcherHost::DatabaseDispatcherHost, message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCreateObjectStore,
                        OnCreateObjectStore)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDeleteObjectStore,
                        OnDeleteObjectStore)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCreateTransaction,
                        OnCreateTransaction)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseClose, OnClose)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDestroyed, OnDestroyed)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseGet, OnGet)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabasePut, OnPut)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseSetIndexKeys, OnSetIndexKeys)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseSetIndexesReady,
                        OnSetIndexesReady)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseOpenCursor, OnOpenCursor)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCount, OnCount)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDeleteRange, OnDeleteRange)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseClear, OnClear)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCreateIndex, OnCreateIndex)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseDeleteIndex, OnDeleteIndex)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseAbort, OnAbort)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_DatabaseCommit, OnCommit)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCreateObjectStore(
    const IndexedDBHostMsg_DatabaseCreateObjectStore_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  int64 host_transaction_id = parent_->HostTransactionId(params.transaction_id);
  connection->database()->CreateObjectStore(host_transaction_id,
                                            params.object_store_id,
                                            params.name,
                                            params.key_path,
                                            params.auto_increment);
  if (parent_->Context()->IsOverQuota(
          database_url_map_[params.ipc_database_id])) {
    connection->database()->Abort(
        host_transaction_id,
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionQuotaError));
  }
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDeleteObjectStore(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  connection->database()->DeleteObjectStore(
      parent_->HostTransactionId(transaction_id), object_store_id);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCreateTransaction(
    const IndexedDBHostMsg_DatabaseCreateTransaction_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  int64 host_transaction_id = parent_->HostTransactionId(params.transaction_id);

  if (transaction_database_map_.find(host_transaction_id) !=
      transaction_database_map_.end()) {
    DLOG(ERROR) << "Duplicate host_transaction_id.";
    return;
  }

  connection->database()->CreateTransaction(
      host_transaction_id, connection, params.object_store_ids, params.mode);
  transaction_database_map_[host_transaction_id] = params.ipc_database_id;
  parent_->RegisterTransactionId(host_transaction_id,
                                 database_url_map_[params.ipc_database_id]);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnClose(
    int32 ipc_database_id) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;
  connection->Close();
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDestroyed(
    int32 ipc_object_id) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection = map_.Lookup(ipc_object_id);
  parent_->Context()
      ->ConnectionClosed(database_url_map_[ipc_object_id], connection);
  database_url_map_.erase(ipc_object_id);
  parent_->DestroyObject(&map_, ipc_object_id);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnGet(
    const IndexedDBHostMsg_DatabaseGet_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      parent_, params.ipc_thread_id, params.ipc_callbacks_id));
  connection->database()->Get(
      parent_->HostTransactionId(params.transaction_id),
      params.object_store_id,
      params.index_id,
      make_scoped_ptr(new IndexedDBKeyRange(params.key_range)),
      params.key_only,
      callbacks);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnPut(
    const IndexedDBHostMsg_DatabasePut_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;
  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      parent_, params.ipc_thread_id, params.ipc_callbacks_id));

  int64 host_transaction_id = parent_->HostTransactionId(params.transaction_id);
  if (params.index_ids.size() != params.index_keys.size()) {
    connection->database()->Abort(
        host_transaction_id,
        IndexedDBDatabaseError(
            blink::WebIDBDatabaseExceptionUnknownError,
            "Malformed IPC message: index_ids.size() != index_keys.size()"));
    parent_->BadMessageReceived();
    return;
  }

  // TODO(alecflett): Avoid a copy here.
  std::string value_copy(params.value);
  connection->database()->Put(
      host_transaction_id,
      params.object_store_id,
      &value_copy,
      make_scoped_ptr(new IndexedDBKey(params.key)),
      static_cast<IndexedDBDatabase::PutMode>(params.put_mode),
      callbacks,
      params.index_ids,
      params.index_keys);
  TransactionIDToSizeMap* map =
      &parent_->database_dispatcher_host_->transaction_size_map_;
  // Size can't be big enough to overflow because it represents the
  // actual bytes passed through IPC.
  (*map)[host_transaction_id] += params.value.size();
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnSetIndexKeys(
    const IndexedDBHostMsg_DatabaseSetIndexKeys_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  int64 host_transaction_id = parent_->HostTransactionId(params.transaction_id);
  if (params.index_ids.size() != params.index_keys.size()) {
    connection->database()->Abort(
        host_transaction_id,
        IndexedDBDatabaseError(
            blink::WebIDBDatabaseExceptionUnknownError,
            "Malformed IPC message: index_ids.size() != index_keys.size()"));
    parent_->BadMessageReceived();
    return;
  }

  connection->database()->SetIndexKeys(
      host_transaction_id,
      params.object_store_id,
      make_scoped_ptr(new IndexedDBKey(params.primary_key)),
      params.index_ids,
      params.index_keys);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnSetIndexesReady(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    const std::vector<int64>& index_ids) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  connection->database()->SetIndexesReady(
      parent_->HostTransactionId(transaction_id), object_store_id, index_ids);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnOpenCursor(
    const IndexedDBHostMsg_DatabaseOpenCursor_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      parent_, params.ipc_thread_id, params.ipc_callbacks_id, -1));
  connection->database()->OpenCursor(
      parent_->HostTransactionId(params.transaction_id),
      params.object_store_id,
      params.index_id,
      make_scoped_ptr(new IndexedDBKeyRange(params.key_range)),
      static_cast<indexed_db::CursorDirection>(params.direction),
      params.key_only,
      static_cast<IndexedDBDatabase::TaskType>(params.task_type),
      callbacks);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCount(
    const IndexedDBHostMsg_DatabaseCount_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      parent_, params.ipc_thread_id, params.ipc_callbacks_id));
  connection->database()->Count(
      parent_->HostTransactionId(params.transaction_id),
      params.object_store_id,
      params.index_id,
      make_scoped_ptr(new IndexedDBKeyRange(params.key_range)),
      callbacks);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDeleteRange(
    const IndexedDBHostMsg_DatabaseDeleteRange_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  scoped_refptr<IndexedDBCallbacks> callbacks(new IndexedDBCallbacks(
      parent_, params.ipc_thread_id, params.ipc_callbacks_id));
  connection->database()->DeleteRange(
      parent_->HostTransactionId(params.transaction_id),
      params.object_store_id,
      make_scoped_ptr(new IndexedDBKeyRange(params.key_range)),
      callbacks);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnClear(
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  scoped_refptr<IndexedDBCallbacks> callbacks(
      new IndexedDBCallbacks(parent_, ipc_thread_id, ipc_callbacks_id));

  connection->database()->Clear(
      parent_->HostTransactionId(transaction_id), object_store_id, callbacks);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnAbort(
    int32 ipc_database_id,
    int64 transaction_id) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  connection->database()->Abort(parent_->HostTransactionId(transaction_id));
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCommit(
    int32 ipc_database_id,
    int64 transaction_id) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  int64 host_transaction_id = parent_->HostTransactionId(transaction_id);
  int64 transaction_size = transaction_size_map_[host_transaction_id];
  if (transaction_size &&
      parent_->Context()->WouldBeOverQuota(
          transaction_url_map_[host_transaction_id], transaction_size)) {
    connection->database()->Abort(
        host_transaction_id,
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionQuotaError));
    return;
  }

  connection->database()->Commit(host_transaction_id);
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnCreateIndex(
    const IndexedDBHostMsg_DatabaseCreateIndex_Params& params) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, params.ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  int64 host_transaction_id = parent_->HostTransactionId(params.transaction_id);
  connection->database()->CreateIndex(host_transaction_id,
                                      params.object_store_id,
                                      params.index_id,
                                      params.name,
                                      params.key_path,
                                      params.unique,
                                      params.multi_entry);
  if (parent_->Context()->IsOverQuota(
          database_url_map_[params.ipc_database_id])) {
    connection->database()->Abort(
        host_transaction_id,
        IndexedDBDatabaseError(blink::WebIDBDatabaseExceptionQuotaError));
  }
}

void IndexedDBDispatcherHost::DatabaseDispatcherHost::OnDeleteIndex(
    int32 ipc_database_id,
    int64 transaction_id,
    int64 object_store_id,
    int64 index_id) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBConnection* connection =
      parent_->GetOrTerminateProcess(&map_, ipc_database_id);
  if (!connection || !connection->IsConnected())
    return;

  connection->database()->DeleteIndex(
      parent_->HostTransactionId(transaction_id), object_store_id, index_id);
}

//////////////////////////////////////////////////////////////////////
// IndexedDBDispatcherHost::CursorDispatcherHost
//

IndexedDBDispatcherHost::CursorDispatcherHost::CursorDispatcherHost(
    IndexedDBDispatcherHost* parent)
    : parent_(parent) {
  map_.set_check_on_null_data(true);
}

IndexedDBDispatcherHost::CursorDispatcherHost::~CursorDispatcherHost() {}

bool IndexedDBDispatcherHost::CursorDispatcherHost::OnMessageReceived(
    const IPC::Message& message,
    bool* msg_is_ok) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(
      IndexedDBDispatcherHost::CursorDispatcherHost, message, *msg_is_ok)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorAdvance, OnAdvance)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorContinue, OnContinue)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorPrefetch, OnPrefetch)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorPrefetchReset, OnPrefetchReset)
    IPC_MESSAGE_HANDLER(IndexedDBHostMsg_CursorDestroyed, OnDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnAdvance(
    int32 ipc_cursor_id,
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    unsigned long count) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBCursor* idb_cursor =
      parent_->GetOrTerminateProcess(&map_, ipc_cursor_id);
  if (!idb_cursor)
    return;

  idb_cursor->Advance(
      count,
      new IndexedDBCallbacks(
          parent_, ipc_thread_id, ipc_callbacks_id, ipc_cursor_id));
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnContinue(
    int32 ipc_cursor_id,
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    const IndexedDBKey& key,
    const IndexedDBKey& primary_key) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBCursor* idb_cursor =
      parent_->GetOrTerminateProcess(&map_, ipc_cursor_id);
  if (!idb_cursor)
    return;

  idb_cursor->Continue(
      key.IsValid() ? make_scoped_ptr(new IndexedDBKey(key))
                    : scoped_ptr<IndexedDBKey>(),
      primary_key.IsValid() ? make_scoped_ptr(new IndexedDBKey(primary_key))
                            : scoped_ptr<IndexedDBKey>(),
      new IndexedDBCallbacks(
          parent_, ipc_thread_id, ipc_callbacks_id, ipc_cursor_id));
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnPrefetch(
    int32 ipc_cursor_id,
    int32 ipc_thread_id,
    int32 ipc_callbacks_id,
    int n) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBCursor* idb_cursor =
      parent_->GetOrTerminateProcess(&map_, ipc_cursor_id);
  if (!idb_cursor)
    return;

  idb_cursor->PrefetchContinue(
      n,
      new IndexedDBCallbacks(
          parent_, ipc_thread_id, ipc_callbacks_id, ipc_cursor_id));
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnPrefetchReset(
    int32 ipc_cursor_id,
    int used_prefetches,
    int unused_prefetches) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  IndexedDBCursor* idb_cursor =
      parent_->GetOrTerminateProcess(&map_, ipc_cursor_id);
  if (!idb_cursor)
    return;

  idb_cursor->PrefetchReset(used_prefetches, unused_prefetches);
}

void IndexedDBDispatcherHost::CursorDispatcherHost::OnDestroyed(
    int32 ipc_object_id) {
  DCHECK(
      parent_->indexed_db_context_->TaskRunner()->RunsTasksOnCurrentThread());
  parent_->DestroyObject(&map_, ipc_object_id);
}

}  // namespace content
