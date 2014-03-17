/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef HTMLTextAreaElement_h
#define HTMLTextAreaElement_h

#include "core/html/HTMLTextFormControlElement.h"

namespace WebCore {

class BeforeTextInsertedEvent;
class ExceptionState;
class VisibleSelection;

class HTMLTextAreaElement FINAL : public HTMLTextFormControlElement {
public:
    static PassRefPtr<HTMLTextAreaElement> create(Document&, HTMLFormElement*);

    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    bool shouldWrapText() const { return m_wrap != NoWrap; }

    virtual String value() const;
    void setValue(const String&);
    String defaultValue() const;
    void setDefaultValue(const String&);
    int textLength() const { return value().length(); }
    virtual int maxLength() const;
    void setMaxLength(int, ExceptionState&);
    // For ValidityState
    virtual String validationMessage() const OVERRIDE;
    virtual bool valueMissing() const OVERRIDE;
    virtual bool tooLong() const OVERRIDE;
    bool isValidValue(const String&) const;

    void rendererWillBeDestroyed();

    void setCols(int);
    void setRows(int);

private:
    HTMLTextAreaElement(Document&, HTMLFormElement*);

    enum WrapMethod { NoWrap, SoftWrap, HardWrap };

    virtual void didAddUserAgentShadowRoot(ShadowRoot&) OVERRIDE;
    // FIXME: Author shadows should be allowed
    // https://bugs.webkit.org/show_bug.cgi?id=92608
    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }

    void handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*) const;
    static String sanitizeUserInputValue(const String&, unsigned maxLength);
    void updateValue() const;
    void setNonDirtyValue(const String&);
    void setValueCommon(const String&);

    virtual bool supportsPlaceholder() const { return true; }
    virtual void updatePlaceholderText();
    virtual bool isEmptyValue() const { return value().isEmpty(); }

    virtual bool isOptionalFormControl() const { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const { return isRequired(); }

    virtual void defaultEventHandler(Event*);
    virtual void handleFocusEvent(Element* oldFocusedNode, FocusDirection) OVERRIDE;

    virtual void subtreeHasChanged();

    virtual bool isEnumeratable() const { return true; }
    virtual bool isInteractiveContent() const OVERRIDE;
    virtual bool supportLabels() const OVERRIDE { return true; }

    virtual const AtomicString& formControlType() const;

    virtual FormControlState saveFormControlState() const OVERRIDE;
    virtual void restoreFormControlState(const FormControlState&) OVERRIDE;

    virtual bool isTextFormControl() const { return true; }

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    virtual RenderObject* createRenderer(RenderStyle*);
    virtual bool appendFormData(FormDataList&, bool);
    virtual void resetImpl() OVERRIDE;
    virtual bool hasCustomFocusLogic() const OVERRIDE;
    virtual bool shouldShowFocusRingOnMouseFocus() const OVERRIDE;
    virtual bool isKeyboardFocusable() const OVERRIDE;
    virtual void updateFocusAppearance(bool restorePreviousSelection);

    virtual void accessKeyAction(bool sendMouseEvents);

    virtual bool shouldUseInputMethod();
    virtual bool matchesReadOnlyPseudoClass() const OVERRIDE;
    virtual bool matchesReadWritePseudoClass() const OVERRIDE;

    bool valueMissing(const String& value) const { return isRequiredFormControl() && !isDisabledOrReadOnly() && value.isEmpty(); }
    bool tooLong(const String&, NeedsToCheckDirtyFlag) const;

    int m_rows;
    int m_cols;
    WrapMethod m_wrap;
    mutable String m_value;
    mutable bool m_isDirty;
};

inline bool isHTMLTextAreaElement(const Node* node)
{
    return node->hasTagName(HTMLNames::textareaTag);
}

inline bool isHTMLTextAreaElement(const Element* element)
{
    return element->hasTagName(HTMLNames::textareaTag);
}

DEFINE_NODE_TYPE_CASTS(HTMLTextAreaElement, hasTagName(HTMLNames::textareaTag));

} //namespace

#endif
