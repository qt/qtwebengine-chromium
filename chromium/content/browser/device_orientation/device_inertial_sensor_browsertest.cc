// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/synchronization/waitable_event.h"
#include "content/browser/device_orientation/data_fetcher_shared_memory.h"
#include "content/browser/device_orientation/device_inertial_sensor_service.h"
#include "content/common/device_orientation/device_motion_hardware_buffer.h"
#include "content/common/device_orientation/device_orientation_hardware_buffer.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/shell/browser/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"

namespace content {

namespace {

class FakeDataFetcher : public DataFetcherSharedMemory {
 public:
  FakeDataFetcher()
      : started_orientation_(false, false),
        stopped_orientation_(false, false),
        started_motion_(false, false),
        stopped_motion_(false, false) {
  }
  virtual ~FakeDataFetcher() { }

  virtual bool Start(ConsumerType consumer_type, void* buffer) OVERRIDE {
    EXPECT_TRUE(buffer);

    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        UpdateMotion(static_cast<DeviceMotionHardwareBuffer*>(buffer));
        started_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        UpdateOrientation(
            static_cast<DeviceOrientationHardwareBuffer*>(buffer));
        started_orientation_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  virtual bool Stop(ConsumerType consumer_type) OVERRIDE {
    switch (consumer_type) {
      case CONSUMER_TYPE_MOTION:
        stopped_motion_.Signal();
        break;
      case CONSUMER_TYPE_ORIENTATION:
        stopped_orientation_.Signal();
        break;
      default:
        return false;
    }
    return true;
  }

  virtual void Fetch(unsigned consumer_bitmask) OVERRIDE {
    FAIL() << "fetch should not be called";
  }

  virtual FetcherType GetType() const OVERRIDE {
    return FETCHER_TYPE_DEFAULT;
  }

  void UpdateMotion(DeviceMotionHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.accelerationX = 1;
    buffer->data.hasAccelerationX = true;
    buffer->data.accelerationY = 2;
    buffer->data.hasAccelerationY = true;
    buffer->data.accelerationZ = 3;
    buffer->data.hasAccelerationZ = true;

    buffer->data.accelerationIncludingGravityX = 4;
    buffer->data.hasAccelerationIncludingGravityX = true;
    buffer->data.accelerationIncludingGravityY = 5;
    buffer->data.hasAccelerationIncludingGravityY = true;
    buffer->data.accelerationIncludingGravityZ = 6;
    buffer->data.hasAccelerationIncludingGravityZ = true;

    buffer->data.rotationRateAlpha = 7;
    buffer->data.hasRotationRateAlpha = true;
    buffer->data.rotationRateBeta = 8;
    buffer->data.hasRotationRateBeta = true;
    buffer->data.rotationRateGamma = 9;
    buffer->data.hasRotationRateGamma = true;

    buffer->data.interval = 100;
    buffer->data.allAvailableSensorsAreActive = true;
    buffer->seqlock.WriteEnd();
  }

  void UpdateOrientation(DeviceOrientationHardwareBuffer* buffer) {
    buffer->seqlock.WriteBegin();
    buffer->data.alpha = 1;
    buffer->data.hasAlpha = true;
    buffer->data.beta = 2;
    buffer->data.hasBeta = true;
    buffer->data.gamma = 3;
    buffer->data.hasGamma = true;
    buffer->data.allAvailableSensorsAreActive = true;
    buffer->seqlock.WriteEnd();
  }

  base::WaitableEvent started_orientation_;
  base::WaitableEvent stopped_orientation_;
  base::WaitableEvent started_motion_;
  base::WaitableEvent stopped_motion_;

 private:

  DISALLOW_COPY_AND_ASSIGN(FakeDataFetcher);
};


class DeviceInertialSensorBrowserTest : public ContentBrowserTest  {
 public:
  DeviceInertialSensorBrowserTest()
      : fetcher_(NULL),
        io_loop_finished_event_(false, false) {
  }

  // From ContentBrowserTest.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    EXPECT_TRUE(!command_line->HasSwitch(switches::kDisableDeviceOrientation));
    EXPECT_TRUE(!command_line->HasSwitch(switches::kDisableDeviceMotion));
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&DeviceInertialSensorBrowserTest::SetUpOnIOThread, this));
    io_loop_finished_event_.Wait();
  }

  void SetUpOnIOThread() {
    fetcher_ = new FakeDataFetcher();
    DeviceInertialSensorService::GetInstance()->
        SetDataFetcherForTests(fetcher_);
    io_loop_finished_event_.Signal();
  }

  FakeDataFetcher* fetcher_;

 private:
  base::WaitableEvent io_loop_finished_event_;
};


IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, OrientationTest) {
  // The test page will register an event handler for orientation events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl(
      "device_orientation", "device_orientation_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_orientation_.Wait();
  fetcher_->stopped_orientation_.Wait();
}

IN_PROC_BROWSER_TEST_F(DeviceInertialSensorBrowserTest, MotionTest) {
  // The test page will register an event handler for motion events,
  // expects to get an event with fake values, then removes the event
  // handler and navigates to #pass.
  GURL test_url = GetTestUrl(
      "device_orientation", "device_motion_test.html");
  NavigateToURLBlockUntilNavigationsComplete(shell(), test_url, 2);

  EXPECT_EQ("pass", shell()->web_contents()->GetLastCommittedURL().ref());
  fetcher_->started_motion_.Wait();
  fetcher_->stopped_motion_.Wait();
}

} //  namespace

} //  namespace content
