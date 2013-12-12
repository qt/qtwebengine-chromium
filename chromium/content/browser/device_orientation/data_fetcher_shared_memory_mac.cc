// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "data_fetcher_shared_memory.h"

#include "base/logging.h"
#include "third_party/sudden_motion_sensor/sudden_motion_sensor_mac.h"

namespace {

const double kMeanGravity = 9.80665;

void FetchMotion(SuddenMotionSensor* sensor,
    content::DeviceMotionHardwareBuffer* buffer) {
  DCHECK(buffer);

  float axis_value[3];
  if (!sensor->ReadSensorValues(axis_value))
    return;

  buffer->seqlock.WriteBegin();
  buffer->data.accelerationIncludingGravityX = axis_value[0] * kMeanGravity;
  buffer->data.hasAccelerationIncludingGravityX = true;
  buffer->data.accelerationIncludingGravityY = axis_value[1] * kMeanGravity;
  buffer->data.hasAccelerationIncludingGravityY = true;
  buffer->data.accelerationIncludingGravityZ = axis_value[2] * kMeanGravity;
  buffer->data.hasAccelerationIncludingGravityZ = true;
  buffer->data.allAvailableSensorsAreActive = true;
  buffer->seqlock.WriteEnd();
}

void FetchOrientation(SuddenMotionSensor* sensor,
    content::DeviceOrientationHardwareBuffer* buffer) {
  DCHECK(buffer);

  // Retrieve per-axis calibrated values.
  float axis_value[3];
  if (!sensor->ReadSensorValues(axis_value))
    return;

  // Transform the accelerometer values to W3C draft angles.
  //
  // Accelerometer values are just dot products of the sensor axes
  // by the gravity vector 'g' with the result for the z axis inverted.
  //
  // To understand this transformation calculate the 3rd row of the z-x-y
  // Euler angles rotation matrix (because of the 'g' vector, only 3rd row
  // affects to the result). Note that z-x-y matrix means R = Ry * Rx * Rz.
  // Then, assume alpha = 0 and you get this:
  //
  // x_acc = sin(gamma)
  // y_acc = - cos(gamma) * sin(beta)
  // z_acc = cos(beta) * cos(gamma)
  //
  // After that the rest is just a bit of trigonometry.
  //
  // Also note that alpha can't be provided but it's assumed to be always zero.
  // This is necessary in order to provide enough information to solve
  // the equations.
  //
  const double kRad2deg = 180.0 / M_PI;
  double beta = kRad2deg * atan2(-axis_value[1], axis_value[2]);
  double gamma = kRad2deg * asin(axis_value[0]);

  // TODO(aousterh): should absolute_ be set to false here?
  // See crbug.com/136010.

  // Make sure that the interval boundaries comply with the specification. At
  // this point, beta is [-180, 180] and gamma is [-90, 90], but the spec has
  // the upper bound open on both.
  if (beta == 180.0)
    beta = -180;  // -180 == 180 (upside-down)
  if (gamma == 90.0)
    gamma = nextafter(90, 0);

  // At this point, DCHECKing is paranoia. Never hurts.
  DCHECK_GE(beta, -180.0);
  DCHECK_LT(beta,  180.0);
  DCHECK_GE(gamma, -90.0);
  DCHECK_LT(gamma,  90.0);

  buffer->seqlock.WriteBegin();
  buffer->data.beta = beta;
  buffer->data.hasBeta = true;
  buffer->data.gamma = gamma;
  buffer->data.hasGamma = true;
  buffer->data.allAvailableSensorsAreActive = true;
  buffer->seqlock.WriteEnd();
}

}  // namespace

namespace content {

DataFetcherSharedMemory::DataFetcherSharedMemory() {
}

DataFetcherSharedMemory::~DataFetcherSharedMemory() {
}

void DataFetcherSharedMemory::Fetch(unsigned consumer_bitmask) {
  DCHECK(base::MessageLoop::current() == GetPollingMessageLoop());
  DCHECK(sudden_motion_sensor_);
  DCHECK(consumer_bitmask & CONSUMER_TYPE_ORIENTATION ||
         consumer_bitmask & CONSUMER_TYPE_MOTION);

  if (consumer_bitmask & CONSUMER_TYPE_ORIENTATION)
    FetchOrientation(sudden_motion_sensor_.get(), orientation_buffer_);
  if (consumer_bitmask & CONSUMER_TYPE_MOTION)
    FetchMotion(sudden_motion_sensor_.get(), motion_buffer_);
}

bool DataFetcherSharedMemory::IsPolling() const {
  return true;
}

bool DataFetcherSharedMemory::Start(ConsumerType consumer_type, void* buffer) {
  DCHECK(base::MessageLoop::current() == GetPollingMessageLoop());
  DCHECK(buffer);

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      motion_buffer_ = static_cast<DeviceMotionHardwareBuffer*>(buffer);
      if (!sudden_motion_sensor_)
        sudden_motion_sensor_.reset(SuddenMotionSensor::Create());
      return sudden_motion_sensor_.get() != NULL;
    case CONSUMER_TYPE_ORIENTATION:
      orientation_buffer_ =
          static_cast<DeviceOrientationHardwareBuffer*>(buffer);
      if (!sudden_motion_sensor_)
        sudden_motion_sensor_.reset(SuddenMotionSensor::Create());
      return sudden_motion_sensor_.get() != NULL;
    default:
      NOTREACHED();
  }
  return false;
}

bool DataFetcherSharedMemory::Stop(ConsumerType consumer_type) {
  DCHECK(base::MessageLoop::current() == GetPollingMessageLoop());

  switch (consumer_type) {
    case CONSUMER_TYPE_MOTION:
      if (motion_buffer_) {
        motion_buffer_->seqlock.WriteBegin();
        motion_buffer_->data.allAvailableSensorsAreActive = false;
        motion_buffer_->seqlock.WriteEnd();
        motion_buffer_ = NULL;
      }
      return true;
    case CONSUMER_TYPE_ORIENTATION:
      if (orientation_buffer_) {
        orientation_buffer_->seqlock.WriteBegin();
        orientation_buffer_->data.allAvailableSensorsAreActive = false;
        orientation_buffer_->seqlock.WriteEnd();
        orientation_buffer_ = NULL;
      }
      return true;
    default:
      NOTREACHED();
  }
  return false;
}

}  // namespace content
