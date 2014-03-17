/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/editing/TextIterator.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/dom/Range.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/FrameView.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/testing/DummyPageHolder.h"
#include "platform/geometry/IntSize.h"
#include "wtf/Compiler.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/Vector.h"
#include "wtf/testing/WTFTestHelpers.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

class TextIteratorTest : public ::testing::Test {
protected:
    virtual void SetUp() OVERRIDE;

    HTMLDocument& document() const;

    Vector<String> iterate(TextIteratorBehavior = TextIteratorDefaultBehavior);

    void setBodyInnerHTML(const char*);
    PassRefPtr<Range> getBodyRange() const;

private:
    OwnPtr<DummyPageHolder> m_dummyPageHolder;

    HTMLDocument* m_document;
};

void TextIteratorTest::SetUp()
{
    m_dummyPageHolder = DummyPageHolder::create(IntSize(800, 600));
    m_document = toHTMLDocument(&m_dummyPageHolder->document());
    ASSERT(m_document);
}

Vector<String> TextIteratorTest::iterate(TextIteratorBehavior iteratorBehavior)
{
    document().view()->updateLayoutAndStyleIfNeededRecursive(); // Force renderers to be created; TextIterator needs them.

    RefPtr<Range> range = getBodyRange();
    TextIterator textIterator(range.get(), iteratorBehavior);
    Vector<String> textChunks;
    while (!textIterator.atEnd()) {
        textChunks.append(textIterator.substring(0, textIterator.length()));
        textIterator.advance();
    }
    return textChunks;
}

HTMLDocument& TextIteratorTest::document() const
{
    return *m_document;
}

void TextIteratorTest::setBodyInnerHTML(const char* bodyContent)
{
    document().body()->setInnerHTML(String::fromUTF8(bodyContent), ASSERT_NO_EXCEPTION);
}

PassRefPtr<Range> TextIteratorTest::getBodyRange() const
{
    RefPtr<Range> range(Range::create(document()));
    range->selectNode(document().body());
    return range.release();
}

Vector<String> createVectorString(const char* const* rawStrings, size_t size)
{
    Vector<String> result;
    result.append(rawStrings, size);
    return result;
}

PassRefPtr<ShadowRoot> createShadowRootForElementWithIDAndSetInnerHTML(TreeScope& scope, const char* hostElementID, const char* shadowRootContent)
{
    RefPtr<ShadowRoot> shadowRoot = scope.getElementById(AtomicString::fromUTF8(hostElementID))->createShadowRoot(ASSERT_NO_EXCEPTION);
    shadowRoot->setInnerHTML(String::fromUTF8(shadowRootContent), ASSERT_NO_EXCEPTION);
    return shadowRoot.release();
}

TEST_F(TextIteratorTest, BasicIteration)
{
    static const char* input = "<p>Hello, \ntext</p><p>iterator.</p>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        "text",
        "\n",
        "\n",
        "iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(input);
    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, NotEnteringTextControls)
{
    static const char* input = "<p>Hello <input type=\"text\" value=\"input\">!</p>";
    static const char* expectedTextChunksRawString[] = {
        "Hello ",
        "",
        "!",
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(input);
    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, EnteringTextControlsWithOption)
{
    static const char* input = "<p>Hello <input type=\"text\" value=\"input\">!</p>";
    static const char* expectedTextChunksRawString[] = {
        "Hello ",
        "\n",
        "input",
        "!",
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(input);
    EXPECT_EQ(expectedTextChunks, iterate(TextIteratorEntersTextControls));
}

TEST_F(TextIteratorTest, EnteringTextControlsWithOptionComplex)
{
    static const char* input = "<input type=\"text\" value=\"Beginning of range\"><div><div><input type=\"text\" value=\"Under DOM nodes\"></div></div><input type=\"text\" value=\"End of range\">";
    static const char* expectedTextChunksRawString[] = {
        "\n", // FIXME: Why newline here?
        "Beginning of range",
        "\n",
        "Under DOM nodes",
        "\n",
        "End of range"
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(input);
    EXPECT_EQ(expectedTextChunks, iterate(TextIteratorEntersTextControls));
}

TEST_F(TextIteratorTest, NotEnteringTextControlHostingShadowTreeEvenWithOption)
{
    static const char* bodyContent = "<div>Hello, <input type=\"text\" value=\"input\" id=\"input\"> iterator.</div>";
    static const char* shadowContent = "<span>shadow</span>";
    // TextIterator doesn't emit "input" nor "shadow" since (1) the renderer for <input> is not created; and
    // (2) we don't (yet) recurse into shadow trees.
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        "", // FIXME: Why is an empty string emitted here?
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "input", shadowContent);

    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, NotEnteringShadowTree)
{
    static const char* bodyContent = "<div>Hello, <span id=\"host\">text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow</span>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ", // TextIterator doesn't emit "text" since its renderer is not created. The shadow tree is ignored.
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);

    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithMultipleShadowTrees)
{
    static const char* bodyContent = "<div>Hello, <span id=\"host\">text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first shadow</span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent2);

    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithNestedShadowTrees)
{
    static const char* bodyContent = "<div>Hello, <span id=\"host-in-document\">text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first <span id=\"host-in-shadow\">shadow</span></span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    RefPtr<ShadowRoot> shadowRoot1 = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host-in-document", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host-in-shadow", shadowContent2);

    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, NotEnteringShadowTreeWithContentInsertionPoint)
{
    static const char* bodyContent = "<div>Hello, <span id=\"host\">text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow <content>content</content></span>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        "text", // In this case a renderer for "text" is created, so it shows up here.
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);

    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id=\"host\">text</span> iterator.</div>";
    static const char* shadowContent = "<span>shadow</span>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        "shadow", // TextIterator emits "shadow" since TextIteratorEntersAuthorShadowRoots is specified.
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);

    EXPECT_EQ(expectedTextChunks, iterate(TextIteratorEntersAuthorShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithMultipleShadowTreesWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id=\"host\">text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first shadow</span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        "second shadow", // The first isn't emitted because a renderer for the first is not created.
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent2);

    EXPECT_EQ(expectedTextChunks, iterate(TextIteratorEntersAuthorShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithNestedShadowTreesWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id=\"host-in-document\">text</span> iterator.</div>";
    static const char* shadowContent1 = "<span>first <span id=\"host-in-shadow\">shadow</span></span>";
    static const char* shadowContent2 = "<span>second shadow</span>";
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        "first ",
        "second shadow",
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    RefPtr<ShadowRoot> shadowRoot1 = createShadowRootForElementWithIDAndSetInnerHTML(document(), "host-in-document", shadowContent1);
    createShadowRootForElementWithIDAndSetInnerHTML(*shadowRoot1, "host-in-shadow", shadowContent2);

    EXPECT_EQ(expectedTextChunks, iterate(TextIteratorEntersAuthorShadowRoots));
}

TEST_F(TextIteratorTest, EnteringShadowTreeWithContentInsertionPointWithOption)
{
    static const char* bodyContent = "<div>Hello, <span id=\"host\">text</span> iterator.</div>";
    static const char* shadowContent = "<span><content>content</content> shadow</span>";
    // In this case a renderer for "text" is created, and emitted AFTER any nodes in the shadow tree.
    // This order does not match the order of the rendered texts, but at this moment it's the expected behavior.
    // FIXME: Fix this. We probably need pure-renderer-based implementation of TextIterator to achieve this.
    static const char* expectedTextChunksRawString[] = {
        "Hello, ",
        " shadow",
        "text",
        " iterator."
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);

    EXPECT_EQ(expectedTextChunks, iterate(TextIteratorEntersAuthorShadowRoots));
}

TEST_F(TextIteratorTest, FullyClipsContents)
{
    static const char* bodyContent =
        "<div style=\"overflow: hidden; width: 200px; height: 0;\">"
        "I'm invisible"
        "</div>";
    Vector<String> expectedTextChunks; // Empty.

    setBodyInnerHTML(bodyContent);
    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, IgnoresContainerClip)
{
    static const char* bodyContent =
        "<div style=\"overflow: hidden; width: 200px; height: 0;\">"
        "<div>I'm not visible</div>"
        "<div style=\"position: absolute; width: 200px; height: 200px; top: 0; right: 0;\">"
        "but I am!"
        "</div>"
        "</div>";
    static const char* expectedTextChunksRawString[] = {
        "but I am!"
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    EXPECT_EQ(expectedTextChunks, iterate());
}

TEST_F(TextIteratorTest, FullyClippedContentsDistributed)
{
    static const char* bodyContent =
        "<div id=\"host\">"
        "<div>Am I visible?</div>"
        "</div>";
    static const char* shadowContent =
        "<div style=\"overflow: hidden; width: 200px; height: 0;\">"
        "<content></content>"
        "</div>";
    static const char* expectedTextChunksRawString[] = {
        "\n",
        // FIXME: The text below is actually invisible but TextIterator currently thinks it's visible.
        "Am I visible?"
    };
    Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);

    EXPECT_EQ(expectedTextChunks, iterate(TextIteratorEntersAuthorShadowRoots));
}

TEST_F(TextIteratorTest, IgnoresContainersClipDistributed)
{
    static const char* bodyContent =
        "<div id=\"host\" style=\"overflow: hidden; width: 200px; height: 0;\">"
        "<div>Nobody can find me!</div>"
        "</div>";
    static const char* shadowContent =
        "<div style=\"position: absolute; width: 200px; height: 200px; top: 0; right: 0;\">"
        "<content></content>"
        "</div>";
    // FIXME: The text below is actually visible but TextIterator currently thinks it's invisible.
    // static const char* expectedTextChunksRawString[] = {
    //     "\n",
    //     "Nobody can find me!"
    // };
    // Vector<String> expectedTextChunks = createVectorString(expectedTextChunksRawString, WTF_ARRAY_LENGTH(expectedTextChunksRawString));
    Vector<String> expectedTextChunks; // Empty.

    setBodyInnerHTML(bodyContent);
    createShadowRootForElementWithIDAndSetInnerHTML(document(), "host", shadowContent);

    EXPECT_EQ(expectedTextChunks, iterate(TextIteratorEntersAuthorShadowRoots));
}

}
