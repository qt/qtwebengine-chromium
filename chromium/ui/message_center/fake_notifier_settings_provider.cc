// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/fake_notifier_settings_provider.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/gfx/image/image.h"

namespace message_center {

FakeNotifierSettingsProvider::NotifierGroupItem::NotifierGroupItem() {
}

FakeNotifierSettingsProvider::NotifierGroupItem::~NotifierGroupItem() {
}

FakeNotifierSettingsProvider::FakeNotifierSettingsProvider()
    : closed_called_count_(0),
      active_item_index_(0) {
}

FakeNotifierSettingsProvider::FakeNotifierSettingsProvider(
    const std::vector<Notifier*>& notifiers)
    : closed_called_count_(0),
      active_item_index_(0) {
  NotifierGroupItem item;
  item.group = new NotifierGroup(gfx::Image(),
                                 UTF8ToUTF16("Fake name"),
                                 UTF8ToUTF16("fake@email.com"),
                                 true);
  item.notifiers = notifiers;
  items_.push_back(item);
}

FakeNotifierSettingsProvider::~FakeNotifierSettingsProvider() {
  for (size_t i = 0; i < items_.size(); ++i) {
    delete items_[i].group;
  }
}

size_t FakeNotifierSettingsProvider::GetNotifierGroupCount() const {
  return items_.size();
}

const message_center::NotifierGroup&
FakeNotifierSettingsProvider::GetNotifierGroupAt(size_t index) const {
  return *(items_[index].group);
}

bool FakeNotifierSettingsProvider::IsNotifierGroupActiveAt(
    size_t index) const {
  return active_item_index_ == index;
}

void FakeNotifierSettingsProvider::SwitchToNotifierGroup(size_t index) {
  active_item_index_ = index;
}

const message_center::NotifierGroup&
FakeNotifierSettingsProvider::GetActiveNotifierGroup() const {
  return *(items_[active_item_index_].group);
}

void FakeNotifierSettingsProvider::GetNotifierList(
    std::vector<Notifier*>* notifiers) {
  notifiers->clear();
  for (size_t i = 0; i < items_[active_item_index_].notifiers.size(); ++i)
    notifiers->push_back(items_[active_item_index_].notifiers[i]);
}

void FakeNotifierSettingsProvider::SetNotifierEnabled(const Notifier& notifier,
                                                      bool enabled) {
  enabled_[&notifier] = enabled;
}

void FakeNotifierSettingsProvider::OnNotifierSettingsClosing() {
  closed_called_count_++;
}

void FakeNotifierSettingsProvider::AddObserver(
    NotifierSettingsObserver* observer) {
}

void FakeNotifierSettingsProvider::RemoveObserver(
    NotifierSettingsObserver* observer) {
}

bool FakeNotifierSettingsProvider::WasEnabled(const Notifier& notifier) {
  return enabled_[&notifier];
}

void FakeNotifierSettingsProvider::AddGroup(
    NotifierGroup* group, const std::vector<Notifier*>& notifiers) {
  NotifierGroupItem item;
  item.group = group;
  item.notifiers = notifiers;
  items_.push_back(item);
}

int FakeNotifierSettingsProvider::closed_called_count() {
  return closed_called_count_;
}

}  // namespace message_center
