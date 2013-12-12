// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_BACKEND_H_
#define WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_BACKEND_H_

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/browser/fileapi/file_permission_policy.h"
#include "webkit/browser/fileapi/open_file_system_mode.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/fileapi/file_system_types.h"

class GURL;

namespace webkit_blob {
class FileStreamReader;
}

namespace fileapi {

class AsyncFileUtil;
class CopyOrMoveFileValidatorFactory;
class FileSystemURL;
class FileStreamWriter;
class FileSystemContext;
class FileSystemFileUtil;
class FileSystemOperation;
class FileSystemQuotaUtil;

// An interface for defining a file system backend.
//
// NOTE: when you implement a new FileSystemBackend for your own
// FileSystem module, please contact to kinuko@chromium.org.
//
class WEBKIT_STORAGE_BROWSER_EXPORT FileSystemBackend {
 public:
  // Callback for InitializeFileSystem.
  typedef base::Callback<void(const GURL& root_url,
                              const std::string& name,
                              base::PlatformFileError error)>
      OpenFileSystemCallback;
  virtual ~FileSystemBackend() {}

  // Returns true if this filesystem backend can handle |type|.
  // One filesystem backend may be able to handle multiple filesystem types.
  virtual bool CanHandleType(FileSystemType type) const = 0;

  // This method is called right after the backend is registered in the
  // FileSystemContext and before any other methods are called. Each backend can
  // do additional initialization which depends on FileSystemContext here.
  virtual void Initialize(FileSystemContext* context) = 0;

  // Opens the filesystem for the given |origin_url| and |type|.
  // This verifies if it is allowed to request (or create) the filesystem
  // and if it can access (or create) the root directory.
  // If |mode| is CREATE_IF_NONEXISTENT calling this may also create
  // the root directory (and/or related database entries etc) for
  // the filesystem if it doesn't exist.
  virtual void OpenFileSystem(
      const GURL& origin_url,
      FileSystemType type,
      OpenFileSystemMode mode,
      const OpenFileSystemCallback& callback) = 0;

  // Returns the specialized AsyncFileUtil for this backend.
  virtual AsyncFileUtil* GetAsyncFileUtil(FileSystemType type) = 0;

  // Returns the specialized CopyOrMoveFileValidatorFactory for this backend
  // and |type|.  If |error_code| is PLATFORM_FILE_OK and the result is NULL,
  // then no validator is required.
  virtual CopyOrMoveFileValidatorFactory* GetCopyOrMoveFileValidatorFactory(
      FileSystemType type, base::PlatformFileError* error_code) = 0;

  // Returns a new instance of the specialized FileSystemOperation for this
  // backend based on the given triplet of |origin_url|, |file_system_type|
  // and |virtual_path|. On failure to create a file system operation, set
  // |error_code| correspondingly.
  // This method is usually dispatched by
  // FileSystemContext::CreateFileSystemOperation.
  virtual FileSystemOperation* CreateFileSystemOperation(
      const FileSystemURL& url,
      FileSystemContext* context,
      base::PlatformFileError* error_code) const = 0;

  // Creates a new file stream reader for a given filesystem URL |url| with an
  // offset |offset|. |expected_modification_time| specifies the expected last
  // modification if the value is non-null, the reader will check the underlying
  // file's actual modification time to see if the file has been modified, and
  // if it does any succeeding read operations should fail with
  // ERR_UPLOAD_FILE_CHANGED error.
  // This method itself does *not* check if the given path exists and is a
  // regular file.
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const = 0;

  // Creates a new file stream writer for a given filesystem URL |url| with an
  // offset |offset|.
  // This method itself does *not* check if the given path exists and is a
  // regular file.
  virtual scoped_ptr<FileStreamWriter> CreateFileStreamWriter(
      const FileSystemURL& url,
      int64 offset,
      FileSystemContext* context) const = 0;

  // Returns the specialized FileSystemQuotaUtil for this backend.
  // This could return NULL if this backend does not support quota.
  virtual FileSystemQuotaUtil* GetQuotaUtil() = 0;
};

// An interface to control external file system access permissions.
// TODO(satorux): Move this out of 'webkit/browser/fileapi'. crbug.com/257279
class ExternalFileSystemBackend : public FileSystemBackend {
 public:
  // Returns true if |url| is allowed to be accessed.
  // This is supposed to perform ExternalFileSystem-specific security
  // checks.
  virtual bool IsAccessAllowed(const fileapi::FileSystemURL& url) const = 0;
  // Returns the list of top level directories that are exposed by this
  // provider. This list is used to set appropriate child process file access
  // permissions.
  virtual std::vector<base::FilePath> GetRootDirectories() const = 0;
  // Grants access to all external file system from extension identified with
  // |extension_id|.
  virtual void GrantFullAccessToExtension(const std::string& extension_id) = 0;
  // Grants access to |virtual_path| from |origin_url|.
  virtual void GrantFileAccessToExtension(
      const std::string& extension_id,
      const base::FilePath& virtual_path) = 0;
  // Revokes file access from extension identified with |extension_id|.
  virtual void RevokeAccessForExtension(
        const std::string& extension_id) = 0;
  // Gets virtual path by known filesystem path. Returns false when filesystem
  // path is not exposed by this provider.
  virtual bool GetVirtualPath(const base::FilePath& file_system_path,
                              base::FilePath* virtual_path) = 0;
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_FILE_SYSTEM_BACKEND_H_
