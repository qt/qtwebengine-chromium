// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/async_file_util_adapter.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/task_runner_util.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_file_util.h"
#include "webkit/browser/fileapi/file_system_operation_context.h"
#include "webkit/browser/fileapi/file_system_url.h"
#include "webkit/common/blob/shareable_file_reference.h"
#include "webkit/common/fileapi/file_system_util.h"

using base::Bind;
using base::Callback;
using base::Owned;
using base::PlatformFileError;
using base::Unretained;
using webkit_blob::ShareableFileReference;

namespace fileapi {

namespace {

class EnsureFileExistsHelper {
 public:
  EnsureFileExistsHelper() : error_(base::PLATFORM_FILE_OK), created_(false) {}

  void RunWork(FileSystemFileUtil* file_util,
               FileSystemOperationContext* context,
               const FileSystemURL& url) {
    error_ = file_util->EnsureFileExists(context, url, &created_);
  }

  void Reply(const AsyncFileUtil::EnsureFileExistsCallback& callback) {
    callback.Run(error_, created_);
  }

 private:
  base::PlatformFileError error_;
  bool created_;
  DISALLOW_COPY_AND_ASSIGN(EnsureFileExistsHelper);
};

class GetFileInfoHelper {
 public:
  GetFileInfoHelper()
      : error_(base::PLATFORM_FILE_OK) {}

  void GetFileInfo(FileSystemFileUtil* file_util,
                   FileSystemOperationContext* context,
                   const FileSystemURL& url) {
    error_ = file_util->GetFileInfo(context, url, &file_info_, &platform_path_);
  }

  void CreateSnapshotFile(FileSystemFileUtil* file_util,
                          FileSystemOperationContext* context,
                          const FileSystemURL& url) {
    scoped_file_ = file_util->CreateSnapshotFile(
        context, url, &error_, &file_info_, &platform_path_);
  }

  void ReplyFileInfo(const AsyncFileUtil::GetFileInfoCallback& callback) {
    callback.Run(error_, file_info_);
  }

  void ReplySnapshotFile(
      const AsyncFileUtil::CreateSnapshotFileCallback& callback) {
    callback.Run(error_, file_info_, platform_path_,
                  ShareableFileReference::GetOrCreate(scoped_file_.Pass()));
  }

 private:
  base::PlatformFileError error_;
  base::PlatformFileInfo file_info_;
  base::FilePath platform_path_;
  webkit_blob::ScopedFile scoped_file_;
  DISALLOW_COPY_AND_ASSIGN(GetFileInfoHelper);
};

class ReadDirectoryHelper {
 public:
  ReadDirectoryHelper() : error_(base::PLATFORM_FILE_OK) {}

  void RunWork(FileSystemFileUtil* file_util,
               FileSystemOperationContext* context,
               const FileSystemURL& url) {
    base::PlatformFileInfo file_info;
    base::FilePath platform_path;
    PlatformFileError error = file_util->GetFileInfo(
        context, url, &file_info, &platform_path);
    if (error != base::PLATFORM_FILE_OK) {
      error_ = error;
      return;
    }
    if (!file_info.is_directory) {
      error_ = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;
      return;
    }

    scoped_ptr<FileSystemFileUtil::AbstractFileEnumerator> file_enum(
        file_util->CreateFileEnumerator(context, url));

    base::FilePath current;
    while (!(current = file_enum->Next()).empty()) {
      DirectoryEntry entry;
      entry.is_directory = file_enum->IsDirectory();
      entry.name = VirtualPath::BaseName(current).value();
      entry.size = file_enum->Size();
      entry.last_modified_time = file_enum->LastModifiedTime();
      entries_.push_back(entry);
    }
    error_ = base::PLATFORM_FILE_OK;
  }

  void Reply(const AsyncFileUtil::ReadDirectoryCallback& callback) {
    callback.Run(error_, entries_, false /* has_more */);
  }

 private:
  base::PlatformFileError error_;
  std::vector<DirectoryEntry> entries_;
  DISALLOW_COPY_AND_ASSIGN(ReadDirectoryHelper);
};

void RunCreateOrOpenCallback(
    const AsyncFileUtil::CreateOrOpenCallback& callback,
    base::PlatformFileError result,
    base::PassPlatformFile file,
    bool created) {
  callback.Run(result, file, base::Closure());
}

}  // namespace

AsyncFileUtilAdapter::AsyncFileUtilAdapter(
    FileSystemFileUtil* sync_file_util)
    : sync_file_util_(sync_file_util) {
  DCHECK(sync_file_util_.get());
}

AsyncFileUtilAdapter::~AsyncFileUtilAdapter() {
}

void AsyncFileUtilAdapter::CreateOrOpen(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int file_flags,
    const CreateOrOpenCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::FileUtilProxy::RelayCreateOrOpen(
      context_ptr->task_runner(),
      Bind(&FileSystemFileUtil::CreateOrOpen, Unretained(sync_file_util_.get()),
           context_ptr, url, file_flags),
      Bind(&FileSystemFileUtil::Close, Unretained(sync_file_util_.get()),
           base::Owned(context_ptr)),
      Bind(&RunCreateOrOpenCallback, callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::EnsureFileExists(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const EnsureFileExistsCallback& callback) {
  EnsureFileExistsHelper* helper = new EnsureFileExistsHelper;
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = context_ptr->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&EnsureFileExistsHelper::RunWork, Unretained(helper),
             sync_file_util_.get(), base::Owned(context_ptr), url),
        Bind(&EnsureFileExistsHelper::Reply, Owned(helper), callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::CreateDirectory(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    bool exclusive,
    bool recursive,
    const StatusCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::PostTaskAndReplyWithResult(
      context_ptr->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CreateDirectory,
           Unretained(sync_file_util_.get()),
           base::Owned(context_ptr), url, exclusive, recursive),
      callback);
  DCHECK(success);
}

void AsyncFileUtilAdapter::GetFileInfo(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const GetFileInfoCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  const bool success = context_ptr->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&GetFileInfoHelper::GetFileInfo, Unretained(helper),
             sync_file_util_.get(), base::Owned(context_ptr), url),
        Bind(&GetFileInfoHelper::ReplyFileInfo, Owned(helper), callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::ReadDirectory(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  ReadDirectoryHelper* helper = new ReadDirectoryHelper;
  const bool success = context_ptr->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&ReadDirectoryHelper::RunWork, Unretained(helper),
             sync_file_util_.get(), base::Owned(context_ptr), url),
        Bind(&ReadDirectoryHelper::Reply, Owned(helper), callback));
  DCHECK(success);
}

void AsyncFileUtilAdapter::Touch(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    const StatusCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::PostTaskAndReplyWithResult(
      context_ptr->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::Touch, Unretained(sync_file_util_.get()),
           base::Owned(context_ptr), url,
           last_access_time, last_modified_time),
      callback);
  DCHECK(success);
}

void AsyncFileUtilAdapter::Truncate(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    int64 length,
    const StatusCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::PostTaskAndReplyWithResult(
      context_ptr->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::Truncate, Unretained(sync_file_util_.get()),
           base::Owned(context_ptr), url, length),
      callback);
  DCHECK(success);
}

void AsyncFileUtilAdapter::CopyFileLocal(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const CopyFileProgressCallback& progress_callback,
    const StatusCallback& callback) {
  // TODO(hidehiko): Support progress_callback.
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::PostTaskAndReplyWithResult(
      context_ptr->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CopyOrMoveFile,
           Unretained(sync_file_util_.get()),
           base::Owned(context_ptr), src_url, dest_url, true /* copy */),
      callback);
  DCHECK(success);
}

void AsyncFileUtilAdapter::MoveFileLocal(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    const StatusCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::PostTaskAndReplyWithResult(
      context_ptr->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CopyOrMoveFile,
           Unretained(sync_file_util_.get()),
           base::Owned(context_ptr), src_url, dest_url, false /* copy */),
      callback);
  DCHECK(success);
}

void AsyncFileUtilAdapter::CopyInForeignFile(
      scoped_ptr<FileSystemOperationContext> context,
      const base::FilePath& src_file_path,
      const FileSystemURL& dest_url,
      const StatusCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::PostTaskAndReplyWithResult(
      context_ptr->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::CopyInForeignFile,
           Unretained(sync_file_util_.get()),
           base::Owned(context_ptr), src_file_path, dest_url),
      callback);
  DCHECK(success);
}

void AsyncFileUtilAdapter::DeleteFile(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::PostTaskAndReplyWithResult(
      context_ptr->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::DeleteFile,
           Unretained(sync_file_util_.get()),
           base::Owned(context_ptr), url),
      callback);
  DCHECK(success);
}

void AsyncFileUtilAdapter::DeleteDirectory(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  const bool success = base::PostTaskAndReplyWithResult(
      context_ptr->task_runner(), FROM_HERE,
      Bind(&FileSystemFileUtil::DeleteDirectory,
           Unretained(sync_file_util_.get()),
           base::Owned(context_ptr), url),
      callback);
  DCHECK(success);
}

void AsyncFileUtilAdapter::DeleteRecursively(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const StatusCallback& callback) {
  callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
}

void AsyncFileUtilAdapter::CreateSnapshotFile(
    scoped_ptr<FileSystemOperationContext> context,
    const FileSystemURL& url,
    const CreateSnapshotFileCallback& callback) {
  FileSystemOperationContext* context_ptr = context.release();
  GetFileInfoHelper* helper = new GetFileInfoHelper;
  const bool success = context_ptr->task_runner()->PostTaskAndReply(
        FROM_HERE,
        Bind(&GetFileInfoHelper::CreateSnapshotFile, Unretained(helper),
             sync_file_util_.get(), base::Owned(context_ptr), url),
        Bind(&GetFileInfoHelper::ReplySnapshotFile, Owned(helper), callback));
  DCHECK(success);
}

}  // namespace fileapi
