// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "content/common/accessibility_node_data.h"
#include "content/common/content_export.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "ui/gfx/native_widget_types.h"

struct AccessibilityHostMsg_EventParams;

namespace content {
class BrowserAccessibility;
#if defined(OS_ANDROID)
class BrowserAccessibilityManagerAndroid;
#endif
#if defined(OS_WIN)
class BrowserAccessibilityManagerWin;
#endif

// Class that can perform actions on behalf of the BrowserAccessibilityManager.
class CONTENT_EXPORT BrowserAccessibilityDelegate {
 public:
  virtual ~BrowserAccessibilityDelegate() {}
  virtual void SetAccessibilityFocus(int acc_obj_id) = 0;
  virtual void AccessibilityDoDefaultAction(int acc_obj_id) = 0;
  virtual void AccessibilityScrollToMakeVisible(
      int acc_obj_id, gfx::Rect subfocus) = 0;
  virtual void AccessibilityScrollToPoint(
      int acc_obj_id, gfx::Point point) = 0;
  virtual void AccessibilitySetTextSelection(
      int acc_obj_id, int start_offset, int end_offset) = 0;
  virtual bool HasFocus() const = 0;
  virtual gfx::Rect GetViewBounds() const = 0;
  virtual gfx::Point GetLastTouchEventLocation() const = 0;
  virtual void FatalAccessibilityTreeError() = 0;
};

class CONTENT_EXPORT BrowserAccessibilityFactory {
 public:
  virtual ~BrowserAccessibilityFactory() {}

  // Create an instance of BrowserAccessibility and return a new
  // reference to it.
  virtual BrowserAccessibility* Create();
};

// Manages a tree of BrowserAccessibility objects.
class CONTENT_EXPORT BrowserAccessibilityManager {
 public:
  // Creates the platform-specific BrowserAccessibilityManager, but
  // with no parent window pointer. Only useful for unit tests.
  static BrowserAccessibilityManager* Create(
      const AccessibilityNodeData& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory = new BrowserAccessibilityFactory());

  virtual ~BrowserAccessibilityManager();

  void Initialize(const AccessibilityNodeData src);

  static AccessibilityNodeData GetEmptyDocument();

  virtual void NotifyAccessibilityEvent(
      blink::WebAXEvent event_type, BrowserAccessibility* node) { }

  // Return a pointer to the root of the tree, does not make a new reference.
  BrowserAccessibility* GetRoot();

  // Removes a node from the manager.
  virtual void RemoveNode(BrowserAccessibility* node);

  // Return a pointer to the object corresponding to the given renderer_id,
  // does not make a new reference.
  BrowserAccessibility* GetFromRendererID(int32 renderer_id);

  // Called to notify the accessibility manager that its associated native
  // view got focused. This implies that it is shown (opposite of WasHidden,
  // below).
  // The touch_event_context parameter indicates that we were called in the
  // context of a touch event.
  void GotFocus(bool touch_event_context);

  // Called to notify the accessibility manager that its associated native
  // view was hidden. When it's no longer hidden, GotFocus will be called.
  void WasHidden();

  // Called to notify the accessibility manager that a mouse down event
  // occurred in the tab.
  void GotMouseDown();

  // Update the focused node to |node|, which may be null.
  // If |notify| is true, send a message to the renderer to set focus
  // to this node.
  void SetFocus(BrowserAccessibility* node, bool notify);

  // Tell the renderer to do the default action for this node.
  void DoDefaultAction(const BrowserAccessibility& node);

  // Tell the renderer to scroll to make |node| visible.
  // In addition, if it's not possible to make the entire object visible,
  // scroll so that the |subfocus| rect is visible at least. The subfocus
  // rect is in local coordinates of the object itself.
  void ScrollToMakeVisible(
      const BrowserAccessibility& node, gfx::Rect subfocus);

  // Tell the renderer to scroll such that |node| is at |point|,
  // where |point| is in global coordinates of the WebContents.
  void ScrollToPoint(
      const BrowserAccessibility& node, gfx::Point point);

  // Tell the renderer to set the text selection on a node.
  void SetTextSelection(
      const BrowserAccessibility& node, int start_offset, int end_offset);

  // Retrieve the bounds of the parent View in screen coordinates.
  gfx::Rect GetViewBounds();

  // Called when the renderer process has notified us of about tree changes.
  // Send a notification to MSAA clients of the change.
  void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>& params);

#if defined(OS_WIN)
  BrowserAccessibilityManagerWin* ToBrowserAccessibilityManagerWin();
#endif

#if defined(OS_ANDROID)
  BrowserAccessibilityManagerAndroid* ToBrowserAccessibilityManagerAndroid();
#endif

  // Return the object that has focus, if it's a descandant of the
  // given root (inclusive). Does not make a new reference.
  BrowserAccessibility* GetFocus(BrowserAccessibility* root);

  // Is the on-screen keyboard allowed to be shown, in response to a
  // focus event on a text box?
  bool IsOSKAllowed(const gfx::Rect& bounds);

  // True by default, but some platforms want to treat the root
  // scroll offsets separately.
  virtual bool UseRootScrollOffsetsWhenComputingBounds();

  // For testing only: update the given nodes as if they were
  // received from the renderer process in OnAccessibilityEvents.
  // Takes up to 7 nodes at once so tests don't need to create a vector
  // each time.
  void UpdateNodesForTesting(
      const AccessibilityNodeData& node,
      const AccessibilityNodeData& node2 = AccessibilityNodeData(),
      const AccessibilityNodeData& node3 = AccessibilityNodeData(),
      const AccessibilityNodeData& node4 = AccessibilityNodeData(),
      const AccessibilityNodeData& node5 = AccessibilityNodeData(),
      const AccessibilityNodeData& node6 = AccessibilityNodeData(),
      const AccessibilityNodeData& node7 = AccessibilityNodeData());

 protected:
  BrowserAccessibilityManager(
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory);

  BrowserAccessibilityManager(
      const AccessibilityNodeData& src,
      BrowserAccessibilityDelegate* delegate,
      BrowserAccessibilityFactory* factory);

  virtual void AddNodeToMap(BrowserAccessibility* node);

  virtual void NotifyRootChanged() {}

 private:
  // The following states keep track of whether or not the
  // on-screen keyboard is allowed to be shown.
  enum OnScreenKeyboardState {
    // Never show the on-screen keyboard because this tab is hidden.
    OSK_DISALLOWED_BECAUSE_TAB_HIDDEN,

    // This tab was just shown, so don't pop-up the on-screen keyboard if a
    // text field gets focus that wasn't the result of an explicit touch.
    OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED,

    // A touch event has occurred within the window, but focus has not
    // explicitly changed. Allow the on-screen keyboard to be shown if the
    // touch event was within the bounds of the currently focused object.
    // Otherwise we'll just wait to see if focus changes.
    OSK_ALLOWED_WITHIN_FOCUSED_OBJECT,

    // Focus has changed within a tab that's already visible. Allow the
    // on-screen keyboard to show anytime that a touch event leads to an
    // editable text control getting focus.
    OSK_ALLOWED
  };

  // Update a set of nodes using data received from the renderer
  // process.
  bool UpdateNodes(const std::vector<AccessibilityNodeData>& nodes);

  // Update one node from the tree using data received from the renderer
  // process. Returns true on success, false on fatal error.
  bool UpdateNode(const AccessibilityNodeData& src);

  void SetRoot(BrowserAccessibility* root);

  BrowserAccessibility* CreateNode(
      BrowserAccessibility* parent,
      int32 renderer_id,
      int32 index_in_parent);

 protected:
  // The object that can perform actions on our behalf.
  BrowserAccessibilityDelegate* delegate_;

  // Factory to create BrowserAccessibility objects (for dependency injection).
  scoped_ptr<BrowserAccessibilityFactory> factory_;

  // The root of the tree of accessible objects and the element that
  // currently has focus, if any.
  BrowserAccessibility* root_;
  BrowserAccessibility* focus_;

  // The on-screen keyboard state.
  OnScreenKeyboardState osk_state_;

  // A mapping from renderer IDs to BrowserAccessibility objects.
  base::hash_map<int32, BrowserAccessibility*> renderer_id_map_;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MANAGER_H_
