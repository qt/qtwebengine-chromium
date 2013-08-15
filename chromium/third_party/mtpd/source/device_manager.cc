// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device_manager.h"

#include <libudev.h>

#include <set>

#include <base/bind.h>
#include <base/logging.h>
#include <base/memory/scoped_ptr.h>
#include <base/stl_util.h>

#include "build_config.h"
#include "device_event_delegate.h"
#include "service_constants.h"

// TODO(thestig) Merge these once libchrome catches up to Chromium's base,
// or when mtpd moves into its own repo. http://crbug.com/221123
#if defined(CROS_BUILD)
#include <base/file_path.h>
#include <base/string_number_conversions.h>
#include <base/string_split.h>
#include <base/stringprintf.h>
#else
#include <base/files/file_path.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/stringprintf.h>
#endif

namespace {

// For GetObjectHandles PTP operations, this tells GetObjectHandles to only
// list the objects of the root of a store.
// Use this when referring to the root node in the context of ReadDirectory().
// This is an implementation detail that is not exposed to the outside.
const uint32_t kPtpGohRootParent = 0xFFFFFFFF;

// Used to identify a PTP USB device interface.
const char kPtpUsbInterfaceClass[] = "6";
const char kPtpUsbInterfaceSubClass[] = "1";
const char kPtpUsbInterfaceProtocol[] = "1";

// Used to identify a vendor-specific USB device interface.
// Manufacturers sometimes do not report MTP/PTP capable devices using the
// well known PTP interface class. See libgphoto2 and libmtp device databases
// for examples.
const char kVendorSpecificUsbInterfaceClass[] = "255";

const char kUsbPrefix[] = "usb";
const char kUDevEventType[] = "udev";
const char kUDevUsbSubsystem[] = "usb";

gboolean GlibRunClosure(gpointer data) {
  base::Closure* cb = reinterpret_cast<base::Closure*>(data);
  cb->Run();
  delete cb;
  return FALSE;
}

std::string RawDeviceToString(const LIBMTP_raw_device_t& device) {
  return base::StringPrintf("%s:%u,%d", kUsbPrefix, device.bus_location,
                            device.devnum);
}

std::string StorageToString(const std::string& usb_bus_str,
                            uint32_t storage_id) {
  return base::StringPrintf("%s:%u", usb_bus_str.c_str(), storage_id);
}

struct LibmtpFileDeleter {
  void operator()(LIBMTP_file_t* file) {
    LIBMTP_destroy_file_t(file);
  }
};

}  // namespace

namespace mtpd {

DeviceManager::DeviceManager(DeviceEventDelegate* delegate)
    : udev_(udev_new()),
      udev_monitor_(NULL),
      udev_monitor_fd_(-1),
      delegate_(delegate),
      weak_ptr_factory_(this) {
  // Set up udev monitoring.
  CHECK(delegate_);
  CHECK(udev_);
  udev_monitor_ = udev_monitor_new_from_netlink(udev_, kUDevEventType);
  CHECK(udev_monitor_);
  int ret = udev_monitor_filter_add_match_subsystem_devtype(udev_monitor_,
                                                            kUDevUsbSubsystem,
                                                            NULL);
  CHECK_EQ(0, ret);
  ret = udev_monitor_enable_receiving(udev_monitor_);
  CHECK_EQ(0, ret);
  udev_monitor_fd_ = udev_monitor_get_fd(udev_monitor_);
  CHECK_GE(udev_monitor_fd_, 0);

  // Initialize libmtp.
  LIBMTP_Init();

  // Trigger a device scan.
  AddDevices(NULL /* no callback source */);
}

DeviceManager::~DeviceManager() {
  RemoveDevices(true /* remove all */);
}

// static
bool DeviceManager::ParseStorageName(const std::string& storage_name,
                                     std::string* usb_bus_str,
                                     uint32_t* storage_id) {
  std::vector<std::string> split_str;
  base::SplitString(storage_name, ':', &split_str);
  if (split_str.size() != 3)
    return false;

  if (split_str[0] != kUsbPrefix)
    return false;

  uint32_t id = 0;
  if (!base::StringToUint(split_str[2], &id))
    return false;

  *usb_bus_str = base::StringPrintf("%s:%s", kUsbPrefix, split_str[1].c_str());
  *storage_id = id;
  return true;
}

// static
bool DeviceManager::IsFolder(const LIBMTP_file_t* path_component,
                             size_t component_idx,
                             size_t num_path_components,
                             uint32_t* file_id) {
  if (path_component->filetype != LIBMTP_FILETYPE_FOLDER)
    return false;

  *file_id = path_component->item_id;
  return true;
}

// static
bool DeviceManager::IsValidComponentInFilePath(
    const LIBMTP_file_t* path_component,
    size_t component_idx,
    size_t num_path_components,
    uint32_t* file_id) {
  bool is_file = (path_component->filetype != LIBMTP_FILETYPE_FOLDER);
  bool is_last = (component_idx == num_path_components - 1);
  if (is_file != is_last)
    return false;

  *file_id = path_component->item_id;
  return true;
}

// static
bool DeviceManager::IsValidComponentInFileOrFolderPath(
    const LIBMTP_file_t* path_component,
    size_t component_idx,
    size_t num_path_components,
    uint32_t* file_id) {
  bool is_file = (path_component->filetype != LIBMTP_FILETYPE_FOLDER);
  bool is_last = (component_idx == num_path_components - 1);
  if (is_file && !is_last)
    return false;

  *file_id = path_component->item_id;
  return true;
}

int DeviceManager::GetDeviceEventDescriptor() const {
  return udev_monitor_fd_;
}

void DeviceManager::ProcessDeviceEvents() {
  udev_device* dev = udev_monitor_receive_device(udev_monitor_);
  CHECK(dev);
  HandleDeviceNotification(dev);
  udev_device_unref(dev);
}

std::vector<std::string> DeviceManager::EnumerateStorages() {
  std::vector<std::string> ret;
  for (MtpDeviceMap::const_iterator device_it = device_map_.begin();
       device_it != device_map_.end();
       ++device_it) {
    const std::string& usb_bus_str = device_it->first;
    const MtpStorageMap& storage_map = device_it->second.second;
    for (MtpStorageMap::const_iterator storage_it = storage_map.begin();
         storage_it != storage_map.end();
         ++storage_it) {
      ret.push_back(StorageToString(usb_bus_str, storage_it->first));
      LOG(INFO) << "Found storage: "
                << StorageToString(usb_bus_str, storage_it->first);
    }
  }
  return ret;
}

bool DeviceManager::HasStorage(const std::string& storage_name) {
  return GetStorageInfo(storage_name) != NULL;
}

const StorageInfo* DeviceManager::GetStorageInfo(
    const std::string& storage_name) {
  std::string usb_bus_str;
  uint32_t storage_id = 0;
  if (!ParseStorageName(storage_name, &usb_bus_str, &storage_id))
    return NULL;

  MtpDeviceMap::const_iterator device_it = device_map_.find(usb_bus_str);
  if (device_it == device_map_.end())
    return NULL;

  const MtpStorageMap& storage_map = device_it->second.second;
  MtpStorageMap::const_iterator storage_it = storage_map.find(storage_id);
  return (storage_it != storage_map.end()) ? &(storage_it->second) : NULL;
}

bool DeviceManager::ReadDirectoryByPath(const std::string& storage_name,
                                        const std::string& file_path,
                                        std::vector<FileEntry>* out) {
  LIBMTP_mtpdevice_t* mtp_device = NULL;
  uint32_t storage_id = 0;
  if (!GetDeviceAndStorageId(storage_name, &mtp_device, &storage_id))
    return false;

  uint32_t file_id = 0;
  if (!PathToFileId(mtp_device, storage_id, file_path, IsFolder, &file_id))
    return false;
  return ReadDirectory(mtp_device, storage_id, file_id, out);
}

bool DeviceManager::ReadDirectoryById(const std::string& storage_name,
                                      uint32_t file_id,
                                      std::vector<FileEntry>* out) {
  LIBMTP_mtpdevice_t* mtp_device = NULL;
  uint32_t storage_id = 0;
  if (!GetDeviceAndStorageId(storage_name, &mtp_device, &storage_id))
    return false;
  if (file_id == kRootFileId)
    file_id = kPtpGohRootParent;
  return ReadDirectory(mtp_device, storage_id, file_id, out);
}

bool DeviceManager::ReadFileChunkByPath(const std::string& storage_name,
                                        const std::string& file_path,
                                        uint32_t offset,
                                        uint32_t count,
                                        std::vector<uint8_t>* out) {
  LIBMTP_mtpdevice_t* mtp_device = NULL;
  uint32_t storage_id = 0;
  if (!GetDeviceAndStorageId(storage_name, &mtp_device, &storage_id))
    return false;

  uint32_t file_id = 0;
  if (!PathToFileId(mtp_device, storage_id, file_path,
                    IsValidComponentInFilePath, &file_id)) {
    return false;
  }
  return ReadFileChunk(mtp_device, file_id, offset, count, out);
}

bool DeviceManager::ReadFileChunkById(const std::string& storage_name,
                                      uint32_t file_id,
                                      uint32_t offset,
                                      uint32_t count,
                                      std::vector<uint8_t>* out) {
  LIBMTP_mtpdevice_t* mtp_device = NULL;
  uint32_t storage_id = 0;
  if (!GetDeviceAndStorageId(storage_name, &mtp_device, &storage_id))
    return false;
  return ReadFileChunk(mtp_device, file_id, offset, count, out);
}

bool DeviceManager::GetFileInfoByPath(const std::string& storage_name,
                                      const std::string& file_path,
                                      FileEntry* out) {
  LIBMTP_mtpdevice_t* mtp_device = NULL;
  uint32_t storage_id = 0;
  if (!GetDeviceAndStorageId(storage_name, &mtp_device, &storage_id))
    return false;

  uint32_t file_id = 0;
  if (!PathToFileId(mtp_device, storage_id, file_path,
                    IsValidComponentInFileOrFolderPath, &file_id)) {
    return false;
  }

  if (file_id == kPtpGohRootParent)
    file_id = kRootFileId;
  return GetFileInfo(mtp_device, storage_id, file_id, out);
}

bool DeviceManager::GetFileInfoById(const std::string& storage_name,
                                    uint32_t file_id,
                                    FileEntry* out) {
  LIBMTP_mtpdevice_t* mtp_device = NULL;
  uint32_t storage_id = 0;
  if (!GetDeviceAndStorageId(storage_name, &mtp_device, &storage_id))
    return false;
  return GetFileInfo(mtp_device, storage_id, file_id, out);
}

bool DeviceManager::AddStorageForTest(const std::string& storage_name,
                                      const StorageInfo& storage_info) {
  std::string device_location;
  uint32_t storage_id;
  if (!ParseStorageName(storage_name, &device_location, &storage_id))
    return false;

  MtpDeviceMap::iterator it = device_map_.find(device_location);
  if (it == device_map_.end()) {
    // New device case.
    MtpStorageMap new_storage_map;
    new_storage_map.insert(std::make_pair(storage_id, storage_info));
    MtpDevice new_mtp_device =
        std::make_pair(static_cast<LIBMTP_mtpdevice_t*>(NULL),
                       new_storage_map);
    device_map_.insert(std::make_pair(device_location, new_mtp_device));
    return true;
  }

  // Existing device case.
  // There should be no real LIBMTP_mtpdevice_t device for this dummy storage.
  MtpDevice& existing_mtp_device = it->second;
  if (existing_mtp_device.first)
    return false;

  // And the storage should not already exist.
  MtpStorageMap& existing_mtp_storage_map = existing_mtp_device.second;
  if (ContainsKey(existing_mtp_storage_map, storage_id))
    return false;

  existing_mtp_storage_map.insert(std::make_pair(storage_id, storage_info));
  return true;
}

bool DeviceManager::PathToFileId(LIBMTP_mtpdevice_t* device,
                                 uint32_t storage_id,
                                 const std::string& file_path,
                                 ProcessPathComponentFunc process_func,
                                 uint32_t* file_id) {
  std::vector<base::FilePath::StringType> path_components;
  base::FilePath(file_path).GetComponents(&path_components);
  uint32_t current_file_id = kPtpGohRootParent;
  const size_t num_path_components = path_components.size();
  for (size_t i = 0; i < num_path_components; ++i) {
    if (path_components[i] == "/")
      continue;

    LIBMTP_file_t* files =
        LIBMTP_Get_Files_And_Folders(device, storage_id, current_file_id);
    // Iterate through all files.
    const uint32_t old_file_id = current_file_id;
    LIBMTP_file_t* file = files;
    while (file != NULL) {
      scoped_ptr<LIBMTP_file_t, LibmtpFileDeleter> current_file(file);
      file = file->next;
      if (current_file.get()->filename != path_components[i])
        continue;

      // Found matching file name. See if it is valid.
      if (!process_func(current_file.get(), i, num_path_components,
                        &current_file_id)) {
        return false;
      }
    }
    // If no matching component was found.
    if (old_file_id == current_file_id)
      return false;
  }
  // Successfully iterated through all path components.
  *file_id = current_file_id;
  return true;
}

bool DeviceManager::ReadDirectory(LIBMTP_mtpdevice_t* device,
                                  uint32_t storage_id,
                                  uint32_t file_id,
                                  std::vector<FileEntry>* out) {
  LIBMTP_file_t* file =
      LIBMTP_Get_Files_And_Folders(device, storage_id, file_id);
  while (file != NULL) {
    scoped_ptr<LIBMTP_file_t, LibmtpFileDeleter> current_file(file);
    file = file->next;
    out->push_back(FileEntry(*current_file));
  }
  return true;
}

bool DeviceManager::ReadFileChunk(LIBMTP_mtpdevice_t* device,
                                  uint32_t file_id,
                                  uint32_t offset,
                                  uint32_t count,
                                  std::vector<uint8_t>* out) {
  // The root node is a virtual node and cannot be read from.
  if (file_id == kRootFileId)
    return false;

  uint8_t* data = NULL;
  uint32_t bytes_read = 0;
  int transfer_status = LIBMTP_Get_File_Chunk(device,
                                              file_id,
                                              offset,
                                              count,
                                              &data,
                                              &bytes_read);

  // Own |data| in a scoper so it gets freed when this function returns.
  scoped_ptr_malloc<uint8_t> scoped_data(data);

  if (transfer_status != 0 || bytes_read != count)
    return false;

  for (size_t i = 0; i < count; ++i)
    out->push_back(data[i]);
  return true;
}

bool DeviceManager::GetFileInfo(LIBMTP_mtpdevice_t* device,
                                uint32_t storage_id,
                                uint32_t file_id,
                                FileEntry* out) {
  LIBMTP_file_t* file = (file_id == kRootFileId) ?
      LIBMTP_new_file_t() :
      LIBMTP_Get_Filemetadata(device, file_id);
  if (!file)
    return false;

  // LIBMTP_Get_Filemetadata() does not know how to handle the root node, so
  // fill in relevant fields in the struct manually. The rest of the struct has
  // already been initialized by LIBMTP_new_file_t().
  if (file_id == kRootFileId) {
    file->storage_id = storage_id;
    file->filename = strdup("/");
    file->filetype = LIBMTP_FILETYPE_FOLDER;
  }

  *out = FileEntry(*file);
  LIBMTP_destroy_file_t(file);
  return true;
}

bool DeviceManager::GetDeviceAndStorageId(const std::string& storage_name,
                                          LIBMTP_mtpdevice_t** mtp_device,
                                          uint32_t* storage_id) {
  std::string usb_bus_str;
  uint32_t id = 0;
  if (!ParseStorageName(storage_name, &usb_bus_str, &id))
    return false;

  MtpDeviceMap::const_iterator device_it = device_map_.find(usb_bus_str);
  if (device_it == device_map_.end())
    return false;

  const MtpStorageMap& storage_map = device_it->second.second;
  if (!ContainsKey(storage_map, id))
    return false;

  *storage_id = id;
  *mtp_device = device_it->second.first;
  return true;
}

void DeviceManager::HandleDeviceNotification(udev_device* device) {
  const char* action = udev_device_get_property_value(device, "ACTION");
  const char* interface = udev_device_get_property_value(device, "INTERFACE");
  if (!action || !interface)
    return;

  // Check the USB interface. Since this gets called many times by udev for a
  // given physical action, use the udev "INTERFACE" event property as a quick
  // way of getting one unique and interesting udev event for a given physical
  // action. At the same time, do some light filtering and ignore events for
  // uninteresting devices.
  const std::string kEventInterface(interface);
  std::vector<std::string> split_usb_interface;
  base::SplitString(kEventInterface, '/', &split_usb_interface);
  if (split_usb_interface.size() != 3)
    return;

  // Check to see if the device has a vendor-specific interface class.
  // In this case, continue and let libmtp figure it out.
  const std::string& usb_interface_class = split_usb_interface[0];
  const std::string& usb_interface_subclass = split_usb_interface[1];
  const std::string& usb_interface_protocol = split_usb_interface[2];
  bool is_interesting_device =
      (usb_interface_class == kVendorSpecificUsbInterfaceClass);
  if (!is_interesting_device) {
    // Many MTP/PTP devices have this PTP interface.
    is_interesting_device =
        (usb_interface_class == kPtpUsbInterfaceClass &&
         usb_interface_subclass == kPtpUsbInterfaceSubClass &&
         usb_interface_protocol == kPtpUsbInterfaceProtocol);
  }
  if (!is_interesting_device)
    return;

  // Handle the action.
  const std::string kEventAction(action);
  if (kEventAction == "add") {
    // Some devices do not respond well when immediately probed. Thus there is
    // a 1 second wait here to give the device to settle down.
    GSource* source = g_timeout_source_new_seconds(1);
    base::Closure* cb =
        new base::Closure(base::Bind(&DeviceManager::AddDevices,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     source));
    g_source_set_callback(source, &GlibRunClosure, cb, NULL);
    g_source_attach(source, NULL);
    return;
  }
  if (kEventAction == "remove") {
    RemoveDevices(false /* !remove_all */);
    return;
  }
  // udev notes the existence of other actions like "change" and "move", but
  // they have never been observed with real MTP/PTP devices in testing.
}

void DeviceManager::AddDevices(GSource* source) {
  if (source) {
    // Matches g_source_attach().
    g_source_destroy(source);
    // Matches the implicit add-ref in g_timeout_source_new_seconds().
    g_source_unref(source);
  }

  // Get raw devices.
  LIBMTP_raw_device_t* raw_devices = NULL;
  int raw_devices_count = 0;
  LIBMTP_error_number_t err =
      LIBMTP_Detect_Raw_Devices(&raw_devices, &raw_devices_count);
  if (err != LIBMTP_ERROR_NONE) {
    LOG(ERROR) << "LIBMTP_Detect_Raw_Devices failed with " << err;
    return;
  }

  // Iterate through raw devices.
  for (int i = 0; i < raw_devices_count; ++i) {
    const std::string usb_bus_str = RawDeviceToString(raw_devices[i]);
    // Skip devices that have already been opened.
    if (ContainsKey(device_map_, usb_bus_str))
      continue;

    // Open the mtp device.
    LIBMTP_mtpdevice_t* mtp_device =
        LIBMTP_Open_Raw_Device_Uncached(&raw_devices[i]);
    if (!mtp_device) {
      LOG(ERROR) << "LIBMTP_Open_Raw_Device_Uncached failed for "
                 << usb_bus_str;
      continue;
    }

    // Fetch fallback vendor / product info.
    scoped_ptr_malloc<char> duplicated_string;
    duplicated_string.reset(LIBMTP_Get_Manufacturername(mtp_device));
    std::string fallback_vendor;
    if (duplicated_string.get())
      fallback_vendor = duplicated_string.get();

    duplicated_string.reset(LIBMTP_Get_Modelname(mtp_device));
    std::string fallback_product;
    if (duplicated_string.get())
      fallback_product = duplicated_string.get();

    // Iterate through storages on the device and add them.
    MtpStorageMap storage_map;
    for (LIBMTP_devicestorage_t* storage = mtp_device->storage;
         storage != NULL;
         storage = storage->next) {
      const std::string storage_name =
          StorageToString(usb_bus_str, storage->id);
      StorageInfo info(storage_name,
                       raw_devices[i].device_entry,
                       *storage,
                       fallback_vendor,
                       fallback_product);
      bool storage_added =
          storage_map.insert(std::make_pair(storage->id, info)).second;
      CHECK(storage_added);
      delegate_->StorageAttached(storage_name);
      LOG(INFO) << "Added storage " << storage_name;
    }
    bool device_added = device_map_.insert(
        std::make_pair(usb_bus_str,
                       std::make_pair(mtp_device, storage_map))).second;
    CHECK(device_added);
    LOG(INFO) << "Added device " << usb_bus_str << " with "
              << storage_map.size() << " storages";
  }
  free(raw_devices);
}

void DeviceManager::RemoveDevices(bool remove_all) {
  LIBMTP_raw_device_t* raw_devices = NULL;
  int raw_devices_count = 0;

  if (!remove_all) {
    LIBMTP_error_number_t err =
        LIBMTP_Detect_Raw_Devices(&raw_devices, &raw_devices_count);
    if (!(err == LIBMTP_ERROR_NONE || err == LIBMTP_ERROR_NO_DEVICE_ATTACHED)) {
      LOG(ERROR) << "LIBMTP_Detect_Raw_Devices failed with " << err;
      return;
    }
  }

  // Populate |devices_set| with all known attached devices.
  typedef std::set<std::string> MtpDeviceSet;
  MtpDeviceSet devices_set;
  for (MtpDeviceMap::const_iterator it = device_map_.begin();
       it != device_map_.end();
       ++it) {
    devices_set.insert(it->first);
  }

  // And remove the ones that are still attached.
  for (int i = 0; i < raw_devices_count; ++i)
    devices_set.erase(RawDeviceToString(raw_devices[i]));

  // The ones left in the set are the detached devices.
  for (MtpDeviceSet::const_iterator it = devices_set.begin();
       it != devices_set.end();
       ++it) {
    LOG(INFO) << "Removed " << *it;
    MtpDeviceMap::iterator device_it = device_map_.find(*it);
    if (device_it == device_map_.end()) {
      NOTREACHED();
      continue;
    }

    // Remove all the storages on that device.
    const std::string& usb_bus_str = device_it->first;
    const MtpStorageMap& storage_map = device_it->second.second;
    for (MtpStorageMap::const_iterator storage_it = storage_map.begin();
         storage_it != storage_map.end();
         ++storage_it) {
      delegate_->StorageDetached(
          StorageToString(usb_bus_str, storage_it->first));
    }

    // Delete the device's map entry and cleanup.
    LIBMTP_mtpdevice_t* mtp_device = device_it->second.first;
    device_map_.erase(device_it);

    // |mtp_device| can be NULL in testing.
    if (!mtp_device)
      continue;

    // When |remove_all| is false, the device has already been detached
    // and this runs after the fact. As such, this call will very
    // likely fail and spew a bunch of error messages. Call it anyway to
    // let libmtp do any cleanup it can.
    LIBMTP_Release_Device(mtp_device);
  }
}

}  // namespace mtpd
