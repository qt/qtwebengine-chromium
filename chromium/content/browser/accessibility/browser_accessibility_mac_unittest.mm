// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_cocoa.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_mac.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#import "ui/base/test/ui_cocoa_test_helper.h"

@interface MockAccessibilityDelegate :
    NSView<BrowserAccessibilityDelegateCocoa>

- (NSPoint)accessibilityPointInScreen:(NSPoint)origin
                                 size:(NSSize)size;
- (void)doDefaultAction:(int32)accessibilityObjectId;
- (void)accessibilitySetTextSelection:(int32)accId
                          startOffset:(int32)startOffset
                            endOffset:(int32)endOffset;
- (void)performShowMenuAction:(BrowserAccessibilityCocoa*)accessibility;
- (void)setAccessibilityFocus:(BOOL)focus
              accessibilityId:(int32)accessibilityObjectId;
- (NSWindow*)window;

@end

@implementation MockAccessibilityDelegate

- (NSPoint)accessibilityPointInScreen:(NSPoint)origin
                                 size:(NSSize)size {
  return NSZeroPoint;
}
- (void)doDefaultAction:(int32)accessibilityObjectId {
}
- (void)accessibilitySetTextSelection:(int32)accId
                          startOffset:(int32)startOffset
                            endOffset:(int32)endOffset {
}
- (void)performShowMenuAction:(BrowserAccessibilityCocoa*)accessibility {
}
- (void)setAccessibilityFocus:(BOOL)focus
              accessibilityId:(int32)accessibilityObjectId {
}
- (NSWindow*)window {
  return nil;
}

@end

namespace content {

class BrowserAccessibilityTest : public ui::CocoaTest {
 public:
  virtual void SetUp() {
    CocoaTest::SetUp();
    RebuildAccessibilityTree();
  }

 protected:
  void RebuildAccessibilityTree() {
    AccessibilityNodeData root;
    root.id = 1000;
    root.location.set_width(500);
    root.location.set_height(100);
    root.role = blink::WebAXRoleRootWebArea;
    root.AddStringAttribute(AccessibilityNodeData::ATTR_HELP, "HelpText");
    root.child_ids.push_back(1001);
    root.child_ids.push_back(1002);

    AccessibilityNodeData child1;
    child1.id = 1001;
    child1.SetName("Child1");
    child1.location.set_width(250);
    child1.location.set_height(100);
    child1.role = blink::WebAXRoleButton;

    AccessibilityNodeData child2;
    child2.id = 1002;
    child2.location.set_x(250);
    child2.location.set_width(250);
    child2.location.set_height(100);
    child2.role = blink::WebAXRoleHeading;

    delegate_.reset([[MockAccessibilityDelegate alloc] init]);
    manager_.reset(
        new BrowserAccessibilityManagerMac(delegate_, root, NULL));
    manager_->UpdateNodesForTesting(child1, child2);
    accessibility_.reset([manager_->GetRoot()->ToBrowserAccessibilityCocoa()
        retain]);
  }

  base::scoped_nsobject<MockAccessibilityDelegate> delegate_;
  base::scoped_nsobject<BrowserAccessibilityCocoa> accessibility_;
  scoped_ptr<BrowserAccessibilityManager> manager_;
};

// Standard hit test.
TEST_F(BrowserAccessibilityTest, HitTestTest) {
  BrowserAccessibilityCocoa* firstChild =
      [accessibility_ accessibilityHitTest:NSMakePoint(50, 50)];
  EXPECT_NSEQ(@"Child1",
      [firstChild accessibilityAttributeValue:NSAccessibilityTitleAttribute]);
}

// Test doing a hit test on the edge of a child.
TEST_F(BrowserAccessibilityTest, EdgeHitTest) {
  BrowserAccessibilityCocoa* firstChild =
      [accessibility_ accessibilityHitTest:NSZeroPoint];
  EXPECT_NSEQ(@"Child1",
      [firstChild accessibilityAttributeValue:NSAccessibilityTitleAttribute]);
}

// This will test a hit test with invalid coordinates.  It is assumed that
// the hit test has been narrowed down to this object or one of its children
// so it should return itself since it has no better hit result.
TEST_F(BrowserAccessibilityTest, InvalidHitTestCoordsTest) {
  BrowserAccessibilityCocoa* hitTestResult =
      [accessibility_ accessibilityHitTest:NSMakePoint(-50, 50)];
  EXPECT_NSEQ(accessibility_, hitTestResult);
}

// Test to ensure querying standard attributes works.
TEST_F(BrowserAccessibilityTest, BasicAttributeTest) {
  NSString* helpText = [accessibility_
      accessibilityAttributeValue:NSAccessibilityHelpAttribute];
  EXPECT_NSEQ(@"HelpText", helpText);
}

// Test querying for an invalid attribute to ensure it doesn't crash.
TEST_F(BrowserAccessibilityTest, InvalidAttributeTest) {
  NSString* shouldBeNil = [accessibility_
      accessibilityAttributeValue:@"NSAnInvalidAttribute"];
  EXPECT_TRUE(shouldBeNil == nil);
}

TEST_F(BrowserAccessibilityTest, RetainedDetachedObjectsReturnNil) {
  // Get the first child.
  BrowserAccessibilityCocoa* retainedFirstChild =
      [accessibility_ accessibilityHitTest:NSMakePoint(50, 50)];
  EXPECT_NSEQ(@"Child1", [retainedFirstChild
      accessibilityAttributeValue:NSAccessibilityTitleAttribute]);

  // Retain it. This simulates what the system might do with an
  // accessibility object.
  [retainedFirstChild retain];

  // Rebuild the accessibility tree, which should detach |retainedFirstChild|.
  RebuildAccessibilityTree();

  // Now any attributes we query should return nil.
  EXPECT_EQ(nil, [retainedFirstChild
      accessibilityAttributeValue:NSAccessibilityTitleAttribute]);

  // Don't leak memory in the test.
  [retainedFirstChild release];
}

}  // namespace content
