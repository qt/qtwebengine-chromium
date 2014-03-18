// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/obfuscated_file_util.h"

#include <queue>
#include <string>
#include <vector>

#include "base/file_util.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "url/gurl.h"
#include "webkit/browser/fileapi/file_observers.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/browser/fileapi/native_file_util.h"
#include "webkit/browser/fileapi/sandbox_file_system_backend.h"
#include "webkit/browser/fileapi/sandbox_isolated_origin_database.h"
#include "webkit/browser/fileapi/sandbox_origin_database.h"
#include "webkit/browser/fileapi/sandbox_prioritized_origin_database.h"
#include "webkit/browser/fileapi/timed_task_helper.h"
#include "webkit/browser/quota/quota_manager.h"
#include "webkit/common/database/database_identifier.h"
#include "webkit/common/fileapi/file_system_util.h"

// Example of various paths:
//   void ObfuscatedFileUtil::DoSomething(const FileSystemURL& url) {
//     base::FilePath virtual_path = url.path();
//     base::FilePath local_path = GetLocalFilePath(url);
//
//     NativeFileUtil::DoSomething(local_path);
//     file_util::DoAnother(local_path);
//  }

namespace fileapi {

namespace {

typedef SandboxDirectoryDatabase::FileId FileId;
typedef SandboxDirectoryDatabase::FileInfo FileInfo;

void InitFileInfo(
    SandboxDirectoryDatabase::FileInfo* file_info,
    SandboxDirectoryDatabase::FileId parent_id,
    const base::FilePath::StringType& file_name) {
  DCHECK(file_info);
  file_info->parent_id = parent_id;
  file_info->name = file_name;
}

// Costs computed as per crbug.com/86114, based on the LevelDB implementation of
// path storage under Linux.  It's not clear if that will differ on Windows, on
// which base::FilePath uses wide chars [since they're converted to UTF-8 for
// storage anyway], but as long as the cost is high enough that one can't cheat
// on quota by storing data in paths, it doesn't need to be all that accurate.
const int64 kPathCreationQuotaCost = 146;  // Bytes per inode, basically.
const int64 kPathByteQuotaCost = 2;  // Bytes per byte of path length in UTF-8.

int64 UsageForPath(size_t length) {
  return kPathCreationQuotaCost +
      static_cast<int64>(length) * kPathByteQuotaCost;
}

bool AllocateQuota(FileSystemOperationContext* context, int64 growth) {
  if (context->allowed_bytes_growth() == quota::QuotaManager::kNoLimit)
    return true;

  int64 new_quota = context->allowed_bytes_growth() - growth;
  if (growth > 0 && new_quota < 0)
    return false;
  context->set_allowed_bytes_growth(new_quota);
  return true;
}

void UpdateUsage(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 growth) {
  context->update_observers()->Notify(
      &FileUpdateObserver::OnUpdate, MakeTuple(url, growth));
}

void TouchDirectory(SandboxDirectoryDatabase* db, FileId dir_id) {
  DCHECK(db);
  if (!db->UpdateModificationTime(dir_id, base::Time::Now()))
    NOTREACHED();
}

enum IsolatedOriginStatus {
  kIsolatedOriginMatch,
  kIsolatedOriginDontMatch,
  kIsolatedOriginStatusMax,
};

}  // namespace

using base::PlatformFile;
using base::PlatformFileError;

class ObfuscatedFileEnumerator
    : public FileSystemFileUtil::AbstractFileEnumerator {
 public:
  ObfuscatedFileEnumerator(
      SandboxDirectoryDatabase* db,
      FileSystemOperationContext* context,
      ObfuscatedFileUtil* obfuscated_file_util,
      const FileSystemURL& root_url,
      bool recursive)
      : db_(db),
        context_(context),
        obfuscated_file_util_(obfuscated_file_util),
        root_url_(root_url),
        recursive_(recursive),
        current_file_id_(0) {
    base::FilePath root_virtual_path = root_url.path();
    FileId file_id;

    if (!db_->GetFileWithPath(root_virtual_path, &file_id))
      return;

    FileRecord record = { file_id, root_virtual_path };
    recurse_queue_.push(record);
  }

  virtual ~ObfuscatedFileEnumerator() {}

  virtual base::FilePath Next() OVERRIDE {
    ProcessRecurseQueue();
    if (display_stack_.empty())
      return base::FilePath();

    current_file_id_ = display_stack_.back();
    display_stack_.pop_back();

    FileInfo file_info;
    base::FilePath platform_file_path;
    base::PlatformFileError error =
        obfuscated_file_util_->GetFileInfoInternal(
            db_, context_, root_url_, current_file_id_,
            &file_info, &current_platform_file_info_, &platform_file_path);
    if (error != base::PLATFORM_FILE_OK)
      return Next();

    base::FilePath virtual_path =
        current_parent_virtual_path_.Append(file_info.name);
    if (recursive_ && file_info.is_directory()) {
      FileRecord record = { current_file_id_, virtual_path };
      recurse_queue_.push(record);
    }
    return virtual_path;
  }

  virtual int64 Size() OVERRIDE {
    return current_platform_file_info_.size;
  }

  virtual base::Time LastModifiedTime() OVERRIDE {
    return current_platform_file_info_.last_modified;
  }

  virtual bool IsDirectory() OVERRIDE {
    return current_platform_file_info_.is_directory;
  }

 private:
  typedef SandboxDirectoryDatabase::FileId FileId;
  typedef SandboxDirectoryDatabase::FileInfo FileInfo;

  struct FileRecord {
    FileId file_id;
    base::FilePath virtual_path;
  };

  void ProcessRecurseQueue() {
    while (display_stack_.empty() && !recurse_queue_.empty()) {
      FileRecord entry = recurse_queue_.front();
      recurse_queue_.pop();
      if (!db_->ListChildren(entry.file_id, &display_stack_)) {
        display_stack_.clear();
        return;
      }
      current_parent_virtual_path_ = entry.virtual_path;
    }
  }

  SandboxDirectoryDatabase* db_;
  FileSystemOperationContext* context_;
  ObfuscatedFileUtil* obfuscated_file_util_;
  FileSystemURL root_url_;
  bool recursive_;

  std::queue<FileRecord> recurse_queue_;
  std::vector<FileId> display_stack_;
  base::FilePath current_parent_virtual_path_;

  FileId current_file_id_;
  base::PlatformFileInfo current_platform_file_info_;
};

class ObfuscatedOriginEnumerator
    : public ObfuscatedFileUtil::AbstractOriginEnumerator {
 public:
  typedef SandboxOriginDatabase::OriginRecord OriginRecord;
  ObfuscatedOriginEnumerator(
      SandboxOriginDatabaseInterface* origin_database,
      const base::FilePath& base_file_path)
      : base_file_path_(base_file_path) {
    if (origin_database)
      origin_database->ListAllOrigins(&origins_);
  }

  virtual ~ObfuscatedOriginEnumerator() {}

  // Returns the next origin.  Returns empty if there are no more origins.
  virtual GURL Next() OVERRIDE {
    OriginRecord record;
    if (!origins_.empty()) {
      record = origins_.back();
      origins_.pop_back();
    }
    current_ = record;
    return webkit_database::GetOriginFromIdentifier(record.origin);
  }

  // Returns the current origin's information.
  virtual bool HasTypeDirectory(const std::string& type_string) const OVERRIDE {
    if (current_.path.empty())
      return false;
    if (type_string.empty()) {
      NOTREACHED();
      return false;
    }
    base::FilePath path =
        base_file_path_.Append(current_.path).AppendASCII(type_string);
    return base::DirectoryExists(path);
  }

 private:
  std::vector<OriginRecord> origins_;
  OriginRecord current_;
  base::FilePath base_file_path_;
};

ObfuscatedFileUtil::ObfuscatedFileUtil(
    quota::SpecialStoragePolicy* special_storage_policy,
    const base::FilePath& file_system_directory,
    base::SequencedTaskRunner* file_task_runner,
    const GetTypeStringForURLCallback& get_type_string_for_url,
    const std::set<std::string>& known_type_strings,
    SandboxFileSystemBackendDelegate* sandbox_delegate)
    : special_storage_policy_(special_storage_policy),
      file_system_directory_(file_system_directory),
      db_flush_delay_seconds_(10 * 60),  // 10 mins.
      file_task_runner_(file_task_runner),
      get_type_string_for_url_(get_type_string_for_url),
      known_type_strings_(known_type_strings),
      sandbox_delegate_(sandbox_delegate) {
}

ObfuscatedFileUtil::~ObfuscatedFileUtil() {
  DropDatabases();
}

PlatformFileError ObfuscatedFileUtil::CreateOrOpen(
    FileSystemOperationContext* context,
    const FileSystemURL& url, int file_flags,
    PlatformFile* file_handle, bool* created) {
  PlatformFileError error = CreateOrOpenInternal(context, url, file_flags,
                                                 file_handle, created);
  if (*file_handle != base::kInvalidPlatformFileValue &&
      file_flags & base::PLATFORM_FILE_WRITE &&
      context->quota_limit_type() == quota::kQuotaLimitTypeUnlimited &&
      sandbox_delegate_) {
    DCHECK_EQ(base::PLATFORM_FILE_OK, error);
    sandbox_delegate_->StickyInvalidateUsageCache(url.origin(), url.type());
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::Close(
    FileSystemOperationContext* context,
    base::PlatformFile file) {
  return NativeFileUtil::Close(file);
}

PlatformFileError ObfuscatedFileUtil::EnsureFileExists(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool* created) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId file_id;
  if (db->GetFileWithPath(url.path(), &file_id)) {
    FileInfo file_info;
    if (!db->GetFileInfo(file_id, &file_info)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    if (file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_NOT_A_FILE;
    if (created)
      *created = false;
    return base::PLATFORM_FILE_OK;
  }
  FileId parent_id;
  if (!db->GetFileWithPath(VirtualPath::DirName(url.path()), &parent_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  InitFileInfo(&file_info, parent_id,
               VirtualPath::BaseName(url.path()).value());

  int64 growth = UsageForPath(file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;
  PlatformFileError error = CreateFile(
      context, base::FilePath(), url, &file_info, 0, NULL);
  if (created && base::PLATFORM_FILE_OK == error) {
    *created = true;
    UpdateUsage(context, url, growth);
    context->change_observers()->Notify(
        &FileChangeObserver::OnCreateFile, MakeTuple(url));
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::CreateDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId file_id;
  if (db->GetFileWithPath(url.path(), &file_id)) {
    FileInfo file_info;
    if (exclusive)
      return base::PLATFORM_FILE_ERROR_EXISTS;
    if (!db->GetFileInfo(file_id, &file_info)) {
      NOTREACHED();
      return base::PLATFORM_FILE_ERROR_FAILED;
    }
    if (!file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
    return base::PLATFORM_FILE_OK;
  }

  std::vector<base::FilePath::StringType> components;
  VirtualPath::GetComponents(url.path(), &components);
  FileId parent_id = 0;
  size_t index;
  for (index = 0; index < components.size(); ++index) {
    base::FilePath::StringType name = components[index];
    if (name == FILE_PATH_LITERAL("/"))
      continue;
    if (!db->GetChildWithName(parent_id, name, &parent_id))
      break;
  }
  if (!db->IsDirectory(parent_id))
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  if (!recursive && components.size() - index > 1)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  bool first = true;
  for (; index < components.size(); ++index) {
    FileInfo file_info;
    file_info.name = components[index];
    if (file_info.name == FILE_PATH_LITERAL("/"))
      continue;
    file_info.modification_time = base::Time::Now();
    file_info.parent_id = parent_id;
    int64 growth = UsageForPath(file_info.name.size());
    if (!AllocateQuota(context, growth))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    base::PlatformFileError error = db->AddFileInfo(file_info, &parent_id);
    if (error != base::PLATFORM_FILE_OK)
      return error;
    UpdateUsage(context, url, growth);
    context->change_observers()->Notify(
        &FileChangeObserver::OnCreateDirectory, MakeTuple(url));
    if (first) {
      first = false;
      TouchDirectory(db, file_info.parent_id);
    }
  }
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::GetFileInfo(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_file_path) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo local_info;
  return GetFileInfoInternal(db, context, url,
                             file_id, &local_info,
                             file_info, platform_file_path);
}

scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
    ObfuscatedFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemURL& root_url) {
  return CreateFileEnumerator(context, root_url, false /* recursive */);
}

PlatformFileError ObfuscatedFileUtil::GetLocalFilePath(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::FilePath* local_path) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info) || file_info.is_directory()) {
    NOTREACHED();
    // Directories have no local file path.
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  *local_path = DataPathToLocalPath(url, file_info.data_path);

  if (local_path->empty())
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::Touch(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, false);
  if (!db)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (file_info.is_directory()) {
    if (!db->UpdateModificationTime(file_id, last_modified_time))
      return base::PLATFORM_FILE_ERROR_FAILED;
    return base::PLATFORM_FILE_OK;
  }
  return NativeFileUtil::Touch(
      DataPathToLocalPath(url, file_info.data_path),
      last_access_time, last_modified_time);
}

PlatformFileError ObfuscatedFileUtil::Truncate(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    int64 length) {
  base::PlatformFileInfo file_info;
  base::FilePath local_path;
  base::PlatformFileError error =
      GetFileInfo(context, url, &file_info, &local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  int64 growth = length - file_info.size;
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;
  error = NativeFileUtil::Truncate(local_path, length);
  if (error == base::PLATFORM_FILE_OK) {
    UpdateUsage(context, url, growth);
    context->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile, MakeTuple(url));
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::CopyOrMoveFile(
    FileSystemOperationContext* context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    bool copy) {
  // Cross-filesystem copies and moves should be handled via CopyInForeignFile.
  DCHECK(src_url.origin() == dest_url.origin());
  DCHECK(src_url.type() == dest_url.type());

  SandboxDirectoryDatabase* db = GetDirectoryDatabase(src_url, true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId src_file_id;
  if (!db->GetFileWithPath(src_url.path(), &src_file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_url.path(),
                                       &dest_file_id);

  FileInfo src_file_info;
  base::PlatformFileInfo src_platform_file_info;
  base::FilePath src_local_path;
  base::PlatformFileError error = GetFileInfoInternal(
      db, context, src_url, src_file_id,
      &src_file_info, &src_platform_file_info, &src_local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (src_file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  FileInfo dest_file_info;
  base::PlatformFileInfo dest_platform_file_info;  // overwrite case only
  base::FilePath dest_local_path;  // overwrite case only
  if (overwrite) {
    base::PlatformFileError error = GetFileInfoInternal(
        db, context, dest_url, dest_file_id,
        &dest_file_info, &dest_platform_file_info, &dest_local_path);
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      overwrite = false;  // fallback to non-overwrite case
    else if (error != base::PLATFORM_FILE_OK)
      return error;
    else if (dest_file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  }
  if (!overwrite) {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(dest_url.path()),
                             &dest_parent_id)) {
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }

    dest_file_info = src_file_info;
    dest_file_info.parent_id = dest_parent_id;
    dest_file_info.name =
        VirtualPath::BaseName(dest_url.path()).value();
  }

  int64 growth = 0;
  if (copy)
    growth += src_platform_file_info.size;
  else
    growth -= UsageForPath(src_file_info.name.size());
  if (overwrite)
    growth -= dest_platform_file_info.size;
  else
    growth += UsageForPath(dest_file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;

  /*
   * Copy-with-overwrite
   *  Just overwrite data file
   * Copy-without-overwrite
   *  Copy backing file
   *  Create new metadata pointing to new backing file.
   * Move-with-overwrite
   *  transaction:
   *    Remove source entry.
   *    Point target entry to source entry's backing file.
   *  Delete target entry's old backing file
   * Move-without-overwrite
   *  Just update metadata
   */
  error = base::PLATFORM_FILE_ERROR_FAILED;
  if (copy) {
    if (overwrite) {
      error = NativeFileUtil::CopyOrMoveFile(
          src_local_path,
          dest_local_path,
          option,
          fileapi::NativeFileUtil::CopyOrMoveModeForDestination(
              dest_url, true /* copy */));
    } else {  // non-overwrite
      error = CreateFile(context, src_local_path,
                         dest_url, &dest_file_info, 0, NULL);
    }
  } else {
    if (overwrite) {
      if (db->OverwritingMoveFile(src_file_id, dest_file_id)) {
        if (base::PLATFORM_FILE_OK !=
            NativeFileUtil::DeleteFile(dest_local_path))
          LOG(WARNING) << "Leaked a backing file.";
        error = base::PLATFORM_FILE_OK;
      } else {
        error = base::PLATFORM_FILE_ERROR_FAILED;
      }
    } else {  // non-overwrite
      if (db->UpdateFileInfo(src_file_id, dest_file_info))
        error = base::PLATFORM_FILE_OK;
      else
        error = base::PLATFORM_FILE_ERROR_FAILED;
    }
  }

  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (overwrite) {
    context->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile,
        MakeTuple(dest_url));
  } else {
    context->change_observers()->Notify(
        &FileChangeObserver::OnCreateFileFrom,
        MakeTuple(dest_url, src_url));
  }

  if (!copy) {
    context->change_observers()->Notify(
        &FileChangeObserver::OnRemoveFile, MakeTuple(src_url));
    TouchDirectory(db, src_file_info.parent_id);
  }

  TouchDirectory(db, dest_file_info.parent_id);

  UpdateUsage(context, dest_url, growth);
  return error;
}

PlatformFileError ObfuscatedFileUtil::CopyInForeignFile(
    FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(dest_url, true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  base::PlatformFileInfo src_platform_file_info;
  if (!base::GetFileInfo(src_file_path, &src_platform_file_info))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileId dest_file_id;
  bool overwrite = db->GetFileWithPath(dest_url.path(),
                                       &dest_file_id);

  FileInfo dest_file_info;
  base::PlatformFileInfo dest_platform_file_info;  // overwrite case only
  if (overwrite) {
    base::FilePath dest_local_path;
    base::PlatformFileError error = GetFileInfoInternal(
        db, context, dest_url, dest_file_id,
        &dest_file_info, &dest_platform_file_info, &dest_local_path);
    if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
      overwrite = false;  // fallback to non-overwrite case
    else if (error != base::PLATFORM_FILE_OK)
      return error;
    else if (dest_file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  }
  if (!overwrite) {
    FileId dest_parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(dest_url.path()),
                             &dest_parent_id)) {
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    if (!dest_file_info.is_directory())
      return base::PLATFORM_FILE_ERROR_FAILED;
    InitFileInfo(&dest_file_info, dest_parent_id,
                 VirtualPath::BaseName(dest_url.path()).value());
  }

  int64 growth = src_platform_file_info.size;
  if (overwrite)
    growth -= dest_platform_file_info.size;
  else
    growth += UsageForPath(dest_file_info.name.size());
  if (!AllocateQuota(context, growth))
    return base::PLATFORM_FILE_ERROR_NO_SPACE;

  base::PlatformFileError error;
  if (overwrite) {
    base::FilePath dest_local_path =
        DataPathToLocalPath(dest_url, dest_file_info.data_path);
    error = NativeFileUtil::CopyOrMoveFile(
        src_file_path, dest_local_path,
        FileSystemOperation::OPTION_NONE,
        fileapi::NativeFileUtil::CopyOrMoveModeForDestination(dest_url,
                                                              true /* copy */));
  } else {
    error = CreateFile(context, src_file_path,
                       dest_url, &dest_file_info, 0, NULL);
  }

  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (overwrite) {
    context->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile, MakeTuple(dest_url));
  } else {
    context->change_observers()->Notify(
        &FileChangeObserver::OnCreateFile, MakeTuple(dest_url));
  }

  UpdateUsage(context, dest_url, growth);
  TouchDirectory(db, dest_file_info.parent_id);
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::DeleteFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  FileInfo file_info;
  base::PlatformFileInfo platform_file_info;
  base::FilePath local_path;
  base::PlatformFileError error = GetFileInfoInternal(
      db, context, url, file_id, &file_info, &platform_file_info, &local_path);
  if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND &&
      error != base::PLATFORM_FILE_OK)
    return error;

  if (file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  int64 growth = -UsageForPath(file_info.name.size()) - platform_file_info.size;
  AllocateQuota(context, growth);
  if (!db->RemoveFileInfo(file_id)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  UpdateUsage(context, url, growth);
  TouchDirectory(db, file_info.parent_id);

  context->change_observers()->Notify(
      &FileChangeObserver::OnRemoveFile, MakeTuple(url));

  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    return base::PLATFORM_FILE_OK;

  error = NativeFileUtil::DeleteFile(local_path);
  if (base::PLATFORM_FILE_OK != error)
    LOG(WARNING) << "Leaked a backing file.";
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::DeleteDirectory(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;

  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }
  if (!file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
  if (!db->RemoveFileInfo(file_id))
    return base::PLATFORM_FILE_ERROR_NOT_EMPTY;
  int64 growth = -UsageForPath(file_info.name.size());
  AllocateQuota(context, growth);
  UpdateUsage(context, url, growth);
  TouchDirectory(db, file_info.parent_id);
  context->change_observers()->Notify(
      &FileChangeObserver::OnRemoveDirectory, MakeTuple(url));
  return base::PLATFORM_FILE_OK;
}

webkit_blob::ScopedFile ObfuscatedFileUtil::CreateSnapshotFile(
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::PlatformFileError* error,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_path) {
  // We're just returning the local file information.
  *error = GetFileInfo(context, url, file_info, platform_path);
  if (*error == base::PLATFORM_FILE_OK && file_info->is_directory) {
    *file_info = base::PlatformFileInfo();
    *error = base::PLATFORM_FILE_ERROR_NOT_A_FILE;
  }
  return webkit_blob::ScopedFile();
}

scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator>
    ObfuscatedFileUtil::CreateFileEnumerator(
    FileSystemOperationContext* context,
    const FileSystemURL& root_url,
    bool recursive) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(root_url, false);
  if (!db) {
    return scoped_ptr<AbstractFileEnumerator>(new EmptyFileEnumerator());
  }
  return scoped_ptr<AbstractFileEnumerator>(
      new ObfuscatedFileEnumerator(db, context, this, root_url, recursive));
}

bool ObfuscatedFileUtil::IsDirectoryEmpty(
    FileSystemOperationContext* context,
    const FileSystemURL& url) {
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, false);
  if (!db)
    return true;  // Not a great answer, but it's what others do.
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id))
    return true;  // Ditto.
  FileInfo file_info;
  if (!db->GetFileInfo(file_id, &file_info)) {
    DCHECK(!file_id);
    // It's the root directory and the database hasn't been initialized yet.
    return true;
  }
  if (!file_info.is_directory())
    return true;
  std::vector<FileId> children;
  // TODO(ericu): This could easily be made faster with help from the database.
  if (!db->ListChildren(file_id, &children))
    return true;
  return children.empty();
}

base::FilePath ObfuscatedFileUtil::GetDirectoryForOriginAndType(
    const GURL& origin,
    const std::string& type_string,
    bool create,
    base::PlatformFileError* error_code) {
  base::FilePath origin_dir = GetDirectoryForOrigin(origin, create, error_code);
  if (origin_dir.empty())
    return base::FilePath();
  if (type_string.empty())
    return origin_dir;
  base::FilePath path = origin_dir.AppendASCII(type_string);
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  if (!base::DirectoryExists(path) &&
      (!create || !base::CreateDirectory(path))) {
    error = create ?
          base::PLATFORM_FILE_ERROR_FAILED :
          base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }

  if (error_code)
    *error_code = error;
  return path;
}

bool ObfuscatedFileUtil::DeleteDirectoryForOriginAndType(
    const GURL& origin,
    const std::string& type_string) {
  base::PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath origin_type_path = GetDirectoryForOriginAndType(
      origin, type_string, false, &error);
  if (origin_type_path.empty())
    return true;
  if (error != base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    // TODO(dmikurube): Consider the return value of DestroyDirectoryDatabase.
    // We ignore its error now since 1) it doesn't matter the final result, and
    // 2) it always returns false in Windows because of LevelDB's
    // implementation.
    // Information about failure would be useful for debugging.
    if (!type_string.empty())
      DestroyDirectoryDatabase(origin, type_string);
    if (!base::DeleteFile(origin_type_path, true /* recursive */))
      return false;
  }

  base::FilePath origin_path = VirtualPath::DirName(origin_type_path);
  DCHECK_EQ(origin_path.value(),
            GetDirectoryForOrigin(origin, false, NULL).value());

  if (!type_string.empty()) {
    // At this point we are sure we had successfully deleted the origin/type
    // directory (i.e. we're ready to just return true).
    // See if we have other directories in this origin directory.
    for (std::set<std::string>::iterator iter = known_type_strings_.begin();
         iter != known_type_strings_.end();
         ++iter) {
      if (*iter == type_string)
        continue;
      if (base::DirectoryExists(origin_path.AppendASCII(*iter))) {
        // Other type's directory exists; just return true here.
        return true;
      }
    }
  }

  // No other directories seem exist. Try deleting the entire origin directory.
  InitOriginDatabase(origin, false);
  if (origin_database_) {
    origin_database_->RemovePathForOrigin(
        webkit_database::GetIdentifierFromOrigin(origin));
  }
  if (!base::DeleteFile(origin_path, true /* recursive */))
    return false;

  return true;
}

ObfuscatedFileUtil::AbstractOriginEnumerator*
ObfuscatedFileUtil::CreateOriginEnumerator() {
  std::vector<SandboxOriginDatabase::OriginRecord> origins;

  InitOriginDatabase(GURL(), false);
  return new ObfuscatedOriginEnumerator(
      origin_database_.get(), file_system_directory_);
}

bool ObfuscatedFileUtil::DestroyDirectoryDatabase(
    const GURL& origin,
    const std::string& type_string) {
  std::string key = GetDirectoryDatabaseKey(origin, type_string);
  if (key.empty())
    return true;
  DirectoryMap::iterator iter = directories_.find(key);
  if (iter != directories_.end()) {
    SandboxDirectoryDatabase* database = iter->second;
    directories_.erase(iter);
    delete database;
  }

  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath path = GetDirectoryForOriginAndType(
      origin, type_string, false, &error);
  if (path.empty() || error == base::PLATFORM_FILE_ERROR_NOT_FOUND)
    return true;
  return SandboxDirectoryDatabase::DestroyDatabase(path);
}

// static
int64 ObfuscatedFileUtil::ComputeFilePathCost(const base::FilePath& path) {
  return UsageForPath(VirtualPath::BaseName(path).value().size());
}

void ObfuscatedFileUtil::MaybePrepopulateDatabase(
    const std::vector<std::string>& type_strings_to_prepopulate) {
  SandboxPrioritizedOriginDatabase database(file_system_directory_);
  std::string origin_string = database.GetPrimaryOrigin();
  if (origin_string.empty() || !database.HasOriginPath(origin_string))
    return;
  const GURL origin = webkit_database::GetOriginFromIdentifier(origin_string);

  // Prepopulate the directory database(s) if and only if this instance
  // has primary origin and the directory database is already there.
  for (size_t i = 0; i < type_strings_to_prepopulate.size(); ++i) {
    const std::string type_string = type_strings_to_prepopulate[i];
    // Only handles known types.
    if (!ContainsKey(known_type_strings_, type_string))
      continue;
    PlatformFileError error = base::PLATFORM_FILE_ERROR_FAILED;
    base::FilePath path = GetDirectoryForOriginAndType(
        origin, type_string, false, &error);
    if (error != base::PLATFORM_FILE_OK)
      continue;
    scoped_ptr<SandboxDirectoryDatabase> db(new SandboxDirectoryDatabase(path));
    if (db->Init(SandboxDirectoryDatabase::FAIL_ON_CORRUPTION)) {
      directories_[GetDirectoryDatabaseKey(origin, type_string)] = db.release();
      MarkUsed();
      // Don't populate more than one database, as it may rather hurt
      // performance.
      break;
    }
  }
}

base::FilePath ObfuscatedFileUtil::GetDirectoryForURL(
    const FileSystemURL& url,
    bool create,
    base::PlatformFileError* error_code) {
  return GetDirectoryForOriginAndType(
      url.origin(), CallGetTypeStringForURL(url), create, error_code);
}

std::string ObfuscatedFileUtil::CallGetTypeStringForURL(
    const FileSystemURL& url) {
  DCHECK(!get_type_string_for_url_.is_null());
  return get_type_string_for_url_.Run(url);
}

PlatformFileError ObfuscatedFileUtil::GetFileInfoInternal(
    SandboxDirectoryDatabase* db,
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    FileId file_id,
    FileInfo* local_info,
    base::PlatformFileInfo* file_info,
    base::FilePath* platform_file_path) {
  DCHECK(db);
  DCHECK(context);
  DCHECK(file_info);
  DCHECK(platform_file_path);

  if (!db->GetFileInfo(file_id, local_info)) {
    NOTREACHED();
    return base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (local_info->is_directory()) {
    file_info->size = 0;
    file_info->is_directory = true;
    file_info->is_symbolic_link = false;
    file_info->last_modified = local_info->modification_time;
    *platform_file_path = base::FilePath();
    // We don't fill in ctime or atime.
    return base::PLATFORM_FILE_OK;
  }
  if (local_info->data_path.empty())
    return base::PLATFORM_FILE_ERROR_INVALID_OPERATION;
  base::FilePath local_path = DataPathToLocalPath(url, local_info->data_path);
  base::PlatformFileError error = NativeFileUtil::GetFileInfo(
      local_path, file_info);
  // We should not follow symbolic links in sandboxed file system.
  if (base::IsLink(local_path)) {
    LOG(WARNING) << "Found a symbolic file.";
    error = base::PLATFORM_FILE_ERROR_NOT_FOUND;
  }
  if (error == base::PLATFORM_FILE_OK) {
    *platform_file_path = local_path;
  } else if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    LOG(WARNING) << "Lost a backing file.";
    InvalidateUsageCache(context, url.origin(), url.type());
    if (!db->RemoveFileInfo(file_id))
      return base::PLATFORM_FILE_ERROR_FAILED;
  }
  return error;
}

PlatformFileError ObfuscatedFileUtil::CreateFile(
    FileSystemOperationContext* context,
    const base::FilePath& src_file_path,
    const FileSystemURL& dest_url,
    FileInfo* dest_file_info, int file_flags, PlatformFile* handle) {
  if (handle)
    *handle = base::kInvalidPlatformFileValue;
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(dest_url, true);

  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath root = GetDirectoryForURL(dest_url, false, &error);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  base::FilePath dest_local_path;
  error = GenerateNewLocalPath(db, context, dest_url, &dest_local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  bool created = false;
  if (!src_file_path.empty()) {
    DCHECK(!file_flags);
    DCHECK(!handle);
    error = NativeFileUtil::CopyOrMoveFile(
        src_file_path, dest_local_path,
        FileSystemOperation::OPTION_NONE,
        fileapi::NativeFileUtil::CopyOrMoveModeForDestination(dest_url,
                                                              true /* copy */));
    created = true;
  } else {
    if (base::PathExists(dest_local_path)) {
      if (!base::DeleteFile(dest_local_path, true /* recursive */)) {
        NOTREACHED();
        return base::PLATFORM_FILE_ERROR_FAILED;
      }
      LOG(WARNING) << "A stray file detected";
      InvalidateUsageCache(context, dest_url.origin(), dest_url.type());
    }

    if (handle) {
      error = NativeFileUtil::CreateOrOpen(
          dest_local_path, file_flags, handle, &created);
      // If this succeeds, we must close handle on any subsequent error.
    } else {
      DCHECK(!file_flags);  // file_flags is only used by CreateOrOpen.
      error = NativeFileUtil::EnsureFileExists(dest_local_path, &created);
    }
  }
  if (error != base::PLATFORM_FILE_OK)
    return error;

  if (!created) {
    NOTREACHED();
    if (handle) {
      DCHECK_NE(base::kInvalidPlatformFileValue, *handle);
      base::ClosePlatformFile(*handle);
      base::DeleteFile(dest_local_path, false /* recursive */);
      *handle = base::kInvalidPlatformFileValue;
    }
    return base::PLATFORM_FILE_ERROR_FAILED;
  }

  // This removes the root, including the trailing slash, leaving a relative
  // path.
  dest_file_info->data_path = base::FilePath(
      dest_local_path.value().substr(root.value().length() + 1));

  FileId file_id;
  error = db->AddFileInfo(*dest_file_info, &file_id);
  if (error != base::PLATFORM_FILE_OK) {
    if (handle) {
      DCHECK_NE(base::kInvalidPlatformFileValue, *handle);
      base::ClosePlatformFile(*handle);
      *handle = base::kInvalidPlatformFileValue;
    }
    base::DeleteFile(dest_local_path, false /* recursive */);
    return error;
  }
  TouchDirectory(db, dest_file_info->parent_id);

  return base::PLATFORM_FILE_OK;
}

base::FilePath ObfuscatedFileUtil::DataPathToLocalPath(
    const FileSystemURL& url, const base::FilePath& data_path) {
  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath root = GetDirectoryForURL(url, false, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::FilePath();
  return root.Append(data_path);
}

std::string ObfuscatedFileUtil::GetDirectoryDatabaseKey(
    const GURL& origin, const std::string& type_string) {
  if (type_string.empty()) {
    LOG(WARNING) << "Unknown filesystem type requested:" << type_string;
    return std::string();
  }
  // For isolated origin we just use a type string as a key.
  return webkit_database::GetIdentifierFromOrigin(origin) +
      type_string;
}

// TODO(ericu): How to do the whole validation-without-creation thing?
// We may not have quota even to create the database.
// Ah, in that case don't even get here?
// Still doesn't answer the quota issue, though.
SandboxDirectoryDatabase* ObfuscatedFileUtil::GetDirectoryDatabase(
    const FileSystemURL& url, bool create) {
  std::string key = GetDirectoryDatabaseKey(
      url.origin(), CallGetTypeStringForURL(url));
  if (key.empty())
    return NULL;

  DirectoryMap::iterator iter = directories_.find(key);
  if (iter != directories_.end()) {
    MarkUsed();
    return iter->second;
  }

  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath path = GetDirectoryForURL(url, create, &error);
  if (error != base::PLATFORM_FILE_OK) {
    LOG(WARNING) << "Failed to get origin+type directory: "
                 << url.DebugString() << " error:" << error;
    return NULL;
  }
  MarkUsed();
  SandboxDirectoryDatabase* database = new SandboxDirectoryDatabase(path);
  directories_[key] = database;
  return database;
}

base::FilePath ObfuscatedFileUtil::GetDirectoryForOrigin(
    const GURL& origin, bool create, base::PlatformFileError* error_code) {
  if (!InitOriginDatabase(origin, create)) {
    if (error_code) {
      *error_code = create ?
          base::PLATFORM_FILE_ERROR_FAILED :
          base::PLATFORM_FILE_ERROR_NOT_FOUND;
    }
    return base::FilePath();
  }
  base::FilePath directory_name;
  std::string id = webkit_database::GetIdentifierFromOrigin(origin);

  bool exists_in_db = origin_database_->HasOriginPath(id);
  if (!exists_in_db && !create) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_NOT_FOUND;
    return base::FilePath();
  }
  if (!origin_database_->GetPathForOrigin(id, &directory_name)) {
    if (error_code)
      *error_code = base::PLATFORM_FILE_ERROR_FAILED;
    return base::FilePath();
  }

  base::FilePath path = file_system_directory_.Append(directory_name);
  bool exists_in_fs = base::DirectoryExists(path);
  if (!exists_in_db && exists_in_fs) {
    if (!base::DeleteFile(path, true)) {
      if (error_code)
        *error_code = base::PLATFORM_FILE_ERROR_FAILED;
      return base::FilePath();
    }
    exists_in_fs = false;
  }

  if (!exists_in_fs) {
    if (!create || !base::CreateDirectory(path)) {
      if (error_code)
        *error_code = create ?
            base::PLATFORM_FILE_ERROR_FAILED :
            base::PLATFORM_FILE_ERROR_NOT_FOUND;
      return base::FilePath();
    }
  }

  if (error_code)
    *error_code = base::PLATFORM_FILE_OK;

  return path;
}

void ObfuscatedFileUtil::InvalidateUsageCache(
    FileSystemOperationContext* context,
    const GURL& origin,
    FileSystemType type) {
  if (sandbox_delegate_)
    sandbox_delegate_->InvalidateUsageCache(origin, type);
}

void ObfuscatedFileUtil::MarkUsed() {
  if (!timer_)
    timer_.reset(new TimedTaskHelper(file_task_runner_.get()));

  if (timer_->IsRunning()) {
    timer_->Reset();
  } else {
    timer_->Start(FROM_HERE,
                  base::TimeDelta::FromSeconds(db_flush_delay_seconds_),
                  base::Bind(&ObfuscatedFileUtil::DropDatabases,
                             base::Unretained(this)));
  }
}

void ObfuscatedFileUtil::DropDatabases() {
  origin_database_.reset();
  STLDeleteContainerPairSecondPointers(
      directories_.begin(), directories_.end());
  directories_.clear();
  timer_.reset();
}

bool ObfuscatedFileUtil::InitOriginDatabase(const GURL& origin_hint,
                                            bool create) {
  if (origin_database_)
    return true;

  if (!create && !base::DirectoryExists(file_system_directory_))
    return false;
  if (!base::CreateDirectory(file_system_directory_)) {
    LOG(WARNING) << "Failed to create FileSystem directory: " <<
        file_system_directory_.value();
    return false;
  }

  SandboxPrioritizedOriginDatabase* prioritized_origin_database =
      new SandboxPrioritizedOriginDatabase(file_system_directory_);
  origin_database_.reset(prioritized_origin_database);

  if (origin_hint.is_empty() || !HasIsolatedStorage(origin_hint))
    return true;

  const std::string isolated_origin_string =
      webkit_database::GetIdentifierFromOrigin(origin_hint);

  // TODO(kinuko): Deprecate this after a few release cycles, e.g. around M33.
  base::FilePath isolated_origin_dir = file_system_directory_.Append(
      SandboxIsolatedOriginDatabase::kObsoleteOriginDirectory);
  if (base::DirectoryExists(isolated_origin_dir) &&
      prioritized_origin_database->GetSandboxOriginDatabase()) {
    SandboxIsolatedOriginDatabase::MigrateBackFromObsoleteOriginDatabase(
        isolated_origin_string,
        file_system_directory_,
        prioritized_origin_database->GetSandboxOriginDatabase());
  }

  prioritized_origin_database->InitializePrimaryOrigin(
      isolated_origin_string);

  return true;
}

PlatformFileError ObfuscatedFileUtil::GenerateNewLocalPath(
    SandboxDirectoryDatabase* db,
    FileSystemOperationContext* context,
    const FileSystemURL& url,
    base::FilePath* local_path) {
  DCHECK(local_path);
  int64 number;
  if (!db || !db->GetNextInteger(&number))
    return base::PLATFORM_FILE_ERROR_FAILED;

  PlatformFileError error = base::PLATFORM_FILE_OK;
  base::FilePath new_local_path = GetDirectoryForURL(url, false, &error);
  if (error != base::PLATFORM_FILE_OK)
    return base::PLATFORM_FILE_ERROR_FAILED;

  // We use the third- and fourth-to-last digits as the directory.
  int64 directory_number = number % 10000 / 100;
  new_local_path = new_local_path.AppendASCII(
      base::StringPrintf("%02" PRId64, directory_number));

  error = NativeFileUtil::CreateDirectory(
      new_local_path, false /* exclusive */, false /* recursive */);
  if (error != base::PLATFORM_FILE_OK)
    return error;

  *local_path =
      new_local_path.AppendASCII(base::StringPrintf("%08" PRId64, number));
  return base::PLATFORM_FILE_OK;
}

PlatformFileError ObfuscatedFileUtil::CreateOrOpenInternal(
    FileSystemOperationContext* context,
    const FileSystemURL& url, int file_flags,
    PlatformFile* file_handle, bool* created) {
  DCHECK(!(file_flags & (base::PLATFORM_FILE_DELETE_ON_CLOSE |
        base::PLATFORM_FILE_HIDDEN | base::PLATFORM_FILE_EXCLUSIVE_READ |
        base::PLATFORM_FILE_EXCLUSIVE_WRITE)));
  SandboxDirectoryDatabase* db = GetDirectoryDatabase(url, true);
  if (!db)
    return base::PLATFORM_FILE_ERROR_FAILED;
  FileId file_id;
  if (!db->GetFileWithPath(url.path(), &file_id)) {
    // The file doesn't exist.
    if (!(file_flags & (base::PLATFORM_FILE_CREATE |
        base::PLATFORM_FILE_CREATE_ALWAYS | base::PLATFORM_FILE_OPEN_ALWAYS)))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileId parent_id;
    if (!db->GetFileWithPath(VirtualPath::DirName(url.path()),
                             &parent_id))
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    FileInfo file_info;
    InitFileInfo(&file_info, parent_id,
                 VirtualPath::BaseName(url.path()).value());

    int64 growth = UsageForPath(file_info.name.size());
    if (!AllocateQuota(context, growth))
      return base::PLATFORM_FILE_ERROR_NO_SPACE;
    PlatformFileError error = CreateFile(
        context, base::FilePath(),
        url, &file_info, file_flags, file_handle);
    if (created && base::PLATFORM_FILE_OK == error) {
      *created = true;
      UpdateUsage(context, url, growth);
      context->change_observers()->Notify(
          &FileChangeObserver::OnCreateFile, MakeTuple(url));
    }
    return error;
  }

  if (file_flags & base::PLATFORM_FILE_CREATE)
    return base::PLATFORM_FILE_ERROR_EXISTS;

  base::PlatformFileInfo platform_file_info;
  base::FilePath local_path;
  FileInfo file_info;
  base::PlatformFileError error = GetFileInfoInternal(
      db, context, url, file_id, &file_info, &platform_file_info, &local_path);
  if (error != base::PLATFORM_FILE_OK)
    return error;
  if (file_info.is_directory())
    return base::PLATFORM_FILE_ERROR_NOT_A_FILE;

  int64 delta = 0;
  if (file_flags & (base::PLATFORM_FILE_CREATE_ALWAYS |
                    base::PLATFORM_FILE_OPEN_TRUNCATED)) {
    // The file exists and we're truncating.
    delta = -platform_file_info.size;
    AllocateQuota(context, delta);
  }

  error = NativeFileUtil::CreateOrOpen(
      local_path, file_flags, file_handle, created);
  if (error == base::PLATFORM_FILE_ERROR_NOT_FOUND) {
    // TODO(tzik): Also invalidate on-memory usage cache in UsageTracker.
    // TODO(tzik): Delete database entry after ensuring the file lost.
    InvalidateUsageCache(context, url.origin(), url.type());
    LOG(WARNING) << "Lost a backing file.";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  // If truncating we need to update the usage.
  if (error == base::PLATFORM_FILE_OK && delta) {
    UpdateUsage(context, url, delta);
    context->change_observers()->Notify(
        &FileChangeObserver::OnModifyFile, MakeTuple(url));
  }
  return error;
}

bool ObfuscatedFileUtil::HasIsolatedStorage(const GURL& origin) {
  return special_storage_policy_.get() &&
      special_storage_policy_->HasIsolatedStorage(origin);
}

}  // namespace fileapi
