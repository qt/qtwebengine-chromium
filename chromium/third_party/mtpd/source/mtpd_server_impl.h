// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MTPD_SERVER_IMPL_H_
#define MTPD_SERVER_IMPL_H_

#include <string>
#include <vector>

#include <base/basictypes.h>
#include <base/compiler_specific.h>

#include "device_event_delegate.h"
#include "device_manager.h"
#include "file_entry.h"
#include "mtpd_server/mtpd_server.h"

namespace mtpd {

class DeviceManager;

// The D-bus server for the mtpd daemon.
//
// Example Usage:
//
// DBus::Connection server_conn = DBus::Connection::SystemBus();
// server_conn.request_name("org.chromium.Mtpd");
// MtpdServer* server = new(std::nothrow) MtpdServer(server_conn, &manager);
//
// At this point the server should be attached to the main loop.

class MtpdServer : public org::chromium::Mtpd_adaptor,
                   public DBus::ObjectAdaptor,
                   public DeviceEventDelegate {
 public:
  explicit MtpdServer(DBus::Connection& connection);
  virtual ~MtpdServer();

  // org::chromium::Mtpd_adaptor implementation.
  virtual std::vector<std::string> EnumerateStorages(
      DBus::Error& error) OVERRIDE;
  virtual std::vector<uint8_t> GetStorageInfo(const std::string& storageName,
                                              DBus::Error& error) OVERRIDE;
  virtual std::string OpenStorage(const std::string& storageName,
                                  const std::string& mode,
                                  DBus::Error& error) OVERRIDE;
  virtual void CloseStorage(const std::string& handle,
                            DBus::Error& error) OVERRIDE;
  virtual std::vector<uint8_t> ReadDirectoryByPath(const std::string& handle,
                                                   const std::string& filePath,
                                                   DBus::Error& error) OVERRIDE;
  virtual std::vector<uint8_t> ReadDirectoryById(const std::string& handle,
                                                 const uint32_t& fileId,
                                                 DBus::Error& error) OVERRIDE;
  virtual std::vector<uint8_t> ReadFileChunkByPath(const std::string& handle,
                                                   const std::string& filePath,
                                                   const uint32_t& offset,
                                                   const uint32_t& count,
                                                   DBus::Error& error) OVERRIDE;
  virtual std::vector<uint8_t> ReadFileChunkById(const std::string& handle,
                                                 const uint32_t& fileId,
                                                 const uint32_t& offset,
                                                 const uint32_t& count,
                                                 DBus::Error& error) OVERRIDE;
  virtual std::vector<uint8_t> GetFileInfoByPath(const std::string& handle,
                                                 const std::string& filePath,
                                                 DBus::Error& error) OVERRIDE;
  virtual std::vector<uint8_t> GetFileInfoById(const std::string& handle,
                                               const uint32_t& fileId,
                                               DBus::Error& error) OVERRIDE;
  virtual bool IsAlive(DBus::Error& error) OVERRIDE;

  // DeviceEventDelegate implementation.
  virtual void StorageAttached(const std::string& storage_name) OVERRIDE;
  virtual void StorageDetached(const std::string& storage_name) OVERRIDE;

  // Returns a file descriptor for monitoring device events.
  int GetDeviceEventDescriptor() const;

  // Processes the available device events.
  void ProcessDeviceEvents();

 private:
  // Handle to StorageName map.
  typedef std::map<std::string, std::string> HandleMap;

  // Returns the StorageName for a handle, or an empty string on failure.
  std::string LookupHandle(const std::string& handle);

  HandleMap handle_map_;

  // Device manager needs to be last, so it is the first to be destroyed.
  DeviceManager device_manager_;

  DISALLOW_COPY_AND_ASSIGN(MtpdServer);
};

}  // namespace mtpd

#endif  // MTPD_SERVER_IMPL_H_
