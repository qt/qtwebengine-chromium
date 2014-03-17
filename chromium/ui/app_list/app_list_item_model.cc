// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_item_model.h"

#include "base/logging.h"
#include "ui/app_list/app_list_item_model_observer.h"

namespace app_list {

AppListItemModel::AppListItemModel(const std::string& id)
    : id_(id),
      highlighted_(false),
      is_installing_(false),
      percent_downloaded_(-1) {
}

AppListItemModel::~AppListItemModel() {
}

void AppListItemModel::SetIcon(const gfx::ImageSkia& icon, bool has_shadow) {
  icon_ = icon;
  has_shadow_ = has_shadow;
  FOR_EACH_OBSERVER(AppListItemModelObserver, observers_, ItemIconChanged());
}

void AppListItemModel::SetTitleAndFullName(const std::string& title,
                                           const std::string& full_name) {
  if (title_ == title && full_name_ == full_name)
    return;

  title_ = title;
  full_name_ = full_name;
  FOR_EACH_OBSERVER(AppListItemModelObserver, observers_, ItemTitleChanged());
}

void AppListItemModel::SetHighlighted(bool highlighted) {
  if (highlighted_ == highlighted)
    return;

  highlighted_ = highlighted;
  FOR_EACH_OBSERVER(AppListItemModelObserver,
                    observers_,
                    ItemHighlightedChanged());
}

void AppListItemModel::SetIsInstalling(bool is_installing) {
  if (is_installing_ == is_installing)
    return;

  is_installing_ = is_installing;
  FOR_EACH_OBSERVER(AppListItemModelObserver,
                    observers_,
                    ItemIsInstallingChanged());
}

void AppListItemModel::SetPercentDownloaded(int percent_downloaded) {
  if (percent_downloaded_ == percent_downloaded)
    return;

  percent_downloaded_ = percent_downloaded;
  FOR_EACH_OBSERVER(AppListItemModelObserver,
                    observers_,
                    ItemPercentDownloadedChanged());
}

void AppListItemModel::AddObserver(AppListItemModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListItemModel::RemoveObserver(AppListItemModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListItemModel::Activate(int event_flags) {
}

const char* AppListItemModel::GetAppType() const {
  static const char* app_type = "";
  return app_type;
}

ui::MenuModel* AppListItemModel::GetContextMenuModel() {
  return NULL;
}

bool AppListItemModel::CompareForTest(const AppListItemModel* other) const {
  return id_ == other->id_ &&
      title_ == other->title_ &&
      position_.Equals(other->position_);
}

std::string AppListItemModel::ToDebugString() const {
  return id_.substr(0, 8) + " '" + title_ + "'"
      + " [" + position_.ToDebugString() + "]";
}

}  // namespace app_list
