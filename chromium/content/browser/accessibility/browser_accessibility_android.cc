// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_android.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/common/accessibility_messages.h"
#include "content/common/accessibility_node_data.h"

namespace content {

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityAndroid();
}

BrowserAccessibilityAndroid::BrowserAccessibilityAndroid() {
  first_time_ = true;
}

bool BrowserAccessibilityAndroid::IsNative() const {
  return true;
}

bool BrowserAccessibilityAndroid::IsLeaf() const {
  if (child_count() == 0)
    return true;

  // Iframes are always allowed to contain children.
  if (IsIframe() ||
      role() == WebKit::WebAXRoleRootWebArea ||
      role() == WebKit::WebAXRoleWebArea) {
    return false;
  }

  // If it has a focusable child, we definitely can't leave out children.
  if (HasFocusableChild())
    return false;

  // Headings with text can drop their children.
  string16 name = GetText();
  if (role() == WebKit::WebAXRoleHeading && !name.empty())
    return true;

  // Focusable nodes with text can drop their children.
  if (HasState(WebKit::WebAXStateFocusable) && !name.empty())
    return true;

  // Nodes with only static text as children can drop their children.
  if (HasOnlyStaticTextChildren())
    return true;

  return false;
}

bool BrowserAccessibilityAndroid::IsCheckable() const {
  bool checkable = false;
  bool is_aria_pressed_defined;
  bool is_mixed;
  GetAriaTristate("aria-pressed", &is_aria_pressed_defined, &is_mixed);
  if (role() == WebKit::WebAXRoleCheckBox ||
      role() == WebKit::WebAXRoleRadioButton ||
      is_aria_pressed_defined) {
    checkable = true;
  }
  if (HasState(WebKit::WebAXStateChecked))
    checkable = true;
  return checkable;
}

bool BrowserAccessibilityAndroid::IsChecked() const {
  return HasState(WebKit::WebAXStateChecked);
}

bool BrowserAccessibilityAndroid::IsClickable() const {
  return (IsLeaf() && !GetText().empty());
}

bool BrowserAccessibilityAndroid::IsEnabled() const {
  return HasState(WebKit::WebAXStateEnabled);
}

bool BrowserAccessibilityAndroid::IsFocusable() const {
  bool focusable = HasState(WebKit::WebAXStateFocusable);
  if (IsIframe() ||
      role() == WebKit::WebAXRoleWebArea) {
    focusable = false;
  }
  return focusable;
}

bool BrowserAccessibilityAndroid::IsFocused() const {
  return manager()->GetFocus(manager()->GetRoot()) == this;
}

bool BrowserAccessibilityAndroid::IsPassword() const {
  return HasState(WebKit::WebAXStateProtected);
}

bool BrowserAccessibilityAndroid::IsScrollable() const {
  int dummy;
  return GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_X_MAX, &dummy);
}

bool BrowserAccessibilityAndroid::IsSelected() const {
  return HasState(WebKit::WebAXStateSelected);
}

bool BrowserAccessibilityAndroid::IsVisibleToUser() const {
  return !HasState(WebKit::WebAXStateInvisible);
}

const char* BrowserAccessibilityAndroid::GetClassName() const {
  const char* class_name = NULL;

  switch(role()) {
    case WebKit::WebAXRoleEditableText:
    case WebKit::WebAXRoleSpinButton:
    case WebKit::WebAXRoleTextArea:
    case WebKit::WebAXRoleTextField:
      class_name = "android.widget.EditText";
      break;
    case WebKit::WebAXRoleSlider:
      class_name = "android.widget.SeekBar";
      break;
    case WebKit::WebAXRoleComboBox:
      class_name = "android.widget.Spinner";
      break;
    case WebKit::WebAXRoleButton:
    case WebKit::WebAXRoleMenuButton:
    case WebKit::WebAXRolePopUpButton:
      class_name = "android.widget.Button";
      break;
    case WebKit::WebAXRoleCheckBox:
      class_name = "android.widget.CheckBox";
      break;
    case WebKit::WebAXRoleRadioButton:
      class_name = "android.widget.RadioButton";
      break;
    case WebKit::WebAXRoleToggleButton:
      class_name = "android.widget.ToggleButton";
      break;
    case WebKit::WebAXRoleCanvas:
    case WebKit::WebAXRoleImage:
      class_name = "android.widget.Image";
      break;
    case WebKit::WebAXRoleProgressIndicator:
      class_name = "android.widget.ProgressBar";
      break;
    case WebKit::WebAXRoleTabList:
      class_name = "android.widget.TabWidget";
      break;
    case WebKit::WebAXRoleGrid:
    case WebKit::WebAXRoleTable:
      class_name = "android.widget.GridView";
      break;
    case WebKit::WebAXRoleList:
    case WebKit::WebAXRoleListBox:
      class_name = "android.widget.ListView";
      break;
    default:
      class_name = "android.view.View";
      break;
  }

  return class_name;
}

string16 BrowserAccessibilityAndroid::GetText() const {
  if (IsIframe() ||
      role() == WebKit::WebAXRoleWebArea) {
    return string16();
  }

  string16 description = GetString16Attribute(
      AccessibilityNodeData::ATTR_DESCRIPTION);
  string16 text;
  if (!name().empty())
    text = base::UTF8ToUTF16(name());
  else if (!description.empty())
    text = description;
  else if (!value().empty())
    text = base::UTF8ToUTF16(value());

  if (text.empty() && HasOnlyStaticTextChildren()) {
    for (uint32 i = 0; i < child_count(); i++) {
      BrowserAccessibility* child = GetChild(i);
      text += static_cast<BrowserAccessibilityAndroid*>(child)->GetText();
    }
  }

  switch(role()) {
    case WebKit::WebAXRoleImageMapLink:
    case WebKit::WebAXRoleLink:
      if (!text.empty())
        text += ASCIIToUTF16(" ");
      text += ASCIIToUTF16("Link");
      break;
    case WebKit::WebAXRoleHeading:
      // Only append "heading" if this node already has text.
      if (!text.empty())
        text += ASCIIToUTF16(" Heading");
      break;
  }

  return text;
}

int BrowserAccessibilityAndroid::GetItemIndex() const {
  int index = 0;
  switch(role()) {
    case WebKit::WebAXRoleListItem:
    case WebKit::WebAXRoleListBoxOption:
      index = index_in_parent();
      break;
    case WebKit::WebAXRoleSlider:
    case WebKit::WebAXRoleProgressIndicator: {
      float value_for_range;
      if (GetFloatAttribute(
              AccessibilityNodeData::ATTR_VALUE_FOR_RANGE, &value_for_range)) {
        index = static_cast<int>(value_for_range);
      }
      break;
    }
  }
  return index;
}

int BrowserAccessibilityAndroid::GetItemCount() const {
  int count = 0;
  switch(role()) {
    case WebKit::WebAXRoleList:
    case WebKit::WebAXRoleListBox:
      count = child_count();
      break;
    case WebKit::WebAXRoleSlider:
    case WebKit::WebAXRoleProgressIndicator: {
      float max_value_for_range;
      if (GetFloatAttribute(AccessibilityNodeData::ATTR_MAX_VALUE_FOR_RANGE,
                            &max_value_for_range)) {
        count = static_cast<int>(max_value_for_range);
      }
      break;
    }
  }
  return count;
}

int BrowserAccessibilityAndroid::GetScrollX() const {
  int value = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_X, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetScrollY() const {
  int value = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_Y, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetMaxScrollX() const {
  int value = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_X_MAX, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetMaxScrollY() const {
  int value = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_Y_MAX, &value);
  return value;
}

int BrowserAccessibilityAndroid::GetTextChangeFromIndex() const {
  size_t index = 0;
  while (index < old_value_.length() &&
         index < new_value_.length() &&
         old_value_[index] == new_value_[index]) {
    index++;
  }
  return index;
}

int BrowserAccessibilityAndroid::GetTextChangeAddedCount() const {
  size_t old_len = old_value_.length();
  size_t new_len = new_value_.length();
  size_t left = 0;
  while (left < old_len &&
         left < new_len &&
         old_value_[left] == new_value_[left]) {
    left++;
  }
  size_t right = 0;
  while (right < old_len &&
         right < new_len &&
         old_value_[old_len - right - 1] == new_value_[new_len - right - 1]) {
    right++;
  }
  return (new_len - left - right);
}

int BrowserAccessibilityAndroid::GetTextChangeRemovedCount() const {
  size_t old_len = old_value_.length();
  size_t new_len = new_value_.length();
  size_t left = 0;
  while (left < old_len &&
         left < new_len &&
         old_value_[left] == new_value_[left]) {
    left++;
  }
  size_t right = 0;
  while (right < old_len &&
         right < new_len &&
         old_value_[old_len - right - 1] == new_value_[new_len - right - 1]) {
    right++;
  }
  return (old_len - left - right);
}

string16 BrowserAccessibilityAndroid::GetTextChangeBeforeText() const {
  return old_value_;
}

int BrowserAccessibilityAndroid::GetSelectionStart() const {
  int sel_start = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_START, &sel_start);
  return sel_start;
}

int BrowserAccessibilityAndroid::GetSelectionEnd() const {
  int sel_end = 0;
  GetIntAttribute(AccessibilityNodeData::ATTR_TEXT_SEL_END, &sel_end);
  return sel_end;
}

int BrowserAccessibilityAndroid::GetEditableTextLength() const {
  return value().length();
}

bool BrowserAccessibilityAndroid::HasFocusableChild() const {
  for (uint32 i = 0; i < child_count(); i++) {
    BrowserAccessibility* child = GetChild(i);
    if (child->HasState(WebKit::WebAXStateFocusable))
      return true;
    if (static_cast<BrowserAccessibilityAndroid*>(child)->HasFocusableChild())
      return true;
  }
  return false;
}

bool BrowserAccessibilityAndroid::HasOnlyStaticTextChildren() const {
  for (uint32 i = 0; i < child_count(); i++) {
    BrowserAccessibility* child = GetChild(i);
    if (child->role() != WebKit::WebAXRoleStaticText)
      return false;
  }
  return true;
}

bool BrowserAccessibilityAndroid::IsIframe() const {
  string16 html_tag = GetString16Attribute(
      AccessibilityNodeData::ATTR_HTML_TAG);
  return html_tag == ASCIIToUTF16("iframe");
}

void BrowserAccessibilityAndroid::PostInitialize() {
  BrowserAccessibility::PostInitialize();

  if (IsEditableText()) {
    if (base::UTF8ToUTF16(value_) != new_value_) {
      old_value_ = new_value_;
      new_value_ = base::UTF8ToUTF16(value_);
    }
  }

  if (role_ == WebKit::WebAXRoleAlert && first_time_)
    manager_->NotifyAccessibilityEvent(WebKit::WebAXEventAlert, this);

  string16 live;
  if (GetString16Attribute(
      AccessibilityNodeData::ATTR_CONTAINER_LIVE_STATUS, &live)) {
    NotifyLiveRegionUpdate(live);
  }

  first_time_ = false;
}

void BrowserAccessibilityAndroid::NotifyLiveRegionUpdate(string16& aria_live) {
  if (!EqualsASCII(aria_live, aria_strings::kAriaLivePolite) &&
      !EqualsASCII(aria_live, aria_strings::kAriaLiveAssertive))
    return;

  string16 text = GetText();
  if (cached_text_ != text) {
    if (!text.empty()) {
      manager_->NotifyAccessibilityEvent(WebKit::WebAXEventShow,
                                         this);
    }
    cached_text_ = text;
  }
}

}  // namespace content
