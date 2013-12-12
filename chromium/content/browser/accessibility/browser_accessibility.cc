// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/common/accessibility_messages.h"

namespace content {

typedef AccessibilityNodeData::BoolAttribute BoolAttribute;
typedef AccessibilityNodeData::FloatAttribute FloatAttribute;
typedef AccessibilityNodeData::IntAttribute IntAttribute;
typedef AccessibilityNodeData::StringAttribute StringAttribute;

#if !defined(OS_MACOSX) && \
    !defined(OS_WIN) && \
    !defined(TOOLKIT_GTK) && \
    !defined(OS_ANDROID)
// We have subclassess of BrowserAccessibility on Mac, Linux/GTK,
// and Win. For any other platform, instantiate the base class.
// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibility();
}
#endif

BrowserAccessibility::BrowserAccessibility()
    : manager_(NULL),
      parent_(NULL),
      index_in_parent_(0),
      renderer_id_(0),
      role_(0),
      state_(0),
      instance_active_(false) {
}

BrowserAccessibility::~BrowserAccessibility() {
}

void BrowserAccessibility::DetachTree(
    std::vector<BrowserAccessibility*>* nodes) {
  nodes->push_back(this);
  for (size_t i = 0; i < children_.size(); i++)
    children_[i]->DetachTree(nodes);
  children_.clear();
  parent_ = NULL;
}

void BrowserAccessibility::InitializeTreeStructure(
    BrowserAccessibilityManager* manager,
    BrowserAccessibility* parent,
    int32 renderer_id,
    int32 index_in_parent) {
  manager_ = manager;
  parent_ = parent;
  renderer_id_ = renderer_id;
  index_in_parent_ = index_in_parent;
}

void BrowserAccessibility::InitializeData(const AccessibilityNodeData& src) {
  DCHECK_EQ(renderer_id_, src.id);
  role_ = src.role;
  state_ = src.state;
  string_attributes_ = src.string_attributes;
  int_attributes_ = src.int_attributes;
  float_attributes_ = src.float_attributes;
  bool_attributes_ = src.bool_attributes;
  intlist_attributes_ = src.intlist_attributes;
  html_attributes_ = src.html_attributes;
  location_ = src.location;
  instance_active_ = true;

  GetStringAttribute(AccessibilityNodeData::ATTR_NAME, &name_);
  GetStringAttribute(AccessibilityNodeData::ATTR_VALUE, &value_);

  PreInitialize();
}

bool BrowserAccessibility::IsNative() const {
  return false;
}

void BrowserAccessibility::SwapChildren(
    std::vector<BrowserAccessibility*>& children) {
  children.swap(children_);
}

void BrowserAccessibility::UpdateParent(BrowserAccessibility* parent,
                                        int index_in_parent) {
  parent_ = parent;
  index_in_parent_ = index_in_parent;
}

void BrowserAccessibility::SetLocation(const gfx::Rect& new_location) {
  location_ = new_location;
}

bool BrowserAccessibility::IsDescendantOf(
    BrowserAccessibility* ancestor) {
  if (this == ancestor) {
    return true;
  } else if (parent_) {
    return parent_->IsDescendantOf(ancestor);
  }

  return false;
}

BrowserAccessibility* BrowserAccessibility::GetChild(uint32 child_index) const {
  DCHECK(child_index < children_.size());
  return children_[child_index];
}

BrowserAccessibility* BrowserAccessibility::GetPreviousSibling() {
  if (parent_ && index_in_parent_ > 0)
    return parent_->children_[index_in_parent_ - 1];

  return NULL;
}

BrowserAccessibility* BrowserAccessibility::GetNextSibling() {
  if (parent_ &&
      index_in_parent_ >= 0 &&
      index_in_parent_ < static_cast<int>(parent_->children_.size() - 1)) {
    return parent_->children_[index_in_parent_ + 1];
  }

  return NULL;
}

gfx::Rect BrowserAccessibility::GetLocalBoundsRect() const {
  gfx::Rect bounds = location_;

  // Walk up the parent chain. Every time we encounter a Web Area, offset
  // based on the scroll bars and then offset based on the origin of that
  // nested web area.
  BrowserAccessibility* parent = parent_;
  bool need_to_offset_web_area =
      (role_ == WebKit::WebAXRoleWebArea ||
       role_ == WebKit::WebAXRoleRootWebArea);
  while (parent) {
    if (need_to_offset_web_area &&
        parent->location().width() > 0 &&
        parent->location().height() > 0) {
      bounds.Offset(parent->location().x(), parent->location().y());
      need_to_offset_web_area = false;
    }

    // On some platforms, we don't want to take the root scroll offsets
    // into account.
    if (parent->role() == WebKit::WebAXRoleRootWebArea &&
        !manager()->UseRootScrollOffsetsWhenComputingBounds()) {
      break;
    }

    if (parent->role() == WebKit::WebAXRoleWebArea ||
        parent->role() == WebKit::WebAXRoleRootWebArea) {
      int sx = 0;
      int sy = 0;
      if (parent->GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_X, &sx) &&
          parent->GetIntAttribute(AccessibilityNodeData::ATTR_SCROLL_Y, &sy)) {
        bounds.Offset(-sx, -sy);
      }
      need_to_offset_web_area = true;
    }
    parent = parent->parent();
  }

  return bounds;
}

gfx::Rect BrowserAccessibility::GetGlobalBoundsRect() const {
  gfx::Rect bounds = GetLocalBoundsRect();

  // Adjust the bounds by the top left corner of the containing view's bounds
  // in screen coordinates.
  bounds.Offset(manager_->GetViewBounds().OffsetFromOrigin());

  return bounds;
}

BrowserAccessibility* BrowserAccessibility::BrowserAccessibilityForPoint(
    const gfx::Point& point) {
  // Walk the children recursively looking for the BrowserAccessibility that
  // most tightly encloses the specified point.
  for (int i = children_.size() - 1; i >= 0; --i) {
    BrowserAccessibility* child = children_[i];
    if (child->GetGlobalBoundsRect().Contains(point))
      return child->BrowserAccessibilityForPoint(point);
  }
  return this;
}

void BrowserAccessibility::Destroy() {
  for (std::vector<BrowserAccessibility*>::iterator iter = children_.begin();
       iter != children_.end();
       ++iter) {
    (*iter)->Destroy();
  }
  children_.clear();

  // Allow the object to fire a TextRemoved notification.
  name_.clear();
  value_.clear();
  PostInitialize();

  manager_->NotifyAccessibilityEvent(
      WebKit::WebAXEventHide, this);

  instance_active_ = false;
  manager_->RemoveNode(this);
  NativeReleaseReference();
}

void BrowserAccessibility::NativeReleaseReference() {
  delete this;
}

bool BrowserAccessibility::HasBoolAttribute(BoolAttribute attribute) const {
  for (size_t i = 0; i < bool_attributes_.size(); ++i) {
    if (bool_attributes_[i].first == attribute)
      return true;
  }

  return false;
}


bool BrowserAccessibility::GetBoolAttribute(BoolAttribute attribute) const {
  for (size_t i = 0; i < bool_attributes_.size(); ++i) {
    if (bool_attributes_[i].first == attribute)
      return bool_attributes_[i].second;
  }

  return false;
}

bool BrowserAccessibility::GetBoolAttribute(
    BoolAttribute attribute, bool* value) const {
  for (size_t i = 0; i < bool_attributes_.size(); ++i) {
    if (bool_attributes_[i].first == attribute) {
      *value = bool_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasFloatAttribute(FloatAttribute attribute) const {
  for (size_t i = 0; i < float_attributes_.size(); ++i) {
    if (float_attributes_[i].first == attribute)
      return true;
  }

  return false;
}

float BrowserAccessibility::GetFloatAttribute(FloatAttribute attribute) const {
  for (size_t i = 0; i < float_attributes_.size(); ++i) {
    if (float_attributes_[i].first == attribute)
      return float_attributes_[i].second;
  }

  return 0.0;
}

bool BrowserAccessibility::GetFloatAttribute(
    FloatAttribute attribute, float* value) const {
  for (size_t i = 0; i < float_attributes_.size(); ++i) {
    if (float_attributes_[i].first == attribute) {
      *value = float_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasIntAttribute(IntAttribute attribute) const {
  for (size_t i = 0; i < int_attributes_.size(); ++i) {
    if (int_attributes_[i].first == attribute)
      return true;
  }

  return false;
}

int BrowserAccessibility::GetIntAttribute(IntAttribute attribute) const {
  for (size_t i = 0; i < int_attributes_.size(); ++i) {
    if (int_attributes_[i].first == attribute)
      return int_attributes_[i].second;
  }

  return 0;
}

bool BrowserAccessibility::GetIntAttribute(
    IntAttribute attribute, int* value) const {
  for (size_t i = 0; i < int_attributes_.size(); ++i) {
    if (int_attributes_[i].first == attribute) {
      *value = int_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::HasStringAttribute(StringAttribute attribute) const {
  for (size_t i = 0; i < string_attributes_.size(); ++i) {
    if (string_attributes_[i].first == attribute)
      return true;
  }

  return false;
}

const std::string& BrowserAccessibility::GetStringAttribute(
    StringAttribute attribute) const {
  CR_DEFINE_STATIC_LOCAL(std::string, empty_string, ());
  for (size_t i = 0; i < string_attributes_.size(); ++i) {
    if (string_attributes_[i].first == attribute)
      return string_attributes_[i].second;
  }

  return empty_string;
}

bool BrowserAccessibility::GetStringAttribute(
    StringAttribute attribute, std::string* value) const {
  for (size_t i = 0; i < string_attributes_.size(); ++i) {
    if (string_attributes_[i].first == attribute) {
      *value = string_attributes_[i].second;
      return true;
    }
  }

  return false;
}

string16 BrowserAccessibility::GetString16Attribute(
    StringAttribute attribute) const {
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return string16();
  return UTF8ToUTF16(value_utf8);
}

bool BrowserAccessibility::GetString16Attribute(
    StringAttribute attribute,
    string16* value) const {
  std::string value_utf8;
  if (!GetStringAttribute(attribute, &value_utf8))
    return false;
  *value = UTF8ToUTF16(value_utf8);
  return true;
}

void BrowserAccessibility::SetStringAttribute(
    StringAttribute attribute, const std::string& value) {
  for (size_t i = 0; i < string_attributes_.size(); ++i) {
    if (string_attributes_[i].first == attribute) {
      string_attributes_[i].second = value;
      return;
    }
  }
  if (!value.empty())
    string_attributes_.push_back(std::make_pair(attribute, value));
}

bool BrowserAccessibility::HasIntListAttribute(
    AccessibilityNodeData::IntListAttribute attribute) const {
  for (size_t i = 0; i < intlist_attributes_.size(); ++i) {
    if (intlist_attributes_[i].first == attribute)
      return true;
  }

  return false;
}

const std::vector<int32>& BrowserAccessibility::GetIntListAttribute(
    AccessibilityNodeData::IntListAttribute attribute) const {
  CR_DEFINE_STATIC_LOCAL(std::vector<int32>, empty_vector, ());
  for (size_t i = 0; i < intlist_attributes_.size(); ++i) {
    if (intlist_attributes_[i].first == attribute)
      return intlist_attributes_[i].second;
  }

  return empty_vector;
}

bool BrowserAccessibility::GetIntListAttribute(
    AccessibilityNodeData::IntListAttribute attribute,
    std::vector<int32>* value) const {
  for (size_t i = 0; i < intlist_attributes_.size(); ++i) {
    if (intlist_attributes_[i].first == attribute) {
      *value = intlist_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, std::string* value) const {
  for (size_t i = 0; i < html_attributes_.size(); i++) {
    const std::string& attr = html_attributes_[i].first;
    if (LowerCaseEqualsASCII(attr, html_attr)) {
      *value = html_attributes_[i].second;
      return true;
    }
  }

  return false;
}

bool BrowserAccessibility::GetHtmlAttribute(
    const char* html_attr, string16* value) const {
  std::string value_utf8;
  if (!GetHtmlAttribute(html_attr, &value_utf8))
    return false;
  *value = UTF8ToUTF16(value_utf8);
  return true;
}

bool BrowserAccessibility::GetAriaTristate(
    const char* html_attr,
    bool* is_defined,
    bool* is_mixed) const {
  *is_defined = false;
  *is_mixed = false;

  string16 value;
  if (!GetHtmlAttribute(html_attr, &value) ||
      value.empty() ||
      EqualsASCII(value, "undefined")) {
    return false;  // Not set (and *is_defined is also false)
  }

  *is_defined = true;

  if (EqualsASCII(value, "true"))
    return true;

  if (EqualsASCII(value, "mixed"))
    *is_mixed = true;

  return false;  // Not set
}

bool BrowserAccessibility::HasState(WebKit::WebAXState state_enum) const {
  return (state_ >> state_enum) & 1;
}

bool BrowserAccessibility::IsEditableText() const {
  // These roles don't have readonly set, but they're not editable text.
  if (role_ == WebKit::WebAXRoleScrollArea ||
      role_ == WebKit::WebAXRoleColumn ||
      role_ == WebKit::WebAXRoleTableHeaderContainer) {
    return false;
  }

  // Note: WebAXStateReadonly being false means it's either a text control,
  // or contenteditable. We also check for editable text roles to cover
  // another element that has role=textbox set on it.
  return (!HasState(WebKit::WebAXStateReadonly) ||
          role_ == WebKit::WebAXRoleTextField ||
          role_ == WebKit::WebAXRoleTextArea);
}

std::string BrowserAccessibility::GetTextRecursive() const {
  if (!name_.empty()) {
    return name_;
  }

  std::string result;
  for (size_t i = 0; i < children_.size(); ++i)
    result += children_[i]->GetTextRecursive();
  return result;
}

}  // namespace content
