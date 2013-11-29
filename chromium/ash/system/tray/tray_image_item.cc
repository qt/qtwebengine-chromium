// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_image_item.h"

#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_utils.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {
namespace internal {

TrayImageItem::TrayImageItem(SystemTray* system_tray, int resource_id)
    : SystemTrayItem(system_tray),
      resource_id_(resource_id),
      tray_view_(NULL) {
}

TrayImageItem::~TrayImageItem() {}

views::View* TrayImageItem::tray_view() {
  return tray_view_;
}

void TrayImageItem::SetImageFromResourceId(int resource_id) {
  resource_id_ = resource_id;
  if (!tray_view_)
    return;
  tray_view_->image_view()->SetImage(ui::ResourceBundle::GetSharedInstance().
      GetImageNamed(resource_id_).ToImageSkia());
}

views::View* TrayImageItem::CreateTrayView(user::LoginStatus status) {
  CHECK(tray_view_ == NULL);
  tray_view_ = new TrayItemView(this);
  tray_view_->CreateImageView();
  tray_view_->image_view()->SetImage(ui::ResourceBundle::GetSharedInstance().
      GetImageNamed(resource_id_).ToImageSkia());
  tray_view_->SetVisible(GetInitialVisibility());
  return tray_view_;
}

views::View* TrayImageItem::CreateDefaultView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayImageItem::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayImageItem::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayImageItem::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  SetTrayImageItemBorder(tray_view_, alignment);
}

void TrayImageItem::DestroyTrayView() {
  tray_view_ = NULL;
}

void TrayImageItem::DestroyDefaultView() {
}

void TrayImageItem::DestroyDetailedView() {
}

}  // namespace internal
}  // namespace ash
