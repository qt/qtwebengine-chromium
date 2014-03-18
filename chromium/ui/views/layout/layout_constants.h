// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_LAYOUT_LAYOUT_CONSTANTS_H_
#define UI_VIEWS_LAYOUT_LAYOUT_CONSTANTS_H_

// This file contains some constants we use to implement our standard panel
// layout.
// see: spec 21/4

namespace views {

// Left or right margin.
const int kPanelHorizMargin = 13;

// Top or bottom margin.
const int kPanelVertMargin = 13;

// If some UI has some sub UI. Indent horizontally by the following value.
const int kPanelHorizIndentation = 24;

// When several controls are aligned vertically, the baseline should be spaced
// by the following number of pixels.
const int kPanelVerticalSpacing = 32;

// Vertical spacing between sub UI.
const int kPanelSubVerticalSpacing = 24;

// Vertical spacing between a label and some control.
const int kLabelToControlVerticalSpacing = 8;

// Small horizontal spacing between controls that are logically related.
const int kRelatedControlSmallHorizontalSpacing = 8;

// Horizontal spacing between controls that are logically related.
const int kRelatedControlHorizontalSpacing = 8;

// Vertical spacing between controls that are logically related.
const int kRelatedControlVerticalSpacing = 8;

// Small vertical spacing between controls that are logically related.
const int kRelatedControlSmallVerticalSpacing = 4;

// Horizontal spacing between controls that are logically unrelated.
const int kUnrelatedControlHorizontalSpacing = 12;

// Larger horizontal spacing between unrelated controls.
const int kUnrelatedControlLargeHorizontalSpacing = 20;

// Vertical spacing between controls that are logically unrelated.
const int kUnrelatedControlVerticalSpacing = 20;

// Larger vertical spacing between unrelated controls.
const int kUnrelatedControlLargeVerticalSpacing = 30;

// Vertical spacing between the edge of the window and the
// top or bottom of a button.
const int kButtonVEdgeMargin = 9;

// Vertical spacing between the edge of the window and the
// left or right of a button.
const int kButtonHEdgeMargin = 13;

// Vertical spacing between the edge of the window and the
// top or bottom of a button (when using new style dialogs).
const int kButtonVEdgeMarginNew = 20;

// Vertical spacing between the edge of the window and the
// left or right of a button (when using new style dialogs).
const int kButtonHEdgeMarginNew = 20;

// Horizontal spacing between buttons that are logically related.
const int kRelatedButtonHSpacing = 6;

// Indent of checkboxes relative to related text.
const int kCheckboxIndent = 10;

// Horizontal spacing between the end of an item (i.e. an icon or a checkbox)
// and the start of its corresponding text.
const int kItemLabelSpacing = 10;

}  // namespace views

#endif  // UI_VIEWS_LAYOUT_LAYOUT_CONSTANTS_H_
