/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "wtf/TreeNode.h"

#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include <gtest/gtest.h>

namespace {

class TestTree : public RefCounted<TestTree>, public TreeNode<TestTree> {
public:
    static PassRefPtr<TestTree> create() { return adoptRef(new TestTree()); }
};

TEST(WTF, TreeNodeAppendChild)
{
    RefPtr<TestTree> root = TestTree::create();
    RefPtr<TestTree> firstChild = TestTree::create();
    RefPtr<TestTree> lastChild = TestTree::create();

    root->appendChild(firstChild.get());
    ASSERT_EQ(root->firstChild(), firstChild.get());
    ASSERT_EQ(root->lastChild(), firstChild.get());
    ASSERT_EQ(firstChild->parent(), root.get());

    root->appendChild(lastChild.get());
    ASSERT_EQ(root->firstChild(), firstChild.get());
    ASSERT_EQ(root->lastChild(), lastChild.get());
    ASSERT_EQ(lastChild->previous(), firstChild.get());
    ASSERT_EQ(firstChild->next(), lastChild.get());
    ASSERT_EQ(lastChild->parent(), root.get());
}

TEST(WTF, TreeNodeInsertBefore)
{
    RefPtr<TestTree> root = TestTree::create();
    RefPtr<TestTree> firstChild = TestTree::create();
    RefPtr<TestTree> middleChild = TestTree::create();
    RefPtr<TestTree> lastChild = TestTree::create();

    // Inserting single node
    root->insertBefore(lastChild.get(), 0);
    ASSERT_EQ(lastChild->parent(), root.get());
    ASSERT_EQ(root->firstChild(), lastChild.get());
    ASSERT_EQ(root->lastChild(), lastChild.get());

    // Then prepend
    root->insertBefore(firstChild.get(), lastChild.get());
    ASSERT_EQ(firstChild->parent(), root.get());
    ASSERT_EQ(root->firstChild(), firstChild.get());
    ASSERT_EQ(root->lastChild(), lastChild.get());
    ASSERT_EQ(firstChild->next(), lastChild.get());
    ASSERT_EQ(firstChild.get(), lastChild->previous());

    // Inserting in the middle
    root->insertBefore(middleChild.get(), lastChild.get());
    ASSERT_EQ(middleChild->parent(), root.get());
    ASSERT_EQ(root->firstChild(), firstChild.get());
    ASSERT_EQ(root->lastChild(), lastChild.get());
    ASSERT_EQ(middleChild->previous(), firstChild.get());
    ASSERT_EQ(middleChild->next(), lastChild.get());
    ASSERT_EQ(firstChild->next(), middleChild.get());
    ASSERT_EQ(lastChild->previous(), middleChild.get());

}

TEST(WTF, TreeNodeRemoveSingle)
{
    RefPtr<TestTree> root = TestTree::create();
    RefPtr<TestTree> child = TestTree::create();
    RefPtr<TestTree> nullNode;

    root->appendChild(child.get());
    root->removeChild(child.get());
    ASSERT_EQ(child->next(), nullNode.get());
    ASSERT_EQ(child->previous(), nullNode.get());
    ASSERT_EQ(child->parent(), nullNode.get());
    ASSERT_EQ(root->firstChild(), nullNode.get());
    ASSERT_EQ(root->lastChild(), nullNode.get());
}

class Trio {
public:
    Trio()
        : root(TestTree::create())
        , firstChild(TestTree::create())
        , middleChild(TestTree::create())
        , lastChild(TestTree::create())
    {
    }

    void appendChildren()
    {
        root->appendChild(firstChild.get());
        root->appendChild(middleChild.get());
        root->appendChild(lastChild.get());
    }

    RefPtr<TestTree> root;
    RefPtr<TestTree> firstChild;
    RefPtr<TestTree> middleChild;
    RefPtr<TestTree> lastChild;
};

TEST(WTF, TreeNodeRemoveMiddle)
{
    Trio trio;
    trio.appendChildren();

    trio.root->removeChild(trio.middleChild.get());
    ASSERT_TRUE(trio.middleChild->orphan());
    ASSERT_EQ(trio.firstChild->next(), trio.lastChild.get());
    ASSERT_EQ(trio.lastChild->previous(), trio.firstChild.get());
    ASSERT_EQ(trio.root->firstChild(), trio.firstChild.get());
    ASSERT_EQ(trio.root->lastChild(), trio.lastChild.get());
}

TEST(WTF, TreeNodeRemoveLast)
{
    RefPtr<TestTree> nullNode;
    Trio trio;
    trio.appendChildren();

    trio.root->removeChild(trio.lastChild.get());
    ASSERT_TRUE(trio.lastChild->orphan());
    ASSERT_EQ(trio.middleChild->next(), nullNode.get());
    ASSERT_EQ(trio.root->firstChild(), trio.firstChild.get());
    ASSERT_EQ(trio.root->lastChild(), trio.middleChild.get());
}

TEST(WTF, TreeNodeRemoveFirst)
{
    RefPtr<TestTree> nullNode;
    Trio trio;
    trio.appendChildren();

    trio.root->removeChild(trio.firstChild.get());
    ASSERT_TRUE(trio.firstChild->orphan());
    ASSERT_EQ(trio.middleChild->previous(), nullNode.get());
    ASSERT_EQ(trio.root->firstChild(), trio.middleChild.get());
    ASSERT_EQ(trio.root->lastChild(), trio.lastChild.get());
}

class TrioWithGrandChild : public Trio {
public:
    TrioWithGrandChild()
        : grandChild(TestTree::create())
    {
    }

    void appendChildren()
    {
        Trio::appendChildren();
        middleChild->appendChild(grandChild.get());
    }

    RefPtr<TestTree> grandChild;
};

TEST(WTF, TreeNodeTraverseNext)
{
    TrioWithGrandChild trio;
    trio.appendChildren();

    TestTree* order[] = {
        trio.root.get(), trio.firstChild.get(), trio.middleChild.get(),
        trio.grandChild.get(), trio.lastChild.get()
    };

    unsigned orderIndex = 0;
    for (TestTree* node = trio.root.get(); node; node = traverseNext(node), orderIndex++)
        ASSERT_EQ(node, order[orderIndex]);
    ASSERT_EQ(orderIndex, sizeof(order) / sizeof(TestTree*));
}

TEST(WTF, TreeNodeTraverseNextPostORder)
{
    TrioWithGrandChild trio;
    trio.appendChildren();


    TestTree* order[] = {
        trio.firstChild.get(),
        trio.grandChild.get(), trio.middleChild.get(), trio.lastChild.get(), trio.root.get()
    };

    unsigned orderIndex = 0;
    for (TestTree* node = traverseFirstPostOrder(trio.root.get()); node; node = traverseNextPostOrder(node), orderIndex++)
        ASSERT_EQ(node, order[orderIndex]);
    ASSERT_EQ(orderIndex, sizeof(order) / sizeof(TestTree*));

}


} // namespace
