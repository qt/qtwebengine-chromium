// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_INERTIAL_SENSOR_SERVICE_H_
#define CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_INERTIAL_SENSOR_SERVICE_H_

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/shared_memory.h"
#include "base/memory/singleton.h"
#include "base/threading/thread_checker.h"
#include "content/browser/device_orientation/inertial_sensor_consts.h"
#include "content/common/content_export.h"

namespace content {

class DataFetcherSharedMemory;
class RenderProcessHost;

// Owns the DeviceMotionProvider (the background polling thread) and keeps
// track of the number of consumers currently using the data (and pausing
// the provider when not in use).
class CONTENT_EXPORT DeviceInertialSensorService {
 public:
  // Returns the DeviceInertialSensorService singleton.
  static DeviceInertialSensorService* GetInstance();

  // Increments the number of users of the provider. The Provider is running
  // when there's > 0 users, and is paused when the count drops to 0.
  // Must be called on the I/O thread.
  void AddConsumer(ConsumerType consumer_type);

  // Removes a consumer. Should be matched with an AddConsumer call.
  // Must be called on the I/O thread.
  void RemoveConsumer(ConsumerType cosumer_type);

  // Returns the shared memory handle of the device motion data duplicated
  // into the given process.
  base::SharedMemoryHandle GetSharedMemoryHandleForProcess(
      ConsumerType consumer_type, base::ProcessHandle handle);

  // Stop/join with the background polling thread in |provider_|.
  void Shutdown();

  // Injects a custom data fetcher for testing purposes. This class takes
  // ownership of the injected object.
  void SetDataFetcherForTests(DataFetcherSharedMemory* test_data_fetcher);

 private:
  friend struct DefaultSingletonTraits<DeviceInertialSensorService>;

  DeviceInertialSensorService();
  virtual ~DeviceInertialSensorService();

  bool ChangeNumberConsumers(ConsumerType consumer_type,
      int delta);
  int GetNumberConsumers(ConsumerType consumer_type) const;

  int num_motion_readers_;
  int num_orientation_readers_;
  bool is_shutdown_;
  scoped_ptr<DataFetcherSharedMemory> data_fetcher_;
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInertialSensorService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVICE_ORIENTATION_DEVICE_INERTIAL_SENSOR_SERVICE_H_
