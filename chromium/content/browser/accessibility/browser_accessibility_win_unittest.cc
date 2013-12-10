// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/scoped_bstr.h"
#include "base/win/scoped_comptr.h"
#include "base/win/scoped_variant.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_manager_win.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/common/accessibility_messages.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/win/atl_module.h"

namespace content {
namespace {


// CountedBrowserAccessibility ------------------------------------------------

// Subclass of BrowserAccessibilityWin that counts the number of instances.
class CountedBrowserAccessibility : public BrowserAccessibilityWin {
 public:
  CountedBrowserAccessibility();
  virtual ~CountedBrowserAccessibility();

  static void reset() { num_instances_ = 0; }
  static int num_instances() { return num_instances_; }

 private:
  static int num_instances_;

  DISALLOW_COPY_AND_ASSIGN(CountedBrowserAccessibility);
};

// static
int CountedBrowserAccessibility::num_instances_ = 0;

CountedBrowserAccessibility::CountedBrowserAccessibility() {
  ++num_instances_;
}

CountedBrowserAccessibility::~CountedBrowserAccessibility() {
  --num_instances_;
}


// CountedBrowserAccessibilityFactory -----------------------------------------

// Factory that creates a CountedBrowserAccessibility.
class CountedBrowserAccessibilityFactory : public BrowserAccessibilityFactory {
 public:
  CountedBrowserAccessibilityFactory();

 private:
  virtual ~CountedBrowserAccessibilityFactory();

  virtual BrowserAccessibility* Create() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(CountedBrowserAccessibilityFactory);
};

CountedBrowserAccessibilityFactory::CountedBrowserAccessibilityFactory() {
}

CountedBrowserAccessibilityFactory::~CountedBrowserAccessibilityFactory() {
}

BrowserAccessibility* CountedBrowserAccessibilityFactory::Create() {
  CComObject<CountedBrowserAccessibility>* instance;
  HRESULT hr = CComObject<CountedBrowserAccessibility>::CreateInstance(
      &instance);
  DCHECK(SUCCEEDED(hr));
  instance->AddRef();
  return instance;
}

}  // namespace


// BrowserAccessibilityTest ---------------------------------------------------

class BrowserAccessibilityTest : public testing::Test {
 public:
  BrowserAccessibilityTest();
  virtual ~BrowserAccessibilityTest();

 private:
  virtual void SetUp() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(BrowserAccessibilityTest);
};

BrowserAccessibilityTest::BrowserAccessibilityTest() {
}

BrowserAccessibilityTest::~BrowserAccessibilityTest() {
}

void BrowserAccessibilityTest::SetUp() {
  ui::win::CreateATLModuleIfNeeded();
}


// Actual tests ---------------------------------------------------------------

// Test that BrowserAccessibilityManager correctly releases the tree of
// BrowserAccessibility instances upon delete.
TEST_F(BrowserAccessibilityTest, TestNoLeaks) {
  // Create AccessibilityNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  AccessibilityNodeData button;
  button.id = 2;
  button.SetName("Button");
  button.role = WebKit::WebAXRoleButton;
  button.state = 0;

  AccessibilityNodeData checkbox;
  checkbox.id = 3;
  checkbox.SetName("Checkbox");
  checkbox.role = WebKit::WebAXRoleCheckBox;
  checkbox.state = 0;

  AccessibilityNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = WebKit::WebAXRoleRootWebArea;
  root.state = 0;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  // Construct a BrowserAccessibilityManager with this
  // AccessibilityNodeData tree and a factory for an instance-counting
  // BrowserAccessibility, and ensure that exactly 3 instances were
  // created. Note that the manager takes ownership of the factory.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          root, NULL, new CountedBrowserAccessibilityFactory()));
  manager->UpdateNodesForTesting(button, checkbox);
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());

  // Delete the manager and test that all 3 instances are deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());

  // Construct a manager again, and this time use the IAccessible interface
  // to get new references to two of the three nodes in the tree.
  manager.reset(BrowserAccessibilityManager::Create(
      root, NULL, new CountedBrowserAccessibilityFactory()));
  manager->UpdateNodesForTesting(button, checkbox);
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());
  IAccessible* root_accessible =
      manager->GetRoot()->ToBrowserAccessibilityWin();
  IDispatch* root_iaccessible = NULL;
  IDispatch* child1_iaccessible = NULL;
  base::win::ScopedVariant childid_self(CHILDID_SELF);
  HRESULT hr = root_accessible->get_accChild(childid_self, &root_iaccessible);
  ASSERT_EQ(S_OK, hr);
  base::win::ScopedVariant one(1);
  hr = root_accessible->get_accChild(one, &child1_iaccessible);
  ASSERT_EQ(S_OK, hr);

  // Now delete the manager, and only one of the three nodes in the tree
  // should be released.
  manager.reset();
  ASSERT_EQ(2, CountedBrowserAccessibility::num_instances());

  // Release each of our references and make sure that each one results in
  // the instance being deleted as its reference count hits zero.
  root_iaccessible->Release();
  ASSERT_EQ(1, CountedBrowserAccessibility::num_instances());
  child1_iaccessible->Release();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestChildrenChange) {
  // Create AccessibilityNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  AccessibilityNodeData text;
  text.id = 2;
  text.role = WebKit::WebAXRoleStaticText;
  text.SetName("old text");
  text.state = 0;

  AccessibilityNodeData root;
  root.id = 1;
  root.SetName("Document");
  root.role = WebKit::WebAXRoleRootWebArea;
  root.state = 0;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // AccessibilityNodeData tree and a factory for an instance-counting
  // BrowserAccessibility.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          root, NULL, new CountedBrowserAccessibilityFactory()));
  manager->UpdateNodesForTesting(text);

  // Query for the text IAccessible and verify that it returns "old text" as its
  // value.
  base::win::ScopedVariant one(1);
  base::win::ScopedComPtr<IDispatch> text_dispatch;
  HRESULT hr = manager->GetRoot()->ToBrowserAccessibilityWin()->get_accChild(
      one, text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedComPtr<IAccessible> text_accessible;
  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  base::win::ScopedVariant childid_self(CHILDID_SELF);
  base::win::ScopedBstr name;
  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"old text", string16(name));
  name.Reset();

  text_dispatch.Release();
  text_accessible.Release();

  // Notify the BrowserAccessibilityManager that the text child has changed.
  AccessibilityNodeData text2;
  text2.id = 2;
  text2.role = WebKit::WebAXRoleStaticText;
  text2.SetName("new text");
  text2.SetName("old text");
  AccessibilityHostMsg_EventParams param;
  param.event_type = WebKit::WebAXEventChildrenChanged;
  param.nodes.push_back(text2);
  param.id = text2.id;
  std::vector<AccessibilityHostMsg_EventParams> events;
  events.push_back(param);
  manager->OnAccessibilityEvents(events);

  // Query for the text IAccessible and verify that it now returns "new text"
  // as its value.
  hr = manager->GetRoot()->ToBrowserAccessibilityWin()->get_accChild(
      one, text_dispatch.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_dispatch.QueryInterface(text_accessible.Receive());
  ASSERT_EQ(S_OK, hr);

  hr = text_accessible->get_accName(childid_self, name.Receive());
  ASSERT_EQ(S_OK, hr);
  EXPECT_EQ(L"new text", string16(name));

  text_dispatch.Release();
  text_accessible.Release();

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestChildrenChangeNoLeaks) {
  // Create AccessibilityNodeData objects for a simple document tree,
  // representing the accessibility information used to initialize
  // BrowserAccessibilityManager.
  AccessibilityNodeData div;
  div.id = 2;
  div.role = WebKit::WebAXRoleGroup;
  div.state = 0;

  AccessibilityNodeData text3;
  text3.id = 3;
  text3.role = WebKit::WebAXRoleStaticText;
  text3.state = 0;

  AccessibilityNodeData text4;
  text4.id = 4;
  text4.role = WebKit::WebAXRoleStaticText;
  text4.state = 0;

  div.child_ids.push_back(3);
  div.child_ids.push_back(4);

  AccessibilityNodeData root;
  root.id = 1;
  root.role = WebKit::WebAXRoleRootWebArea;
  root.state = 0;
  root.child_ids.push_back(2);

  // Construct a BrowserAccessibilityManager with this
  // AccessibilityNodeData tree and a factory for an instance-counting
  // BrowserAccessibility and ensure that exactly 4 instances were
  // created. Note that the manager takes ownership of the factory.
  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          root, NULL, new CountedBrowserAccessibilityFactory()));
  manager->UpdateNodesForTesting(div, text3, text4);
  ASSERT_EQ(4, CountedBrowserAccessibility::num_instances());

  // Notify the BrowserAccessibilityManager that the div node and its children
  // were removed and ensure that only one BrowserAccessibility instance exists.
  root.child_ids.clear();
  AccessibilityHostMsg_EventParams param;
  param.event_type = WebKit::WebAXEventChildrenChanged;
  param.nodes.push_back(root);
  param.id = root.id;
  std::vector<AccessibilityHostMsg_EventParams> events;
  events.push_back(param);
  manager->OnAccessibilityEvents(events);
  ASSERT_EQ(1, CountedBrowserAccessibility::num_instances());

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestTextBoundaries) {
  std::string text1_value = "One two three.\nFour five six.";

  AccessibilityNodeData text1;
  text1.id = 11;
  text1.role = WebKit::WebAXRoleTextField;
  text1.state = 0;
  text1.AddStringAttribute(AccessibilityNodeData::ATTR_VALUE, text1_value);
  std::vector<int32> line_breaks;
  line_breaks.push_back(15);
  text1.AddIntListAttribute(
      AccessibilityNodeData::ATTR_LINE_BREAKS, line_breaks);

  AccessibilityNodeData root;
  root.id = 1;
  root.role = WebKit::WebAXRoleRootWebArea;
  root.state = 0;
  root.child_ids.push_back(11);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          root, NULL, new CountedBrowserAccessibilityFactory()));
  manager->UpdateNodesForTesting(text1);
  ASSERT_EQ(2, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();
  BrowserAccessibilityWin* text1_obj =
      root_obj->GetChild(0)->ToBrowserAccessibilityWin();

  long text1_len;
  ASSERT_EQ(S_OK, text1_obj->get_nCharacters(&text1_len));

  base::win::ScopedBstr text;
  ASSERT_EQ(S_OK, text1_obj->get_text(0, text1_len, text.Receive()));
  ASSERT_EQ(text1_value, base::UTF16ToUTF8(string16(text)));
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_text(0, 4, text.Receive()));
  ASSERT_STREQ(L"One ", text);
  text.Reset();

  long start;
  long end;
  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_CHAR, &start, &end, text.Receive()));
  ASSERT_EQ(1, start);
  ASSERT_EQ(2, end);
  ASSERT_STREQ(L"n", text);
  text.Reset();

  ASSERT_EQ(S_FALSE, text1_obj->get_textAtOffset(
      text1_len, IA2_TEXT_BOUNDARY_CHAR, &start, &end, text.Receive()));
  ASSERT_EQ(text1_len, start);
  ASSERT_EQ(text1_len, end);
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  ASSERT_EQ(0, start);
  ASSERT_EQ(3, end);
  ASSERT_STREQ(L"One", text);
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      6, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  ASSERT_EQ(4, start);
  ASSERT_EQ(7, end);
  ASSERT_STREQ(L"two", text);
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      text1_len, IA2_TEXT_BOUNDARY_WORD, &start, &end, text.Receive()));
  ASSERT_EQ(25, start);
  ASSERT_EQ(29, end);
  ASSERT_STREQ(L"six.", text);
  text.Reset();

  ASSERT_EQ(S_OK, text1_obj->get_textAtOffset(
      1, IA2_TEXT_BOUNDARY_LINE, &start, &end, text.Receive()));
  ASSERT_EQ(0, start);
  ASSERT_EQ(15, end);
  ASSERT_STREQ(L"One two three.\n", text);
  text.Reset();

  ASSERT_EQ(S_OK,
            text1_obj->get_text(0, IA2_TEXT_OFFSET_LENGTH, text.Receive()));
  ASSERT_STREQ(L"One two three.\nFour five six.", text);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestSimpleHypertext) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";

  AccessibilityNodeData text1;
  text1.id = 11;
  text1.role = WebKit::WebAXRoleStaticText;
  text1.state = 1 << WebKit::WebAXStateReadonly;
  text1.SetName(text1_name);

  AccessibilityNodeData text2;
  text2.id = 12;
  text2.role = WebKit::WebAXRoleStaticText;
  text2.state = 1 << WebKit::WebAXStateReadonly;
  text2.SetName(text2_name);

  AccessibilityNodeData root;
  root.id = 1;
  root.role = WebKit::WebAXRoleRootWebArea;
  root.state = 1 << WebKit::WebAXStateReadonly;
  root.child_ids.push_back(11);
  root.child_ids.push_back(12);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          root, NULL, new CountedBrowserAccessibilityFactory()));
  manager->UpdateNodesForTesting(root, text1, text2);
  ASSERT_EQ(3, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();

  long text_len;
  ASSERT_EQ(S_OK, root_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  ASSERT_EQ(S_OK, root_obj->get_text(0, text_len, text.Receive()));
  EXPECT_EQ(text1_name + text2_name, base::UTF16ToUTF8(string16(text)));

  long hyperlink_count;
  ASSERT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(0, hyperlink_count);

  base::win::ScopedComPtr<IAccessibleHyperlink> hyperlink;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(-1, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(0, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(28, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(29, hyperlink.Receive()));

  long hyperlink_index;
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(28, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlinkIndex(-1, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlinkIndex(29, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestComplexHypertext) {
  const std::string text1_name = "One two three.";
  const std::string text2_name = " Four five six.";
  const std::string button1_text_name = "red";
  const std::string link1_text_name = "blue";

  AccessibilityNodeData text1;
  text1.id = 11;
  text1.role = WebKit::WebAXRoleStaticText;
  text1.state = 1 << WebKit::WebAXStateReadonly;
  text1.SetName(text1_name);

  AccessibilityNodeData text2;
  text2.id = 12;
  text2.role = WebKit::WebAXRoleStaticText;
  text2.state = 1 << WebKit::WebAXStateReadonly;
  text2.SetName(text2_name);

  AccessibilityNodeData button1, button1_text;
  button1.id = 13;
  button1_text.id = 15;
  button1_text.SetName(button1_text_name);
  button1.role = WebKit::WebAXRoleButton;
  button1_text.role = WebKit::WebAXRoleStaticText;
  button1.state = 1 << WebKit::WebAXStateReadonly;
  button1_text.state = 1 << WebKit::WebAXStateReadonly;
  button1.child_ids.push_back(15);

  AccessibilityNodeData link1, link1_text;
  link1.id = 14;
  link1_text.id = 16;
  link1_text.SetName(link1_text_name);
  link1.role = WebKit::WebAXRoleLink;
  link1_text.role = WebKit::WebAXRoleStaticText;
  link1.state = 1 << WebKit::WebAXStateReadonly;
  link1_text.state = 1 << WebKit::WebAXStateReadonly;
  link1.child_ids.push_back(16);

  AccessibilityNodeData root;
  root.id = 1;
  root.role = WebKit::WebAXRoleRootWebArea;
  root.state = 1 << WebKit::WebAXStateReadonly;
  root.child_ids.push_back(11);
  root.child_ids.push_back(13);
  root.child_ids.push_back(12);
  root.child_ids.push_back(14);

  CountedBrowserAccessibility::reset();
  scoped_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(
          root, NULL, new CountedBrowserAccessibilityFactory()));
  manager->UpdateNodesForTesting(root,
                                 text1, button1, button1_text,
                                 text2, link1, link1_text);

  ASSERT_EQ(7, CountedBrowserAccessibility::num_instances());

  BrowserAccessibilityWin* root_obj =
      manager->GetRoot()->ToBrowserAccessibilityWin();

  long text_len;
  ASSERT_EQ(S_OK, root_obj->get_nCharacters(&text_len));

  base::win::ScopedBstr text;
  ASSERT_EQ(S_OK, root_obj->get_text(0, text_len, text.Receive()));
  const std::string embed = base::UTF16ToUTF8(
      BrowserAccessibilityWin::kEmbeddedCharacter);
  EXPECT_EQ(text1_name + embed + text2_name + embed,
            UTF16ToUTF8(string16(text)));
  text.Reset();

  long hyperlink_count;
  ASSERT_EQ(S_OK, root_obj->get_nHyperlinks(&hyperlink_count));
  EXPECT_EQ(2, hyperlink_count);

  base::win::ScopedComPtr<IAccessibleHyperlink> hyperlink;
  base::win::ScopedComPtr<IAccessibleText> hypertext;
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(-1, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(2, hyperlink.Receive()));
  EXPECT_EQ(E_INVALIDARG, root_obj->get_hyperlink(28, hyperlink.Receive()));

  EXPECT_EQ(S_OK, root_obj->get_hyperlink(0, hyperlink.Receive()));
  EXPECT_EQ(S_OK,
            hyperlink.QueryInterface<IAccessibleText>(hypertext.Receive()));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 3, text.Receive()));
  EXPECT_STREQ(button1_text_name.c_str(),
               base::UTF16ToUTF8(string16(text)).c_str());
  text.Reset();
  hyperlink.Release();
  hypertext.Release();

  EXPECT_EQ(S_OK, root_obj->get_hyperlink(1, hyperlink.Receive()));
  EXPECT_EQ(S_OK,
            hyperlink.QueryInterface<IAccessibleText>(hypertext.Receive()));
  EXPECT_EQ(S_OK, hypertext->get_text(0, 4, text.Receive()));
  EXPECT_STREQ(link1_text_name.c_str(),
               base::UTF16ToUTF8(string16(text)).c_str());
  text.Reset();
  hyperlink.Release();
  hypertext.Release();

  long hyperlink_index;
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(0, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(E_FAIL, root_obj->get_hyperlinkIndex(28, &hyperlink_index));
  EXPECT_EQ(-1, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(14, &hyperlink_index));
  EXPECT_EQ(0, hyperlink_index);
  EXPECT_EQ(S_OK, root_obj->get_hyperlinkIndex(30, &hyperlink_index));
  EXPECT_EQ(1, hyperlink_index);

  // Delete the manager and test that all BrowserAccessibility instances are
  // deleted.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

TEST_F(BrowserAccessibilityTest, TestCreateEmptyDocument) {
  // Try creating an empty document with busy state. Readonly is
  // set automatically.
  CountedBrowserAccessibility::reset();
  const int32 busy_state = 1 << WebKit::WebAXStateBusy;
  const int32 readonly_state = 1 << WebKit::WebAXStateReadonly;
  const int32 enabled_state = 1 << WebKit::WebAXStateEnabled;
  scoped_ptr<BrowserAccessibilityManager> manager(
      new BrowserAccessibilityManagerWin(
          GetDesktopWindow(),
          NULL,
          BrowserAccessibilityManagerWin::GetEmptyDocument(),
          NULL,
          new CountedBrowserAccessibilityFactory()));

  // Verify the root is as we expect by default.
  BrowserAccessibility* root = manager->GetRoot();
  EXPECT_EQ(0, root->renderer_id());
  EXPECT_EQ(WebKit::WebAXRoleRootWebArea, root->role());
  EXPECT_EQ(busy_state | readonly_state | enabled_state, root->state());

  // Tree with a child textfield.
  AccessibilityNodeData tree1_1;
  tree1_1.id = 1;
  tree1_1.role = WebKit::WebAXRoleRootWebArea;
  tree1_1.child_ids.push_back(2);

  AccessibilityNodeData tree1_2;
  tree1_2.id = 2;
  tree1_2.role = WebKit::WebAXRoleTextField;

  // Process a load complete.
  std::vector<AccessibilityHostMsg_EventParams> params;
  params.push_back(AccessibilityHostMsg_EventParams());
  AccessibilityHostMsg_EventParams* msg = &params[0];
  msg->event_type = WebKit::WebAXEventLoadComplete;
  msg->nodes.push_back(tree1_1);
  msg->nodes.push_back(tree1_2);
  msg->id = tree1_1.id;
  manager->OnAccessibilityEvents(params);

  // Save for later comparison.
  BrowserAccessibility* acc1_2 = manager->GetFromRendererID(2);

  // Verify the root has changed.
  EXPECT_NE(root, manager->GetRoot());

  // And the proper child remains.
  EXPECT_EQ(WebKit::WebAXRoleTextField, acc1_2->role());
  EXPECT_EQ(2, acc1_2->renderer_id());

  // Tree with a child button.
  AccessibilityNodeData tree2_1;
  tree2_1.id = 1;
  tree2_1.role = WebKit::WebAXRoleRootWebArea;
  tree2_1.child_ids.push_back(3);

  AccessibilityNodeData tree2_2;
  tree2_2.id = 3;
  tree2_2.role = WebKit::WebAXRoleButton;

  msg->nodes.clear();
  msg->nodes.push_back(tree2_1);
  msg->nodes.push_back(tree2_2);
  msg->id = tree2_1.id;

  // Fire another load complete.
  manager->OnAccessibilityEvents(params);

  BrowserAccessibility* acc2_2 = manager->GetFromRendererID(3);

  // Verify the root has changed.
  EXPECT_NE(root, manager->GetRoot());

  // And the new child exists.
  EXPECT_EQ(WebKit::WebAXRoleButton, acc2_2->role());
  EXPECT_EQ(3, acc2_2->renderer_id());

  // Ensure we properly cleaned up.
  manager.reset();
  ASSERT_EQ(0, CountedBrowserAccessibility::num_instances());
}

}  // namespace content
