// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/ax_inspect_factory.h"

#include "content/browser/accessibility/accessibility_event_recorder_mac.h"
#include "content/browser/accessibility/accessibility_tree_formatter_blink.h"
#include "content/browser/accessibility/accessibility_tree_formatter_mac.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

// static
std::unique_ptr<ui::AXTreeFormatter>
AXInspectFactory::CreatePlatformFormatter() {
  return AXInspectFactory::CreateFormatter(kMac);
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreatePlatformRecorder(
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  return AXInspectFactory::CreateRecorder(kMac, manager, pid, selector);
}

// static
std::unique_ptr<ui::AXTreeFormatter> AXInspectFactory::CreateFormatter(
    AXInspectFactory::Type type) {
  // Developer mode: crash immediately on any accessibility fatal error.
  // This only runs during integration tests, or if a developer is
  // using an inspection tool, e.g. chrome://accessibility.
  BrowserAccessibilityManager::AlwaysFailFast();

  switch (type) {
    case kBlink:
      return std::make_unique<AccessibilityTreeFormatterBlink>();
    case kMac:
      return std::make_unique<AccessibilityTreeFormatterMac>();
    default:
      NOTREACHED() << "Unsupported inspect type " << type;
  }
  return nullptr;
}

// static
std::unique_ptr<ui::AXEventRecorder> AXInspectFactory::CreateRecorder(
    AXInspectFactory::Type type,
    BrowserAccessibilityManager* manager,
    base::ProcessId pid,
    const ui::AXTreeSelector& selector) {
  // Developer mode: crash immediately on any accessibility fatal error.
  // This only runs during integration tests, or if a developer is
  // using an inspection tool, e.g. chrome://accessibility.
  BrowserAccessibilityManager::AlwaysFailFast();

  switch (type) {
    case kMac:
      return std::make_unique<AccessibilityEventRecorderMac>(manager, pid,
                                                             selector);
    default:
      NOTREACHED() << "Unsupported inspect type " << type;
  }
  return nullptr;
}

}  // namespace content
