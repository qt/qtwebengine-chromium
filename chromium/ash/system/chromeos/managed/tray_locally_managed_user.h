// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_LOCALLY_MANAGED_TRAY_LOCALLY_MANAGED_USER_H
#define ASH_SYSTEM_CHROMEOS_LOCALLY_MANAGED_TRAY_LOCALLY_MANAGED_USER_H

#include "ash/ash_export.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/view_click_listener.h"
#include "base/strings/string16.h"

namespace ash {
class SystemTray;

namespace internal {

class LabelTrayView;

class ASH_EXPORT TrayLocallyManagedUser : public SystemTrayItem,
                                          public ViewClickListener {
 public:
  explicit TrayLocallyManagedUser(SystemTray* system_tray);
  virtual ~TrayLocallyManagedUser();

  // If message is not empty updates content of default view, otherwise hides
  // tray items.
  void UpdateMessage();

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void UpdateAfterLoginStatusChange(user::LoginStatus status) OVERRIDE;

  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE;

 private:
  friend class TrayLocallyManagedUserTest;

  static const char kNotificationId[];

  void CreateOrUpdateNotification(const base::string16& new_message);

  LabelTrayView* tray_view_;
  // Previous login status to avoid showing notification upon unlock.
  user::LoginStatus status_;

  DISALLOW_COPY_AND_ASSIGN(TrayLocallyManagedUser);
};

} // namespace internal
} // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_LOCALLY_MANAGED_TRAY_LOCALLY_MANAGED_USER_H
