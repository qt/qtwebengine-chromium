// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/sandbox_prioritized_origin_database.h"

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_platform_file_closer.h"
#include "base/logging.h"
#include "base/pickle.h"
#include "base/platform_file.h"
#include "webkit/browser/fileapi/sandbox_isolated_origin_database.h"
#include "webkit/browser/fileapi/sandbox_origin_database.h"

namespace fileapi {

namespace {

const base::FilePath::CharType kPrimaryDirectory[] =
    FILE_PATH_LITERAL("primary");
const base::FilePath::CharType kPrimaryOriginFile[] =
    FILE_PATH_LITERAL("primary.origin");

bool WritePrimaryOriginFile(const base::FilePath& path,
                            const std::string& origin) {
  base::PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
  bool created;
  base::PlatformFile file = base::CreatePlatformFile(
      path,
      base::PLATFORM_FILE_OPEN_ALWAYS |
      base::PLATFORM_FILE_WRITE,
      &created, &error);
  base::ScopedPlatformFileCloser closer(&file);
  if (error != base::PLATFORM_FILE_OK ||
      file == base::kInvalidPlatformFileValue)
    return false;
  base::TruncatePlatformFile(file, 0);
  Pickle pickle;
  pickle.WriteString(origin);
  base::WritePlatformFile(file, 0, static_cast<const char*>(pickle.data()),
                          pickle.size());
  base::FlushPlatformFile(file);
  return true;
}

bool ReadPrimaryOriginFile(const base::FilePath& path,
                           std::string* origin) {
  std::string buffer;
  if (!base::ReadFileToString(path, &buffer))
    return false;
  Pickle pickle(buffer.data(), buffer.size());
  PickleIterator iter(pickle);
  return pickle.ReadString(&iter, origin) && !origin->empty();
}

}  // namespace

SandboxPrioritizedOriginDatabase::SandboxPrioritizedOriginDatabase(
    const base::FilePath& file_system_directory)
    : file_system_directory_(file_system_directory),
      primary_origin_file_(
          file_system_directory_.Append(kPrimaryOriginFile)) {
}

SandboxPrioritizedOriginDatabase::~SandboxPrioritizedOriginDatabase() {
}

bool SandboxPrioritizedOriginDatabase::InitializePrimaryOrigin(
    const std::string& origin) {
  if (!primary_origin_database_) {
    if (!MaybeLoadPrimaryOrigin() && ResetPrimaryOrigin(origin)) {
      MaybeMigrateDatabase(origin);
      primary_origin_database_.reset(
          new SandboxIsolatedOriginDatabase(
              origin,
              file_system_directory_,
              base::FilePath(kPrimaryDirectory)));
      return true;
    }
  }

  if (primary_origin_database_)
    return primary_origin_database_->HasOriginPath(origin);

  return false;
}

std::string SandboxPrioritizedOriginDatabase::GetPrimaryOrigin() {
  MaybeLoadPrimaryOrigin();
  if (primary_origin_database_)
    return primary_origin_database_->origin();
  return std::string();
}

bool SandboxPrioritizedOriginDatabase::HasOriginPath(
    const std::string& origin) {
  MaybeInitializeDatabases(false);
  if (primary_origin_database_ &&
      primary_origin_database_->HasOriginPath(origin))
    return true;
  if (origin_database_)
    return origin_database_->HasOriginPath(origin);
  return false;
}

bool SandboxPrioritizedOriginDatabase::GetPathForOrigin(
    const std::string& origin, base::FilePath* directory) {
  MaybeInitializeDatabases(true);
  if (primary_origin_database_ &&
      primary_origin_database_->GetPathForOrigin(origin, directory))
    return true;
  DCHECK(origin_database_);
  return origin_database_->GetPathForOrigin(origin, directory);
}

bool SandboxPrioritizedOriginDatabase::RemovePathForOrigin(
    const std::string& origin) {
  MaybeInitializeDatabases(false);
  if (primary_origin_database_ &&
      primary_origin_database_->HasOriginPath(origin)) {
    primary_origin_database_.reset();
    base::DeleteFile(file_system_directory_.Append(kPrimaryOriginFile),
                     true /* recursive */);
    return true;
  }
  if (origin_database_)
    return origin_database_->RemovePathForOrigin(origin);
  return true;
}

bool SandboxPrioritizedOriginDatabase::ListAllOrigins(
    std::vector<OriginRecord>* origins) {
  // SandboxOriginDatabase may clear the |origins|, so call this before
  // primary_origin_database_.
  MaybeInitializeDatabases(false);
  if (origin_database_ && !origin_database_->ListAllOrigins(origins))
    return false;
  if (primary_origin_database_)
    return primary_origin_database_->ListAllOrigins(origins);
  return true;
}

void SandboxPrioritizedOriginDatabase::DropDatabase() {
  primary_origin_database_.reset();
  origin_database_.reset();
}

bool SandboxPrioritizedOriginDatabase::MaybeLoadPrimaryOrigin() {
  if (primary_origin_database_)
    return true;
  std::string saved_origin;
  if (!ReadPrimaryOriginFile(primary_origin_file_, &saved_origin))
    return false;
  primary_origin_database_.reset(
      new SandboxIsolatedOriginDatabase(
          saved_origin,
          file_system_directory_,
          base::FilePath(kPrimaryDirectory)));
  return true;
}

bool SandboxPrioritizedOriginDatabase::ResetPrimaryOrigin(
    const std::string& origin) {
  DCHECK(!primary_origin_database_);
  if (!WritePrimaryOriginFile(primary_origin_file_, origin))
    return false;
  // We reset the primary origin directory too.
  // (This means the origin file corruption causes data loss
  // We could keep the directory there as the same origin will likely
  // become the primary origin, but let's play conservatively.)
  base::DeleteFile(file_system_directory_.Append(kPrimaryDirectory),
                   true /* recursive */);
  return true;
}

void SandboxPrioritizedOriginDatabase::MaybeMigrateDatabase(
    const std::string& origin) {
  MaybeInitializeNonPrimaryDatabase(false);
  if (!origin_database_)
    return;
  if (origin_database_->HasOriginPath(origin)) {
    base::FilePath directory_name;
    if (origin_database_->GetPathForOrigin(origin, &directory_name) &&
        directory_name != base::FilePath(kPrimaryOriginFile)) {
      base::FilePath from_path = file_system_directory_.Append(directory_name);
      base::FilePath to_path = file_system_directory_.Append(kPrimaryDirectory);

      if (base::PathExists(to_path))
        base::DeleteFile(to_path, true /* recursive */);
      base::Move(from_path, to_path);
    }

    origin_database_->RemovePathForOrigin(origin);
  }

  std::vector<OriginRecord> origins;
  origin_database_->ListAllOrigins(&origins);
  if (origins.empty()) {
    origin_database_->RemoveDatabase();
    origin_database_.reset();
  }
}

void SandboxPrioritizedOriginDatabase::MaybeInitializeDatabases(
    bool create) {
  MaybeLoadPrimaryOrigin();
  MaybeInitializeNonPrimaryDatabase(create);
}

void SandboxPrioritizedOriginDatabase::MaybeInitializeNonPrimaryDatabase(
    bool create) {
  if (origin_database_)
    return;

  origin_database_.reset(new SandboxOriginDatabase(file_system_directory_));
  if (!create && !base::DirectoryExists(origin_database_->GetDatabasePath())) {
    origin_database_.reset();
    return;
  }
}

SandboxOriginDatabase*
SandboxPrioritizedOriginDatabase::GetSandboxOriginDatabase() {
  MaybeInitializeNonPrimaryDatabase(true);
  return origin_database_.get();
}

}  // namespace fileapi
