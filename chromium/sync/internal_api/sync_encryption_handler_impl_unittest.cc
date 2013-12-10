// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/sync_encryption_handler_impl.h"

#include <string>

#include "base/base64.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/tracked_objects.h"
#include "sync/internal_api/public/base/model_type_test_util.h"
#include "sync/internal_api/public/read_node.h"
#include "sync/internal_api/public/read_transaction.h"
#include "sync/internal_api/public/test/test_user_share.h"
#include "sync/internal_api/public/write_node.h"
#include "sync/internal_api/public/write_transaction.h"
#include "sync/protocol/nigori_specifics.pb.h"
#include "sync/protocol/sync.pb.h"
#include "sync/syncable/entry.h"
#include "sync/syncable/mutable_entry.h"
#include "sync/syncable/syncable_write_transaction.h"
#include "sync/test/engine/test_id_factory.h"
#include "sync/test/fake_encryptor.h"
#include "sync/util/cryptographer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::AtLeast;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrictMock;

// The raw keystore key the server sends.
static const char kRawKeystoreKey[] = "keystore_key";
// Base64 encoded version of |kRawKeystoreKey|.
static const char kKeystoreKey[] = "a2V5c3RvcmVfa2V5";

class SyncEncryptionHandlerObserverMock
    : public SyncEncryptionHandler::Observer {
 public:
  MOCK_METHOD2(OnPassphraseRequired,
               void(PassphraseRequiredReason,
                    const sync_pb::EncryptedData&));  // NOLINT
  MOCK_METHOD0(OnPassphraseAccepted, void());  // NOLINT
  MOCK_METHOD2(OnBootstrapTokenUpdated,
               void(const std::string&, BootstrapTokenType type));  // NOLINT
  MOCK_METHOD2(OnEncryptedTypesChanged,
               void(ModelTypeSet, bool));  // NOLINT
  MOCK_METHOD0(OnEncryptionComplete, void());  // NOLINT
  MOCK_METHOD1(OnCryptographerStateChanged, void(Cryptographer*));  // NOLINT
  MOCK_METHOD2(OnPassphraseTypeChanged, void(PassphraseType,
                                             base::Time));  // NOLINT
};

google::protobuf::RepeatedPtrField<google::protobuf::string>
BuildEncryptionKeyProto(std::string encryption_key) {
  google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
  keys.Add()->assign(encryption_key);
  return keys;
}

}  // namespace

class SyncEncryptionHandlerImplTest : public ::testing::Test {
 public:
  SyncEncryptionHandlerImplTest() {}
  virtual ~SyncEncryptionHandlerImplTest() {}

  virtual void SetUp() {
    test_user_share_.SetUp();
    SetUpEncryption();
    CreateRootForType(NIGORI);
  }

  virtual void TearDown() {
    PumpLoop();
    test_user_share_.TearDown();
  }

 protected:
  void SetUpEncryption() {
    encryption_handler_.reset(
        new SyncEncryptionHandlerImpl(user_share(),
                                      &encryptor_,
                                      std::string(),
                                      std::string() /* bootstrap tokens */));
    encryption_handler_->AddObserver(&observer_);
  }

  void CreateRootForType(ModelType model_type) {
    syncer::syncable::Directory* directory = user_share()->directory.get();

    std::string tag_name = ModelTypeToRootTag(model_type);

    syncable::WriteTransaction wtrans(FROM_HERE, syncable::UNITTEST, directory);
    syncable::MutableEntry node(&wtrans,
                                syncable::CREATE,
                                model_type,
                                wtrans.root_id(),
                                tag_name);
    node.PutUniqueServerTag(tag_name);
    node.PutIsDir(true);
    node.PutServerIsDir(false);
    node.PutIsUnsynced(false);
    node.PutIsUnappliedUpdate(false);
    node.PutServerVersion(20);
    node.PutBaseVersion(20);
    node.PutIsDel(false);
    node.PutId(ids_.MakeServer(tag_name));
    sync_pb::EntitySpecifics specifics;
    syncer::AddDefaultFieldValue(model_type, &specifics);
    node.PutSpecifics(specifics);
  }

  void PumpLoop() {
    message_loop_.RunUntilIdle();
  }

  // Getters for tests.
  UserShare* user_share() { return test_user_share_.user_share(); }
  SyncEncryptionHandlerImpl* encryption_handler() {
      return encryption_handler_.get();
  }
  SyncEncryptionHandlerObserverMock* observer() { return &observer_; }
  Cryptographer* GetCryptographer() {
    return encryption_handler_->GetCryptographerUnsafe();
  }

  void VerifyMigratedNigori(PassphraseType passphrase_type,
                            const std::string& passphrase) {
    VerifyMigratedNigoriWithTimestamp(0, passphrase_type, passphrase);
  }

  void VerifyMigratedNigoriWithTimestamp(
      int64 migration_time,
      PassphraseType passphrase_type,
      const std::string& passphrase) {
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    const sync_pb::NigoriSpecifics& nigori = nigori_node.GetNigoriSpecifics();
    if (migration_time > 0)
      EXPECT_EQ(migration_time, nigori.keystore_migration_time());
    else
      EXPECT_TRUE(nigori.has_keystore_migration_time());
    EXPECT_TRUE(nigori.keybag_is_frozen());
    if (passphrase_type == CUSTOM_PASSPHRASE ||
        passphrase_type == FROZEN_IMPLICIT_PASSPHRASE) {
      EXPECT_TRUE(nigori.encrypt_everything());
      EXPECT_TRUE(nigori.keystore_decryptor_token().blob().empty());
      if (passphrase_type == CUSTOM_PASSPHRASE) {
        EXPECT_EQ(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE,
                  nigori.passphrase_type());
        if (!encryption_handler()->custom_passphrase_time().is_null()) {
          EXPECT_EQ(nigori.custom_passphrase_time(),
                    TimeToProtoTime(
                        encryption_handler()->custom_passphrase_time()));
        }
      } else {
        EXPECT_EQ(sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE,
                  nigori.passphrase_type());
      }
    } else {
      EXPECT_FALSE(nigori.encrypt_everything());
      EXPECT_FALSE(nigori.keystore_decryptor_token().blob().empty());
      EXPECT_EQ(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE,
                nigori.passphrase_type());
      Cryptographer keystore_cryptographer(&encryptor_);
      KeyParams params = {"localhost", "dummy", kKeystoreKey};
      keystore_cryptographer.AddKey(params);
      EXPECT_TRUE(keystore_cryptographer.CanDecryptUsingDefaultKey(
          nigori.keystore_decryptor_token()));
    }

    Cryptographer temp_cryptographer(&encryptor_);
    KeyParams params = {"localhost", "dummy", passphrase};
    temp_cryptographer.AddKey(params);
    EXPECT_TRUE(temp_cryptographer.CanDecryptUsingDefaultKey(
        nigori.encryption_keybag()));
  }

  sync_pb::NigoriSpecifics BuildMigratedNigori(
      PassphraseType passphrase_type,
      int64 migration_time,
      const std::string& default_passphrase,
      const std::string& keystore_key) {
    DCHECK_NE(passphrase_type, IMPLICIT_PASSPHRASE);
    Cryptographer other_cryptographer(GetCryptographer()->encryptor());

    std::string default_key = default_passphrase;
    if (default_key.empty()) {
      default_key = keystore_key;
    } else {
      KeyParams keystore_params = {"localhost", "dummy", keystore_key};
      other_cryptographer.AddKey(keystore_params);
    }
    KeyParams params = {"localhost", "dummy", default_key};
    other_cryptographer.AddKey(params);
    EXPECT_TRUE(other_cryptographer.is_ready());

    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(migration_time);

    if (passphrase_type == KEYSTORE_PASSPHRASE) {
      sync_pb::EncryptedData keystore_decryptor_token;
      EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
          other_cryptographer,
          keystore_key,
          &keystore_decryptor_token));
      nigori.mutable_keystore_decryptor_token()->CopyFrom(
          keystore_decryptor_token);
      nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    } else {
      nigori.set_encrypt_everything(true);
      nigori.set_passphrase_type(
          passphrase_type == CUSTOM_PASSPHRASE ?
              sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE :
              sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE);
    }
    return nigori;
  }

  // Build a migrated nigori node with the specified default passphrase
  // and keystore key and initialize the encryption handler with it.
  void InitKeystoreMigratedNigori(int64 migration_time,
                                  const std::string& default_passphrase,
                                  const std::string& keystore_key) {
    {
      WriteTransaction trans(FROM_HERE, user_share());
      WriteNode nigori_node(&trans);
      ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
      sync_pb::NigoriSpecifics nigori = BuildMigratedNigori(
          KEYSTORE_PASSPHRASE,
          migration_time,
          default_passphrase,
          keystore_key);
      nigori_node.SetNigoriSpecifics(nigori);
    }

    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AtLeast(1));
    EXPECT_CALL(*observer(),
                OnEncryptedTypesChanged(_, false));
    EXPECT_CALL(*observer(),
                OnEncryptionComplete()).Times(AtLeast(1));
    encryption_handler()->Init();
    EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
    EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
    EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
    Mock::VerifyAndClearExpectations(observer());
  }

  // Build a migrated nigori node with the specified default passphrase
  // as a custom passphrase.
  void InitCustomPassMigratedNigori(int64 migration_time,
                                    const std::string& default_passphrase) {
    {
      WriteTransaction trans(FROM_HERE, user_share());
      WriteNode nigori_node(&trans);
      ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
      sync_pb::NigoriSpecifics nigori = BuildMigratedNigori(
          CUSTOM_PASSPHRASE,
          migration_time,
          default_passphrase,
          kKeystoreKey);
      nigori_node.SetNigoriSpecifics(nigori);
    }

    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AtLeast(1));
    EXPECT_CALL(*observer(),
                OnEncryptedTypesChanged(_, true)).Times(AtLeast(1));
    EXPECT_CALL(*observer(),
                OnEncryptionComplete()).Times(AtLeast(1));
    encryption_handler()->Init();
    EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
    EXPECT_EQ(encryption_handler()->GetPassphraseType(), CUSTOM_PASSPHRASE);
    EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
    Mock::VerifyAndClearExpectations(observer());
  }

  // Build an unmigrated nigori node with the specified passphrase and type and
  // initialize the encryption handler with it.
  void InitUnmigratedNigori(const std::string& default_passphrase,
                            PassphraseType passphrase_type) {
    DCHECK_NE(passphrase_type, FROZEN_IMPLICIT_PASSPHRASE);
    Cryptographer other_cryptographer(GetCryptographer()->encryptor());
    KeyParams default_key = {"localhost", "dummy", default_passphrase};
    other_cryptographer.AddKey(default_key);
    EXPECT_TRUE(other_cryptographer.is_ready());

    {
      WriteTransaction trans(FROM_HERE, user_share());
      WriteNode nigori_node(&trans);
      ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
      sync_pb::NigoriSpecifics nigori;
      other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
      nigori.set_keybag_is_frozen(passphrase_type == CUSTOM_PASSPHRASE);
      nigori_node.SetNigoriSpecifics(nigori);
    }

    if (passphrase_type != IMPLICIT_PASSPHRASE) {
      EXPECT_CALL(*observer(),
                  OnPassphraseTypeChanged(passphrase_type, _));
    }
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AtLeast(1));
    EXPECT_CALL(*observer(),
                OnEncryptedTypesChanged(_, false));
    encryption_handler()->Init();
    EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
    EXPECT_EQ(encryption_handler()->GetPassphraseType(), passphrase_type);
    EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
    Mock::VerifyAndClearExpectations(observer());
  }

 protected:
  TestUserShare test_user_share_;
  FakeEncryptor encryptor_;
  scoped_ptr<SyncEncryptionHandlerImpl> encryption_handler_;
  StrictMock<SyncEncryptionHandlerObserverMock> observer_;
  TestIdFactory ids_;
  base::MessageLoop message_loop_;
};

// Verify that the encrypted types are being written to and read from the
// nigori node properly.
TEST_F(SyncEncryptionHandlerImplTest, NigoriEncryptionTypes) {
  sync_pb::NigoriSpecifics nigori;

  StrictMock<SyncEncryptionHandlerObserverMock> observer2;
  SyncEncryptionHandlerImpl handler2(user_share(),
                                     &encryptor_,
                                     std::string(),
                                     std::string() /* bootstrap tokens */);
  handler2.AddObserver(&observer2);

  // Just set the sensitive types (shouldn't trigger any notifications).
  ModelTypeSet encrypted_types(SyncEncryptionHandler::SensitiveTypes());
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->MergeEncryptedTypes(
        encrypted_types,
        trans.GetWrappedTrans());
    encryption_handler()->UpdateNigoriFromEncryptedTypes(
        &nigori,
        trans.GetWrappedTrans());
    handler2.UpdateEncryptedTypesFromNigori(nigori, trans.GetWrappedTrans());
  }
  EXPECT_TRUE(encrypted_types.Equals(
      encryption_handler()->GetEncryptedTypesUnsafe()));
  EXPECT_TRUE(encrypted_types.Equals(
      handler2.GetEncryptedTypesUnsafe()));

  Mock::VerifyAndClearExpectations(observer());
  Mock::VerifyAndClearExpectations(&observer2);

  ModelTypeSet encrypted_user_types = EncryptableUserTypes();

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(
                  HasModelTypes(encrypted_user_types), false));
  EXPECT_CALL(observer2,
              OnEncryptedTypesChanged(
                  HasModelTypes(encrypted_user_types), false));

  // Set all encrypted types
  encrypted_types = EncryptableUserTypes();
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->MergeEncryptedTypes(
        encrypted_types,
        trans.GetWrappedTrans());
    encryption_handler()->UpdateNigoriFromEncryptedTypes(
        &nigori,
        trans.GetWrappedTrans());
    handler2.UpdateEncryptedTypesFromNigori(nigori, trans.GetWrappedTrans());
  }
  EXPECT_TRUE(encrypted_types.Equals(
      encryption_handler()->GetEncryptedTypesUnsafe()));
  EXPECT_TRUE(encrypted_types.Equals(handler2.GetEncryptedTypesUnsafe()));

  // Receiving an empty nigori should not reset any encrypted types or trigger
  // an observer notification.
  Mock::VerifyAndClearExpectations(observer());
  Mock::VerifyAndClearExpectations(&observer2);
  nigori = sync_pb::NigoriSpecifics();
  {
    WriteTransaction trans(FROM_HERE, user_share());
    handler2.UpdateEncryptedTypesFromNigori(nigori, trans.GetWrappedTrans());
  }
  EXPECT_TRUE(encrypted_types.Equals(
      encryption_handler()->GetEncryptedTypesUnsafe()));
}

// Verify the encryption handler processes the encrypt everything field
// properly.
TEST_F(SyncEncryptionHandlerImplTest, EncryptEverythingExplicit) {
  sync_pb::NigoriSpecifics nigori;
  nigori.set_encrypt_everything(true);

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(
                  HasModelTypes(EncryptableUserTypes()), true));

  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  ModelTypeSet encrypted_types =
      encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_TRUE(encrypted_types.Equals(ModelTypeSet(PASSWORDS)));

  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori,
        trans.GetWrappedTrans());
  }

  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_TRUE(encrypted_types.HasAll(EncryptableUserTypes()));

  // Receiving the nigori node again shouldn't trigger another notification.
  Mock::VerifyAndClearExpectations(observer());
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori,
        trans.GetWrappedTrans());
  }
}

// Verify the encryption handler can detect an implicit encrypt everything state
// (from clients that failed to write the encrypt everything field).
TEST_F(SyncEncryptionHandlerImplTest, EncryptEverythingImplicit) {
  sync_pb::NigoriSpecifics nigori;
  nigori.set_encrypt_bookmarks(true);  // Non-passwords = encrypt everything

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(
                  HasModelTypes(EncryptableUserTypes()), true));

  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  ModelTypeSet encrypted_types =
      encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_TRUE(encrypted_types.Equals(ModelTypeSet(PASSWORDS)));

  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori,
        trans.GetWrappedTrans());
  }

  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_TRUE(encrypted_types.HasAll(EncryptableUserTypes()));

  // Receiving a nigori node with encrypt everything explicitly set shouldn't
  // trigger another notification.
  Mock::VerifyAndClearExpectations(observer());
  nigori.set_encrypt_everything(true);
  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori,
        trans.GetWrappedTrans());
  }
}

// Verify the encryption handler can deal with new versions treating new types
// as Sensitive, and that it does not consider this an implicit encrypt
// everything case.
TEST_F(SyncEncryptionHandlerImplTest, UnknownSensitiveTypes) {
  sync_pb::NigoriSpecifics nigori;
  nigori.set_encrypt_everything(false);
  nigori.set_encrypt_bookmarks(true);

  ModelTypeSet expected_encrypted_types =
      SyncEncryptionHandler::SensitiveTypes();
  expected_encrypted_types.Put(BOOKMARKS);

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(
                  HasModelTypes(expected_encrypted_types), false));

  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  ModelTypeSet encrypted_types =
      encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_TRUE(encrypted_types.Equals(ModelTypeSet(PASSWORDS)));

  {
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->UpdateEncryptedTypesFromNigori(
        nigori,
        trans.GetWrappedTrans());
  }

  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  encrypted_types = encryption_handler()->GetEncryptedTypesUnsafe();
  EXPECT_TRUE(encrypted_types.Equals(ModelTypeSet(BOOKMARKS, PASSWORDS)));
}

// Receive an old nigori with old encryption keys and encrypted types. We should
// not revert our default key or encrypted types, and should post a task to
// overwrite the existing nigori with the correct data.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveOldNigori) {
  KeyParams old_key = {"localhost", "dummy", "old"};
  KeyParams current_key = {"localhost", "dummy", "cur"};

  // Data for testing encryption/decryption.
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  other_cryptographer.AddKey(old_key);
  sync_pb::EntitySpecifics other_encrypted_specifics;
  other_encrypted_specifics.mutable_bookmark()->set_title("title");
  other_cryptographer.Encrypt(
      other_encrypted_specifics,
      other_encrypted_specifics.mutable_encrypted());
  sync_pb::EntitySpecifics our_encrypted_specifics;
  our_encrypted_specifics.mutable_bookmark()->set_title("title2");
  ModelTypeSet encrypted_types = EncryptableUserTypes();

  // Set up the current encryption state (containing both keys and encrypt
  // everything).
  sync_pb::NigoriSpecifics current_nigori_specifics;
  GetCryptographer()->AddKey(old_key);
  GetCryptographer()->AddKey(current_key);
  GetCryptographer()->Encrypt(
      our_encrypted_specifics,
      our_encrypted_specifics.mutable_encrypted());
  GetCryptographer()->GetKeys(
      current_nigori_specifics.mutable_encryption_keybag());
  current_nigori_specifics.set_encrypt_everything(true);

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptedTypesChanged(
      HasModelTypes(EncryptableUserTypes()), true));
  {
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(
        current_nigori_specifics,
        trans.GetWrappedTrans());
  }
  Mock::VerifyAndClearExpectations(observer());

  // Now set up the old nigori specifics and apply it on top.
  // Has an old set of keys, and no encrypted types.
  sync_pb::NigoriSpecifics old_nigori;
  other_cryptographer.GetKeys(old_nigori.mutable_encryption_keybag());

  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_)).Times(AnyNumber());
  {
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(
        old_nigori,
        trans.GetWrappedTrans());
  }
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_FALSE(GetCryptographer()->has_pending_keys());

  // Encryption handler should have posted a task to overwrite the old
  // specifics.
  PumpLoop();

  {
    // The cryptographer should be able to decrypt both sets of keys and still
    // be encrypting with the newest, and the encrypted types should be the
    // most recent.
    // In addition, the nigori node should match the current encryption state.
    ReadTransaction trans(FROM_HERE, user_share());
    ReadNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    const sync_pb::NigoriSpecifics& nigori = nigori_node.GetNigoriSpecifics();
    EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(
        our_encrypted_specifics.encrypted()));
    EXPECT_TRUE(GetCryptographer()->CanDecrypt(
        other_encrypted_specifics.encrypted()));
    EXPECT_TRUE(GetCryptographer()->CanDecrypt(nigori.encryption_keybag()));
    EXPECT_TRUE(nigori.encrypt_everything());
    EXPECT_TRUE(
        GetCryptographer()->CanDecryptUsingDefaultKey(
            nigori.encryption_keybag()));
  }
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
}

// Ensure setting the keystore key works, updates the bootstrap token, and
// triggers a non-backwards compatible migration. Then verify that the
// bootstrap token can be correctly parsed by the encryption handler at startup
// time.
TEST_F(SyncEncryptionHandlerImplTest, SetKeystoreMigratesAndUpdatesBootstrap) {
  // Passing no keys should do nothing.
  EXPECT_CALL(*observer(), OnBootstrapTokenUpdated(_, _)).Times(0);
  {
    WriteTransaction trans(FROM_HERE, user_share());
    EXPECT_FALSE(GetCryptographer()->is_initialized());
    EXPECT_TRUE(encryption_handler()->NeedKeystoreKey(trans.GetWrappedTrans()));
    EXPECT_FALSE(encryption_handler()->SetKeystoreKeys(
        BuildEncryptionKeyProto(std::string()), trans.GetWrappedTrans()));
    EXPECT_TRUE(encryption_handler()->NeedKeystoreKey(trans.GetWrappedTrans()));
  }
  Mock::VerifyAndClearExpectations(observer());

  // Build a set of keystore keys.
  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);
  google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
  keys.Add()->assign(kRawOldKeystoreKey);
  keys.Add()->assign(kRawKeystoreKey);

  // Pass them to the encryption handler, triggering a migration and bootstrap
  // token update.
  std::string encoded_key;
  std::string keystore_bootstrap;
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_));
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(), OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_,
                                      KEYSTORE_BOOTSTRAP_TOKEN)).
      WillOnce(SaveArg<0>(&keystore_bootstrap));
  {
    WriteTransaction trans(FROM_HERE, user_share());
    EXPECT_TRUE(
        encryption_handler()->SetKeystoreKeys(
            keys,
            trans.GetWrappedTrans()));
    EXPECT_FALSE(
        encryption_handler()->NeedKeystoreKey(trans.GetWrappedTrans()));
    EXPECT_FALSE(GetCryptographer()->is_initialized());
  }
  PumpLoop();
  EXPECT_TRUE(GetCryptographer()->is_initialized());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kKeystoreKey);

  // Ensure the bootstrap is encoded properly (a base64 encoded encrypted blob
  // of list values containing the keystore keys).
  std::string decoded_bootstrap;
  ASSERT_TRUE(base::Base64Decode(keystore_bootstrap, &decoded_bootstrap));
  std::string decrypted_bootstrap;
  ASSERT_TRUE(
      GetCryptographer()->encryptor()->DecryptString(decoded_bootstrap,
                                                     &decrypted_bootstrap));
  JSONStringValueSerializer json(decrypted_bootstrap);
  scoped_ptr<base::Value> deserialized_keystore_keys(
      json.Deserialize(NULL, NULL));
  ASSERT_TRUE(deserialized_keystore_keys.get());
  base::ListValue* keystore_list = NULL;
  deserialized_keystore_keys->GetAsList(&keystore_list);
  ASSERT_TRUE(keystore_list);
  ASSERT_EQ(2U, keystore_list->GetSize());
  std::string test_string;
  keystore_list->GetString(0, &test_string);
  ASSERT_EQ(old_keystore_key, test_string);
  keystore_list->GetString(1, &test_string);
  ASSERT_EQ(kKeystoreKey, test_string);


  // Now make sure a new encryption handler can correctly parse the bootstrap
  // token.
  SyncEncryptionHandlerImpl handler2(user_share(),
                                     &encryptor_,
                                     std::string(),  // Cryptographer bootstrap.
                                     keystore_bootstrap);

  {
    WriteTransaction trans(FROM_HERE, user_share());
    EXPECT_FALSE(handler2.NeedKeystoreKey(trans.GetWrappedTrans()));
  }
}

// Ensure GetKeystoreDecryptor only updates the keystore decryptor token if it
// wasn't already set properly. Otherwise, the decryptor should remain the
// same.
TEST_F(SyncEncryptionHandlerImplTest, GetKeystoreDecryptor) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_TRUE(other_cryptographer.is_ready());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer,
      kKeystoreKey,
      &encrypted));
  std::string serialized = encrypted.SerializeAsString();
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer,
      kKeystoreKey,
      &encrypted));
  EXPECT_EQ(serialized, encrypted.SerializeAsString());
}

// Test that we don't attempt to migrate while an implicit passphrase is pending
// and that once we do decrypt pending keys we migrate the nigori. Once
// migrated, we should be in keystore passphrase state.
TEST_F(SyncEncryptionHandlerImplTest, MigrateOnDecryptImplicitPass) {
  const char kOtherKey[] = "other";
  {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
    Mock::VerifyAndClearExpectations(observer());
  }
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    Cryptographer other_cryptographer(GetCryptographer()->encryptor());
    KeyParams other_key = {"localhost", "dummy", kOtherKey};
    other_cryptographer.AddKey(other_key);

    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(false);
    nigori.set_encrypt_everything(false);
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnPassphraseRequired(_, _));
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  encryption_handler()->SetDecryptionPassphrase(kOtherKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(KEYSTORE_PASSPHRASE, encryption_handler()->GetPassphraseType());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kOtherKey);
}

// Test that we don't attempt to migrate while a custom passphrase is pending,
// and that once we do decrypt pending keys we migrate the nigori. Once
// migrated, we should be in custom passphrase state with encrypt everything.
TEST_F(SyncEncryptionHandlerImplTest, MigrateOnDecryptCustomPass) {
  const char kOtherKey[] = "other";
  {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
    Mock::VerifyAndClearExpectations(observer());
  }
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    Cryptographer other_cryptographer(GetCryptographer()->encryptor());
    KeyParams other_key = {"localhost", "dummy", kOtherKey};
    other_cryptographer.AddKey(other_key);

    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_encrypt_everything(false);
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnPassphraseRequired(_, _));
    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete()).Times(2);
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  encryption_handler()->SetDecryptionPassphrase(kOtherKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(CUSTOM_PASSPHRASE, encryption_handler()->GetPassphraseType());
  VerifyMigratedNigori(CUSTOM_PASSPHRASE, kOtherKey);
}

// Test that we trigger a migration when we set the keystore key, had an
// implicit passphrase, and did not have encrypt everything. We should switch
// to KEYSTORE_PASSPHRASE.
TEST_F(SyncEncryptionHandlerImplTest, MigrateOnKeystoreKeyAvailableImplicit) {
  const char kCurKey[] = "cur";
  KeyParams current_key = {"localhost", "dummy", kCurKey};
  GetCryptographer()->AddKey(current_key);
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->Init();
  Mock::VerifyAndClearExpectations(observer());

  {
    ReadTransaction trans(FROM_HERE, user_share());
    // Once we provide a keystore key, we should perform the migration.
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  // The actual migration gets posted, so run all pending tasks.
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(KEYSTORE_PASSPHRASE,
            encryption_handler()->GetPassphraseType());
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kCurKey);
}

// Test that we trigger a migration when we set the keystore key, had an
// implicit passphrase, and encrypt everything enabled. We should switch to
// FROZEN_IMPLICIT_PASSPHRASE.
TEST_F(SyncEncryptionHandlerImplTest,
       MigrateOnKeystoreKeyAvailableFrozenImplicit) {
  const char kCurKey[] = "cur";
  KeyParams current_key = {"localhost", "dummy", kCurKey};
  GetCryptographer()->AddKey(current_key);
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->Init();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->EnableEncryptEverything();

  {
    ReadTransaction trans(FROM_HERE, user_share());
    // Once we provide a keystore key, we should perform the migration.
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(FROZEN_IMPLICIT_PASSPHRASE, _));
  // The actual migration gets posted, so run all pending tasks.
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(FROZEN_IMPLICIT_PASSPHRASE,
            encryption_handler()->GetPassphraseType());
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigori(FROZEN_IMPLICIT_PASSPHRASE, kCurKey);
}

// Test that we trigger a migration when we set the keystore key, had a
// custom passphrase, and encrypt everything enabled. The passphrase state
// should remain as CUSTOM_PASSPHRASE, and encrypt everything stay the same.
TEST_F(SyncEncryptionHandlerImplTest,
       MigrateOnKeystoreKeyAvailableCustomWithEncryption) {
  const char kCurKey[] = "cur";
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseRequired(_, _));
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  encryption_handler()->Init();
  encryption_handler()->SetEncryptionPassphrase(kCurKey, true);
  EXPECT_FALSE(encryption_handler()->custom_passphrase_time().is_null());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->EnableEncryptEverything();
  Mock::VerifyAndClearExpectations(observer());

  {
    ReadTransaction trans(FROM_HERE, user_share());
    // Once we provide a keystore key, we should perform the migration.
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  // The actual migration gets posted, so run all pending tasks.
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(CUSTOM_PASSPHRASE,
            encryption_handler()->GetPassphraseType());
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigori(CUSTOM_PASSPHRASE, kCurKey);
}

// Test that we trigger a migration when we set the keystore key, had a
// custom passphrase, and did not have encrypt everything. The passphrase state
// should remain as CUSTOM_PASSPHRASE, and encrypt everything should be enabled.
TEST_F(SyncEncryptionHandlerImplTest,
       MigrateOnKeystoreKeyAvailableCustomNoEncryption) {
  const char kCurKey[] = "cur";
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseRequired(_, _));
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  encryption_handler()->Init();
  encryption_handler()->SetEncryptionPassphrase(kCurKey, true);
  EXPECT_FALSE(encryption_handler()->custom_passphrase_time().is_null());
  Mock::VerifyAndClearExpectations(observer());

  {
    ReadTransaction trans(FROM_HERE, user_share());
    // Once we provide a keystore key, we should perform the migration.
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  // The actual migration gets posted, so run all pending tasks.
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(CUSTOM_PASSPHRASE,
            encryption_handler()->GetPassphraseType());
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigori(CUSTOM_PASSPHRASE, kCurKey);
}

// Test that we can handle receiving a migrated nigori node in the
// KEYSTORE_PASS state, and use the keystore decryptor token to decrypt the
// keybag.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveMigratedNigoriKeystorePass) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData keystore_decryptor_token;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_TRUE(other_cryptographer.is_ready());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer,
      kKeystoreKey,
      &keystore_decryptor_token));
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  EXPECT_FALSE(GetCryptographer()->is_ready());
  EXPECT_NE(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);

  // Now build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer should be
  // initialized properly to decrypt both kCurKey and kKeystoreKey.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);

    EXPECT_CALL(*observer(), OnPassphraseAccepted());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, KEYSTORE_PASSPHRASE, kCurKey);

  // Check that the cryptographer still encrypts with the current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  Cryptographer keystore_cryptographer(GetCryptographer()->encryptor());
  KeyParams keystore_key =  {"localhost", "dummy", kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// Test that we handle receiving migrated nigori's with
// FROZEN_IMPLICIT_PASSPHRASE state. We should be in a pending key state until
// we supply the pending frozen implicit passphrase key.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveMigratedNigoriFrozenImplicitPass) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(FROZEN_IMPLICIT_PASSPHRASE, _));
    EXPECT_CALL(*observer(),
                OnPassphraseRequired(_, _));
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnEncryptedTypesChanged(_, true));
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.set_keybag_is_frozen(true);
    nigori.set_passphrase_type(
        sync_pb::NigoriSpecifics::FROZEN_IMPLICIT_PASSPHRASE);
    nigori.set_keystore_migration_time(1);
    nigori.set_encrypt_everything(true);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(FROZEN_IMPLICIT_PASSPHRASE,
            encryption_handler()->GetPassphraseType());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  encryption_handler()->SetDecryptionPassphrase(kCurKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  VerifyMigratedNigoriWithTimestamp(1, FROZEN_IMPLICIT_PASSPHRASE, kCurKey);

  // Check that the cryptographer still encrypts with the current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  Cryptographer keystore_cryptographer(GetCryptographer()->encryptor());
  KeyParams keystore_key =  {"localhost", "dummy", kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// Test that we handle receiving migrated nigori's with
// CUSTOM_PASSPHRASE state. We should be in a pending key state until we
// provide the custom passphrase key.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveMigratedNigoriCustomPass) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());

  {
    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
    EXPECT_CALL(*observer(),
                OnPassphraseRequired(_, _));
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnEncryptedTypesChanged(_, true));
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.set_keybag_is_frozen(true);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
    nigori.set_keystore_migration_time(1);
    nigori.set_encrypt_everything(true);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(CUSTOM_PASSPHRASE, encryption_handler()->GetPassphraseType());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());

  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  encryption_handler()->SetDecryptionPassphrase(kCurKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kCurKey);

  // Check that the cryptographer still encrypts with the current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  Cryptographer keystore_cryptographer(GetCryptographer()->encryptor());
  KeyParams keystore_key =  {"localhost", "dummy", kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// Test that if we have a migrated nigori with a custom passphrase, then receive
// and old implicit passphrase nigori, we properly overwrite it with the current
// state.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveUnmigratedNigoriAfterMigration) {
  const char kOldKey[] = "old";
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  KeyParams old_key = {"localhost", "dummy", kOldKey};
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  GetCryptographer()->AddKey(old_key);
  GetCryptographer()->AddKey(cur_key);

  // Build a migrated nigori with full encryption.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    GetCryptographer()->GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
    nigori.set_encrypt_everything(true);
    nigori_node.SetNigoriSpecifics(nigori);
  }

  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true)).Times(2);
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), CUSTOM_PASSPHRASE);
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kCurKey);

  {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  Mock::VerifyAndClearExpectations(observer());

  // Now build an old unmigrated nigori node with old encrypted types. We should
  // properly overwrite it with the migrated + encrypt everything state.
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  {
    Cryptographer other_cryptographer(GetCryptographer()->encryptor());
    other_cryptographer.AddKey(old_key);
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(false);
    nigori.set_encrypt_everything(false);
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  PumpLoop();

  // Verify we're still migrated and have proper encryption state.
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), CUSTOM_PASSPHRASE);
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kCurKey);
}

// Test that if we have a migrated nigori with a custom passphrase, then receive
// a migrated nigori with a keystore passphrase, we properly overwrite it with
// the current state.
TEST_F(SyncEncryptionHandlerImplTest, ReceiveOldMigratedNigori) {
  const char kOldKey[] = "old";
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  KeyParams old_key = {"localhost", "dummy", kOldKey};
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  GetCryptographer()->AddKey(old_key);
  GetCryptographer()->AddKey(cur_key);

  // Build a migrated nigori with full encryption.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    GetCryptographer()->GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::CUSTOM_PASSPHRASE);
    nigori.set_encrypt_everything(true);
    nigori_node.SetNigoriSpecifics(nigori);
  }

  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true)).Times(2);
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), CUSTOM_PASSPHRASE);
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kCurKey);

  {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  Mock::VerifyAndClearExpectations(observer());

  // Now build an old keystore nigori node with old encrypted types. We should
  // properly overwrite it with the migrated + encrypt everything state.
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    Cryptographer other_cryptographer(GetCryptographer()->encryptor());
    other_cryptographer.AddKey(old_key);
    encryption_handler()->GetKeystoreDecryptor(
        other_cryptographer,
        kKeystoreKey,
        nigori.mutable_keystore_decryptor_token());
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_encrypt_everything(false);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori.set_keystore_migration_time(1);
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  PumpLoop();

  // Verify we're still migrated and have proper encryption state.
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), CUSTOM_PASSPHRASE);
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kCurKey);
}

// Test that if we receive the keystore key after receiving a migrated nigori
// node, we properly use the keystore decryptor token to decrypt the keybag.
TEST_F(SyncEncryptionHandlerImplTest, SetKeystoreAfterReceivingMigratedNigori) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData keystore_decryptor_token;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_TRUE(other_cryptographer.is_ready());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer,
      kKeystoreKey,
      &keystore_decryptor_token));
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  EXPECT_FALSE(GetCryptographer()->is_ready());
  EXPECT_NE(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);

  // Now build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer should be
  // initialized properly to decrypt both kCurKey and kKeystoreKey.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);

    EXPECT_CALL(*observer(),
                OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    EXPECT_CALL(*observer(),
                OnPassphraseRequired(_, _));
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  // Run any tasks posted via AppplyNigoriUpdate.
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  PumpLoop();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, KEYSTORE_PASSPHRASE, kCurKey);

  // Check that the cryptographer still encrypts with the current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  Cryptographer keystore_cryptographer(GetCryptographer()->encryptor());
  KeyParams keystore_key =  {"localhost", "dummy", kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// Test that after receiving a migrated nigori and decrypting it using the
// keystore key, we can then switch to a custom passphrase. The nigori should
// remain migrated and encrypt everything should be enabled.
TEST_F(SyncEncryptionHandlerImplTest, SetCustomPassAfterMigration) {
  const char kOldKey[] = "old";
  sync_pb::EncryptedData keystore_decryptor_token;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kOldKey};
  other_cryptographer.AddKey(cur_key);
  EXPECT_TRUE(other_cryptographer.is_ready());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer,
      kKeystoreKey,
      &keystore_decryptor_token));

  // Build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer should be
  // initialized properly to decrypt both kOldKey and kKeystoreKey.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori_node.SetNigoriSpecifics(nigori);
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }

  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  const char kNewKey[] = "new_key";
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete()).Times(2);
  encryption_handler()->SetEncryptionPassphrase(kNewKey, true);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), CUSTOM_PASSPHRASE);
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  EXPECT_FALSE(encryption_handler()->custom_passphrase_time().is_null());
  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kNewKey);

  // Check that the cryptographer can decrypt the old key.
  sync_pb::EncryptedData old_encrypted;
  other_cryptographer.EncryptString("string", &old_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(old_encrypted));

  // Check that the cryptographer can decrypt keystore key based encryption.
  Cryptographer keystore_cryptographer(GetCryptographer()->encryptor());
  KeyParams keystore_key =  {"localhost", "dummy", kKeystoreKey};
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));

  // Check the the cryptographer is encrypting with the new key.
  KeyParams new_key = {"localhost", "dummy", kNewKey};
  Cryptographer new_cryptographer(GetCryptographer()->encryptor());
  new_cryptographer.AddKey(new_key);
  sync_pb::EncryptedData new_encrypted;
  new_cryptographer.EncryptString("string", &new_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(new_encrypted));
}

// Test that if a client without a keystore key (e.g. one without keystore
// encryption enabled) receives a migrated nigori and then attempts to set a
// custom passphrase, it also enables encrypt everything. The nigori node
// should remain migrated.
TEST_F(SyncEncryptionHandlerImplTest,
       SetCustomPassAfterMigrationNoKeystoreKey) {
  const char kOldKey[] = "old";
  sync_pb::EncryptedData keystore_decryptor_token;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kOldKey};
  other_cryptographer.AddKey(cur_key);
  KeyParams keystore_key = {"localhost", "dummy", kKeystoreKey};
  other_cryptographer.AddNonDefaultKey(keystore_key);
  EXPECT_TRUE(other_cryptographer.is_ready());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer,
      kKeystoreKey,
      &keystore_decryptor_token));

  // Build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer will have
  // pending keys until we provide the decryption passphrase.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori_node.SetNigoriSpecifics(nigori);
  }

  EXPECT_CALL(*observer(),
              OnPassphraseRequired(_, _));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->SetDecryptionPassphrase(kOldKey);
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  const char kNewKey[] = "new_key";
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(CUSTOM_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete()).Times(2);
  encryption_handler()->SetEncryptionPassphrase(kNewKey, true);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), CUSTOM_PASSPHRASE);
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  EXPECT_FALSE(encryption_handler()->custom_passphrase_time().is_null());
  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kNewKey);

  // Check that the cryptographer can decrypt the old key.
  sync_pb::EncryptedData old_encrypted;
  other_cryptographer.EncryptString("string", &old_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(old_encrypted));

  // Check that the cryptographer can still decrypt keystore key based
  // encryption (should have been extracted from the encryption keybag).
  Cryptographer keystore_cryptographer(GetCryptographer()->encryptor());
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));

  // Check the the cryptographer is encrypting with the new key.
  KeyParams new_key = {"localhost", "dummy", kNewKey};
  Cryptographer new_cryptographer(GetCryptographer()->encryptor());
  new_cryptographer.AddKey(new_key);
  sync_pb::EncryptedData new_encrypted;
  new_cryptographer.EncryptString("string", &new_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(new_encrypted));
}

// Test that if a client without a keystore key (e.g. one without keystore
// encryption enabled) receives a migrated nigori and then attempts to set a
// new implicit passphrase, we do not modify the nigori node (the implicit
// passphrase is dropped).
TEST_F(SyncEncryptionHandlerImplTest,
       SetImplicitPassAfterMigrationNoKeystoreKey) {
  const char kOldKey[] = "old";
  sync_pb::EncryptedData keystore_decryptor_token;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kOldKey};
  other_cryptographer.AddKey(cur_key);
  KeyParams keystore_key = {"localhost", "dummy", kKeystoreKey};
  other_cryptographer.AddNonDefaultKey(keystore_key);
  EXPECT_TRUE(other_cryptographer.is_ready());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer,
      kKeystoreKey,
      &keystore_decryptor_token));

  // Build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer will have
  // pending keys until we provide the decryption passphrase.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori_node.SetNigoriSpecifics(nigori);
  }

  EXPECT_CALL(*observer(),
              OnPassphraseRequired(_, _));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->SetDecryptionPassphrase(kOldKey);
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  // Should get dropped on the floor silently.
  const char kNewKey[] = "new_key";
  encryption_handler()->SetEncryptionPassphrase(kNewKey, false);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, KEYSTORE_PASSPHRASE, kOldKey);

  // Check that the cryptographer can decrypt the old key.
  sync_pb::EncryptedData old_encrypted;
  other_cryptographer.EncryptString("string", &old_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(old_encrypted));

  // Check that the cryptographer can still decrypt keystore key based
  // encryption (due to extracting the keystore key from the encryption keybag).
  Cryptographer keystore_cryptographer(GetCryptographer()->encryptor());
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));

  // Check the the cryptographer does not have the new key.
  KeyParams new_key = {"localhost", "dummy", kNewKey};
  Cryptographer new_cryptographer(GetCryptographer()->encryptor());
  new_cryptographer.AddKey(new_key);
  sync_pb::EncryptedData new_encrypted;
  new_cryptographer.EncryptString("string", &new_encrypted);
  EXPECT_FALSE(GetCryptographer()->CanDecryptUsingDefaultKey(new_encrypted));
}

// Test that if a client without a keystore key (e.g. one without keystore
// encryption enabled) receives a migrated nigori in keystore passphrase state
// and then attempts to enable encrypt everything, we switch to a custom
// passphrase. The nigori should remain migrated.
TEST_F(SyncEncryptionHandlerImplTest,
       MigrateOnEncryptEverythingKeystorePassphrase) {
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData keystore_decryptor_token;
  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  other_cryptographer.AddKey(cur_key);
  KeyParams keystore_key = {"localhost", "dummy", kKeystoreKey};
  other_cryptographer.AddNonDefaultKey(keystore_key);
  EXPECT_TRUE(other_cryptographer.is_ready());
  EXPECT_TRUE(encryption_handler()->GetKeystoreDecryptor(
      other_cryptographer,
      kKeystoreKey,
      &keystore_decryptor_token));

  // Build a nigori node with the generated keystore decryptor token and
  // initialize the encryption handler with it. The cryptographer will have
  // pending keys until we provide the decryption passphrase.
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    nigori.mutable_keystore_decryptor_token()->CopyFrom(
        keystore_decryptor_token);
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_keystore_migration_time(1);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori_node.SetNigoriSpecifics(nigori);
  }
  EXPECT_CALL(*observer(),
              OnPassphraseRequired(_, _));
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  encryption_handler()->Init();
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->has_pending_keys());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->SetDecryptionPassphrase(kCurKey);
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(FROZEN_IMPLICIT_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
  encryption_handler()->EnableEncryptEverything();
  Mock::VerifyAndClearExpectations(observer());

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(FROZEN_IMPLICIT_PASSPHRASE,
            encryption_handler()->GetPassphraseType());
  EXPECT_TRUE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigoriWithTimestamp(1, FROZEN_IMPLICIT_PASSPHRASE, kCurKey);

  // Check that the cryptographer is encrypting using the frozen current key.
  sync_pb::EncryptedData current_encrypted;
  other_cryptographer.EncryptString("string", &current_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecryptUsingDefaultKey(current_encrypted));

  // Check that the cryptographer can still decrypt keystore key based
  // encryption (due to extracting the keystore key from the encryption keybag).
  Cryptographer keystore_cryptographer(GetCryptographer()->encryptor());
  keystore_cryptographer.AddKey(keystore_key);
  sync_pb::EncryptedData keystore_encrypted;
  keystore_cryptographer.EncryptString("string", &keystore_encrypted);
  EXPECT_TRUE(GetCryptographer()->CanDecrypt(keystore_encrypted));
}

// If we receive a nigori migrated and with a KEYSTORE_PASSPHRASE type, but
// using an old default key (i.e. old GAIA password), we should overwrite the
// nigori, updating the keybag and keystore decryptor.
TEST_F(SyncEncryptionHandlerImplTest,
       ReceiveMigratedNigoriWithOldPassphrase) {
  const char kOldKey[] = "old";
  const char kCurKey[] = "cur";
  sync_pb::EncryptedData encrypted;
  KeyParams old_key = {"localhost", "dummy", kOldKey};
  KeyParams cur_key = {"localhost", "dummy", kCurKey};
  GetCryptographer()->AddKey(old_key);
  GetCryptographer()->AddKey(cur_key);

  Cryptographer other_cryptographer(GetCryptographer()->encryptor());
  other_cryptographer.AddKey(old_key);
  EXPECT_TRUE(other_cryptographer.is_ready());

  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, false));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete());
  encryption_handler()->Init();
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());

  {
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                              kRawKeystoreKey),
                                          trans.GetWrappedTrans());
  }
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kCurKey);

  // Now build an old keystore passphrase nigori node.
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  {
    WriteTransaction trans(FROM_HERE, user_share());
    WriteNode nigori_node(&trans);
    ASSERT_EQ(nigori_node.InitByTagLookup(kNigoriTag), BaseNode::INIT_OK);
    sync_pb::NigoriSpecifics nigori;
    Cryptographer other_cryptographer(GetCryptographer()->encryptor());
    other_cryptographer.AddKey(old_key);
    encryption_handler()->GetKeystoreDecryptor(
        other_cryptographer,
        kKeystoreKey,
        nigori.mutable_keystore_decryptor_token());
    other_cryptographer.GetKeys(nigori.mutable_encryption_keybag());
    nigori.set_keybag_is_frozen(true);
    nigori.set_encrypt_everything(false);
    nigori.set_passphrase_type(sync_pb::NigoriSpecifics::KEYSTORE_PASSPHRASE);
    nigori.set_keystore_migration_time(1);
    encryption_handler()->ApplyNigoriUpdate(nigori, trans.GetWrappedTrans());
    nigori_node.SetNigoriSpecifics(nigori);
  }
  PumpLoop();

  // Verify we're still migrated and have proper encryption state.
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kCurKey);
}

// Trigger a key rotation upon receiving new keys if we already had a keystore
// migrated nigori with the gaia key as the default (still in backwards
// compatible mode).
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysGaiaDefault) {
  // Destroy the existing nigori node so we init without a nigori node.
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();

  const char kOldGaiaKey[] = "old_gaia_key";
  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);
  {
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
      encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                                kRawOldKeystoreKey),
                                            trans.GetWrappedTrans());
  }
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Then init the nigori node with a backwards compatible set of keys.
  CreateRootForType(NIGORI);
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  InitKeystoreMigratedNigori(1, kOldGaiaKey, old_keystore_key);

  // Now set some new keystore keys.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  {
    google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
    keys.Add()->assign(kRawOldKeystoreKey);
    keys.Add()->assign(kRawKeystoreKey);
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(keys,
                                          trans.GetWrappedTrans());
  }
  // Pump for any posted tasks.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Verify we're still migrated and have proper encryption state. We should
  // have rotated the keybag so that it's now encrypted with the newest keystore
  // key (instead of the old gaia key).
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kKeystoreKey);
}

// Trigger a key rotation upon receiving new keys if we already had a keystore
// migrated nigori with the keystore key as the default.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysKeystoreDefault) {
  // Destroy the existing nigori node so we init without a nigori node.
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();

  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);
  {
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
      encryption_handler()->SetKeystoreKeys(BuildEncryptionKeyProto(
                                                kRawOldKeystoreKey),
                                            trans.GetWrappedTrans());
  }
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Then init the nigori node with a non-backwards compatible set of keys.
  CreateRootForType(NIGORI);
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  InitKeystoreMigratedNigori(1, old_keystore_key, old_keystore_key);

  // Now set some new keystore keys.
  EXPECT_CALL(*observer(), OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(), OnEncryptionComplete());
  {
    google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
    keys.Add()->assign(kRawOldKeystoreKey);
    keys.Add()->assign(kRawKeystoreKey);
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(keys,
                                          trans.GetWrappedTrans());
  }
  // Pump for any posted tasks.
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Verify we're still migrated and have proper encryption state. We should
  // have rotated the keybag so that it's now encrypted with the newest keystore
  // key (instead of the old gaia key).
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kKeystoreKey);
}

// Trigger a key rotation upon when a pending gaia passphrase is resolved.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysAfterPendingGaiaResolved) {
  const char kOldGaiaKey[] = "old_gaia_key";
  const char kRawOldKeystoreKey[] = "old_keystore_key";

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _));
  InitUnmigratedNigori(kOldGaiaKey, IMPLICIT_PASSPHRASE);

  {
    // Pass multiple keystore keys, signaling a rotation has happened.
    google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
    keys.Add()->assign(kRawOldKeystoreKey);
    keys.Add()->assign(kRawKeystoreKey);
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(keys,
                                          trans.GetWrappedTrans());
  }
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Resolve the pending keys. This should trigger the key rotation.
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete()).Times(AtLeast(1));
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  encryption_handler()->SetDecryptionPassphrase(kOldGaiaKey);
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(KEYSTORE_PASSPHRASE, encryption_handler()->GetPassphraseType());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kKeystoreKey);
}

// When signing in for the first time, make sure we can rotate keys if we
// already have a keystore migrated nigori.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysGaiaDefaultOnInit) {
  // Destroy the existing nigori node so we init without a nigori node.
  TearDown();
  test_user_share_.SetUp();
  SetUpEncryption();

  const char kOldGaiaKey[] = "old_gaia_key";
  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);

  // Set two keys, signaling that a rotation has been performed. No nigori
  // node is present yet, so we can't rotate.
  {
    google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
    keys.Add()->assign(kRawOldKeystoreKey);
    keys.Add()->assign(kRawKeystoreKey);
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    ReadTransaction trans(FROM_HERE, user_share());
    encryption_handler()->SetKeystoreKeys(keys,
                                          trans.GetWrappedTrans());
  }

  // Then init the nigori node with an old set of keys.
  CreateRootForType(NIGORI);
  EXPECT_CALL(*observer(), OnPassphraseAccepted());
  InitKeystoreMigratedNigori(1, kOldGaiaKey, old_keystore_key);
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Verify we're still migrated and have proper encryption state. We should
  // have rotated the keybag so that it's now encrypted with the newest keystore
  // key (instead of the old gaia key).
  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_TRUE(GetCryptographer()->is_ready());
  EXPECT_EQ(encryption_handler()->GetPassphraseType(), KEYSTORE_PASSPHRASE);
  EXPECT_FALSE(encryption_handler()->EncryptEverythingEnabled());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kKeystoreKey);
}

// Trigger a key rotation when a migrated nigori (with an old keystore key) is
// applied.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysWhenMigratedNigoriArrives) {
  const char kOldGaiaKey[] = "old_gaia_key";
  const char kRawOldKeystoreKey[] = "old_keystore_key";
  std::string old_keystore_key;
  base::Base64Encode(kRawOldKeystoreKey, &old_keystore_key);

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _));
  InitUnmigratedNigori(kOldGaiaKey, IMPLICIT_PASSPHRASE);

  {
    // Pass multiple keystore keys, signaling a rotation has happened.
    google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
    keys.Add()->assign(kRawOldKeystoreKey);
    keys.Add()->assign(kRawKeystoreKey);
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(keys,
                                          trans.GetWrappedTrans());
  }
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Now simulate downloading a nigori node that was migrated before the
  // keys were rotated, and hence still encrypt with the old gaia key.
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnPassphraseTypeChanged(KEYSTORE_PASSPHRASE, _));
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete()).Times(AtLeast(1));
  {
    sync_pb::NigoriSpecifics nigori = BuildMigratedNigori(
        KEYSTORE_PASSPHRASE,
        1,
        kOldGaiaKey,
        old_keystore_key);
    // Update the encryption handler.
    WriteTransaction trans(FROM_HERE, user_share());
    encryption_handler()->ApplyNigoriUpdate(
        nigori,
        trans.GetWrappedTrans());
  }
  EXPECT_FALSE(encryption_handler()->MigratedToKeystore());
  PumpLoop();

  EXPECT_TRUE(encryption_handler()->MigratedToKeystore());
  EXPECT_EQ(KEYSTORE_PASSPHRASE, encryption_handler()->GetPassphraseType());
  VerifyMigratedNigori(KEYSTORE_PASSPHRASE, kKeystoreKey);
}

// Verify that performing a migration while having more than one keystore key
// preserves a custom passphrase.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysUnmigratedCustomPassphrase) {
  const char kCustomPass[] = "custom_passphrase";
  const char kRawOldKeystoreKey[] = "old_keystore_key";

  EXPECT_CALL(*observer(), OnPassphraseRequired(_, _));
  InitUnmigratedNigori(kCustomPass, CUSTOM_PASSPHRASE);

  {
    // Pass multiple keystore keys, signaling a rotation has happened.
    google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
    keys.Add()->assign(kRawOldKeystoreKey);
    keys.Add()->assign(kRawKeystoreKey);
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    encryption_handler()->SetKeystoreKeys(keys,
                                          trans.GetWrappedTrans());
  }
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  // Pass the decryption passphrase. This will also trigger the migration,
  // but should not overwrite the default key.
  EXPECT_CALL(*observer(),
              OnCryptographerStateChanged(_)).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnPassphraseAccepted());
  EXPECT_CALL(*observer(),
              OnEncryptedTypesChanged(_, true));
  EXPECT_CALL(*observer(),
              OnEncryptionComplete()).Times(AnyNumber());
  EXPECT_CALL(*observer(),
              OnBootstrapTokenUpdated(_, PASSPHRASE_BOOTSTRAP_TOKEN));
  encryption_handler()->SetDecryptionPassphrase(kCustomPass);
  Mock::VerifyAndClearExpectations(observer());

  VerifyMigratedNigori(CUSTOM_PASSPHRASE, kCustomPass);
}

// Verify that a key rotation done after we've migrated a custom passphrase
// nigori node preserves the custom passphrase.
TEST_F(SyncEncryptionHandlerImplTest, RotateKeysMigratedCustomPassphrase) {
  const char kCustomPass[] = "custom_passphrase";
  const char kRawOldKeystoreKey[] = "old_keystore_key";

  KeyParams custom_key = {"localhost", "dummy", kCustomPass};
  GetCryptographer()->AddKey(custom_key);

  InitCustomPassMigratedNigori(1, kCustomPass);
  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kCustomPass);

  {
    // Pass multiple keystore keys, signaling a rotation has happened.
    google::protobuf::RepeatedPtrField<google::protobuf::string> keys;
    keys.Add()->assign(kRawOldKeystoreKey);
    keys.Add()->assign(kRawKeystoreKey);
    ReadTransaction trans(FROM_HERE, user_share());
    EXPECT_CALL(*observer(),
                OnBootstrapTokenUpdated(_, KEYSTORE_BOOTSTRAP_TOKEN));
    EXPECT_CALL(*observer(),
                OnCryptographerStateChanged(_)).Times(AnyNumber());
    encryption_handler()->SetKeystoreKeys(keys,
                                          trans.GetWrappedTrans());
  }
  PumpLoop();
  Mock::VerifyAndClearExpectations(observer());

  VerifyMigratedNigoriWithTimestamp(1, CUSTOM_PASSPHRASE, kCustomPass);
}

}  // namespace syncer
