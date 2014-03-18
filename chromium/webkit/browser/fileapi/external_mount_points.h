// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_BROWSER_FILEAPI_EXTERNAL_MOUNT_POINTS_H_
#define WEBKIT_BROWSER_FILEAPI_EXTERNAL_MOUNT_POINTS_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "webkit/browser/fileapi/mount_points.h"
#include "webkit/browser/webkit_storage_browser_export.h"
#include "webkit/common/fileapi/file_system_mount_option.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace base {
class FilePath;
}

namespace fileapi {
class FileSystemURL;
}

namespace fileapi {

// Manages external filesystem namespaces that are identified by 'mount name'
// and are persisted until RevokeFileSystem is called.
// Files in an external filesystem are identified by a filesystem URL like:
//
//   filesystem:<origin>/external/<mount_name>/relative/path
//
class WEBKIT_STORAGE_BROWSER_EXPORT ExternalMountPoints
    : public base::RefCountedThreadSafe<ExternalMountPoints>,
      public MountPoints {
 public:
  static ExternalMountPoints* GetSystemInstance();
  static scoped_refptr<ExternalMountPoints> CreateRefCounted();

  // Registers a new named external filesystem.
  // The |path| is registered as the root path of the mount point which
  // is identified by a URL "filesystem:.../external/mount_name".
  //
  // For example, if the path "/media/removable" is registered with
  // the mount_name "removable", a filesystem URL like
  // "filesystem:.../external/removable/a/b" will be resolved as
  // "/media/removable/a/b".
  //
  // The |mount_name| should NOT contain a path separator '/'.
  // Returns false if the given name is already registered.
  //
  // Overlapping mount points in a single MountPoints instance are not allowed.
  // Adding mount point whose path overlaps with an existing mount point will
  // fail.
  //
  // If not empty, |path| must be absolute. It is allowed for the path to be
  // empty, but |GetVirtualPath| will not work for those mount points.
  //
  // An external file system registered by this method can be revoked
  // by calling RevokeFileSystem with |mount_name|.
  bool RegisterFileSystem(const std::string& mount_name,
                          FileSystemType type,
                          const FileSystemMountOption& mount_option,
                          const base::FilePath& path);

  // MountPoints overrides.
  virtual bool HandlesFileSystemMountType(FileSystemType type) const OVERRIDE;
  virtual bool RevokeFileSystem(const std::string& mount_name) OVERRIDE;
  virtual bool GetRegisteredPath(const std::string& mount_name,
                                 base::FilePath* path) const OVERRIDE;
  virtual bool CrackVirtualPath(
      const base::FilePath& virtual_path,
      std::string* mount_name,
      FileSystemType* type,
      base::FilePath* path,
      FileSystemMountOption* mount_option) const OVERRIDE;
  virtual FileSystemURL CrackURL(const GURL& url) const OVERRIDE;
  virtual FileSystemURL CreateCrackedFileSystemURL(
      const GURL& origin,
      FileSystemType type,
      const base::FilePath& path) const OVERRIDE;

  // Returns a list of registered MountPointInfos (of <mount_name, path>).
  void AddMountPointInfosTo(std::vector<MountPointInfo>* mount_points) const;

  // Converts a path on a registered file system to virtual path relative to the
  // file system root. E.g. if 'Downloads' file system is mapped to
  // '/usr/local/home/Downloads', and |absolute| path is set to
  // '/usr/local/home/Downloads/foo', the method will set |virtual_path| to
  // 'Downloads/foo'.
  // Returns false if the path cannot be resolved (e.g. if the path is not
  // part of any registered filesystem).
  //
  // Returned virtual_path will have normalized path separators.
  bool GetVirtualPath(const base::FilePath& absolute_path,
                      base::FilePath* virtual_path) const;

  // Returns the virtual root path that looks like /<mount_name>.
  base::FilePath CreateVirtualRootPath(const std::string& mount_name) const;

  FileSystemURL CreateExternalFileSystemURL(
      const GURL& origin,
      const std::string& mount_name,
      const base::FilePath& path) const;

  // Revoke all registered filesystems. Used only by testing (for clean-ups).
  void RevokeAllFileSystems();

 private:
  friend class base::RefCountedThreadSafe<ExternalMountPoints>;

  // Represents each file system instance (defined in the .cc).
  class Instance;

  typedef std::map<std::string, Instance*> NameToInstance;

  // Reverse map from registered path to its corresponding mount name.
  typedef std::map<base::FilePath, std::string> PathToName;

  // Use |GetSystemInstance| of |CreateRefCounted| to get an instance.
  ExternalMountPoints();
  virtual ~ExternalMountPoints();

  // MountPoint overrides.
  virtual FileSystemURL CrackFileSystemURL(
      const FileSystemURL& url) const OVERRIDE;

  // Performs sanity checks on the new mount point.
  // Checks the following:
  //  - there is no registered mount point with mount_name
  //  - path does not contain a reference to a parent
  //  - path is absolute
  //  - path does not overlap with an existing mount point path.
  //
  // |lock_| should be taken before calling this method.
  bool ValidateNewMountPoint(const std::string& mount_name,
                             const base::FilePath& path);

  // This lock needs to be obtained when accessing the instance_map_.
  mutable base::Lock lock_;

  NameToInstance instance_map_;
  PathToName path_to_name_map_;

  DISALLOW_COPY_AND_ASSIGN(ExternalMountPoints);
};

}  // namespace fileapi

#endif  // WEBKIT_BROWSER_FILEAPI_EXTERNAL_MOUNT_POINTS_H_
