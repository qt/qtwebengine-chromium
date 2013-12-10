// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/default_system_tray_delegate.h"

#include <string>

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/volume_control_delegate.h"
#include "base/message_loop/message_loop.h"
#include "base/time/time.h"

namespace ash {

namespace {

class DefaultVolumnControlDelegate : public VolumeControlDelegate {
 public:
  DefaultVolumnControlDelegate() {}
  virtual ~DefaultVolumnControlDelegate() {}

  virtual bool HandleVolumeMute(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeDown(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }
  virtual bool HandleVolumeUp(const ui::Accelerator& accelerator) OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DefaultVolumnControlDelegate);
};

}  // namespace

DefaultSystemTrayDelegate::DefaultSystemTrayDelegate()
    : bluetooth_enabled_(true),
      volume_control_delegate_(new DefaultVolumnControlDelegate) {
}

DefaultSystemTrayDelegate::~DefaultSystemTrayDelegate() {
}

void DefaultSystemTrayDelegate::Initialize() {
}

void DefaultSystemTrayDelegate::Shutdown() {
}

bool DefaultSystemTrayDelegate::GetTrayVisibilityOnStartup() {
  return true;
}

user::LoginStatus DefaultSystemTrayDelegate::GetUserLoginStatus() const {
  return user::LOGGED_IN_USER;
}

bool DefaultSystemTrayDelegate::IsOobeCompleted() const {
  return true;
}

void DefaultSystemTrayDelegate::ChangeProfilePicture() {
}

const std::string DefaultSystemTrayDelegate::GetEnterpriseDomain() const {
  return std::string();
}

const base::string16 DefaultSystemTrayDelegate::GetEnterpriseMessage() const {
  return string16();
}

const std::string
DefaultSystemTrayDelegate::GetLocallyManagedUserManager() const {
  return std::string();
}

const base::string16
DefaultSystemTrayDelegate::GetLocallyManagedUserManagerName()
    const {
  return string16();
}

const base::string16 DefaultSystemTrayDelegate::GetLocallyManagedUserMessage()
    const {
  return string16();
}

bool DefaultSystemTrayDelegate::SystemShouldUpgrade() const {
  return true;
}

base::HourClockType DefaultSystemTrayDelegate::GetHourClockType() const {
  return base::k24HourClock;
}

void DefaultSystemTrayDelegate::ShowSettings() {
}

bool DefaultSystemTrayDelegate::ShouldShowSettings() {
  return true;
}

void DefaultSystemTrayDelegate::ShowDateSettings() {
}

void DefaultSystemTrayDelegate::ShowNetworkSettings(
    const std::string& service_path) {
}

void DefaultSystemTrayDelegate::ShowBluetoothSettings() {
}

void DefaultSystemTrayDelegate::ShowDisplaySettings() {
}

void DefaultSystemTrayDelegate::ShowChromeSlow() {
}

bool DefaultSystemTrayDelegate::ShouldShowDisplayNotification() {
  return false;
}

void DefaultSystemTrayDelegate::ShowDriveSettings() {
}

void DefaultSystemTrayDelegate::ShowIMESettings() {
}

void DefaultSystemTrayDelegate::ShowHelp() {
}

void DefaultSystemTrayDelegate::ShowAccessibilityHelp() {
}

void DefaultSystemTrayDelegate::ShowAccessibilitySettings() {
}

void DefaultSystemTrayDelegate::ShowPublicAccountInfo() {
}

void DefaultSystemTrayDelegate::ShowEnterpriseInfo() {
}

void DefaultSystemTrayDelegate::ShowLocallyManagedUserInfo() {
}

void DefaultSystemTrayDelegate::ShowUserLogin() {
}

void DefaultSystemTrayDelegate::ShutDown() {
}

void DefaultSystemTrayDelegate::SignOut() {
}

void DefaultSystemTrayDelegate::RequestLockScreen() {
}

void DefaultSystemTrayDelegate::RequestRestartForUpdate() {
}

void DefaultSystemTrayDelegate::GetAvailableBluetoothDevices(
    BluetoothDeviceList* list) {
}

void DefaultSystemTrayDelegate::BluetoothStartDiscovering() {
}

void DefaultSystemTrayDelegate::BluetoothStopDiscovering() {
}

void DefaultSystemTrayDelegate::ConnectToBluetoothDevice(
    const std::string& address) {
}

void DefaultSystemTrayDelegate::GetCurrentIME(IMEInfo* info) {
}

void DefaultSystemTrayDelegate::GetAvailableIMEList(IMEInfoList* list) {
}

void DefaultSystemTrayDelegate::GetCurrentIMEProperties(
    IMEPropertyInfoList* list) {
}

void DefaultSystemTrayDelegate::SwitchIME(const std::string& ime_id) {
}

void DefaultSystemTrayDelegate::ActivateIMEProperty(const std::string& key) {
}

void DefaultSystemTrayDelegate::CancelDriveOperation(int32 operation_id) {
}

void DefaultSystemTrayDelegate::GetDriveOperationStatusList(
    ash::DriveOperationStatusList*) {
}

void DefaultSystemTrayDelegate::ConfigureNetwork(
    const std::string& network_id) {
}

void DefaultSystemTrayDelegate::EnrollOrConfigureNetwork(
    const std::string& network_id,
    gfx::NativeWindow parent_window) {
}

void DefaultSystemTrayDelegate::ManageBluetoothDevices() {
}

void DefaultSystemTrayDelegate::ToggleBluetooth() {
  bluetooth_enabled_ = !bluetooth_enabled_;
}

bool DefaultSystemTrayDelegate::IsBluetoothDiscovering() {
  return false;
}

void DefaultSystemTrayDelegate::ShowMobileSimDialog() {
}

void DefaultSystemTrayDelegate::ShowMobileSetupDialog(
    const std::string& service_path) {
}

void DefaultSystemTrayDelegate::ShowOtherWifi() {
}

void DefaultSystemTrayDelegate::ShowOtherVPN() {
}

void DefaultSystemTrayDelegate::ShowOtherCellular() {
}

bool DefaultSystemTrayDelegate::GetBluetoothAvailable() {
  return true;
}

bool DefaultSystemTrayDelegate::GetBluetoothEnabled() {
  return bluetooth_enabled_;
}

void DefaultSystemTrayDelegate::ChangeProxySettings() {
}

VolumeControlDelegate* DefaultSystemTrayDelegate::GetVolumeControlDelegate()
    const {
  return volume_control_delegate_.get();
}

void DefaultSystemTrayDelegate::SetVolumeControlDelegate(
    scoped_ptr<VolumeControlDelegate> delegate) {
  volume_control_delegate_ = delegate.Pass();
}

bool DefaultSystemTrayDelegate::GetSessionStartTime(
    base::TimeTicks* session_start_time) {
  return false;
}

bool DefaultSystemTrayDelegate::GetSessionLengthLimit(
     base::TimeDelta* session_length_limit) {
  return false;
}

int DefaultSystemTrayDelegate::GetSystemTrayMenuWidth() {
  // This is the default width for English languages.
  return 300;
}

void DefaultSystemTrayDelegate::MaybeSpeak(const std::string& utterance) const {
}

}  // namespace ash
