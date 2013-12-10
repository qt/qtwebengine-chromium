// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_model.h"

#include "ui/app_list/app_list_item_model.h"
#include "ui/app_list/app_list_model_observer.h"
#include "ui/app_list/search_box_model.h"
#include "ui/app_list/search_result.h"

namespace app_list {

AppListModel::User::User() : active(false) {}

AppListModel::User::~User() {}

AppListModel::AppListModel()
    : apps_(new Apps),
      search_box_(new SearchBoxModel),
      results_(new SearchResults),
      signed_in_(false),
      status_(STATUS_NORMAL) {
}

AppListModel::~AppListModel() {
}

void AppListModel::AddObserver(AppListModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListModel::RemoveObserver(AppListModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListModel::SetStatus(Status status) {
  if (status_ == status)
    return;

  status_ = status;
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListModelStatusChanged());
}

void AppListModel::SetUsers(const Users& users) {
  users_ = users;
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListModelUsersChanged());
}

void AppListModel::SetSignedIn(bool signed_in) {
  if (signed_in_ == signed_in)
    return;

  signed_in_ = signed_in;
  FOR_EACH_OBSERVER(AppListModelObserver,
                    observers_,
                    OnAppListModelSigninStatusChanged());
}

}  // namespace app_list
