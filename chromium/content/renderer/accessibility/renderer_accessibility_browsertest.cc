// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "content/common/accessibility_node_data.h"
#include "content/common/view_messages.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/accessibility/renderer_accessibility_complete.h"
#include "content/renderer/render_view_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebSize.h"
#include "third_party/WebKit/public/web/WebAXObject.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebView.h"

using blink::WebAXObject;
using blink::WebDocument;

namespace content {

class TestRendererAccessibilityComplete : public RendererAccessibilityComplete {
 public:
  explicit TestRendererAccessibilityComplete(RenderViewImpl* render_view)
    : RendererAccessibilityComplete(render_view),
      browser_tree_node_count_(0) {
  }

  int browser_tree_node_count() { return browser_tree_node_count_; }

  struct TestBrowserTreeNode : public BrowserTreeNode {
    TestBrowserTreeNode(TestRendererAccessibilityComplete* owner)
        : owner_(owner) {
      owner_->browser_tree_node_count_++;
    }

    virtual ~TestBrowserTreeNode() {
      owner_->browser_tree_node_count_--;
    }

   private:
    TestRendererAccessibilityComplete* owner_;
  };

  virtual BrowserTreeNode* CreateBrowserTreeNode() OVERRIDE {
    return new TestBrowserTreeNode(this);
  }

  void SendPendingAccessibilityEvents() {
    RendererAccessibilityComplete::SendPendingAccessibilityEvents();
  }

private:
  int browser_tree_node_count_;
};

class RendererAccessibilityTest : public RenderViewTest {
 public:
  RendererAccessibilityTest() {}

  RenderViewImpl* view() {
    return static_cast<RenderViewImpl*>(view_);
  }

  virtual void SetUp() {
    RenderViewTest::SetUp();
    sink_ = &render_thread_->sink();
  }

  void SetMode(AccessibilityMode mode) {
    view()->OnSetAccessibilityMode(mode);
  }

  void GetLastAccEvent(
      AccessibilityHostMsg_EventParams* params) {
    const IPC::Message* message =
        sink_->GetUniqueMessageMatching(AccessibilityHostMsg_Events::ID);
    ASSERT_TRUE(message);
    Tuple1<std::vector<AccessibilityHostMsg_EventParams> > param;
    AccessibilityHostMsg_Events::Read(message, &param);
    ASSERT_GE(param.a.size(), 1U);
    *params = param.a[0];
  }

  int CountAccessibilityNodesSentToBrowser() {
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    return event.nodes.size();
  }

 protected:
  IPC::TestSink* sink_;

  DISALLOW_COPY_AND_ASSIGN(RendererAccessibilityTest);
};

TEST_F(RendererAccessibilityTest, EditableTextModeFocusEvents) {
  // This is not a test of true web accessibility, it's a test of
  // a mode used on Windows 8 in Metro mode where an extremely simplified
  // accessibility tree containing only the current focused node is
  // generated.
  SetMode(AccessibilityModeEditableTextOnly);

  // Set a minimum size and give focus so simulated events work.
  view()->webwidget()->resize(blink::WebSize(500, 500));
  view()->webwidget()->setFocus(true);

  std::string html =
      "<body>"
      "  <input>"
      "  <textarea></textarea>"
      "  <p contentEditable>Editable</p>"
      "  <div tabindex=0 role=textbox>Textbox</div>"
      "  <button>Button</button>"
      "  <a href=#>Link</a>"
      "</body>";

  // Load the test page.
  LoadHTML(html.c_str());

  // We should have sent a message to the browser with the initial focus
  // on the document.
  {
    SCOPED_TRACE("Initial focus on document");
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(event.event_type,
              blink::WebAXEventLayoutComplete);
    EXPECT_EQ(event.id, 1);
    EXPECT_EQ(event.nodes.size(), 2U);
    EXPECT_EQ(event.nodes[0].id, 1);
    EXPECT_EQ(event.nodes[0].role,
              blink::WebAXRoleRootWebArea);
    EXPECT_EQ(event.nodes[0].state,
              (1U << blink::WebAXStateReadonly) |
              (1U << blink::WebAXStateFocusable) |
              (1U << blink::WebAXStateFocused));
    EXPECT_EQ(event.nodes[0].child_ids.size(), 1U);
  }

  // Now focus the input element, and check everything again.
  {
    SCOPED_TRACE("input");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('input').focus();");
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(event.event_type,
              blink::WebAXEventFocus);
    EXPECT_EQ(event.id, 3);
    EXPECT_EQ(event.nodes[0].id, 1);
    EXPECT_EQ(event.nodes[0].role,
              blink::WebAXRoleRootWebArea);
    EXPECT_EQ(event.nodes[0].state,
              (1U << blink::WebAXStateReadonly) |
              (1U << blink::WebAXStateFocusable));
    EXPECT_EQ(event.nodes[0].child_ids.size(), 1U);
    EXPECT_EQ(event.nodes[1].id, 3);
    EXPECT_EQ(event.nodes[1].role,
              blink::WebAXRoleGroup);
    EXPECT_EQ(event.nodes[1].state,
              (1U << blink::WebAXStateFocusable) |
              (1U << blink::WebAXStateFocused));
  }

  // Check other editable text nodes.
  {
    SCOPED_TRACE("textarea");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('textarea').focus();");
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(event.id, 4);
    EXPECT_EQ(event.nodes[1].state,
              (1U << blink::WebAXStateFocusable) |
              (1U << blink::WebAXStateFocused));
  }

  {
    SCOPED_TRACE("contentEditable");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('p').focus();");
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(event.id, 5);
    EXPECT_EQ(event.nodes[1].state,
              (1U << blink::WebAXStateFocusable) |
              (1U << blink::WebAXStateFocused));
  }

  {
    SCOPED_TRACE("role=textarea");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('div').focus();");
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(event.id, 6);
    EXPECT_EQ(event.nodes[1].state,
              (1U << blink::WebAXStateFocusable) |
              (1U << blink::WebAXStateFocused));
  }

  // Try focusing things that aren't editable text.
  {
    SCOPED_TRACE("button");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('button').focus();");
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(event.id, 7);
    EXPECT_EQ(event.nodes[1].state,
              (1U << blink::WebAXStateFocusable) |
              (1U << blink::WebAXStateFocused) |
              (1U << blink::WebAXStateReadonly));
  }

  {
    SCOPED_TRACE("link");
    sink_->ClearMessages();
    ExecuteJavaScript("document.querySelector('a').focus();");
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(event.id, 8);
    EXPECT_EQ(event.nodes[1].state,
              (1U << blink::WebAXStateFocusable) |
              (1U << blink::WebAXStateFocused) |
              (1U << blink::WebAXStateReadonly));
  }

  // Clear focus.
  {
    SCOPED_TRACE("Back to document.");
    sink_->ClearMessages();
    ExecuteJavaScript("document.activeElement.blur()");
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(event.id, 1);
  }
}

TEST_F(RendererAccessibilityTest, SendFullAccessibilityTreeOnReload) {
  // The job of RendererAccessibilityComplete is to serialize the
  // accessibility tree built by WebKit and send it to the browser.
  // When the accessibility tree changes, it tries to send only
  // the nodes that actually changed or were reparented. This test
  // ensures that the messages sent are correct in cases when a page
  // reloads, and that internal state is properly garbage-collected.
  std::string html =
      "<body>"
      "  <div role='group' id='A'>"
      "    <div role='group' id='A1'></div>"
      "    <div role='group' id='A2'></div>"
      "  </div>"
      "</body>";
  LoadHTML(html.c_str());

  // Creating a RendererAccessibilityComplete should sent the tree
  // to the browser.
  scoped_ptr<TestRendererAccessibilityComplete> accessibility(
      new TestRendererAccessibilityComplete(view()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  // If we post another event but the tree doesn't change,
  // we should only send 1 node to the browser.
  sink_->ClearMessages();
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  accessibility->HandleWebAccessibilityEvent(
      root_obj,
      blink::WebAXEventLayoutComplete);
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(1, CountAccessibilityNodesSentToBrowser());
  {
    // Make sure it's the root object that was updated.
    AccessibilityHostMsg_EventParams event;
    GetLastAccEvent(&event);
    EXPECT_EQ(root_obj.axID(), event.nodes[0].id);
  }

  // If we reload the page and send a event, we should send
  // all 4 nodes to the browser. Also double-check that we didn't
  // leak any of the old BrowserTreeNodes.
  LoadHTML(html.c_str());
  document = view()->GetWebView()->mainFrame()->document();
  root_obj = document.accessibilityObject();
  sink_->ClearMessages();
  accessibility->HandleWebAccessibilityEvent(
      root_obj,
      blink::WebAXEventLayoutComplete);
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  // Even if the first event is sent on an element other than
  // the root, the whole tree should be updated because we know
  // the browser doesn't have the root element.
  LoadHTML(html.c_str());
  document = view()->GetWebView()->mainFrame()->document();
  root_obj = document.accessibilityObject();
  sink_->ClearMessages();
  const WebAXObject& first_child = root_obj.childAt(0);
  accessibility->HandleWebAccessibilityEvent(
      first_child,
      blink::WebAXEventLiveRegionChanged);
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());
}

// http://crbug.com/253537
#if defined(OS_ANDROID)
#define MAYBE_AccessibilityMessagesQueueWhileSwappedOut \
        DISABLED_AccessibilityMessagesQueueWhileSwappedOut
#else
#define MAYBE_AccessibilityMessagesQueueWhileSwappedOut \
        AccessibilityMessagesQueueWhileSwappedOut
#endif

TEST_F(RendererAccessibilityTest,
       MAYBE_AccessibilityMessagesQueueWhileSwappedOut) {
  std::string html =
      "<body>"
      "  <p>Hello, world.</p>"
      "</body>";
  LoadHTML(html.c_str());

  // Creating a RendererAccessibilityComplete should send the tree
  // to the browser.
  scoped_ptr<TestRendererAccessibilityComplete> accessibility(
      new TestRendererAccessibilityComplete(view()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(5, accessibility->browser_tree_node_count());
  EXPECT_EQ(5, CountAccessibilityNodesSentToBrowser());

  // Post a "value changed" event, but then swap out
  // before sending it. It shouldn't send the event while
  // swapped out.
  sink_->ClearMessages();
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  accessibility->HandleWebAccessibilityEvent(
      root_obj,
      blink::WebAXEventValueChanged);
  view()->OnSwapOut();
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_FALSE(sink_->GetUniqueMessageMatching(
      AccessibilityHostMsg_Events::ID));

  // Navigate, so we're not swapped out anymore. Now we should
  // send accessibility events again. Note that the
  // message that was queued up before will be quickly discarded
  // because the element it was referring to no longer exists,
  // so the event here is from loading this new page.
  ViewMsg_Navigate_Params nav_params;
  nav_params.url = GURL("data:text/html,<p>Hello, again.</p>");
  nav_params.navigation_type = ViewMsg_Navigate_Type::NORMAL;
  nav_params.transition = PAGE_TRANSITION_TYPED;
  nav_params.current_history_list_length = 1;
  nav_params.current_history_list_offset = 0;
  nav_params.pending_history_list_offset = 1;
  nav_params.page_id = -1;
  view()->OnNavigate(nav_params);
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_TRUE(sink_->GetUniqueMessageMatching(
      AccessibilityHostMsg_Events::ID));
}

TEST_F(RendererAccessibilityTest, HideAccessibilityObject) {
  // Test RendererAccessibilityComplete and make sure it sends the
  // proper event to the browser when an object in the tree
  // is hidden, but its children are not.
  std::string html =
      "<body>"
      "  <div role='group' id='A'>"
      "    <div role='group' id='B'>"
      "      <div role='group' id='C' style='visibility:visible'>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</body>";
  LoadHTML(html.c_str());

  scoped_ptr<TestRendererAccessibilityComplete> accessibility(
      new TestRendererAccessibilityComplete(view()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  EXPECT_EQ(4, CountAccessibilityNodesSentToBrowser());

  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  WebAXObject node_a = root_obj.childAt(0);
  WebAXObject node_b = node_a.childAt(0);
  WebAXObject node_c = node_b.childAt(0);

  // Hide node 'B' ('C' stays visible).
  ExecuteJavaScript(
      "document.getElementById('B').style.visibility = 'hidden';");
  // Force layout now.
  ExecuteJavaScript("document.getElementById('B').offsetLeft;");

  // Send a childrenChanged on 'A'.
  sink_->ClearMessages();
  accessibility->HandleWebAccessibilityEvent(
      node_a,
      blink::WebAXEventChildrenChanged);

  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(3, accessibility->browser_tree_node_count());
  AccessibilityHostMsg_EventParams event;
  GetLastAccEvent(&event);
  ASSERT_EQ(3U, event.nodes.size());

  // RendererAccessibilityComplete notices that 'C' is being reparented,
  // so it updates 'B' first to remove 'C' as a child, then 'A' to add it,
  // and finally it updates 'C'.
  EXPECT_EQ(node_b.axID(), event.nodes[0].id);
  EXPECT_EQ(node_a.axID(), event.nodes[1].id);
  EXPECT_EQ(node_c.axID(), event.nodes[2].id);
  EXPECT_EQ(3, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RendererAccessibilityTest, ShowAccessibilityObject) {
  // Test RendererAccessibilityComplete and make sure it sends the
  // proper event to the browser when an object in the tree
  // is shown, causing its own already-visible children to be
  // reparented to it.
  std::string html =
      "<body>"
      "  <div role='group' id='A'>"
      "    <div role='group' id='B' style='visibility:hidden'>"
      "      <div role='group' id='C' style='visibility:visible'>"
      "      </div>"
      "    </div>"
      "  </div>"
      "</body>";
  LoadHTML(html.c_str());

  scoped_ptr<TestRendererAccessibilityComplete> accessibility(
      new TestRendererAccessibilityComplete(view()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(3, accessibility->browser_tree_node_count());
  EXPECT_EQ(3, CountAccessibilityNodesSentToBrowser());

  // Show node 'B', then send a childrenChanged on 'A'.
  ExecuteJavaScript(
      "document.getElementById('B').style.visibility = 'visible';");
  ExecuteJavaScript("document.getElementById('B').offsetLeft;");

  sink_->ClearMessages();
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  WebAXObject node_a = root_obj.childAt(0);
  accessibility->HandleWebAccessibilityEvent(
      node_a,
      blink::WebAXEventChildrenChanged);

  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(4, accessibility->browser_tree_node_count());
  AccessibilityHostMsg_EventParams event;
  GetLastAccEvent(&event);
  ASSERT_EQ(3U, event.nodes.size());
  EXPECT_EQ(3, CountAccessibilityNodesSentToBrowser());
}

TEST_F(RendererAccessibilityTest, DetachAccessibilityObject) {
  // Test RendererAccessibilityComplete and make sure it sends the
  // proper event to the browser when an object in the tree
  // is detached, but its children are not. This can happen when
  // a layout occurs and an anonymous render block is no longer needed.
  std::string html =
      "<body aria-label='Body'>"
      "<span>1</span><span style='display:block'>2</span>"
      "</body>";
  LoadHTML(html.c_str());

  scoped_ptr<TestRendererAccessibilityComplete> accessibility(
      new TestRendererAccessibilityComplete(view()));
  accessibility->SendPendingAccessibilityEvents();
  EXPECT_EQ(7, accessibility->browser_tree_node_count());
  EXPECT_EQ(7, CountAccessibilityNodesSentToBrowser());

  // Initially, the accessibility tree looks like this:
  //
  //   Document
  //   +--Body
  //      +--Anonymous Block
  //         +--Static Text "1"
  //            +--Inline Text Box "1"
  //      +--Static Text "2"
  //         +--Inline Text Box "2"
  WebDocument document = view()->GetWebView()->mainFrame()->document();
  WebAXObject root_obj = document.accessibilityObject();
  WebAXObject body = root_obj.childAt(0);
  WebAXObject anonymous_block = body.childAt(0);
  WebAXObject text_1 = anonymous_block.childAt(0);
  WebAXObject text_2 = body.childAt(1);

  // Change the display of the second 'span' back to inline, which causes the
  // anonymous block to be destroyed.
  ExecuteJavaScript(
      "document.querySelectorAll('span')[1].style.display = 'inline';");
  // Force layout now.
  ExecuteJavaScript("document.body.offsetLeft;");

  // Send a childrenChanged on the body.
  sink_->ClearMessages();
  accessibility->HandleWebAccessibilityEvent(
      body,
      blink::WebAXEventChildrenChanged);

  accessibility->SendPendingAccessibilityEvents();

  // Afterwards, the accessibility tree looks like this:
  //
  //   Document
  //   +--Body
  //      +--Static Text "1"
  //         +--Inline Text Box "1"
  //      +--Static Text "2"
  //         +--Inline Text Box "2"
  //
  // We just assert that there are now four nodes in the
  // accessibility tree and that only three nodes needed
  // to be updated (the body, the static text 1, and
  // the static text 2).
  EXPECT_EQ(6, accessibility->browser_tree_node_count());

  AccessibilityHostMsg_EventParams event;
  GetLastAccEvent(&event);
  ASSERT_EQ(5U, event.nodes.size());

  EXPECT_EQ(body.axID(), event.nodes[0].id);
  EXPECT_EQ(text_1.axID(), event.nodes[1].id);
  // The third event is to update text_2, but its id changes
  // so we don't have a test expectation for it.
}

}  // namespace content
