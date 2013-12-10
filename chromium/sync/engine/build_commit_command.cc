// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/build_commit_command.h"

#include <limits>
#include <set>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/internal_api/public/base/unique_position.h"
#include "sync/protocol/bookmark_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/ordered_commit_set.h"
#include "sync/sessions/sync_session.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/syncable_base_transaction.h"
#include "sync/syncable/syncable_changes_version.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/util/time.h"

using std::set;
using std::string;
using std::vector;

namespace syncer {

using sessions::SyncSession;
using syncable::Entry;
using syncable::IS_DEL;
using syncable::IS_UNAPPLIED_UPDATE;
using syncable::IS_UNSYNCED;
using syncable::Id;
using syncable::SPECIFICS;
using syncable::UNIQUE_POSITION;

BuildCommitCommand::BuildCommitCommand(
    syncable::BaseTransaction* trans,
    const sessions::OrderedCommitSet& batch_commit_set,
    sync_pb::ClientToServerMessage* commit_message,
    ExtensionsActivity::Records* extensions_activity_buffer)
  : trans_(trans),
    batch_commit_set_(batch_commit_set),
    commit_message_(commit_message),
    extensions_activity_buffer_(extensions_activity_buffer) {
}

BuildCommitCommand::~BuildCommitCommand() {}

void BuildCommitCommand::AddExtensionsActivityToMessage(
    SyncSession* session, sync_pb::CommitMessage* message) {
  // We only send ExtensionsActivity to the server if bookmarks are being
  // committed.
  ExtensionsActivity* activity = session->context()->extensions_activity();
  if (batch_commit_set_.HasBookmarkCommitId()) {
    // This isn't perfect, since the set of extensions activity may not
    // correlate exactly with the items being committed.  That's OK as
    // long as we're looking for a rough estimate of extensions activity,
    // not an precise mapping of which commits were triggered by which
    // extension.
    //
    // We will push this list of extensions activity back into the
    // ExtensionsActivityMonitor if this commit fails.  That's why we must keep
    // a copy of these records in the session.
    activity->GetAndClearRecords(extensions_activity_buffer_);

    const ExtensionsActivity::Records& records =
        *extensions_activity_buffer_;
    for (ExtensionsActivity::Records::const_iterator it =
         records.begin();
         it != records.end(); ++it) {
      sync_pb::ChromiumExtensionsActivity* activity_message =
          message->add_extensions_activity();
      activity_message->set_extension_id(it->second.extension_id);
      activity_message->set_bookmark_writes_since_last_commit(
          it->second.bookmark_write_count);
    }
  }
}

void BuildCommitCommand::AddClientConfigParamsToMessage(
    SyncSession* session, sync_pb::CommitMessage* message) {
  const ModelSafeRoutingInfo& routing_info = session->context()->routing_info();
  sync_pb::ClientConfigParams* config_params = message->mutable_config_params();
  for (std::map<ModelType, ModelSafeGroup>::const_iterator iter =
          routing_info.begin(); iter != routing_info.end(); ++iter) {
    if (ProxyTypes().Has(iter->first))
      continue;
    int field_number = GetSpecificsFieldNumberFromModelType(iter->first);
    config_params->mutable_enabled_type_ids()->Add(field_number);
  }
  config_params->set_tabs_datatype_enabled(
      routing_info.count(syncer::PROXY_TABS) > 0);
}

namespace {
void SetEntrySpecifics(const Entry& meta_entry,
                       sync_pb::SyncEntity* sync_entry) {
  // Add the new style extension and the folder bit.
  sync_entry->mutable_specifics()->CopyFrom(meta_entry.GetSpecifics());
  sync_entry->set_folder(meta_entry.GetIsDir());

  CHECK(!sync_entry->specifics().password().has_client_only_encrypted_data());
  DCHECK_EQ(meta_entry.GetModelType(), GetModelType(*sync_entry));
}
}  // namespace

SyncerError BuildCommitCommand::ExecuteImpl(SyncSession* session) {
  commit_message_->set_share(session->context()->account_name());
  commit_message_->set_message_contents(sync_pb::ClientToServerMessage::COMMIT);

  sync_pb::CommitMessage* commit_message = commit_message_->mutable_commit();
  commit_message->set_cache_guid(trans_->directory()->cache_guid());
  AddExtensionsActivityToMessage(session, commit_message);
  AddClientConfigParamsToMessage(session, commit_message);

  for (size_t i = 0; i < batch_commit_set_.Size(); i++) {
    int64 handle = batch_commit_set_.GetCommitHandleAt(i);
    sync_pb::SyncEntity* sync_entry = commit_message->add_entries();

    Entry meta_entry(trans_, syncable::GET_BY_HANDLE, handle);
    CHECK(meta_entry.good());

    DCHECK_NE(0UL,
              session->context()->routing_info().count(
                  meta_entry.GetModelType()))
        << "Committing change to datatype that's not actively enabled.";

    BuildCommitItem(meta_entry, sync_entry);
  }


  return SYNCER_OK;
}

// static.
void BuildCommitCommand::BuildCommitItem(
    const syncable::Entry& meta_entry,
    sync_pb::SyncEntity* sync_entry) {
  syncable::Id id = meta_entry.GetId();
  sync_entry->set_id_string(SyncableIdToProto(id));

  string name = meta_entry.GetNonUniqueName();
  CHECK(!name.empty());  // Make sure this isn't an update.
  // Note: Truncation is also performed in WriteNode::SetTitle(..). But this
  // call is still necessary to handle any title changes that might originate
  // elsewhere, or already be persisted in the directory.
  TruncateUTF8ToByteSize(name, 255, &name);
  sync_entry->set_name(name);

  // Set the non_unique_name.  If we do, the server ignores
  // the |name| value (using |non_unique_name| instead), and will return
  // in the CommitResponse a unique name if one is generated.
  // We send both because it may aid in logging.
  sync_entry->set_non_unique_name(name);

  if (!meta_entry.GetUniqueClientTag().empty()) {
    sync_entry->set_client_defined_unique_tag(
        meta_entry.GetUniqueClientTag());
  }

  // Deleted items with server-unknown parent ids can be a problem so we set
  // the parent to 0. (TODO(sync): Still true in protocol?).
  Id new_parent_id;
  if (meta_entry.GetIsDel() &&
      !meta_entry.GetParentId().ServerKnows()) {
    new_parent_id = syncable::BaseTransaction::root_id();
  } else {
    new_parent_id = meta_entry.GetParentId();
  }
  sync_entry->set_parent_id_string(SyncableIdToProto(new_parent_id));

  // If our parent has changed, send up the old one so the server
  // can correctly deal with multiple parents.
  // TODO(nick): With the server keeping track of the primary sync parent,
  // it should not be necessary to provide the old_parent_id: the version
  // number should suffice.
  if (new_parent_id != meta_entry.GetServerParentId() &&
      0 != meta_entry.GetBaseVersion() &&
      syncable::CHANGES_VERSION != meta_entry.GetBaseVersion()) {
    sync_entry->set_old_parent_id(
        SyncableIdToProto(meta_entry.GetServerParentId()));
  }

  int64 version = meta_entry.GetBaseVersion();
  if (syncable::CHANGES_VERSION == version || 0 == version) {
    // Undeletions are only supported for items that have a client tag.
    DCHECK(!id.ServerKnows() ||
           !meta_entry.GetUniqueClientTag().empty())
        << meta_entry;

    // Version 0 means to create or undelete an object.
    sync_entry->set_version(0);
  } else {
    DCHECK(id.ServerKnows()) << meta_entry;
    sync_entry->set_version(meta_entry.GetBaseVersion());
  }
  sync_entry->set_ctime(TimeToProtoTime(meta_entry.GetCtime()));
  sync_entry->set_mtime(TimeToProtoTime(meta_entry.GetMtime()));

  // Deletion is final on the server, let's move things and then delete them.
  if (meta_entry.GetIsDel()) {
    sync_entry->set_deleted(true);
  } else {
    if (meta_entry.GetSpecifics().has_bookmark()) {
      // Both insert_after_item_id and position_in_parent fields are set only
      // for legacy reasons.  See comments in sync.proto for more information.
      const Id& prev_id = meta_entry.GetPredecessorId();
      string prev_id_string =
          prev_id.IsRoot() ? string() : prev_id.GetServerId();
      sync_entry->set_insert_after_item_id(prev_id_string);
      sync_entry->set_position_in_parent(
          meta_entry.GetUniquePosition().ToInt64());
      meta_entry.GetUniquePosition().ToProto(
          sync_entry->mutable_unique_position());
    }
    SetEntrySpecifics(meta_entry, sync_entry);
  }
}

}  // namespace syncer
