// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/browser/fileapi/file_system_url.h"

#include <sstream>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/escape.h"
#include "webkit/common/fileapi/file_system_types.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace fileapi {

namespace {

}  // namespace

FileSystemURL::FileSystemURL()
    : is_valid_(false),
      mount_type_(kFileSystemTypeUnknown),
      type_(kFileSystemTypeUnknown) {
}

// static
FileSystemURL FileSystemURL::CreateForTest(const GURL& url) {
  return FileSystemURL(url);
}

FileSystemURL FileSystemURL::CreateForTest(const GURL& origin,
                                           FileSystemType mount_type,
                                           const base::FilePath& virtual_path) {
  return FileSystemURL(origin, mount_type, virtual_path);
}

// static
bool FileSystemURL::ParseFileSystemSchemeURL(
    const GURL& url,
    GURL* origin_url,
    FileSystemType* mount_type,
    base::FilePath* virtual_path) {
  GURL origin;
  FileSystemType file_system_type = kFileSystemTypeUnknown;

  if (!url.is_valid() || !url.SchemeIsFileSystem())
    return false;

  const struct {
    FileSystemType type;
    const char* dir;
  } kValidTypes[] = {
    { kFileSystemTypePersistent, kPersistentDir },
    { kFileSystemTypeTemporary, kTemporaryDir },
    { kFileSystemTypeIsolated, kIsolatedDir },
    { kFileSystemTypeExternal, kExternalDir },
    { kFileSystemTypeTest, kTestDir },
  };

  // A path of the inner_url contains only mount type part (e.g. "/temporary").
  DCHECK(url.inner_url());
  std::string inner_path = url.inner_url()->path();
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(kValidTypes); ++i) {
    if (inner_path == kValidTypes[i].dir) {
      file_system_type = kValidTypes[i].type;
      break;
    }
  }

  if (file_system_type == kFileSystemTypeUnknown)
    return false;

  std::string path = net::UnescapeURLComponent(url.path(),
      net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS |
      net::UnescapeRule::CONTROL_CHARS);

  // Ensure the path is relative.
  while (!path.empty() && path[0] == '/')
    path.erase(0, 1);

  base::FilePath converted_path = base::FilePath::FromUTF8Unsafe(path);

  // All parent references should have been resolved in the renderer.
  if (converted_path.ReferencesParent())
    return false;

  if (origin_url)
    *origin_url = url.GetOrigin();
  if (mount_type)
    *mount_type = file_system_type;
  if (virtual_path)
    *virtual_path = converted_path.NormalizePathSeparators().
        StripTrailingSeparators();

  return true;
}

FileSystemURL::FileSystemURL(const GURL& url)
    : mount_type_(kFileSystemTypeUnknown),
      type_(kFileSystemTypeUnknown) {
  is_valid_ = ParseFileSystemSchemeURL(url, &origin_, &mount_type_,
                                       &virtual_path_);
  path_ = virtual_path_;
  type_ = mount_type_;
}

FileSystemURL::FileSystemURL(const GURL& origin,
                             FileSystemType mount_type,
                             const base::FilePath& virtual_path)
    : is_valid_(true),
      origin_(origin),
      mount_type_(mount_type),
      virtual_path_(virtual_path.NormalizePathSeparators()),
      type_(mount_type),
      path_(virtual_path.NormalizePathSeparators()) {
}

FileSystemURL::FileSystemURL(const GURL& origin,
                             FileSystemType mount_type,
                             const base::FilePath& virtual_path,
                             const std::string& mount_filesystem_id,
                             FileSystemType cracked_type,
                             const base::FilePath& cracked_path,
                             const std::string& filesystem_id)
    : is_valid_(true),
      origin_(origin),
      mount_type_(mount_type),
      virtual_path_(virtual_path.NormalizePathSeparators()),
      mount_filesystem_id_(mount_filesystem_id),
      type_(cracked_type),
      path_(cracked_path.NormalizePathSeparators()),
      filesystem_id_(filesystem_id) {
}

FileSystemURL::~FileSystemURL() {}

GURL FileSystemURL::ToGURL() const {
  if (!is_valid_)
    return GURL();

  std::string url = GetFileSystemRootURI(origin_, mount_type_).spec();
  if (url.empty())
    return GURL();

  url.append(virtual_path_.AsUTF8Unsafe());

  // Build nested GURL.
  return GURL(url);
}

std::string FileSystemURL::DebugString() const {
  if (!is_valid_)
    return "invalid filesystem: URL";
  std::ostringstream ss;
  ss << GetFileSystemRootURI(origin_, mount_type_);

  // filesystem_id_ will be non empty for (and only for) cracked URLs.
  if (!filesystem_id_.empty()) {
    ss << virtual_path_.value();
    ss << " (";
    ss << GetFileSystemTypeString(type_) << "@" << filesystem_id_ << ":";
    ss << path_.value();
    ss << ")";
  } else {
    ss << path_.value();
  }
  return ss.str();
}

bool FileSystemURL::IsParent(const FileSystemURL& child) const {
  return IsInSameFileSystem(child) &&
         path().IsParent(child.path());
}

bool FileSystemURL::IsInSameFileSystem(const FileSystemURL& other) const {
  return origin() == other.origin() &&
         type() == other.type() &&
         filesystem_id() == other.filesystem_id();
}

bool FileSystemURL::operator==(const FileSystemURL& that) const {
  return origin_ == that.origin_ &&
      type_ == that.type_ &&
      path_ == that.path_ &&
      filesystem_id_ == that.filesystem_id_ &&
      is_valid_ == that.is_valid_;
}

bool FileSystemURL::Comparator::operator()(const FileSystemURL& lhs,
                                           const FileSystemURL& rhs) const {
  DCHECK(lhs.is_valid_ && rhs.is_valid_);
  if (lhs.origin_ != rhs.origin_)
    return lhs.origin_ < rhs.origin_;
  if (lhs.type_ != rhs.type_)
    return lhs.type_ < rhs.type_;
  if (lhs.filesystem_id_ != rhs.filesystem_id_)
    return lhs.filesystem_id_ < rhs.filesystem_id_;
  return lhs.path_ < rhs.path_;
}

}  // namespace fileapi
