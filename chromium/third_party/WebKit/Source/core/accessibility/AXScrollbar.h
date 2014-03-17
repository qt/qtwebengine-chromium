/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXScrollbar_h
#define AXScrollbar_h

#include "core/accessibility/AXMockObject.h"

namespace WebCore {

class Scrollbar;

class AXScrollbar : public AXMockObject {
public:
    static PassRefPtr<AXScrollbar> create(Scrollbar*);

    Scrollbar* scrollbar() const { return m_scrollbar.get(); }

private:
    explicit AXScrollbar(Scrollbar*);

    virtual void detachFromParent();

    virtual bool canSetValueAttribute() const OVERRIDE { return true; }

    virtual bool isAXScrollbar() const OVERRIDE { return true; }
    virtual LayoutRect elementRect() const OVERRIDE;

    virtual AccessibilityRole roleValue() const OVERRIDE { return ScrollBarRole; }
    virtual AccessibilityOrientation orientation() const OVERRIDE;
    virtual Document* document() const OVERRIDE;

    virtual bool isEnabled() const OVERRIDE;

    // Assumes float [0..1]
    virtual void setValue(float) OVERRIDE;
    virtual float valueForRange() const OVERRIDE;

    RefPtr<Scrollbar> m_scrollbar;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXScrollbar, isAXScrollbar());

} // namespace WebCore

#endif // AXScrollbar_h
