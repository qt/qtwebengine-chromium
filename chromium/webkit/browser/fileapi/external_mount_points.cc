// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/external_mount_points.h"

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace {

// Normalizes file path so it has normalized separators and ends with exactly
// one separator. Paths have to be normalized this way for use in
// GetVirtualPath method. Separators cannot be completely stripped, or
// GetVirtualPath could not working in some edge cases.
// For example, /a/b/c(1)/d would be erroneously resolved as c/d if the
// following mount points were registered: "/a/b/c", "/a/b/c(1)". (Note:
// "/a/b/c" < "/a/b/c(1)" < "/a/b/c/").
base::FilePath NormalizeFilePath(const base::FilePath& path) {
  if (path.empty())
    return path;

  base::FilePath::StringType path_str = path.StripTrailingSeparators().value();
  if (!base::FilePath::IsSeparator(path_str[path_str.length() - 1]))
    path_str.append(FILE_PATH_LITERAL("/"));

  return base::FilePath(path_str).NormalizePathSeparators();
}

// Wrapper around ref-counted ExternalMountPoints that will be used to lazily
// create and initialize LazyInstance system ExternalMountPoints.
class SystemMountPointsLazyWrapper {
 public:
  SystemMountPointsLazyWrapper()
      : system_mount_points_(fileapi::ExternalMountPoints::CreateRefCounted()) {
  }

  ~SystemMountPointsLazyWrapper() {}

  fileapi::ExternalMountPoints* get() {
    return system_mount_points_.get();
  }

 private:
  scoped_refptr<fileapi::ExternalMountPoints> system_mount_points_;
};

base::LazyInstance<SystemMountPointsLazyWrapper>::Leaky
    g_external_mount_points = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace fileapi {

class ExternalMountPoints::Instance {
 public:
  Instance(FileSystemType type,
           const base::FilePath& path,
           const FileSystemMountOption& mount_option)
      : type_(type),
        path_(path.StripTrailingSeparators()),
        mount_option_(mount_option) {}
  ~Instance() {}

  FileSystemType type() const { return type_; }
  const base::FilePath& path() const { return path_; }
  const FileSystemMountOption& mount_option() const { return mount_option_; }

 private:
  const FileSystemType type_;
  const base::FilePath path_;
  const FileSystemMountOption mount_option_;

  DISALLOW_COPY_AND_ASSIGN(Instance);
};

//--------------------------------------------------------------------------

// static
ExternalMountPoints* ExternalMountPoints::GetSystemInstance() {
  return g_external_mount_points.Pointer()->get();
}

// static
scoped_refptr<ExternalMountPoints> ExternalMountPoints::CreateRefCounted() {
  return new ExternalMountPoints();
}

bool ExternalMountPoints::RegisterFileSystem(
    const std::string& mount_name,
    FileSystemType type,
    const FileSystemMountOption& mount_option,
    const base::FilePath& path_in) {
  // COPY_SYNC_OPTION_SYNC is only applicable to native local file system.
  DCHECK(type == kFileSystemTypeNativeLocal ||
         mount_option.copy_sync_option() != COPY_SYNC_OPTION_SYNC);

  base::AutoLock locker(lock_);

  base::FilePath path = NormalizeFilePath(path_in);
  if (!ValidateNewMountPoint(mount_name, path))
    return false;

  instance_map_[mount_name] = new Instance(type, path, mount_option);
  if (!path.empty())
    path_to_name_map_.insert(std::make_pair(path, mount_name));
  return true;
}

bool ExternalMountPoints::HandlesFileSystemMountType(
    FileSystemType type) const {
  return type == kFileSystemTypeExternal ||
         type == kFileSystemTypeNativeForPlatformApp;
}

bool ExternalMountPoints::RevokeFileSystem(const std::string& mount_name) {
  base::AutoLock locker(lock_);
  NameToInstance::iterator found = instance_map_.find(mount_name);
  if (found == instance_map_.end())
    return false;
  Instance* instance = found->second;
  path_to_name_map_.erase(NormalizeFilePath(instance->path()));
  delete found->second;
  instance_map_.erase(found);
  return true;
}

bool ExternalMountPoints::GetRegisteredPath(
    const std::string& filesystem_id, base::FilePath* path) const {
  DCHECK(path);
  base::AutoLock locker(lock_);
  NameToInstance::const_iterator found = instance_map_.find(filesystem_id);
  if (found == instance_map_.end())
    return false;
  *path = found->second->path();
  return true;
}

bool ExternalMountPoints::CrackVirtualPath(
    const base::FilePath& virtual_path,
    std::string* mount_name,
    FileSystemType* type,
    base::FilePath* path,
    FileSystemMountOption* mount_option) const {
  DCHECK(mount_name);
  DCHECK(path);

  // The path should not contain any '..' references.
  if (virtual_path.ReferencesParent())
    return false;

  // The virtual_path should comprise of <mount_name> and <relative_path> parts.
  std::vector<base::FilePath::StringType> components;
  virtual_path.GetComponents(&components);
  if (components.size() < 1)
    return false;

  std::vector<base::FilePath::StringType>::iterator component_iter =
      components.begin();
  std::string maybe_mount_name =
      base::FilePath(*component_iter++).MaybeAsASCII();
  if (maybe_mount_name.empty())
    return false;

  base::FilePath cracked_path;
  {
    base::AutoLock locker(lock_);
    NameToInstance::const_iterator found_instance =
        instance_map_.find(maybe_mount_name);
    if (found_instance == instance_map_.end())
      return false;

    *mount_name = maybe_mount_name;
    const Instance* instance = found_instance->second;
    if (type)
      *type = instance->type();
    cracked_path = instance->path();
    *mount_option = instance->mount_option();
  }

  for (; component_iter != components.end(); ++component_iter)
    cracked_path = cracked_path.Append(*component_iter);
  *path = cracked_path;
  return true;
}

FileSystemURL ExternalMountPoints::CrackURL(const GURL& url) const {
  FileSystemURL filesystem_url = FileSystemURL(url);
  if (!filesystem_url.is_valid())
    return FileSystemURL();
  return CrackFileSystemURL(filesystem_url);
}

FileSystemURL ExternalMountPoints::CreateCrackedFileSystemURL(
    const GURL& origin,
    FileSystemType type,
    const base::FilePath& path) const {
  return CrackFileSystemURL(FileSystemURL(origin, type, path));
}

void ExternalMountPoints::AddMountPointInfosTo(
    std::vector<MountPointInfo>* mount_points) const {
  base::AutoLock locker(lock_);
  DCHECK(mount_points);
  for (NameToInstance::const_iterator iter = instance_map_.begin();
       iter != instance_map_.end(); ++iter) {
    mount_points->push_back(MountPointInfo(iter->first, iter->second->path()));
  }
}

bool ExternalMountPoints::GetVirtualPath(const base::FilePath& path_in,
                                         base::FilePath* virtual_path) const {
  DCHECK(virtual_path);

  base::AutoLock locker(lock_);

  base::FilePath path = NormalizeFilePath(path_in);
  std::map<base::FilePath, std::string>::const_reverse_iterator iter(
      path_to_name_map_.upper_bound(path));
  if (iter == path_to_name_map_.rend())
    return false;

  *virtual_path = CreateVirtualRootPath(iter->second);
  if (iter->first == path)
    return true;
  return iter->first.AppendRelativePath(path, virtual_path);
}

base::FilePath ExternalMountPoints::CreateVirtualRootPath(
    const std::string& mount_name) const {
  return base::FilePath().AppendASCII(mount_name);
}

FileSystemURL ExternalMountPoints::CreateExternalFileSystemURL(
    const GURL& origin,
    const std::string& mount_name,
    const base::FilePath& path) const {
  return CreateCrackedFileSystemURL(
      origin,
      fileapi::kFileSystemTypeExternal,
      // Avoid using FilePath::Append as path may be an absolute path.
      base::FilePath(
          CreateVirtualRootPath(mount_name).value() +
          base::FilePath::kSeparators[0] + path.value()));
}

void ExternalMountPoints::RevokeAllFileSystems() {
  NameToInstance instance_map_copy;
  {
    base::AutoLock locker(lock_);
    instance_map_copy = instance_map_;
    instance_map_.clear();
    path_to_name_map_.clear();
  }
  STLDeleteContainerPairSecondPointers(instance_map_copy.begin(),
                                       instance_map_copy.end());
}

ExternalMountPoints::ExternalMountPoints() {}

ExternalMountPoints::~ExternalMountPoints() {
  STLDeleteContainerPairSecondPointers(instance_map_.begin(),
                                       instance_map_.end());
}

FileSystemURL ExternalMountPoints::CrackFileSystemURL(
    const FileSystemURL& url) const {
  if (!HandlesFileSystemMountType(url.type()))
    return FileSystemURL();

  base::FilePath virtual_path = url.path();
  if (url.type() == kFileSystemTypeNativeForPlatformApp) {
#if defined(OS_CHROMEOS)
    // On Chrome OS, find a mount point and virtual path for the external fs.
    if (!GetVirtualPath(url.path(), &virtual_path))
      return FileSystemURL();
#else
    // On other OS, it is simply a native local path.
    return FileSystemURL(
        url.origin(), url.mount_type(), url.virtual_path(),
        url.mount_filesystem_id(), kFileSystemTypeNativeLocal,
        url.path(), url.filesystem_id(), url.mount_option());
#endif
  }

  std::string mount_name;
  FileSystemType cracked_type;
  base::FilePath cracked_path;
  FileSystemMountOption cracked_mount_option;

  if (!CrackVirtualPath(virtual_path, &mount_name, &cracked_type,
                        &cracked_path, &cracked_mount_option)) {
    return FileSystemURL();
  }

  return FileSystemURL(
      url.origin(), url.mount_type(), url.virtual_path(),
      !url.filesystem_id().empty() ? url.filesystem_id() : mount_name,
      cracked_type, cracked_path, mount_name, cracked_mount_option);
}

bool ExternalMountPoints::ValidateNewMountPoint(const std::string& mount_name,
                                                const base::FilePath& path) {
  lock_.AssertAcquired();

  // Mount name must not be empty.
  if (mount_name.empty())
    return false;

  // Verify there is no registered mount point with the same name.
  NameToInstance::iterator found = instance_map_.find(mount_name);
  if (found != instance_map_.end())
    return false;

  // Allow empty paths.
  if (path.empty())
    return true;

  // Verify path is legal.
  if (path.ReferencesParent() || !path.IsAbsolute())
    return false;

  // Check there the new path does not overlap with one of the existing ones.
  std::map<base::FilePath, std::string>::reverse_iterator potential_parent(
      path_to_name_map_.upper_bound(path));
  if (potential_parent != path_to_name_map_.rend()) {
    if (potential_parent->first == path ||
        potential_parent->first.IsParent(path)) {
      return false;
    }
  }

  std::map<base::FilePath, std::string>::iterator potential_child =
      path_to_name_map_.upper_bound(path);
  if (potential_child == path_to_name_map_.end())
    return true;
  return !(potential_child->first == path) &&
         !path.IsParent(potential_child->first);
}

}  // namespace fileapi
