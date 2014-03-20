// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_node.h"

namespace ui {

AXNode::AXNode(AXNode* parent, int32 id, int32 index_in_parent)
    : index_in_parent_(index_in_parent),
      parent_(parent) {
  data_.id = id;
}

AXNode::~AXNode() {
}

void AXNode::SetData(const AXNodeData& src) {
  data_ = src;
}

void AXNode::SetIndexInParent(int index_in_parent) {
  index_in_parent_ = index_in_parent;
}

void AXNode::SwapChildren(std::vector<AXNode*>& children) {
  children.swap(children_);
}

void AXNode::Destroy() {
  delete this;
}

}  // namespace ui
