// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/engine/sync_directory_update_handler.h"

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "sync/engine/syncer_proto_util.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/test/test_entry_factory.h"
#include "sync/protocol/sync.pb.h"
#include "sync/sessions/status_controller.h"
#include "sync/syncable/directory.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_model_neutral_write_transaction.h"
#include "sync/syncable/syncable_proto_util.h"
#include "sync/syncable/syncable_read_transaction.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/fake_model_worker.h"
#include "sync/test/engine/test_directory_setter_upper.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/engine/test_syncable_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

using syncable::UNITTEST;

// A test harness for tests that focus on processing updates.
//
// Update processing is what occurs when we first download updates.  It converts
// the received protobuf message into information in the syncable::Directory.
// Any invalid or redundant updates will be dropped at this point.
class SyncDirectoryUpdateHandlerProcessUpdateTest : public ::testing::Test {
 public:
  SyncDirectoryUpdateHandlerProcessUpdateTest()
      : ui_worker_(new FakeModelWorker(GROUP_UI)) {
  }

  virtual ~SyncDirectoryUpdateHandlerProcessUpdateTest() {}

  virtual void SetUp() OVERRIDE {
    dir_maker_.SetUp();
  }

  virtual void TearDown() OVERRIDE {
    dir_maker_.TearDown();
  }

  syncable::Directory* dir() {
    return dir_maker_.directory();
  }
 protected:
  scoped_ptr<sync_pb::SyncEntity> CreateUpdate(
      const std::string& id,
      const std::string& parent,
      const ModelType& type);

  // This exists mostly to give tests access to the protected member function.
  // Warning: This takes the syncable directory lock.
  void UpdateSyncEntities(
      SyncDirectoryUpdateHandler* handler,
      const SyncEntityList& applicable_updates,
      sessions::StatusController* status);

  // Another function to access private member functions.
  void UpdateProgressMarkers(
      SyncDirectoryUpdateHandler* handler,
      const sync_pb::DataTypeProgressMarker& progress);

  scoped_refptr<FakeModelWorker> ui_worker() {
    return ui_worker_;
  }

 private:
  base::MessageLoop loop_;  // Needed to initialize the directory.
  TestDirectorySetterUpper dir_maker_;
  scoped_refptr<FakeModelWorker> ui_worker_;
};

scoped_ptr<sync_pb::SyncEntity>
SyncDirectoryUpdateHandlerProcessUpdateTest::CreateUpdate(
    const std::string& id,
    const std::string& parent,
    const ModelType& type) {
  scoped_ptr<sync_pb::SyncEntity> e(new sync_pb::SyncEntity());
  e->set_id_string(id);
  e->set_parent_id_string(parent);
  e->set_non_unique_name(id);
  e->set_name(id);
  e->set_version(1000);
  AddDefaultFieldValue(type, e->mutable_specifics());
  return e.Pass();
}

void SyncDirectoryUpdateHandlerProcessUpdateTest::UpdateSyncEntities(
    SyncDirectoryUpdateHandler* handler,
    const SyncEntityList& applicable_updates,
    sessions::StatusController* status) {
  syncable::ModelNeutralWriteTransaction trans(FROM_HERE, UNITTEST, dir());
  handler->UpdateSyncEntities(&trans, applicable_updates, status);
}

void SyncDirectoryUpdateHandlerProcessUpdateTest::UpdateProgressMarkers(
    SyncDirectoryUpdateHandler* handler,
    const sync_pb::DataTypeProgressMarker& progress) {
  handler->UpdateProgressMarker(progress);
}

static const char kCacheGuid[] = "IrcjZ2jyzHDV9Io4+zKcXQ==";

// Test that the bookmark tag is set on newly downloaded items.
TEST_F(SyncDirectoryUpdateHandlerProcessUpdateTest, NewBookmarkTag) {
  SyncDirectoryUpdateHandler handler(dir(), BOOKMARKS, ui_worker());
  sync_pb::GetUpdatesResponse gu_response;
  sessions::StatusController status;

  // Add a bookmark item to the update message.
  std::string root = syncable::GetNullId().GetServerId();
  syncable::Id server_id = syncable::Id::CreateFromServerId("b1");
  scoped_ptr<sync_pb::SyncEntity> e =
      CreateUpdate(SyncableIdToProto(server_id), root, BOOKMARKS);
  e->set_originator_cache_guid(
      std::string(kCacheGuid, arraysize(kCacheGuid)-1));
  syncable::Id client_id = syncable::Id::CreateFromClientString("-2");
  e->set_originator_client_item_id(client_id.GetServerId());
  e->set_position_in_parent(0);

  // Add it to the applicable updates list.
  SyncEntityList bookmark_updates;
  bookmark_updates.push_back(e.get());

  // Process the update.
  UpdateSyncEntities(&handler, bookmark_updates, &status);

  syncable::ReadTransaction trans(FROM_HERE, dir());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());
  EXPECT_TRUE(UniquePosition::IsValidSuffix(entry.GetUniqueBookmarkTag()));
  EXPECT_TRUE(entry.GetServerUniquePosition().IsValid());

  // If this assertion fails, that might indicate that the algorithm used to
  // generate bookmark tags has been modified.  This could have implications for
  // bookmark ordering.  Please make sure you know what you're doing if you
  // intend to make such a change.
  EXPECT_EQ("6wHRAb3kbnXV5GHrejp4/c1y5tw=", entry.GetUniqueBookmarkTag());
}

// Test the receipt of a type root node.
TEST_F(SyncDirectoryUpdateHandlerProcessUpdateTest,
       ReceiveServerCreatedBookmarkFolders) {
  SyncDirectoryUpdateHandler handler(dir(), BOOKMARKS, ui_worker());
  sync_pb::GetUpdatesResponse gu_response;
  sessions::StatusController status;

  // Create an update that mimics the bookmark root.
  syncable::Id server_id = syncable::Id::CreateFromServerId("xyz");
  std::string root = syncable::GetNullId().GetServerId();
  scoped_ptr<sync_pb::SyncEntity> e =
      CreateUpdate(SyncableIdToProto(server_id), root, BOOKMARKS);
  e->set_server_defined_unique_tag("google_chrome_bookmarks");
  e->set_folder(true);

  // Add it to the applicable updates list.
  SyncEntityList bookmark_updates;
  bookmark_updates.push_back(e.get());

  EXPECT_FALSE(SyncerProtoUtil::ShouldMaintainPosition(*e));

  // Process it.
  UpdateSyncEntities(&handler, bookmark_updates, &status);

  // Verify the results.
  syncable::ReadTransaction trans(FROM_HERE, dir());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());

  EXPECT_FALSE(entry.ShouldMaintainPosition());
  EXPECT_FALSE(entry.GetUniquePosition().IsValid());
  EXPECT_FALSE(entry.GetServerUniquePosition().IsValid());
  EXPECT_TRUE(entry.GetUniqueBookmarkTag().empty());
}

// Test the receipt of a non-bookmark item.
TEST_F(SyncDirectoryUpdateHandlerProcessUpdateTest, ReceiveNonBookmarkItem) {
  SyncDirectoryUpdateHandler handler(dir(), PREFERENCES, ui_worker());
  sync_pb::GetUpdatesResponse gu_response;
  sessions::StatusController status;

  std::string root = syncable::GetNullId().GetServerId();
  syncable::Id server_id = syncable::Id::CreateFromServerId("xyz");
  scoped_ptr<sync_pb::SyncEntity> e =
      CreateUpdate(SyncableIdToProto(server_id), root, PREFERENCES);
  e->set_server_defined_unique_tag("9PGRuKdX5sHyGMB17CvYTXuC43I=");

  // Add it to the applicable updates list.
  SyncEntityList autofill_updates;
  autofill_updates.push_back(e.get());

  EXPECT_FALSE(SyncerProtoUtil::ShouldMaintainPosition(*e));

  // Process it.
  UpdateSyncEntities(&handler, autofill_updates, &status);

  syncable::ReadTransaction trans(FROM_HERE, dir());
  syncable::Entry entry(&trans, syncable::GET_BY_ID, server_id);
  ASSERT_TRUE(entry.good());

  EXPECT_FALSE(entry.ShouldMaintainPosition());
  EXPECT_FALSE(entry.GetUniquePosition().IsValid());
  EXPECT_FALSE(entry.GetServerUniquePosition().IsValid());
  EXPECT_TRUE(entry.GetUniqueBookmarkTag().empty());
}

// Tests the setting of progress markers.
TEST_F(SyncDirectoryUpdateHandlerProcessUpdateTest, ProcessNewProgressMarkers) {
  SyncDirectoryUpdateHandler handler(dir(), BOOKMARKS, ui_worker());

  sync_pb::DataTypeProgressMarker progress;
  progress.set_data_type_id(GetSpecificsFieldNumberFromModelType(BOOKMARKS));
  progress.set_token("token");

  UpdateProgressMarkers(&handler, progress);

  sync_pb::DataTypeProgressMarker saved;
  dir()->GetDownloadProgress(BOOKMARKS, &saved);

  EXPECT_EQ(progress.token(), saved.token());
  EXPECT_EQ(progress.data_type_id(), saved.data_type_id());
}

// A test harness for tests that focus on applying updates.
//
// Update application is performed when we want to take updates that were
// previously downloaded, processed, and stored in our syncable::Directory
// and use them to update our local state (both the Directory's local state
// and the model's local state, though these tests focus only on the Directory's
// local state).
//
// This is kept separate from the update processing test in part for historical
// reasons, and in part because these tests may require a bit more infrastrcture
// in the future.  Update application should happen on a different thread a lot
// of the time so these tests may end up requiring more infrastructure than the
// update processing tests.  Currently, we're bypassing most of those issues by
// using FakeModelWorkers, so there's not much difference between the two test
// harnesses.
class SyncDirectoryUpdateHandlerApplyUpdateTest : public ::testing::Test {
 public:
  SyncDirectoryUpdateHandlerApplyUpdateTest()
      : ui_worker_(new FakeModelWorker(GROUP_UI)),
        password_worker_(new FakeModelWorker(GROUP_PASSWORD)),
        passive_worker_(new FakeModelWorker(GROUP_PASSIVE)),
        update_handler_map_deleter_(&update_handler_map_) {}

  virtual void SetUp() OVERRIDE {
    dir_maker_.SetUp();
    entry_factory_.reset(new TestEntryFactory(directory()));

    update_handler_map_.insert(std::make_pair(
        BOOKMARKS,
        new SyncDirectoryUpdateHandler(directory(), BOOKMARKS, ui_worker_)));
    update_handler_map_.insert(std::make_pair(
        PASSWORDS,
        new SyncDirectoryUpdateHandler(directory(),
                                       PASSWORDS,
                                       password_worker_)));
  }

  virtual void TearDown() OVERRIDE {
    dir_maker_.TearDown();
  }

 protected:
  void ApplyBookmarkUpdates(sessions::StatusController* status) {
    update_handler_map_[BOOKMARKS]->ApplyUpdates(status);
  }

  void ApplyPasswordUpdates(sessions::StatusController* status) {
    update_handler_map_[PASSWORDS]->ApplyUpdates(status);
  }

  TestEntryFactory* entry_factory() {
    return entry_factory_.get();
  }

  syncable::Directory* directory() {
    return dir_maker_.directory();
  }

 private:
  base::MessageLoop loop_;  // Needed to initialize the directory.
  TestDirectorySetterUpper dir_maker_;
  scoped_ptr<TestEntryFactory> entry_factory_;

  scoped_refptr<FakeModelWorker> ui_worker_;
  scoped_refptr<FakeModelWorker> password_worker_;
  scoped_refptr<FakeModelWorker> passive_worker_;

  UpdateHandlerMap update_handler_map_;
  STLValueDeleter<UpdateHandlerMap> update_handler_map_deleter_;
};

namespace {
sync_pb::EntitySpecifics DefaultBookmarkSpecifics() {
  sync_pb::EntitySpecifics result;
  AddDefaultFieldValue(BOOKMARKS, &result);
  return result;
}
} // namespace

// Test update application for a few bookmark items.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest, SimpleBookmark) {
  sessions::StatusController status;

  std::string root_server_id = syncable::GetNullId().GetServerId();
  int64 parent_handle =
      entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
          "parent", DefaultBookmarkSpecifics(), root_server_id);
  int64 child_handle =
      entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
          "child", DefaultBookmarkSpecifics(), "parent");

  ApplyBookmarkUpdates(&status);

  EXPECT_EQ(0, status.num_encryption_conflicts())
      << "Simple update shouldn't result in conflicts";
  EXPECT_EQ(0, status.num_hierarchy_conflicts())
      << "Simple update shouldn't result in conflicts";
  EXPECT_EQ(2, status.num_updates_applied())
      << "All items should have been successfully applied";

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    syncable::Entry parent(&trans, syncable::GET_BY_HANDLE, parent_handle);
    syncable::Entry child(&trans, syncable::GET_BY_HANDLE, child_handle);

    ASSERT_TRUE(parent.good());
    ASSERT_TRUE(child.good());

    EXPECT_FALSE(parent.GetIsUnsynced());
    EXPECT_FALSE(parent.GetIsUnappliedUpdate());
    EXPECT_FALSE(child.GetIsUnsynced());
    EXPECT_FALSE(child.GetIsUnappliedUpdate());
  }
}

// Test that the applicator can handle updates delivered out of order.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest,
       BookmarkChildrenBeforeParent) {
  // Start with some bookmarks whose parents are unknown.
  std::string root_server_id = syncable::GetNullId().GetServerId();
  int64 a_handle = entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
      "a_child_created_first", DefaultBookmarkSpecifics(), "parent");
  int64 x_handle = entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
      "x_child_created_first", DefaultBookmarkSpecifics(), "parent");

  // Update application will fail.
  sessions::StatusController status1;
  ApplyBookmarkUpdates(&status1);
  EXPECT_EQ(0, status1.num_updates_applied());
  EXPECT_EQ(2, status1.num_hierarchy_conflicts());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    syncable::Entry a(&trans, syncable::GET_BY_HANDLE, a_handle);
    syncable::Entry x(&trans, syncable::GET_BY_HANDLE, x_handle);

    ASSERT_TRUE(a.good());
    ASSERT_TRUE(x.good());

    EXPECT_TRUE(a.GetIsUnappliedUpdate());
    EXPECT_TRUE(x.GetIsUnappliedUpdate());
  }

  // Now add their parent and a few siblings.
  entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
      "parent", DefaultBookmarkSpecifics(), root_server_id);
  entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
      "a_child_created_second", DefaultBookmarkSpecifics(), "parent");
  entry_factory()->CreateUnappliedNewBookmarkItemWithParent(
      "x_child_created_second", DefaultBookmarkSpecifics(), "parent");

  // Update application will succeed.
  sessions::StatusController status2;
  ApplyBookmarkUpdates(&status2);
  EXPECT_EQ(5, status2.num_updates_applied())
      << "All updates should have been successfully applied";

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());

    syncable::Entry a(&trans, syncable::GET_BY_HANDLE, a_handle);
    syncable::Entry x(&trans, syncable::GET_BY_HANDLE, x_handle);

    ASSERT_TRUE(a.good());
    ASSERT_TRUE(x.good());

    EXPECT_FALSE(a.GetIsUnappliedUpdate());
    EXPECT_FALSE(x.GetIsUnappliedUpdate());
  }
}

// Try to apply changes on an item that is both IS_UNSYNCED and
// IS_UNAPPLIED_UPDATE.  Conflict resolution should be performed.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest, SimpleBookmarkConflict) {
  int64 handle = entry_factory()->CreateUnappliedAndUnsyncedBookmarkItem("x");

  int original_server_version = -10;
  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry e(&trans, syncable::GET_BY_HANDLE, handle);
    original_server_version = e.GetServerVersion();
    ASSERT_NE(original_server_version, e.GetBaseVersion());
    EXPECT_TRUE(e.GetIsUnsynced());
  }

  sessions::StatusController status;
  ApplyBookmarkUpdates(&status);
  EXPECT_EQ(1, status.num_server_overwrites())
      << "Unsynced and unapplied item conflict should be resolved";
  EXPECT_EQ(0, status.num_updates_applied())
      << "Update should not be applied; we should override the server.";

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry e(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(e.good());
    EXPECT_EQ(original_server_version, e.GetServerVersion());
    EXPECT_EQ(original_server_version, e.GetBaseVersion());
    EXPECT_FALSE(e.GetIsUnappliedUpdate());

    // The unsynced flag will remain set until we successfully commit the item.
    EXPECT_TRUE(e.GetIsUnsynced());
  }
}

// Create a simple conflict that is also a hierarchy conflict.  If we were to
// follow the normal "server wins" logic, we'd end up violating hierarchy
// constraints.  The hierarchy conflict must take precedence.  We can not allow
// the update to be applied.  The item must remain in the conflict state.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest, HierarchyAndSimpleConflict) {
  // Create a simply-conflicting item.  It will start with valid parent ids.
  int64 handle = entry_factory()->CreateUnappliedAndUnsyncedBookmarkItem(
      "orphaned_by_server");
  {
    // Manually set the SERVER_PARENT_ID to bad value.
    // A bad parent indicates a hierarchy conflict.
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    syncable::MutableEntry entry(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(entry.good());

    entry.PutServerParentId(TestIdFactory::MakeServer("bogus_parent"));
  }

  sessions::StatusController status;
  ApplyBookmarkUpdates(&status);
  EXPECT_EQ(0, status.num_updates_applied());
  EXPECT_EQ(0, status.num_server_overwrites());
  EXPECT_EQ(1, status.num_hierarchy_conflicts());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry e(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(e.good());
    EXPECT_TRUE(e.GetIsUnappliedUpdate());
    EXPECT_TRUE(e.GetIsUnsynced());
  }
}

// Attempt to apply an udpate that would create a bookmark folder loop.  This
// application should fail.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest, BookmarkFolderLoop) {
  // Item 'X' locally has parent of 'root'.  Server is updating it to have
  // parent of 'Y'.

  // Create it as a child of root node.
  int64 handle = entry_factory()->CreateSyncedItem("X", BOOKMARKS, true);

  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    syncable::MutableEntry entry(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(entry.good());

    // Re-parent from root to "Y"
    entry.PutServerVersion(entry_factory()->GetNextRevision());
    entry.PutIsUnappliedUpdate(true);
    entry.PutServerParentId(TestIdFactory::MakeServer("Y"));
  }

  // Item 'Y' is child of 'X'.
  entry_factory()->CreateUnsyncedItem(
      TestIdFactory::MakeServer("Y"), TestIdFactory::MakeServer("X"), "Y", true,
      BOOKMARKS, NULL);

  // If the server's update were applied, we would have X be a child of Y, and Y
  // as a child of X.  That's a directory loop.  The UpdateApplicator should
  // prevent the update from being applied and note that this is a hierarchy
  // conflict.

  sessions::StatusController status;
  ApplyBookmarkUpdates(&status);

  // This should count as a hierarchy conflict.
  EXPECT_EQ(1, status.num_hierarchy_conflicts());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry e(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(e.good());
    EXPECT_TRUE(e.GetIsUnappliedUpdate());
    EXPECT_FALSE(e.GetIsUnsynced());
  }
}

// Test update application where the update has been orphaned by a local folder
// deletion.  The update application attempt should fail.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest,
       HierarchyConflictDeletedParent) {
  // Create a locally deleted parent item.
  int64 parent_handle;
  entry_factory()->CreateUnsyncedItem(
      syncable::Id::CreateFromServerId("parent"), TestIdFactory::root(),
      "parent", true, BOOKMARKS, &parent_handle);
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    syncable::MutableEntry entry(&trans,
                                 syncable::GET_BY_HANDLE,
                                 parent_handle);
    entry.PutIsDel(true);
  }

  // Create an incoming child from the server.
  int64 child_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "child", DefaultBookmarkSpecifics(), "parent");

  // The server's update may seem valid to some other client, but on this client
  // that new item's parent no longer exists.  The update should not be applied
  // and the update applicator should indicate this is a hierarchy conflict.

  sessions::StatusController status;
  ApplyBookmarkUpdates(&status);
  EXPECT_EQ(1, status.num_hierarchy_conflicts());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry child(&trans, syncable::GET_BY_HANDLE, child_handle);
    ASSERT_TRUE(child.good());
    EXPECT_TRUE(child.GetIsUnappliedUpdate());
    EXPECT_FALSE(child.GetIsUnsynced());
  }
}

// Attempt to apply an update that deletes a folder where the folder has
// locally-created children.  The update application should fail.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest,
       HierarchyConflictDeleteNonEmptyDirectory) {
  // Create a server-deleted folder as a child of root node.
  int64 parent_handle =
      entry_factory()->CreateSyncedItem("parent", BOOKMARKS, true);
  {
    syncable::WriteTransaction trans(FROM_HERE, UNITTEST, directory());
    syncable::MutableEntry entry(&trans,
                                 syncable::GET_BY_HANDLE,
                                 parent_handle);
    ASSERT_TRUE(entry.good());

    // Delete it on the server.
    entry.PutServerVersion(entry_factory()->GetNextRevision());
    entry.PutIsUnappliedUpdate(true);
    entry.PutServerParentId(TestIdFactory::root());
    entry.PutServerIsDel(true);
  }

  // Create a local child of the server-deleted directory.
  entry_factory()->CreateUnsyncedItem(
      TestIdFactory::MakeServer("child"), TestIdFactory::MakeServer("parent"),
      "child", false, BOOKMARKS, NULL);

  // The server's request to delete the directory must be ignored, otherwise our
  // unsynced new child would be orphaned.  This is a hierarchy conflict.

  sessions::StatusController status;
  ApplyBookmarkUpdates(&status);

  // This should count as a hierarchy conflict.
  EXPECT_EQ(1, status.num_hierarchy_conflicts());

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry parent(&trans, syncable::GET_BY_HANDLE, parent_handle);
    ASSERT_TRUE(parent.good());
    EXPECT_TRUE(parent.GetIsUnappliedUpdate());
    EXPECT_FALSE(parent.GetIsUnsynced());
  }
}

// Attempt to apply updates where the updated item's parent is not known to this
// client.  The update application attempt should fail.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest,
       HierarchyConflictUnknownParent) {
  // We shouldn't be able to do anything with either of these items.
  int64 x_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "some_item", DefaultBookmarkSpecifics(), "unknown_parent");
  int64 y_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "some_other_item", DefaultBookmarkSpecifics(), "some_item");

  sessions::StatusController status;
  ApplyBookmarkUpdates(&status);

  EXPECT_EQ(2, status.num_hierarchy_conflicts())
      << "All updates with an unknown ancestors should be in conflict";
  EXPECT_EQ(0, status.num_updates_applied())
      << "No item with an unknown ancestor should be applied";

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry x(&trans, syncable::GET_BY_HANDLE, x_handle);
    syncable::Entry y(&trans, syncable::GET_BY_HANDLE, y_handle);
    ASSERT_TRUE(x.good());
    ASSERT_TRUE(y.good());
    EXPECT_TRUE(x.GetIsUnappliedUpdate());
    EXPECT_TRUE(y.GetIsUnappliedUpdate());
    EXPECT_FALSE(x.GetIsUnsynced());
    EXPECT_FALSE(y.GetIsUnsynced());
  }
}

// Attempt application of a mix of items.  Some update application attempts will
// fail due to hierarchy conflicts.  Others should succeed.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest, ItemsBothKnownAndUnknown) {
  // See what happens when there's a mixture of good and bad updates.
  std::string root_server_id = syncable::GetNullId().GetServerId();
  int64 u1_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "first_unknown_item", DefaultBookmarkSpecifics(), "unknown_parent");
  int64 k1_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "first_known_item", DefaultBookmarkSpecifics(), root_server_id);
  int64 u2_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "second_unknown_item", DefaultBookmarkSpecifics(), "unknown_parent");
  int64 k2_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "second_known_item", DefaultBookmarkSpecifics(), "first_known_item");
  int64 k3_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "third_known_item", DefaultBookmarkSpecifics(), "fourth_known_item");
  int64 k4_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "fourth_known_item", DefaultBookmarkSpecifics(), root_server_id);

  sessions::StatusController status;
  ApplyBookmarkUpdates(&status);

  EXPECT_EQ(2, status.num_hierarchy_conflicts())
      << "The updates with unknown ancestors should be in conflict";
  EXPECT_EQ(4, status.num_updates_applied())
      << "The updates with known ancestors should be successfully applied";

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry u1(&trans, syncable::GET_BY_HANDLE, u1_handle);
    syncable::Entry u2(&trans, syncable::GET_BY_HANDLE, u2_handle);
    syncable::Entry k1(&trans, syncable::GET_BY_HANDLE, k1_handle);
    syncable::Entry k2(&trans, syncable::GET_BY_HANDLE, k2_handle);
    syncable::Entry k3(&trans, syncable::GET_BY_HANDLE, k3_handle);
    syncable::Entry k4(&trans, syncable::GET_BY_HANDLE, k4_handle);
    ASSERT_TRUE(u1.good());
    ASSERT_TRUE(u2.good());
    ASSERT_TRUE(k1.good());
    ASSERT_TRUE(k2.good());
    ASSERT_TRUE(k3.good());
    ASSERT_TRUE(k4.good());
    EXPECT_TRUE(u1.GetIsUnappliedUpdate());
    EXPECT_TRUE(u2.GetIsUnappliedUpdate());
    EXPECT_FALSE(k1.GetIsUnappliedUpdate());
    EXPECT_FALSE(k2.GetIsUnappliedUpdate());
    EXPECT_FALSE(k3.GetIsUnappliedUpdate());
    EXPECT_FALSE(k4.GetIsUnappliedUpdate());
  }
}

// Attempt application of password upates where the passphrase is known.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest, DecryptablePassword) {
  // Decryptable password updates should be applied.
  Cryptographer* cryptographer;
  {
    // Storing the cryptographer separately is bad, but for this test we
    // know it's safe.
    syncable::ReadTransaction trans(FROM_HERE, directory());
    cryptographer = directory()->GetCryptographer(&trans);
  }

  KeyParams params = {"localhost", "dummy", "foobar"};
  cryptographer->AddKey(params);

  sync_pb::EntitySpecifics specifics;
  sync_pb::PasswordSpecificsData data;
  data.set_origin("http://example.com");

  cryptographer->Encrypt(data,
                         specifics.mutable_password()->mutable_encrypted());
  int64 handle =
      entry_factory()->CreateUnappliedNewItem("item", specifics, false);

  sessions::StatusController status;
  ApplyPasswordUpdates(&status);

  EXPECT_EQ(1, status.num_updates_applied())
      << "The updates that can be decrypted should be applied";

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry e(&trans, syncable::GET_BY_HANDLE, handle);
    ASSERT_TRUE(e.good());
    EXPECT_FALSE(e.GetIsUnappliedUpdate());
    EXPECT_FALSE(e.GetIsUnsynced());
  }
}

// Attempt application of encrypted items when the passphrase is not known.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest, UndecryptableData) {
  // Undecryptable updates should not be applied.
  sync_pb::EntitySpecifics encrypted_bookmark;
  encrypted_bookmark.mutable_encrypted();
  AddDefaultFieldValue(BOOKMARKS, &encrypted_bookmark);
  std::string root_server_id = syncable::GetNullId().GetServerId();
  int64 folder_handle = entry_factory()->CreateUnappliedNewItemWithParent(
      "folder",
      encrypted_bookmark,
      root_server_id);
  int64 bookmark_handle = entry_factory()->CreateUnappliedNewItem(
      "item2",
      encrypted_bookmark,
      false);
  sync_pb::EntitySpecifics encrypted_password;
  encrypted_password.mutable_password();
  int64 password_handle = entry_factory()->CreateUnappliedNewItem(
      "item3",
      encrypted_password,
      false);

  sessions::StatusController status;
  ApplyBookmarkUpdates(&status);
  ApplyPasswordUpdates(&status);

  EXPECT_EQ(3, status.num_encryption_conflicts())
      << "Updates that can't be decrypted should be in encryption conflict";
  EXPECT_EQ(0, status.num_updates_applied())
      << "No update that can't be decrypted should be applied";

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry folder(&trans, syncable::GET_BY_HANDLE, folder_handle);
    syncable::Entry bm(&trans, syncable::GET_BY_HANDLE, bookmark_handle);
    syncable::Entry pw(&trans, syncable::GET_BY_HANDLE, password_handle);
    ASSERT_TRUE(folder.good());
    ASSERT_TRUE(bm.good());
    ASSERT_TRUE(pw.good());
    EXPECT_TRUE(folder.GetIsUnappliedUpdate());
    EXPECT_TRUE(bm.GetIsUnappliedUpdate());
    EXPECT_TRUE(pw.GetIsUnappliedUpdate());
  }
}

// Test a mix of decryptable and undecryptable updates.
TEST_F(SyncDirectoryUpdateHandlerApplyUpdateTest, SomeUndecryptablePassword) {
  Cryptographer* cryptographer;

  int64 decryptable_handle = -1;
  int64 undecryptable_handle = -1;

  // Only decryptable password updates should be applied.
  {
    sync_pb::EntitySpecifics specifics;
    sync_pb::PasswordSpecificsData data;
    data.set_origin("http://example.com/1");
    {
      syncable::ReadTransaction trans(FROM_HERE, directory());
      cryptographer = directory()->GetCryptographer(&trans);

      KeyParams params = {"localhost", "dummy", "foobar"};
      cryptographer->AddKey(params);

      cryptographer->Encrypt(data,
          specifics.mutable_password()->mutable_encrypted());
    }
    decryptable_handle =
        entry_factory()->CreateUnappliedNewItem("item1", specifics, false);
  }
  {
    // Create a new cryptographer, independent of the one in the session.
    Cryptographer other_cryptographer(cryptographer->encryptor());
    KeyParams params = {"localhost", "dummy", "bazqux"};
    other_cryptographer.AddKey(params);

    sync_pb::EntitySpecifics specifics;
    sync_pb::PasswordSpecificsData data;
    data.set_origin("http://example.com/2");

    other_cryptographer.Encrypt(data,
        specifics.mutable_password()->mutable_encrypted());
    undecryptable_handle =
        entry_factory()->CreateUnappliedNewItem("item2", specifics, false);
  }

  sessions::StatusController status;
  ApplyPasswordUpdates(&status);

  EXPECT_EQ(1, status.num_encryption_conflicts())
      << "The updates that can't be decrypted should be in encryption "
      << "conflict";
  EXPECT_EQ(1, status.num_updates_applied())
      << "The undecryptable password update shouldn't be applied";

  {
    syncable::ReadTransaction trans(FROM_HERE, directory());
    syncable::Entry e1(&trans, syncable::GET_BY_HANDLE, decryptable_handle);
    syncable::Entry e2(&trans, syncable::GET_BY_HANDLE, undecryptable_handle);
    ASSERT_TRUE(e1.good());
    ASSERT_TRUE(e2.good());
    EXPECT_FALSE(e1.GetIsUnappliedUpdate());
    EXPECT_TRUE(e2.GetIsUnappliedUpdate());
  }
}

}  // namespace syncer
