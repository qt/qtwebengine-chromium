// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/commit.h"

#include "base/debug/trace_event.h"
#include "sync/engine/build_commit_command.h"
#include "sync/engine/get_commit_ids.h"
#include "sync/engine/process_commit_response_command.h"
#include "sync/engine/syncer.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_write_transaction.h"

namespace syncer {

using sessions::SyncSession;
using sessions::StatusController;
using syncable::SYNCER;
using syncable::WriteTransaction;

namespace {

// Sets the SYNCING bits of all items in the commit set to value_to_set.
void SetAllSyncingBitsToValue(WriteTransaction* trans,
                              const sessions::OrderedCommitSet& commit_set,
                              bool value_to_set) {
  const std::vector<int64>& commit_handles = commit_set.GetAllCommitHandles();
  for (std::vector<int64>::const_iterator it = commit_handles.begin();
       it != commit_handles.end(); ++it) {
    syncable::MutableEntry entry(trans, syncable::GET_BY_HANDLE, *it);
    if (entry.good()) {
      entry.PutSyncing(value_to_set);
    }
  }
}

// Sets the SYNCING bits for all items in the OrderedCommitSet.
void SetSyncingBits(WriteTransaction* trans,
                    const sessions::OrderedCommitSet& commit_set) {
  SetAllSyncingBitsToValue(trans, commit_set, true);
}

// Clears the SYNCING bits for all items in the OrderedCommitSet.
void ClearSyncingBits(syncable::Directory* dir,
                      const sessions::OrderedCommitSet& commit_set) {
  WriteTransaction trans(FROM_HERE, SYNCER, dir);
  SetAllSyncingBitsToValue(&trans, commit_set, false);
}

// Helper function that finds sync items that are ready to be committed to the
// server and serializes them into a commit message protobuf.  It will return
// false iff there are no entries ready to be committed at this time.
//
// The OrderedCommitSet parameter is an output parameter which will contain
// the set of all items which are to be committed.  The number of items in
// the set shall not exceed the maximum batch size.  (The default batch size
// is currently 25, though it can be overwritten by the server.)
//
// The ClientToServerMessage parameter is an output parameter which will contain
// the commit message which should be sent to the server.  It is valid iff the
// return value of this function is true.
bool PrepareCommitMessage(
    sessions::SyncSession* session,
    ModelTypeSet requested_types,
    sessions::OrderedCommitSet* commit_set,
    sync_pb::ClientToServerMessage* commit_message,
    ExtensionsActivity::Records* extensions_activity_buffer) {
  TRACE_EVENT0("sync", "PrepareCommitMessage");

  commit_set->Clear();
  commit_message->Clear();

  WriteTransaction trans(FROM_HERE, SYNCER, session->context()->directory());

  // Fetch the items to commit.
  const size_t batch_size = session->context()->max_commit_batch_size();
  GetCommitIds(&trans, requested_types, batch_size, commit_set);

  DVLOG(1) << "Commit message will contain " << commit_set->Size() << " items.";
  if (commit_set->Empty()) {
    return false;
  }

  // Serialize the message.
  BuildCommitCommand build_commit_command(&trans,
                                          *commit_set,
                                          commit_message,
                                          extensions_activity_buffer);
  build_commit_command.Execute(session);

  SetSyncingBits(&trans, *commit_set);
  return true;
}

SyncerError BuildAndPostCommitsImpl(ModelTypeSet requested_types,
                                    Syncer* syncer,
                                    sessions::SyncSession* session,
                                    sessions::OrderedCommitSet* commit_set) {
  ModelTypeSet commit_request_types;
  while (!syncer->ExitRequested()) {
    sync_pb::ClientToServerMessage commit_message;
    ExtensionsActivity::Records extensions_activity_buffer;

    if (!PrepareCommitMessage(session,
                              requested_types,
                              commit_set,
                              &commit_message,
                              &extensions_activity_buffer)) {
      break;
    }

    commit_request_types.PutAll(commit_set->Types());
    session->mutable_status_controller()->set_commit_request_types(
        commit_request_types);

    sync_pb::ClientToServerResponse commit_response;

    DVLOG(1) << "Sending commit message.";
    TRACE_EVENT_BEGIN0("sync", "PostCommit");
    const SyncerError post_result = SyncerProtoUtil::PostClientToServerMessage(
        &commit_message, &commit_response, session);
    TRACE_EVENT_END0("sync", "PostCommit");

    // TODO(rlarocque): Put all the post-commit logic in one place.
    // See crbug.com/196338.

    if (post_result != SYNCER_OK) {
      LOG(WARNING) << "Post commit failed";
      return post_result;
    }

    if (!commit_response.has_commit()) {
      LOG(WARNING) << "Commit response has no commit body!";
      return SERVER_RESPONSE_VALIDATION_FAILED;
    }

    const size_t num_responses = commit_response.commit().entryresponse_size();
    if (num_responses != commit_set->Size()) {
      LOG(ERROR)
         << "Commit response has wrong number of entries! "
         << "Expected: " << commit_set->Size() << ", "
         << "Got: " << num_responses;
      return SERVER_RESPONSE_VALIDATION_FAILED;
    }

    TRACE_EVENT_BEGIN0("sync", "ProcessCommitResponse");
    ProcessCommitResponseCommand process_response_command(
        *commit_set, commit_message, commit_response);
    const SyncerError processing_result =
        process_response_command.Execute(session);
    TRACE_EVENT_END0("sync", "ProcessCommitResponse");

    // If the commit failed, return the data to the ExtensionsActivityMonitor.
    if (session->status_controller().
        model_neutral_state().num_successful_bookmark_commits == 0) {
      ExtensionsActivity* extensions_activity =
          session->context()->extensions_activity();
      extensions_activity->PutRecords(extensions_activity_buffer);
    }

    if (processing_result != SYNCER_OK) {
      return processing_result;
    }
    session->SendEventNotification(SyncEngineEvent::STATUS_CHANGED);
  }

  return SYNCER_OK;
}

}  // namespace


SyncerError BuildAndPostCommits(ModelTypeSet requested_types,
                                Syncer* syncer,
                                sessions::SyncSession* session) {
  sessions::OrderedCommitSet commit_set(session->context()->routing_info());
  SyncerError result =
      BuildAndPostCommitsImpl(requested_types, syncer, session, &commit_set);
  if (result != SYNCER_OK) {
    ClearSyncingBits(session->context()->directory(), commit_set);
  }
  return result;
}

}  // namespace syncer
