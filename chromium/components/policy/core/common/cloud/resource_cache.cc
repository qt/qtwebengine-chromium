// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/cloud/resource_cache.h"

#include "base/base64.h"
#include "base/callback.h"
#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/logging.h"
#include "base/safe_numerics.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"

namespace policy {

namespace {

// Verifies that |value| is not empty and encodes it into base64url format,
// which is safe to use as a file name on all platforms.
bool Base64Encode(const std::string& value, std::string* encoded) {
  DCHECK(!value.empty());
  if (value.empty())
    return false;
  base::Base64Encode(value, encoded);
  base::ReplaceChars(*encoded, "+", "-", encoded);
  base::ReplaceChars(*encoded, "/", "_", encoded);
  return true;
}

// Decodes all elements of |input| from base64url format and stores the decoded
// elements in |output|.
bool Base64Encode(const std::set<std::string>& input,
                  std::set<std::string>* output) {
  output->clear();
  for (std::set<std::string>::const_iterator it = input.begin();
       it != input.end(); ++it) {
    std::string encoded;
    if (!Base64Encode(*it, &encoded)) {
      output->clear();
      return false;
    }
    output->insert(encoded);
  }
  return true;
}

// Decodes |encoded| from base64url format and verifies that the result is not
// emtpy.
bool Base64Decode(const std::string& encoded, std::string* value) {
  std::string buffer;
  base::ReplaceChars(encoded, "-", "+", &buffer);
  base::ReplaceChars(buffer, "_", "/", &buffer);
  return base::Base64Decode(buffer, value) && !value->empty();
}

}  // namespace

ResourceCache::ResourceCache(
    const base::FilePath& cache_dir,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : cache_dir_(cache_dir),
      task_runner_(task_runner) {
}

ResourceCache::~ResourceCache() {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
}

bool ResourceCache::Store(const std::string& key,
                          const std::string& subkey,
                          const std::string& data) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::FilePath subkey_path;
  // Delete the file before writing to it. This ensures that the write does not
  // follow a symlink planted at |subkey_path|, clobbering a file outside the
  // cache directory. The mechanism is meant to foil file-system-level attacks
  // where a symlink is planted in the cache directory before Chrome has
  // started. An attacker controlling a process running concurrently with Chrome
  // would be able to race against the protection by re-creating the symlink
  // between these two calls. There is nothing in file_util that could be used
  // to protect against such races, especially as the cache is cross-platform
  // and therefore cannot use any POSIX-only tricks.
  int size = base::checked_numeric_cast<int>(data.size());
  return VerifyKeyPathAndGetSubkeyPath(key, true, subkey, &subkey_path) &&
         base::DeleteFile(subkey_path, false) &&
         (file_util::WriteFile(subkey_path, data.data(), size) == size);
}

bool ResourceCache::Load(const std::string& key,
                         const std::string& subkey,
                         std::string* data) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::FilePath subkey_path;
  // Only read from |subkey_path| if it is not a symlink.
  if (!VerifyKeyPathAndGetSubkeyPath(key, false, subkey, &subkey_path) ||
      base::IsLink(subkey_path)) {
    return false;
  }
  data->clear();
  return base::ReadFileToString(subkey_path, data);
}

void ResourceCache::LoadAllSubkeys(
    const std::string& key,
    std::map<std::string, std::string>* contents) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  contents->clear();
  base::FilePath key_path;
  if (!VerifyKeyPath(key, false, &key_path))
    return;

  base::FileEnumerator enumerator(key_path, false, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string encoded_subkey = path.BaseName().MaybeAsASCII();
    std::string subkey;
    std::string data;
    // Only read from |subkey_path| if it is not a symlink and its name is
    // a base64-encoded string.
    if (!base::IsLink(path) &&
        Base64Decode(encoded_subkey, &subkey) &&
        base::ReadFileToString(path, &data)) {
      (*contents)[subkey].swap(data);
    }
  }
}

void ResourceCache::Delete(const std::string& key, const std::string& subkey) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::FilePath subkey_path;
  if (VerifyKeyPathAndGetSubkeyPath(key, false, subkey, &subkey_path))
    base::DeleteFile(subkey_path, false);
  // Delete() does nothing if the directory given to it is not empty. Hence, the
  // call below deletes the directory representing |key| if its last subkey was
  // just removed and does nothing otherwise.
  base::DeleteFile(subkey_path.DirName(), false);
}

void ResourceCache::Clear(const std::string& key) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::FilePath key_path;
  if (VerifyKeyPath(key, false, &key_path))
    base::DeleteFile(key_path, true);
}

void ResourceCache::FilterSubkeys(const std::string& key,
                                  const SubkeyFilter& test) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  base::FilePath key_path;
  if (!VerifyKeyPath(key, false, &key_path))
    return;

  base::FileEnumerator enumerator(key_path, false, base::FileEnumerator::FILES);
  for (base::FilePath subkey_path = enumerator.Next();
       !subkey_path.empty(); subkey_path = enumerator.Next()) {
    std::string subkey;
    // Delete files with invalid names, and files whose subkey doesn't pass the
    // filter.
    if (!Base64Decode(subkey_path.BaseName().MaybeAsASCII(), &subkey) ||
        test.Run(subkey)) {
      base::DeleteFile(subkey_path, true);
    }
  }

  // Delete() does nothing if the directory given to it is not empty. Hence, the
  // call below deletes the directory representing |key| if all of its subkeys
  // were just removed and does nothing otherwise.
  base::DeleteFile(key_path, false);
}

void ResourceCache::PurgeOtherKeys(const std::set<std::string>& keys_to_keep) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  std::set<std::string> encoded_keys_to_keep;
  if (!Base64Encode(keys_to_keep, &encoded_keys_to_keep))
    return;

  base::FileEnumerator enumerator(
      cache_dir_, false, base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string name(path.BaseName().MaybeAsASCII());
    if (encoded_keys_to_keep.find(name) == encoded_keys_to_keep.end())
      base::DeleteFile(path, true);
  }
}

void ResourceCache::PurgeOtherSubkeys(
    const std::string& key,
    const std::set<std::string>& subkeys_to_keep) {
  DCHECK(task_runner_->RunsTasksOnCurrentThread());
  base::FilePath key_path;
  if (!VerifyKeyPath(key, false, &key_path))
    return;

  std::set<std::string> encoded_subkeys_to_keep;
  if (!Base64Encode(subkeys_to_keep, &encoded_subkeys_to_keep))
    return;

  base::FileEnumerator enumerator(key_path, false, base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    const std::string name(path.BaseName().MaybeAsASCII());
    if (encoded_subkeys_to_keep.find(name) == encoded_subkeys_to_keep.end())
      base::DeleteFile(path, false);
  }
  // Delete() does nothing if the directory given to it is not empty. Hence, the
  // call below deletes the directory representing |key| if all of its subkeys
  // were just removed and does nothing otherwise.
  base::DeleteFile(key_path, false);
}

bool ResourceCache::VerifyKeyPath(const std::string& key,
                                  bool allow_create,
                                  base::FilePath* path) {
  std::string encoded;
  if (!Base64Encode(key, &encoded))
    return false;
  *path = cache_dir_.AppendASCII(encoded);
  return allow_create ? base::CreateDirectory(*path) :
                        base::DirectoryExists(*path);
}

bool ResourceCache::VerifyKeyPathAndGetSubkeyPath(const std::string& key,
                                                  bool allow_create_key,
                                                  const std::string& subkey,
                                                  base::FilePath* path) {
  base::FilePath key_path;
  std::string encoded;
  if (!VerifyKeyPath(key, allow_create_key, &key_path) ||
      !Base64Encode(subkey, &encoded)) {
    return false;
  }
  *path = key_path.AppendASCII(encoded);
  return true;
}


}  // namespace policy
