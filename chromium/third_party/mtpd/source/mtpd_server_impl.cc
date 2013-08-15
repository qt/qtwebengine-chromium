// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mtpd_server_impl.h"

#include <base/logging.h>
#include <base/rand_util.h>
#include <base/stl_util.h>

#include "build_config.h"
#include "service_constants.h"

// TODO(thestig) Merge these once libchrome catches up to Chromium's base,
// or when mtpd moves into its own repo. http://crbug.com/221123
#if defined(CROS_BUILD)
#include <base/string_number_conversions.h>
#else
#include <base/strings/string_number_conversions.h>
#endif

namespace mtpd {

namespace {

// Maximum number of bytes to read from the device at one time. This is set low
// enough such that a reasonable device can read this much data before D-Bus
// times out.
const uint32_t kMaxReadCount = 1024 * 1024;

const char kInvalidHandleErrorMessage[] = "Invalid handle ";

void SetInvalidHandleError(const std::string& handle, DBus::Error* error) {
  std::string error_msg = kInvalidHandleErrorMessage + handle;
  error->set(kMtpdServiceError, error_msg.c_str());
}

template<typename ReturnType>
ReturnType InvalidHandle(const std::string& handle, DBus::Error* error) {
  SetInvalidHandleError(handle, error);
  return ReturnType();
}


}  // namespace

MtpdServer::MtpdServer(DBus::Connection& connection)
    : DBus::ObjectAdaptor(connection, kMtpdServicePath),
      device_manager_(this) {
}

MtpdServer::~MtpdServer() {
}

std::vector<std::string> MtpdServer::EnumerateStorages(DBus::Error& error) {
  return device_manager_.EnumerateStorages();
}

std::vector<uint8_t> MtpdServer::GetStorageInfo(const std::string& storageName,
                                                DBus::Error& error) {
  const StorageInfo* info = device_manager_.GetStorageInfo(storageName);
  return info ? info->ToDBusFormat() : StorageInfo().ToDBusFormat();
}

std::string MtpdServer::OpenStorage(const std::string& storageName,
                                    const std::string& mode,
                                    DBus::Error& error) {
  // TODO(thestig) Handle read-write and possibly append-only modes.
  if (mode != kReadOnlyMode) {
    std::string error_msg = "Cannot open " + storageName + " in mode: " + mode;
    error.set(kMtpdServiceError, error_msg.c_str());
    return std::string();
  }

  if (!device_manager_.HasStorage(storageName)) {
    std::string error_msg = "Cannot open unknown storage " + storageName;
    error.set(kMtpdServiceError, error_msg.c_str());
    return std::string();
  }

  std::string id;
  uint32_t random_data[4];
  do {
    base::RandBytes(random_data, sizeof(random_data));
    id = base::HexEncode(random_data, sizeof(random_data));
  } while (ContainsKey(handle_map_, id));

  handle_map_.insert(std::make_pair(id, storageName));
  return id;
}

void MtpdServer::CloseStorage(const std::string& handle, DBus::Error& error) {
  if (handle_map_.erase(handle) == 0)
    SetInvalidHandleError(handle, &error);
}

std::vector<uint8_t> MtpdServer::ReadDirectoryByPath(
    const std::string& handle,
    const std::string& filePath,
    DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty()) {
    SetInvalidHandleError(handle, &error);
    return FileEntry::EmptyFileEntriesToDBusFormat();
  }

  std::vector<FileEntry> directory_listing;
  if (!device_manager_.ReadDirectoryByPath(storage_name,
                                           filePath,
                                           &directory_listing)) {
    error.set(kMtpdServiceError, "ReadDirectoryByPath failed");
    return FileEntry::EmptyFileEntriesToDBusFormat();
  }
  return FileEntry::FileEntriesToDBusFormat(directory_listing);
}

std::vector<uint8_t> MtpdServer::ReadDirectoryById(const std::string& handle,
                                                   const uint32_t& fileId,
                                                   DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty()) {
    SetInvalidHandleError(handle, &error);
    return FileEntry::EmptyFileEntriesToDBusFormat();
  }

  std::vector<FileEntry> directory_listing;
  if (!device_manager_.ReadDirectoryById(storage_name,
                                         fileId,
                                         &directory_listing)) {
    error.set(kMtpdServiceError, "ReadDirectoryById failed");
    return FileEntry::EmptyFileEntriesToDBusFormat();
  }
  return FileEntry::FileEntriesToDBusFormat(directory_listing);
}

std::vector<uint8_t> MtpdServer::ReadFileChunkByPath(
    const std::string& handle,
    const std::string& filePath,
    const uint32_t& offset,
    const uint32_t& count,
    DBus::Error& error) {
  if (count > kMaxReadCount || count == 0) {
    error.set(kMtpdServiceError, "Invalid count for ReadFileChunkByPath");
    return std::vector<uint8_t>();
  }
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty())
    return InvalidHandle<std::vector<uint8_t> >(handle, &error);

  std::vector<uint8_t> file_contents;
  if (!device_manager_.ReadFileChunkByPath(storage_name, filePath, offset,
                                           count, &file_contents)) {
    error.set(kMtpdServiceError, "ReadFileChunkByPath failed");
    return std::vector<uint8_t>();
  }
  return file_contents;
}

std::vector<uint8_t> MtpdServer::ReadFileChunkById(const std::string& handle,
                                                   const uint32_t& fileId,
                                                   const uint32_t& offset,
                                                   const uint32_t& count,
                                                   DBus::Error& error) {
  if (count > kMaxReadCount || count == 0) {
    error.set(kMtpdServiceError, "Invalid count for ReadFileChunkById");
    return std::vector<uint8_t>();
  }
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty())
    return InvalidHandle<std::vector<uint8_t> >(handle, &error);

  std::vector<uint8_t> file_contents;
  if (!device_manager_.ReadFileChunkById(storage_name, fileId, offset, count,
                                         &file_contents)) {
    error.set(kMtpdServiceError, "ReadFileChunkById failed");
    return std::vector<uint8_t>();
  }
  return file_contents;
}

std::vector<uint8_t> MtpdServer::GetFileInfoByPath(const std::string& handle,
                                                   const std::string& filePath,
                                                   DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty()) {
    SetInvalidHandleError(handle, &error);
    return FileEntry().ToDBusFormat();
  }

  FileEntry entry;
  if (!device_manager_.GetFileInfoByPath(storage_name, filePath, &entry)) {
    error.set(kMtpdServiceError, "GetFileInfoByPath failed");
    return FileEntry().ToDBusFormat();
  }
  return entry.ToDBusFormat();
}

std::vector<uint8_t> MtpdServer::GetFileInfoById(const std::string& handle,
                                                 const uint32_t& fileId,
                                                 DBus::Error& error) {
  std::string storage_name = LookupHandle(handle);
  if (storage_name.empty()) {
    SetInvalidHandleError(handle, &error);
    return FileEntry().ToDBusFormat();
  }

  FileEntry entry;
  if (!device_manager_.GetFileInfoById(storage_name, fileId, &entry)) {
    error.set(kMtpdServiceError, "GetFileInfoById failed");
    return FileEntry().ToDBusFormat();
  }
  return entry.ToDBusFormat();
}

bool MtpdServer::IsAlive(DBus::Error& error) {
  return true;
}

void MtpdServer::StorageAttached(const std::string& storage_name) {
  // Fire DBus signal.
  MTPStorageAttached(storage_name);
}

void MtpdServer::StorageDetached(const std::string& storage_name) {
  // Fire DBus signal.
  MTPStorageDetached(storage_name);
}

int MtpdServer::GetDeviceEventDescriptor() const {
  return device_manager_.GetDeviceEventDescriptor();
}

void MtpdServer::ProcessDeviceEvents() {
  device_manager_.ProcessDeviceEvents();
}

std::string MtpdServer::LookupHandle(const std::string& handle) {
  HandleMap::const_iterator it = handle_map_.find(handle);
  return (it == handle_map_.end()) ? std::string() : it->second;
}

}  // namespace mtpd
