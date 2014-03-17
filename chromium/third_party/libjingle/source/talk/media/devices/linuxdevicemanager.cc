/*
 * libjingle
 * Copyright 2004 Google Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
 * EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "talk/media/devices/linuxdevicemanager.h"

#include <unistd.h>
#include "talk/base/fileutils.h"
#include "talk/base/linux.h"
#include "talk/base/logging.h"
#include "talk/base/pathutils.h"
#include "talk/base/physicalsocketserver.h"
#include "talk/base/stream.h"
#include "talk/base/stringutils.h"
#include "talk/base/thread.h"
#include "talk/media/base/mediacommon.h"
#include "talk/media/devices/libudevsymboltable.h"
#include "talk/media/devices/v4llookup.h"
#include "talk/sound/platformsoundsystem.h"
#include "talk/sound/platformsoundsystemfactory.h"
#include "talk/sound/sounddevicelocator.h"
#include "talk/sound/soundsysteminterface.h"

namespace cricket {

DeviceManagerInterface* DeviceManagerFactory::Create() {
  return new LinuxDeviceManager();
}

class LinuxDeviceWatcher
    : public DeviceWatcher,
      private talk_base::Dispatcher {
 public:
  explicit LinuxDeviceWatcher(DeviceManagerInterface* dm);
  virtual ~LinuxDeviceWatcher();
  virtual bool Start();
  virtual void Stop();

 private:
  virtual uint32 GetRequestedEvents();
  virtual void OnPreEvent(uint32 ff);
  virtual void OnEvent(uint32 ff, int err);
  virtual int GetDescriptor();
  virtual bool IsDescriptorClosed();

  DeviceManagerInterface* manager_;
  LibUDevSymbolTable libudev_;
  struct udev* udev_;
  struct udev_monitor* udev_monitor_;
  bool registered_;
};

static const char* const kFilteredAudioDevicesName[] = {
#if defined(CHROMEOS)
    "surround40:",
    "surround41:",
    "surround50:",
    "surround51:",
    "surround71:",
    "iec958:",      // S/PDIF
#endif
    NULL,
};
static const char* kFilteredVideoDevicesName[] = {
    NULL,
};

LinuxDeviceManager::LinuxDeviceManager()
    : sound_system_(new PlatformSoundSystemFactory()) {
  set_watcher(new LinuxDeviceWatcher(this));
}

LinuxDeviceManager::~LinuxDeviceManager() {
}

bool LinuxDeviceManager::GetAudioDevices(bool input,
                                         std::vector<Device>* devs) {
  devs->clear();
  if (!sound_system_.get()) {
    return false;
  }
  SoundSystemInterface::SoundDeviceLocatorList list;
  bool success;
  if (input) {
    success = sound_system_->EnumerateCaptureDevices(&list);
  } else {
    success = sound_system_->EnumeratePlaybackDevices(&list);
  }
  if (!success) {
    LOG(LS_ERROR) << "Can't enumerate devices";
    sound_system_.release();
    return false;
  }
  // We have to start the index at 1 because webrtc VoiceEngine puts the default
  // device at index 0, but Enumerate(Capture|Playback)Devices does not include
  // a locator for the default device.
  int index = 1;
  for (SoundSystemInterface::SoundDeviceLocatorList::iterator i = list.begin();
       i != list.end();
       ++i, ++index) {
    devs->push_back(Device((*i)->name(), index));
  }
  SoundSystemInterface::ClearSoundDeviceLocatorList(&list);
  sound_system_.release();
  return FilterDevices(devs, kFilteredAudioDevicesName);
}

static const std::string kVideoMetaPathK2_4("/proc/video/dev/");
static const std::string kVideoMetaPathK2_6("/sys/class/video4linux/");

enum MetaType { M2_4, M2_6, NONE };

static void ScanDeviceDirectory(const std::string& devdir,
                                std::vector<Device>* devices) {
  talk_base::scoped_ptr<talk_base::DirectoryIterator> directoryIterator(
      talk_base::Filesystem::IterateDirectory());

  if (directoryIterator->Iterate(talk_base::Pathname(devdir))) {
    do {
      std::string filename = directoryIterator->Name();
      std::string device_name = devdir + filename;
      if (!directoryIterator->IsDots()) {
        if (filename.find("video") == 0 &&
            V4LLookup::IsV4L2Device(device_name)) {
          devices->push_back(Device(device_name, device_name));
        }
      }
    } while (directoryIterator->Next());
  }
}

static std::string GetVideoDeviceNameK2_6(const std::string& device_meta_path) {
  std::string device_name;

  talk_base::scoped_ptr<talk_base::FileStream> device_meta_stream(
      talk_base::Filesystem::OpenFile(device_meta_path, "r"));

  if (device_meta_stream) {
    if (device_meta_stream->ReadLine(&device_name) != talk_base::SR_SUCCESS) {
      LOG(LS_ERROR) << "Failed to read V4L2 device meta " << device_meta_path;
    }
    device_meta_stream->Close();
  }

  return device_name;
}

static std::string Trim(const std::string& s, const std::string& drop = " \t") {
  std::string::size_type first = s.find_first_not_of(drop);
  std::string::size_type last  = s.find_last_not_of(drop);

  if (first == std::string::npos || last == std::string::npos)
    return std::string("");

  return s.substr(first, last - first + 1);
}

static std::string GetVideoDeviceNameK2_4(const std::string& device_meta_path) {
  talk_base::ConfigParser::MapVector all_values;

  talk_base::ConfigParser config_parser;
  talk_base::FileStream* file_stream =
      talk_base::Filesystem::OpenFile(device_meta_path, "r");

  if (file_stream == NULL) return "";

  config_parser.Attach(file_stream);
  config_parser.Parse(&all_values);

  for (talk_base::ConfigParser::MapVector::iterator i = all_values.begin();
      i != all_values.end(); ++i) {
    talk_base::ConfigParser::SimpleMap::iterator device_name_i =
        i->find("name");

    if (device_name_i != i->end()) {
      return device_name_i->second;
    }
  }

  return "";
}

static std::string GetVideoDeviceName(MetaType meta,
    const std::string& device_file_name) {
  std::string device_meta_path;
  std::string device_name;
  std::string meta_file_path;

  if (meta == M2_6) {
    meta_file_path = kVideoMetaPathK2_6 + device_file_name + "/name";

    LOG(LS_INFO) << "Trying " + meta_file_path;
    device_name = GetVideoDeviceNameK2_6(meta_file_path);

    if (device_name.empty()) {
      meta_file_path = kVideoMetaPathK2_6 + device_file_name + "/model";

      LOG(LS_INFO) << "Trying " << meta_file_path;
      device_name = GetVideoDeviceNameK2_6(meta_file_path);
    }
  } else {
    meta_file_path = kVideoMetaPathK2_4 + device_file_name;
    LOG(LS_INFO) << "Trying " << meta_file_path;
    device_name = GetVideoDeviceNameK2_4(meta_file_path);
  }

  if (device_name.empty()) {
    device_name = "/dev/" + device_file_name;
    LOG(LS_ERROR)
      << "Device name not found, defaulting to device path " << device_name;
  }

  LOG(LS_INFO) << "Name for " << device_file_name << " is " << device_name;

  return Trim(device_name);
}

static void ScanV4L2Devices(std::vector<Device>* devices) {
  LOG(LS_INFO) << ("Enumerating V4L2 devices");

  MetaType meta;
  std::string metadata_dir;

  talk_base::scoped_ptr<talk_base::DirectoryIterator> directoryIterator(
      talk_base::Filesystem::IterateDirectory());

  // Try and guess kernel version
  if (directoryIterator->Iterate(kVideoMetaPathK2_6)) {
    meta = M2_6;
    metadata_dir = kVideoMetaPathK2_6;
  } else if (directoryIterator->Iterate(kVideoMetaPathK2_4)) {
    meta = M2_4;
    metadata_dir = kVideoMetaPathK2_4;
  } else {
    meta = NONE;
  }

  if (meta != NONE) {
    LOG(LS_INFO) << "V4L2 device metadata found at " << metadata_dir;

    do {
      std::string filename = directoryIterator->Name();

      if (filename.find("video") == 0) {
        std::string device_path = "/dev/" + filename;

        if (V4LLookup::IsV4L2Device(device_path)) {
          devices->push_back(
              Device(GetVideoDeviceName(meta, filename), device_path));
        }
      }
    } while (directoryIterator->Next());
  } else {
    LOG(LS_ERROR) << "Unable to detect v4l2 metadata directory";
  }

  if (devices->size() == 0) {
    LOG(LS_INFO) << "Plan B. Scanning all video devices in /dev directory";
    ScanDeviceDirectory("/dev/", devices);
  }

  LOG(LS_INFO) << "Total V4L2 devices found : " << devices->size();
}

bool LinuxDeviceManager::GetVideoCaptureDevices(std::vector<Device>* devices) {
  devices->clear();
  ScanV4L2Devices(devices);
  return FilterDevices(devices, kFilteredVideoDevicesName);
}

LinuxDeviceWatcher::LinuxDeviceWatcher(DeviceManagerInterface* dm)
    : DeviceWatcher(dm),
      manager_(dm),
      udev_(NULL),
      udev_monitor_(NULL),
      registered_(false) {
}

LinuxDeviceWatcher::~LinuxDeviceWatcher() {
}

bool LinuxDeviceWatcher::Start() {
  // We deliberately return true in the failure paths here because libudev is
  // not a critical component of a Linux system so it may not be present/usable,
  // and we don't want to halt LinuxDeviceManager initialization in such a case.
  if (!libudev_.Load() || IsWrongLibUDevAbiVersion(libudev_.GetDllHandle())) {
    LOG(LS_WARNING)
        << "libudev not present/usable; LinuxDeviceWatcher disabled";
    return true;
  }
  udev_ = libudev_.udev_new()();
  if (!udev_) {
    LOG_ERR(LS_ERROR) << "udev_new()";
    return true;
  }
  // The second argument here is the event source. It can be either "kernel" or
  // "udev", but "udev" is the only correct choice. Apps listen on udev and the
  // udev daemon in turn listens on the kernel.
  udev_monitor_ = libudev_.udev_monitor_new_from_netlink()(udev_, "udev");
  if (!udev_monitor_) {
    LOG_ERR(LS_ERROR) << "udev_monitor_new_from_netlink()";
    return true;
  }
  // We only listen for changes in the video devices. Audio devices are more or
  // less unimportant because receiving device change notifications really only
  // matters for broadcasting updated send/recv capabilities based on whether
  // there is at least one device available, and almost all computers have at
  // least one audio device. Also, PulseAudio device notifications don't come
  // from the udev daemon, they come from the PulseAudio daemon, so we'd only
  // want to listen for audio device changes from udev if using ALSA. For
  // simplicity, we don't bother with any audio stuff at all.
  if (libudev_.udev_monitor_filter_add_match_subsystem_devtype()(
          udev_monitor_, "video4linux", NULL) < 0) {
    LOG_ERR(LS_ERROR) << "udev_monitor_filter_add_match_subsystem_devtype()";
    return true;
  }
  if (libudev_.udev_monitor_enable_receiving()(udev_monitor_) < 0) {
    LOG_ERR(LS_ERROR) << "udev_monitor_enable_receiving()";
    return true;
  }
  static_cast<talk_base::PhysicalSocketServer*>(
      talk_base::Thread::Current()->socketserver())->Add(this);
  registered_ = true;
  return true;
}

void LinuxDeviceWatcher::Stop() {
  if (registered_) {
    static_cast<talk_base::PhysicalSocketServer*>(
        talk_base::Thread::Current()->socketserver())->Remove(this);
    registered_ = false;
  }
  if (udev_monitor_) {
    libudev_.udev_monitor_unref()(udev_monitor_);
    udev_monitor_ = NULL;
  }
  if (udev_) {
    libudev_.udev_unref()(udev_);
    udev_ = NULL;
  }
  libudev_.Unload();
}

uint32 LinuxDeviceWatcher::GetRequestedEvents() {
  return talk_base::DE_READ;
}

void LinuxDeviceWatcher::OnPreEvent(uint32 ff) {
  // Nothing to do.
}

void LinuxDeviceWatcher::OnEvent(uint32 ff, int err) {
  udev_device* device = libudev_.udev_monitor_receive_device()(udev_monitor_);
  if (!device) {
    // Probably the socket connection to the udev daemon was terminated (perhaps
    // the daemon crashed or is being restarted?).
    LOG_ERR(LS_WARNING) << "udev_monitor_receive_device()";
    // Stop listening to avoid potential livelock (an fd with EOF in it is
    // always considered readable).
    static_cast<talk_base::PhysicalSocketServer*>(
        talk_base::Thread::Current()->socketserver())->Remove(this);
    registered_ = false;
    return;
  }
  // Else we read the device successfully.

  // Since we already have our own filesystem-based device enumeration code, we
  // simply re-enumerate rather than inspecting the device event.
  libudev_.udev_device_unref()(device);
  manager_->SignalDevicesChange();
}

int LinuxDeviceWatcher::GetDescriptor() {
  return libudev_.udev_monitor_get_fd()(udev_monitor_);
}

bool LinuxDeviceWatcher::IsDescriptorClosed() {
  // If it is closed then we will just get an error in
  // udev_monitor_receive_device and unregister, so we don't need to check for
  // it separately.
  return false;
}

};  // namespace cricket
