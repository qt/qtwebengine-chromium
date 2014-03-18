// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/user_cloud_policy_store.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "policy/proto/cloud_policy.pb.h"
#include "policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

enum PolicyLoadStatus {
  // Policy blob was successfully loaded and parsed.
  LOAD_RESULT_SUCCESS,

  // No previously stored policy was found.
  LOAD_RESULT_NO_POLICY_FILE,

  // Could not load the previously stored policy due to either a parse or
  // file read error.
  LOAD_RESULT_LOAD_ERROR,
};

// Struct containing the result of a policy load - if |status| ==
// LOAD_RESULT_SUCCESS, |policy| is initialized from the policy file on disk.
struct PolicyLoadResult {
  PolicyLoadStatus status;
  em::PolicyFetchResponse policy;
};

namespace {

// Subdirectory in the user's profile for storing user policies.
const base::FilePath::CharType kPolicyDir[] = FILE_PATH_LITERAL("Policy");
// File in the above directory for storing user policy data.
const base::FilePath::CharType kPolicyCacheFile[] =
    FILE_PATH_LITERAL("User Policy");

// Loads policy from the backing file. Returns a PolicyLoadResult with the
// results of the fetch.
policy::PolicyLoadResult LoadPolicyFromDisk(const base::FilePath& path) {
  policy::PolicyLoadResult result;
  // If the backing file does not exist, just return.
  if (!base::PathExists(path)) {
    result.status = policy::LOAD_RESULT_NO_POLICY_FILE;
    return result;
  }
  std::string data;
  if (!base::ReadFileToString(path, &data) ||
      !result.policy.ParseFromArray(data.c_str(), data.size())) {
    LOG(WARNING) << "Failed to read or parse policy data from " << path.value();
    result.status = policy::LOAD_RESULT_LOAD_ERROR;
    return result;
  }

  result.status = policy::LOAD_RESULT_SUCCESS;
  return result;
}

// Stores policy to the backing file (must be called via a task on
// the background thread).
void StorePolicyToDiskOnBackgroundThread(
    const base::FilePath& path,
    const em::PolicyFetchResponse& policy) {
  DVLOG(1) << "Storing policy to " << path.value();
  std::string data;
  if (!policy.SerializeToString(&data)) {
    DLOG(WARNING) << "Failed to serialize policy data";
    return;
  }

  if (!base::CreateDirectory(path.DirName())) {
    DLOG(WARNING) << "Failed to create directory " << path.DirName().value();
    return;
  }

  int size = data.size();
  if (file_util::WriteFile(path, data.c_str(), size) != size) {
    DLOG(WARNING) << "Failed to write " << path.value();
  }
}

}  // namespace

UserCloudPolicyStore::UserCloudPolicyStore(
    const base::FilePath& path,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : UserCloudPolicyStoreBase(background_task_runner),
      weak_factory_(this),
      backing_file_path_(path) {}

UserCloudPolicyStore::~UserCloudPolicyStore() {}

// static
scoped_ptr<UserCloudPolicyStore> UserCloudPolicyStore::Create(
    const base::FilePath& profile_path,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner) {
  base::FilePath path =
      profile_path.Append(kPolicyDir).Append(kPolicyCacheFile);
  return make_scoped_ptr(
      new UserCloudPolicyStore(path, background_task_runner));
}

void UserCloudPolicyStore::SetSigninUsername(const std::string& username) {
  signin_username_ = username;
}

void UserCloudPolicyStore::LoadImmediately() {
  DVLOG(1) << "Initiating immediate policy load from disk";
  // Cancel any pending Load/Store/Validate operations.
  weak_factory_.InvalidateWeakPtrs();
  // Load the policy from disk...
  PolicyLoadResult result = LoadPolicyFromDisk(backing_file_path_);
  // ...and install it, reporting success/failure to any observers.
  PolicyLoaded(false, result);
}

void UserCloudPolicyStore::Clear() {
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(
          base::IgnoreResult(&base::DeleteFile), backing_file_path_, false));
  policy_.reset();
  policy_map_.Clear();
  NotifyStoreLoaded();
}

void UserCloudPolicyStore::Load() {
  DVLOG(1) << "Initiating policy load from disk";
  // Cancel any pending Load/Store/Validate operations.
  weak_factory_.InvalidateWeakPtrs();

  // Start a new Load operation and have us get called back when it is
  // complete.
  base::PostTaskAndReplyWithResult(
      background_task_runner(),
      FROM_HERE,
      base::Bind(&LoadPolicyFromDisk, backing_file_path_),
      base::Bind(&UserCloudPolicyStore::PolicyLoaded,
                 weak_factory_.GetWeakPtr(), true));
}

void UserCloudPolicyStore::PolicyLoaded(bool validate_in_background,
                                        PolicyLoadResult result) {
  switch (result.status) {
    case LOAD_RESULT_LOAD_ERROR:
      status_ = STATUS_LOAD_ERROR;
      NotifyStoreError();
      break;

    case LOAD_RESULT_NO_POLICY_FILE:
      DVLOG(1) << "No policy found on disk";
      NotifyStoreLoaded();
      break;

    case LOAD_RESULT_SUCCESS: {
      // Found policy on disk - need to validate it before it can be used.
      scoped_ptr<em::PolicyFetchResponse> cloud_policy(
          new em::PolicyFetchResponse(result.policy));
      Validate(cloud_policy.Pass(),
               validate_in_background,
               base::Bind(
                   &UserCloudPolicyStore::InstallLoadedPolicyAfterValidation,
                   weak_factory_.GetWeakPtr()));
      break;
    }
    default:
      NOTREACHED();
  }
}

void UserCloudPolicyStore::InstallLoadedPolicyAfterValidation(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  if (!validator->success()) {
    DVLOG(1) << "Validation failed: status=" << validation_status_;
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  DVLOG(1) << "Validation succeeded - installing policy with dm_token: " <<
      validator->policy_data()->request_token();
  DVLOG(1) << "Device ID: " << validator->policy_data()->device_id();

  InstallPolicy(validator->policy_data().Pass(), validator->payload().Pass());
  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

void UserCloudPolicyStore::Store(const em::PolicyFetchResponse& policy) {
  // Stop any pending requests to store policy, then validate the new policy
  // before storing it.
  weak_factory_.InvalidateWeakPtrs();
  scoped_ptr<em::PolicyFetchResponse> policy_copy(
      new em::PolicyFetchResponse(policy));
  Validate(policy_copy.Pass(),
           true,
           base::Bind(&UserCloudPolicyStore::StorePolicyAfterValidation,
                      weak_factory_.GetWeakPtr()));
}

void UserCloudPolicyStore::Validate(
    scoped_ptr<em::PolicyFetchResponse> policy,
    bool validate_in_background,
    const UserCloudPolicyValidator::CompletionCallback& callback) {
  // Configure the validator.
  scoped_ptr<UserCloudPolicyValidator> validator = CreateValidator(
      policy.Pass(),
      CloudPolicyValidatorBase::TIMESTAMP_NOT_BEFORE);

  // Validate the username if the user is signed in.
  if (!signin_username_.empty())
    validator->ValidateUsername(signin_username_);

  if (validate_in_background) {
    // Start validation in the background. The Validator will free itself once
    // validation is complete.
    validator.release()->StartValidation(callback);
  } else {
    // Run validation immediately and invoke the callback with the results.
    validator->RunValidation();
    callback.Run(validator.get());
  }
}

void UserCloudPolicyStore::StorePolicyAfterValidation(
    UserCloudPolicyValidator* validator) {
  validation_status_ = validator->status();
  DVLOG(1) << "Policy validation complete: status = " << validation_status_;
  if (!validator->success()) {
    status_ = STATUS_VALIDATION_ERROR;
    NotifyStoreError();
    return;
  }

  // Persist the validated policy (just fire a task - don't bother getting a
  // reply because we can't do anything if it fails).
  background_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&StorePolicyToDiskOnBackgroundThread,
                 backing_file_path_, *validator->policy()));
  InstallPolicy(validator->policy_data().Pass(), validator->payload().Pass());
  status_ = STATUS_OK;
  NotifyStoreLoaded();
}

}  // namespace policy
