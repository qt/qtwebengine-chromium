/*
 * Copyright (C) 2010 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef AXMenuListOption_h
#define AXMenuListOption_h

#include "core/accessibility/AXMockObject.h"

namespace WebCore {

class AXMenuListPopup;
class HTMLElement;

class AXMenuListOption : public AXMockObject {
public:
    static PassRefPtr<AXMenuListOption> create() { return adoptRef(new AXMenuListOption); }

    void setElement(HTMLElement*);

private:
    AXMenuListOption();

    virtual bool isMenuListOption() const OVERRIDE { return true; }

    virtual AccessibilityRole roleValue() const OVERRIDE { return MenuListOptionRole; }
    virtual bool canHaveChildren() const OVERRIDE { return false; }

    virtual Element* actionElement() const OVERRIDE;
    virtual bool isEnabled() const OVERRIDE;
    virtual bool isVisible() const OVERRIDE;
    virtual bool isOffScreen() const OVERRIDE;
    virtual bool isSelected() const OVERRIDE;
    virtual void setSelected(bool) OVERRIDE;
    virtual bool canSetSelectedAttribute() const OVERRIDE;
    virtual LayoutRect elementRect() const OVERRIDE;
    virtual String stringValue() const OVERRIDE;
    virtual bool computeAccessibilityIsIgnored() const OVERRIDE;

    RefPtr<HTMLElement> m_element;
};

DEFINE_AX_OBJECT_TYPE_CASTS(AXMenuListOption, isMenuListOption());

} // namespace WebCore

#endif // AXMenuListOption_h
