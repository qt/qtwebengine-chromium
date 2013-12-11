// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_
#define WEBKIT_BROWSER_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_

#include <queue>
#include <stack>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "webkit/browser/fileapi/file_system_operation.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace fileapi {

class FileSystemContext;
class FileSystemOperationRunner;

// A base class for recursive operation delegates.
//
// In short, each subclass should override ProcessFile and ProcessDirectory
// to process a directory or a file. To start the recursive operation it
// should also call StartRecursiveOperation.
class WEBKIT_STORAGE_BROWSER_EXPORT RecursiveOperationDelegate
    : public base::SupportsWeakPtr<RecursiveOperationDelegate> {
 public:
  typedef FileSystemOperation::StatusCallback StatusCallback;
  typedef FileSystemOperation::FileEntryList FileEntryList;

  virtual ~RecursiveOperationDelegate();

  // This is called when the consumer of this instance starts a non-recursive
  // operation.
  virtual void Run() = 0;

  // This is called when the consumer of this instance starts a recursive
  // operation.
  virtual void RunRecursively() = 0;

  // This is called each time a file is found while recursively
  // performing an operation.
  virtual void ProcessFile(const FileSystemURL& url,
                           const StatusCallback& callback) = 0;

  // This is called each time a directory is found while recursively
  // performing an operation.
  virtual void ProcessDirectory(const FileSystemURL& url,
                                const StatusCallback& callback) = 0;


  // This is called each time after files and subdirectories for a
  // directory is processed while recursively performing an operation.
  virtual void PostProcessDirectory(const FileSystemURL& url,
                                    const StatusCallback& callback) = 0;

  // Cancels the currently running operation.
  void Cancel();

 protected:
  explicit RecursiveOperationDelegate(FileSystemContext* file_system_context);

  // Starts to process files/directories recursively from the given |root|.
  // This will call ProcessFile and ProcessDirectory on each file or directory.
  //
  // First, this tries to call ProcessFile with |root| regardless whether it is
  // actually a file or a directory. If it is a directory, ProcessFile should
  // return PLATFORM_FILE_NOT_A_FILE.
  //
  // For each directory, the recursive operation works as follows:
  // ProcessDirectory is called first for the directory.
  // Then the directory contents are read (to obtain its sub directories and
  // files in it).
  // ProcessFile is called for found files. This may run in parallel.
  // The same step is recursively applied to each subdirectory.
  // After all files and subdirectories in a directory are processed,
  // PostProcessDirectory is called for the directory.
  // Here is an example;
  // a_dir/ -+- b1_dir/ -+- c1_dir/ -+- d1_file
  //         |           |           |
  //         |           +- c2_file  +- d2_file
  //         |
  //         +- b2_dir/ --- e_dir/
  //         |
  //         +- b3_file
  //         |
  //         +- b4_file
  // Then traverse order is:
  // ProcessFile(a_dir) (This should return PLATFORM_FILE_NOT_A_FILE).
  // ProcessDirectory(a_dir).
  // ProcessFile(b3_file), ProcessFile(b4_file). (in parallel).
  // ProcessDirectory(b1_dir).
  // ProcessFile(c2_file)
  // ProcessDirectory(c1_dir).
  // ProcessFile(d1_file), ProcessFile(d2_file). (in parallel).
  // PostProcessDirectory(c1_dir)
  // PostProcessDirectory(b1_dir).
  // ProcessDirectory(b2_dir)
  // ProcessDirectory(e_dir)
  // PostProcessDirectory(e_dir)
  // PostProcessDirectory(b2_dir)
  // PostProcessDirectory(a_dir)
  //
  // |callback| is fired with base::PLATFORM_FILE_OK when every file/directory
  // under |root| is processed, or fired earlier when any suboperation fails.
  void StartRecursiveOperation(const FileSystemURL& root,
                               const StatusCallback& callback);

  FileSystemContext* file_system_context() { return file_system_context_; }
  const FileSystemContext* file_system_context() const {
    return file_system_context_;
  }

  FileSystemOperationRunner* operation_runner();

 private:
  void DidTryProcessFile(const FileSystemURL& root,
                         base::PlatformFileError error);
  void ProcessNextDirectory();
  void DidProcessDirectory(base::PlatformFileError error);
  void DidReadDirectory(const FileSystemURL& parent,
                        base::PlatformFileError error,
                        const FileEntryList& entries,
                        bool has_more);
  void ProcessPendingFiles();
  void DidProcessFile(base::PlatformFileError error);
  void ProcessSubDirectory();
  void DidPostProcessDirectory(base::PlatformFileError error);

  // Called when all recursive operation is done (or an error occurs).
  void Done(base::PlatformFileError error);

  FileSystemContext* file_system_context_;
  StatusCallback callback_;
  std::stack<FileSystemURL> pending_directories_;
  std::stack<std::queue<FileSystemURL> > pending_directory_stack_;
  std::queue<FileSystemURL> pending_files_;
  int inflight_operations_;
  bool canceled_;

  DISALLOW_COPY_AND_ASSIGN(RecursiveOperationDelegate);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_RECURSIVE_OPERATION_DELEGATE_H_
