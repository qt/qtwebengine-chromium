// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_directory_commit_contribution.h"

#include "sync/engine/commit_util.h"
#include "sync/engine/get_commit_ids.h"
#include "sync/engine/syncer_util.h"
#include "sync/syncable/model_neutral_mutable_entry.h"
#include "sync/syncable/syncable_model_neutral_write_transaction.h"

namespace syncer {

using syncable::GET_BY_HANDLE;
using syncable::SYNCER;

SyncDirectoryCommitContribution::~SyncDirectoryCommitContribution() {
  DCHECK(!syncing_bits_set_);
}

// static.
SyncDirectoryCommitContribution* SyncDirectoryCommitContribution::Build(
    syncable::Directory* dir,
    ModelType type,
    size_t max_entries) {
  std::vector<int64> metahandles;

  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir);
  GetCommitIdsForType(&trans, type, max_entries, &metahandles);

  if (metahandles.empty())
    return NULL;

  google::protobuf::RepeatedPtrField<sync_pb::SyncEntity> entities;
  for (std::vector<int64>::iterator it = metahandles.begin();
       it != metahandles.end(); ++it) {
    sync_pb::SyncEntity* entity = entities.Add();
    syncable::ModelNeutralMutableEntry entry(&trans, GET_BY_HANDLE, *it);
    commit_util::BuildCommitItem(entry, entity);
    entry.PutSyncing(true);
  }

  return new SyncDirectoryCommitContribution(metahandles, entities, dir);
}

void SyncDirectoryCommitContribution::AddToCommitMessage(
    sync_pb::ClientToServerMessage* msg) {
  DCHECK(syncing_bits_set_);
  sync_pb::CommitMessage* commit_message = msg->mutable_commit();
  entries_start_index_ = commit_message->entries_size();
  std::copy(entities_.begin(),
            entities_.end(),
            RepeatedPtrFieldBackInserter(commit_message->mutable_entries()));
}

SyncerError SyncDirectoryCommitContribution::ProcessCommitResponse(
    const sync_pb::ClientToServerResponse& response,
    sessions::StatusController* status) {
  DCHECK(syncing_bits_set_);
  const sync_pb::CommitResponse& commit_response = response.commit();

  int transient_error_commits = 0;
  int conflicting_commits = 0;
  int error_commits = 0;
  int successes = 0;

  std::set<syncable::Id> deleted_folders;
  {
    syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
    for (size_t i = 0; i < metahandles_.size(); ++i) {
      sync_pb::CommitResponse::ResponseType response_type =
          commit_util::ProcessSingleCommitResponse(
              &trans,
              commit_response.entryresponse(entries_start_index_ + i),
              entities_.Get(i),
              metahandles_[i],
              &deleted_folders);
      switch (response_type) {
        case sync_pb::CommitResponse::INVALID_MESSAGE:
          ++error_commits;
          break;
        case sync_pb::CommitResponse::CONFLICT:
          ++conflicting_commits;
          status->increment_num_server_conflicts();
          break;
        case sync_pb::CommitResponse::SUCCESS:
          ++successes;
          {
            syncable::Entry e(&trans, GET_BY_HANDLE, metahandles_[i]);
            if (e.GetModelType() == BOOKMARKS)
              status->increment_num_successful_bookmark_commits();
          }
          status->increment_num_successful_commits();
          break;
        case sync_pb::CommitResponse::OVER_QUOTA:
          // We handle over quota like a retry, which is same as transient.
        case sync_pb::CommitResponse::RETRY:
        case sync_pb::CommitResponse::TRANSIENT_ERROR:
          ++transient_error_commits;
          break;
        default:
          LOG(FATAL) << "Bad return from ProcessSingleCommitResponse";
      }
    }
    MarkDeletedChildrenSynced(dir_, &trans, &deleted_folders);
  }

  int commit_count = static_cast<int>(metahandles_.size());
  if (commit_count == successes) {
    return SYNCER_OK;
  } else if (error_commits > 0) {
    return SERVER_RETURN_UNKNOWN_ERROR;
  } else if (transient_error_commits > 0) {
    return SERVER_RETURN_TRANSIENT_ERROR;
  } else if (conflicting_commits > 0) {
    // This means that the server already has an item with this version, but
    // we haven't seen that update yet.
    //
    // A well-behaved client should respond to this by proceeding to the
    // download updates phase, fetching the conflicting items, then attempting
    // to resolve the conflict.  That's not what this client does.
    //
    // We don't currently have any code to support that exceptional control
    // flow.  Instead, we abort the current sync cycle and start a new one.  The
    // end result is the same.
    return SERVER_RETURN_CONFLICT;
  } else {
    LOG(FATAL) << "Inconsistent counts when processing commit response";
    return SYNCER_OK;
  }
}

void SyncDirectoryCommitContribution::CleanUp() {
  DCHECK(syncing_bits_set_);
  UnsetSyncingBits();
}

size_t SyncDirectoryCommitContribution::GetNumEntries() const {
  return metahandles_.size();
}

SyncDirectoryCommitContribution::SyncDirectoryCommitContribution(
    const std::vector<int64>& metahandles,
    const google::protobuf::RepeatedPtrField<sync_pb::SyncEntity>& entities,
    syncable::Directory* dir)
  : dir_(dir),
    metahandles_(metahandles),
    entities_(entities),
    entries_start_index_(0xDEADBEEF),
    syncing_bits_set_(true) {
}

void SyncDirectoryCommitContribution::UnsetSyncingBits() {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, SYNCER, dir_);
  for (std::vector<int64>::const_iterator it = metahandles_.begin();
       it != metahandles_.end(); ++it) {
    syncable::ModelNeutralMutableEntry entry(&trans, GET_BY_HANDLE, *it);
    entry.PutSyncing(false);
  }
  syncing_bits_set_ = false;
}

}  // namespace syncer
