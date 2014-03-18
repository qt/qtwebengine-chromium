// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace ui {

TEST(AXTreeTest, SerializeSimpleAXTree) {
  AXNodeData root;
  root.id = 1;
  root.role = AX_ROLE_ROOT_WEB_AREA;
  root.child_ids.push_back(2);
  root.child_ids.push_back(3);

  AXNodeData button;
  button.id = 2;
  button.role = AX_ROLE_BUTTON;
  button.state = 0;

  AXNodeData checkbox;
  checkbox.id = 3;
  checkbox.role = AX_ROLE_CHECK_BOX;

  AXTreeUpdate initial_state;
  initial_state.nodes.push_back(root);
  initial_state.nodes.push_back(button);
  initial_state.nodes.push_back(checkbox);
  AXSerializableTree src_tree(initial_state);

  scoped_ptr<AXTreeSource<AXNode> > tree_source(
      src_tree.CreateTreeSource());
  AXTreeSerializer<AXNode> serializer(tree_source.get());
  AXTreeUpdate update;
  serializer.SerializeChanges(src_tree.GetRoot(), &update);

  AXTree dst_tree;
  ASSERT_TRUE(dst_tree.Unserialize(update));

  AXNode* root_node = dst_tree.GetRoot();
  ASSERT_TRUE(root_node != NULL);
  EXPECT_EQ(root.id, root_node->id());
  EXPECT_EQ(root.role, root_node->data().role);

  ASSERT_EQ(2, root_node->child_count());

  AXNode* button_node = root_node->ChildAtIndex(0);
  EXPECT_EQ(button.id, button_node->id());
  EXPECT_EQ(button.role, button_node->data().role);

  AXNode* checkbox_node = root_node->ChildAtIndex(1);
  EXPECT_EQ(checkbox.id, checkbox_node->id());
  EXPECT_EQ(checkbox.role, checkbox_node->data().role);
}

TEST(AXTreeTest, DeleteUnknownSubtreeFails) {
  AXNodeData root;
  root.id = 1;
  root.role = AX_ROLE_ROOT_WEB_AREA;

  AXTreeUpdate initial_state;
  initial_state.nodes.push_back(root);
  AXTree tree(initial_state);

  // This should fail because we're asking it to delete
  // a subtree rooted at id=2, which doesn't exist.
  AXTreeUpdate update;
  update.node_id_to_clear = 2;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].id = AX_ROLE_ROOT_WEB_AREA;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Bad node_id_to_clear: 2", tree.error());
}

TEST(AXTreeTest, LeaveOrphanedDeletedSubtreeFails) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[0].child_ids.push_back(3);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[2].id = 3;
  AXTree tree(initial_state);

  // This should fail because we delete a subtree rooted at id=2
  // but never update it.
  AXTreeUpdate update;
  update.node_id_to_clear = 2;
  update.nodes.resize(1);
  update.nodes[0].id = 3;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Nodes left pending by the update: 2", tree.error());
}

TEST(AXTreeTest, LeaveOrphanedNewChildFails) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  AXTree tree(initial_state);

  // This should fail because we add a new child to the root node
  // but never update it.
  AXTreeUpdate update;
  update.nodes.resize(1);
  update.nodes[0].id = 1;
  update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  update.nodes[0].child_ids.push_back(2);
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Nodes left pending by the update: 2", tree.error());
}

TEST(AXTreeTest, DuplicateChildIdFails) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(1);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  AXTree tree(initial_state);

  // This should fail because a child id appears twice.
  AXTreeUpdate update;
  update.nodes.resize(2);
  update.nodes[0].id = 1;
  update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  update.nodes[0].child_ids.push_back(2);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[1].id = 2;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Node 1 has duplicate child id 2", tree.error());
}

TEST(AXTreeTest, InvalidReparentingFails) {
  AXTreeUpdate initial_state;
  initial_state.nodes.resize(3);
  initial_state.nodes[0].id = 1;
  initial_state.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  initial_state.nodes[0].child_ids.push_back(2);
  initial_state.nodes[1].id = 2;
  initial_state.nodes[1].child_ids.push_back(3);
  initial_state.nodes[2].id = 3;

  AXTree tree(initial_state);

  // This should fail because node 3 is reparented from node 2 to node 1
  // without deleting node 1's subtree first.
  AXTreeUpdate update;
  update.nodes.resize(3);
  update.nodes[0].id = 1;
  update.nodes[0].role = AX_ROLE_ROOT_WEB_AREA;
  update.nodes[0].child_ids.push_back(3);
  update.nodes[0].child_ids.push_back(2);
  update.nodes[1].id = 2;
  update.nodes[2].id = 3;
  EXPECT_FALSE(tree.Unserialize(update));
  ASSERT_EQ("Node 3 reparented from 2 to 1", tree.error());
}

}  // namespace ui
