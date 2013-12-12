/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Pasteboard_h
#define Pasteboard_h

#include "public/platform/WebClipboard.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/WTFString.h"
#include "wtf/Vector.h"

// FIXME: This class is too high-level to be in the platform directory, since it
// uses the DOM and makes calls to Editor. It should either be divested of its
// knowledge of the frame and editor or moved into the editing directory.

namespace WebCore {

class ArchiveResource;
class ChromiumDataObject;
class DocumentFragment;
class Frame;
class HitTestResult;
class KURL;
class Node;
class Range;
class SharedBuffer;

class Pasteboard {
    WTF_MAKE_NONCOPYABLE(Pasteboard); WTF_MAKE_FAST_ALLOCATED;
public:
    enum SmartReplaceOption {
        CanSmartReplace,
        CannotSmartReplace
    };

    static Pasteboard* generalPasteboard();
    void writeSelection(Range*, bool canSmartCopyOrDelete, const String& text);
    void writePlainText(const String&, SmartReplaceOption);
    void writeURL(const KURL&, const String&);
    void writeImage(Node*, const KURL&, const String& title);
    void writeDataObject(PassRefPtr<ChromiumDataObject>);
    bool canSmartReplace();
    PassRefPtr<DocumentFragment> documentFragment(Frame*, PassRefPtr<Range>, bool allowPlainText, bool& chosePlainText);
    String plainText();

    bool isSelectionMode() const;
    void setSelectionMode(bool);

private:
    Pasteboard();

    WebKit::WebClipboard::Buffer m_buffer;
};

} // namespace WebCore

#endif // Pasteboard_h
