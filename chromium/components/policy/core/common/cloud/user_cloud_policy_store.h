// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_H_
#define COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/policy/core/common/cloud/user_cloud_policy_store_base.h"
#include "components/policy/policy_export.h"

namespace base {
class SequencedTaskRunner;
}

namespace policy {

// Implements a cloud policy store that is stored in a simple file in the user's
// profile directory. This is used on (non-chromeos) platforms that do not have
// a secure storage implementation.
class POLICY_EXPORT UserCloudPolicyStore : public UserCloudPolicyStoreBase {
 public:
  // Creates a policy store associated with a signed-in (or in the progress of
  // it) user.
  UserCloudPolicyStore(
      const base::FilePath& policy_file,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  virtual ~UserCloudPolicyStore();

  // Factory method for creating a UserCloudPolicyStore for a profile with path
  // |profile_path|.
  static scoped_ptr<UserCloudPolicyStore> Create(
      const base::FilePath& profile_path,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // Sets the username from signin for validation of the policy.
  void SetSigninUsername(const std::string& username);

  // Loads policy immediately on the current thread. Virtual for mocks.
  virtual void LoadImmediately();

  // Deletes any existing policy blob and notifies observers via OnStoreLoaded()
  // that the blob has changed. Virtual for mocks.
  virtual void Clear();

  // CloudPolicyStore implementation.
  virtual void Load() OVERRIDE;
  virtual void Store(
      const enterprise_management::PolicyFetchResponse& policy) OVERRIDE;

 protected:
  std::string signin_username_;

 private:
  // Callback invoked when a new policy has been loaded from disk. If
  // |validate_in_background| is true, then policy is validated via a background
  // thread.
  void PolicyLoaded(bool validate_in_background,
                    struct PolicyLoadResult policy_load_result);

  // Starts policy blob validation. |callback| is invoked once validation is
  // complete. If |validate_in_background| is true, then the validation work
  // occurs on a background thread (results are sent back to the calling
  // thread).
  void Validate(
      scoped_ptr<enterprise_management::PolicyFetchResponse> policy,
      bool validate_in_background,
      const UserCloudPolicyValidator::CompletionCallback& callback);

  // Callback invoked to install a just-loaded policy after validation has
  // finished.
  void InstallLoadedPolicyAfterValidation(UserCloudPolicyValidator* validator);

  // Callback invoked to store the policy after validation has finished.
  void StorePolicyAfterValidation(UserCloudPolicyValidator* validator);

  // WeakPtrFactory used to create callbacks for validating and storing policy.
  base::WeakPtrFactory<UserCloudPolicyStore> weak_factory_;

  // Path to file where we store persisted policy.
  base::FilePath backing_file_path_;

  DISALLOW_COPY_AND_ASSIGN(UserCloudPolicyStore);
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CLOUD_USER_CLOUD_POLICY_STORE_H_
