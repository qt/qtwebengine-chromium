// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/device_orientation/data_fetcher_impl_android.h"

#include <string.h>

#include "base/android/jni_android.h"
#include "base/memory/singleton.h"
#include "content/browser/device_orientation/orientation.h"
#include "jni/DeviceMotionAndOrientation_jni.h"

using base::android::AttachCurrentThread;

namespace content {

namespace {

// This should match ProviderImpl::kDesiredSamplingIntervalMs.
// TODO(husky): Make that constant public so we can use it directly.
const int kPeriodInMilliseconds = 100;

}  // namespace

DataFetcherImplAndroid::DataFetcherImplAndroid()
    : number_active_device_motion_sensors_(0),
      device_motion_buffer_(NULL),
      device_orientation_buffer_(NULL),
      is_motion_buffer_ready_(false),
      is_orientation_buffer_ready_(false) {
  memset(received_motion_data_, 0, sizeof(received_motion_data_));
  device_orientation_.Reset(
      Java_DeviceMotionAndOrientation_getInstance(AttachCurrentThread()));
}

DataFetcherImplAndroid::~DataFetcherImplAndroid() {
}

bool DataFetcherImplAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

DataFetcherImplAndroid* DataFetcherImplAndroid::GetInstance() {
  return Singleton<DataFetcherImplAndroid,
                   LeakySingletonTraits<DataFetcherImplAndroid> >::get();
}

const DeviceData* DataFetcherImplAndroid::GetDeviceData(
    DeviceData::Type type) {
  if (type != DeviceData::kTypeOrientation)
    return NULL;
  return GetOrientation();
}

const Orientation* DataFetcherImplAndroid::GetOrientation() {
  // Do we have a new orientation value? (It's safe to do this outside the lock
  // because we only skip the lock if the value is null. We always enter the
  // lock if we're going to make use of the new value.)
  if (next_orientation_.get()) {
    base::AutoLock autolock(next_orientation_lock_);
    next_orientation_.swap(current_orientation_);
  }
  if (!current_orientation_.get())
    return new Orientation();
  return current_orientation_.get();
}

void DataFetcherImplAndroid::GotOrientation(
    JNIEnv*, jobject, double alpha, double beta, double gamma) {
  {
    // TODO(timvolodine): remove this part once Device Orientation is
    // completely implemented using shared memory.
    base::AutoLock autolock(next_orientation_lock_);

    Orientation* orientation = new Orientation();
    orientation->set_alpha(alpha);
    orientation->set_beta(beta);
    orientation->set_gamma(gamma);
    orientation->set_absolute(true);
    next_orientation_ = orientation;
  }

  if (device_orientation_buffer_) {
    device_orientation_buffer_->seqlock.WriteBegin();
    device_orientation_buffer_->data.alpha = alpha;
    device_orientation_buffer_->data.hasAlpha = true;
    device_orientation_buffer_->data.beta = beta;
    device_orientation_buffer_->data.hasBeta = true;
    device_orientation_buffer_->data.gamma = gamma;
    device_orientation_buffer_->data.hasGamma = true;
    device_orientation_buffer_->seqlock.WriteEnd();

    if (!is_orientation_buffer_ready_)
      SetOrientationBufferReadyStatus(true);
  }
}

void DataFetcherImplAndroid::GotAcceleration(
    JNIEnv*, jobject, double x, double y, double z) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.accelerationX = x;
  device_motion_buffer_->data.hasAccelerationX = true;
  device_motion_buffer_->data.accelerationY = y;
  device_motion_buffer_->data.hasAccelerationY = true;
  device_motion_buffer_->data.accelerationZ = z;
  device_motion_buffer_->data.hasAccelerationZ = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!is_motion_buffer_ready_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void DataFetcherImplAndroid::GotAccelerationIncludingGravity(
    JNIEnv*, jobject, double x, double y, double z) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.accelerationIncludingGravityX = x;
  device_motion_buffer_->data.hasAccelerationIncludingGravityX = true;
  device_motion_buffer_->data.accelerationIncludingGravityY = y;
  device_motion_buffer_->data.hasAccelerationIncludingGravityY = true;
  device_motion_buffer_->data.accelerationIncludingGravityZ = z;
  device_motion_buffer_->data.hasAccelerationIncludingGravityZ = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!is_motion_buffer_ready_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] = 1;
    CheckMotionBufferReadyToRead();
  }
}

void DataFetcherImplAndroid::GotRotationRate(
    JNIEnv*, jobject, double alpha, double beta, double gamma) {
  base::AutoLock autolock(motion_buffer_lock_);

  if (!device_motion_buffer_)
    return;

  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.rotationRateAlpha = alpha;
  device_motion_buffer_->data.hasRotationRateAlpha = true;
  device_motion_buffer_->data.rotationRateBeta = beta;
  device_motion_buffer_->data.hasRotationRateBeta = true;
  device_motion_buffer_->data.rotationRateGamma = gamma;
  device_motion_buffer_->data.hasRotationRateGamma = true;
  device_motion_buffer_->seqlock.WriteEnd();

  if (!is_motion_buffer_ready_) {
    received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] = 1;
    CheckMotionBufferReadyToRead();
  }
}

bool DataFetcherImplAndroid::Start(DeviceData::Type event_type) {
  DCHECK(!device_orientation_.is_null());
  return Java_DeviceMotionAndOrientation_start(
      AttachCurrentThread(), device_orientation_.obj(),
      reinterpret_cast<jint>(this), static_cast<jint>(event_type),
      kPeriodInMilliseconds);
}

void DataFetcherImplAndroid::Stop(DeviceData::Type event_type) {
  DCHECK(!device_orientation_.is_null());
  Java_DeviceMotionAndOrientation_stop(
      AttachCurrentThread(), device_orientation_.obj(),
      static_cast<jint>(event_type));
}

int DataFetcherImplAndroid::GetNumberActiveDeviceMotionSensors() {
  DCHECK(!device_orientation_.is_null());
  return Java_DeviceMotionAndOrientation_getNumberActiveDeviceMotionSensors(
      AttachCurrentThread(), device_orientation_.obj());
}


// ----- Shared memory API methods

// --- Device Motion

bool DataFetcherImplAndroid::StartFetchingDeviceMotionData(
    DeviceMotionHardwareBuffer* buffer) {
  DCHECK(buffer);
  {
    base::AutoLock autolock(motion_buffer_lock_);
    device_motion_buffer_ = buffer;
    ClearInternalMotionBuffers();
  }
  bool success = Start(DeviceData::kTypeMotion);

  // If no motion data can ever be provided, the number of active device motion
  // sensors will be zero. In that case flag the shared memory buffer
  // as ready to read, as it will not change anyway.
  number_active_device_motion_sensors_ = GetNumberActiveDeviceMotionSensors();
  {
    base::AutoLock autolock(motion_buffer_lock_);
    CheckMotionBufferReadyToRead();
  }
  return success;
}

void DataFetcherImplAndroid::StopFetchingDeviceMotionData() {
  Stop(DeviceData::kTypeMotion);
  {
    base::AutoLock autolock(motion_buffer_lock_);
    if (device_motion_buffer_) {
      ClearInternalMotionBuffers();
      device_motion_buffer_ = NULL;
    }
  }
}

void DataFetcherImplAndroid::CheckMotionBufferReadyToRead() {
  if (received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION] +
      received_motion_data_[RECEIVED_MOTION_DATA_ACCELERATION_INCL_GRAVITY] +
      received_motion_data_[RECEIVED_MOTION_DATA_ROTATION_RATE] ==
      number_active_device_motion_sensors_) {
    device_motion_buffer_->seqlock.WriteBegin();
    device_motion_buffer_->data.interval = kPeriodInMilliseconds;
    device_motion_buffer_->seqlock.WriteEnd();
    SetMotionBufferReadyStatus(true);
  }
}

void DataFetcherImplAndroid::SetMotionBufferReadyStatus(bool ready) {
  device_motion_buffer_->seqlock.WriteBegin();
  device_motion_buffer_->data.allAvailableSensorsAreActive = ready;
  device_motion_buffer_->seqlock.WriteEnd();
  is_motion_buffer_ready_ = ready;
}

void DataFetcherImplAndroid::ClearInternalMotionBuffers() {
  memset(received_motion_data_, 0, sizeof(received_motion_data_));
  number_active_device_motion_sensors_ = 0;
  SetMotionBufferReadyStatus(false);
}

// --- Device Orientation

void DataFetcherImplAndroid::SetOrientationBufferReadyStatus(bool ready) {
  device_orientation_buffer_->seqlock.WriteBegin();
  device_orientation_buffer_->data.allAvailableSensorsAreActive = ready;
  device_orientation_buffer_->seqlock.WriteEnd();
  is_orientation_buffer_ready_ = ready;
}

bool DataFetcherImplAndroid::StartFetchingDeviceOrientationData(
    DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(buffer);
  device_orientation_buffer_ = buffer;
  bool success = Start(DeviceData::kTypeOrientation);

  // If Start() was unsuccessful then set the buffer ready flag to true
  // to start firing all-null events.
  SetOrientationBufferReadyStatus(!success);
  return success;
}

void DataFetcherImplAndroid::StopFetchingDeviceOrientationData() {
  Stop(DeviceData::kTypeOrientation);
  if (device_orientation_buffer_) {
    SetOrientationBufferReadyStatus(false);
    device_orientation_buffer_ = NULL;
  }
}

}  // namespace content
