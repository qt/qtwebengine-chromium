// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_
#define ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/system/bluetooth/bluetooth_observer.h"
#include "ash/system/chromeos/tray_tracing.h"
#include "ash/system/date/clock_observer.h"
#include "ash/system/drive/drive_observer.h"
#include "ash/system/ime/ime_observer.h"
#include "ash/system/locale/locale_observer.h"
#include "ash/system/logout_button/logout_button_observer.h"
#include "ash/system/session_length_limit/session_length_limit_observer.h"
#include "ash/system/tray_accessibility.h"
#include "ash/system/tray_caps_lock.h"
#include "ash/system/user/update_observer.h"
#include "ash/system/user/user_observer.h"
#include "base/observer_list.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/enterprise/enterprise_domain_observer.h"
#include "ash/system/chromeos/network/network_observer.h"
#include "ash/system/chromeos/screen_security/screen_capture_observer.h"
#include "ash/system/chromeos/screen_security/screen_share_observer.h"
#endif

namespace ash {

#if defined(OS_CHROMEOS)
class NetworkStateNotifier;
#endif

class ASH_EXPORT SystemTrayNotifier {
public:
  SystemTrayNotifier();
  ~SystemTrayNotifier();

  void AddAccessibilityObserver(AccessibilityObserver* observer);
  void RemoveAccessibilityObserver(AccessibilityObserver* observer);

  void AddBluetoothObserver(BluetoothObserver* observer);
  void RemoveBluetoothObserver(BluetoothObserver* observer);

  void AddCapsLockObserver(CapsLockObserver* observer);
  void RemoveCapsLockObserver(CapsLockObserver* observer);

  void AddClockObserver(ClockObserver* observer);
  void RemoveClockObserver(ClockObserver* observer);

  void AddDriveObserver(DriveObserver* observer);
  void RemoveDriveObserver(DriveObserver* observer);

  void AddIMEObserver(IMEObserver* observer);
  void RemoveIMEObserver(IMEObserver* observer);

  void AddLocaleObserver(LocaleObserver* observer);
  void RemoveLocaleObserver(LocaleObserver* observer);

  void AddLogoutButtonObserver(LogoutButtonObserver* observer);
  void RemoveLogoutButtonObserver(LogoutButtonObserver* observer);

  void AddSessionLengthLimitObserver(SessionLengthLimitObserver* observer);
  void RemoveSessionLengthLimitObserver(SessionLengthLimitObserver* observer);

  void AddTracingObserver(TracingObserver* observer);
  void RemoveTracingObserver(TracingObserver* observer);

  void AddUpdateObserver(UpdateObserver* observer);
  void RemoveUpdateObserver(UpdateObserver* observer);

  void AddUserObserver(UserObserver* observer);
  void RemoveUserObserver(UserObserver* observer);

#if defined(OS_CHROMEOS)
  void AddNetworkObserver(NetworkObserver* observer);
  void RemoveNetworkObserver(NetworkObserver* observer);

  void AddEnterpriseDomainObserver(EnterpriseDomainObserver* observer);
  void RemoveEnterpriseDomainObserver(EnterpriseDomainObserver* observer);

  void AddScreenCaptureObserver(ScreenCaptureObserver* observer);
  void RemoveScreenCaptureObserver(ScreenCaptureObserver* observer);

  void AddScreenShareObserver(ScreenShareObserver* observer);
  void RemoveScreenShareObserver(ScreenShareObserver* observer);
#endif

  void NotifyAccessibilityModeChanged(
      AccessibilityNotificationVisibility notify);
  void NotifyTracingModeChanged(bool value);
  void NotifyRefreshBluetooth();
  void NotifyBluetoothDiscoveringChanged();
  void NotifyCapsLockChanged(bool enabled, bool search_mapped_to_caps_lock);
  void NotifyRefreshClock();
  void NotifyDateFormatChanged();
  void NotifySystemClockTimeUpdated();
  void NotifyDriveJobUpdated(const DriveOperationStatus& status);
  void NotifyRefreshIME(bool show_message);
  void NotifyShowLoginButtonChanged(bool show_login_button);
  void NotifyLocaleChanged(LocaleObserver::Delegate* delegate,
                           const std::string& cur_locale,
                           const std::string& from_locale,
                           const std::string& to_locale);
  void NotifySessionStartTimeChanged();
  void NotifySessionLengthLimitChanged();
  void NotifyUpdateRecommended(UpdateObserver::UpdateSeverity severity);
  void NotifyUserUpdate();
  void NotifyUserAddedToSession();
#if defined(OS_CHROMEOS)
  void NotifyRequestToggleWifi();
  void NotifyEnterpriseDomainChanged();
  void NotifyScreenCaptureStart(const base::Closure& stop_callback,
                                const base::string16& sharing_app_name);
  void NotifyScreenCaptureStop();
  void NotifyScreenShareStart(const base::Closure& stop_callback,
                              const base::string16& helper_name);
  void NotifyScreenShareStop();

  NetworkStateNotifier* network_state_notifier() {
    return network_state_notifier_.get();
  }
#endif

 private:
  ObserverList<AccessibilityObserver> accessibility_observers_;
  ObserverList<BluetoothObserver> bluetooth_observers_;
  ObserverList<CapsLockObserver> caps_lock_observers_;
  ObserverList<ClockObserver> clock_observers_;
  ObserverList<DriveObserver> drive_observers_;
  ObserverList<IMEObserver> ime_observers_;
  ObserverList<LocaleObserver> locale_observers_;
  ObserverList<LogoutButtonObserver> logout_button_observers_;
  ObserverList<SessionLengthLimitObserver> session_length_limit_observers_;
  ObserverList<TracingObserver> tracing_observers_;
  ObserverList<UpdateObserver> update_observers_;
  ObserverList<UserObserver> user_observers_;
#if defined(OS_CHROMEOS)
  ObserverList<NetworkObserver> network_observers_;
  ObserverList<EnterpriseDomainObserver> enterprise_domain_observers_;
  ObserverList<ScreenCaptureObserver> screen_capture_observers_;
  ObserverList<ScreenShareObserver> screen_share_observers_;
  scoped_ptr<NetworkStateNotifier> network_state_notifier_;
#endif

  DISALLOW_COPY_AND_ASSIGN(SystemTrayNotifier);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_SYSTEM_TRAY_NOTIFIER_H_
