// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/mock_audio_manager.h"

#include "base/logging.h"
#include "base/message_loop/message_loop_proxy.h"
#include "media/audio/audio_parameters.h"

namespace media {

MockAudioManager::MockAudioManager(base::MessageLoopProxy* message_loop_proxy)
    : message_loop_proxy_(message_loop_proxy) {
}

MockAudioManager::~MockAudioManager() {
}

bool MockAudioManager::HasAudioOutputDevices() {
  return true;
}

bool MockAudioManager::HasAudioInputDevices() {
  return true;
}

base::string16 MockAudioManager::GetAudioInputDeviceModel() {
  return base::string16();
}

void MockAudioManager::ShowAudioInputSettings() {
}

void MockAudioManager::GetAudioInputDeviceNames(
    AudioDeviceNames* device_names) {
  DCHECK(device_names->empty());
  device_names->push_back(media::AudioDeviceName("fake_device_name_1",
                                                 "fake_device_id_1"));
  device_names->push_back(media::AudioDeviceName("fake_device_name_2",
                                                 "fake_device_id_2"));
}

void MockAudioManager::GetAudioOutputDeviceNames(
    AudioDeviceNames* device_names) {
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStream(
    const media::AudioParameters& params,
    const std::string& device_id,
    const std::string& input_device_id) {
  NOTREACHED();
  return NULL;
}

media::AudioOutputStream* MockAudioManager::MakeAudioOutputStreamProxy(
    const media::AudioParameters& params,
    const std::string& device_id,
    const std::string& input_device_id) {
  NOTREACHED();
  return NULL;
}

media::AudioInputStream* MockAudioManager::MakeAudioInputStream(
    const media::AudioParameters& params,
    const std::string& device_id) {
  NOTREACHED();
  return NULL;
}

scoped_refptr<base::MessageLoopProxy> MockAudioManager::GetMessageLoop() {
  return message_loop_proxy_;
}

scoped_refptr<base::MessageLoopProxy> MockAudioManager::GetWorkerLoop() {
  return message_loop_proxy_;
}

void MockAudioManager::AddOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
}

void MockAudioManager::RemoveOutputDeviceChangeListener(
    AudioDeviceListener* listener) {
}

AudioParameters MockAudioManager::GetDefaultOutputStreamParameters() {
  return AudioParameters();
}

AudioParameters MockAudioManager::GetOutputStreamParameters(
      const std::string& device_id) {
  return AudioParameters();
}

AudioParameters MockAudioManager::GetInputStreamParameters(
    const std::string& device_id) {
  return AudioParameters();
}

std::string MockAudioManager::GetAssociatedOutputDeviceID(
    const std::string& input_device_id) {
  return std::string();
}

scoped_ptr<AudioLog> MockAudioManager::CreateAudioLog(
    AudioLogFactory::AudioComponent component) {
  return scoped_ptr<AudioLog>();
}

void MockAudioManager::FixWedgedAudio() {}

}  // namespace media.
