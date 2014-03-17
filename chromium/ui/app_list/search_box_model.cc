// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/search_box_model.h"

#include "base/metrics/histogram.h"
#include "ui/app_list/search_box_model_observer.h"

namespace app_list {

SearchBoxModel::ButtonProperty::ButtonProperty(
    const gfx::ImageSkia& icon,
    const base::string16& tooltip)
    : icon(icon),
      tooltip(tooltip) {
}

SearchBoxModel::ButtonProperty::~ButtonProperty() {
}

SearchBoxModel::SearchBoxModel() {
}

SearchBoxModel::~SearchBoxModel() {
}

void SearchBoxModel::SetIcon(const gfx::ImageSkia& icon) {
  icon_ = icon;
  FOR_EACH_OBSERVER(SearchBoxModelObserver, observers_, IconChanged());
}

void SearchBoxModel::SetSpeechRecognitionButton(
    scoped_ptr<SearchBoxModel::ButtonProperty> speech_button) {
  speech_button_ = speech_button.Pass();
  FOR_EACH_OBSERVER(SearchBoxModelObserver,
                    observers_,
                    SpeechRecognitionButtonPropChanged());
}

void SearchBoxModel::SetHintText(const base::string16& hint_text) {
  if (hint_text_ == hint_text)
    return;

  hint_text_ = hint_text;
  FOR_EACH_OBSERVER(SearchBoxModelObserver, observers_, HintTextChanged());
}

void SearchBoxModel::SetSelectionModel(const gfx::SelectionModel& sel) {
  if (selection_model_ == sel)
    return;

  selection_model_ = sel;
  FOR_EACH_OBSERVER(SearchBoxModelObserver,
                    observers_,
                    SelectionModelChanged());
}

void SearchBoxModel::SetText(const base::string16& text) {
  if (text_ == text)
    return;

  // Log that a new search has been commenced whenever the text box text
  // transitions from empty to non-empty.
  if (text_.empty() && !text.empty()) {
    UMA_HISTOGRAM_ENUMERATION("Apps.AppListSearchCommenced", 1, 2);
  }
  text_ = text;
  FOR_EACH_OBSERVER(SearchBoxModelObserver, observers_, TextChanged());
}

void SearchBoxModel::AddObserver(SearchBoxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void SearchBoxModel::RemoveObserver(SearchBoxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace app_list
