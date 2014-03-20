/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "core/html/forms/PasswordInputType.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "InputTypeNames.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/forms/FormController.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "core/frame/Settings.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {

PassRefPtr<InputType> PasswordInputType::create(HTMLInputElement& element)
{
    return adoptRef(new PasswordInputType(element));
}

void PasswordInputType::countUsage()
{
    countUsageIfVisible(UseCounter::InputTypePassword);
    if (element().fastHasAttribute(HTMLNames::maxlengthAttr))
        countUsageIfVisible(UseCounter::InputTypePasswordMaxLength);
}

bool PasswordInputType::isPasswordGenerationEnabled() const
{
    if (isPasswordGenerationDecorationEnabled())
        return true;
    if (Page* page = element().document().page())
        return page->chrome().client().isPasswordGenerationEnabled();
    return false;
}

bool PasswordInputType::isPasswordGenerationDecorationEnabled() const
{
    if (Page* page = element().document().page())
        return page->settings().passwordGenerationDecorationEnabled();
    return false;
}

bool PasswordInputType::needsContainer() const
{
    return BaseTextInputType::needsContainer() || isPasswordGenerationEnabled();
}

void PasswordInputType::createShadowSubtree()
{
    BaseTextInputType::createShadowSubtree();
    if (!isPasswordGenerationEnabled())
        return;
    RefPtr<PasswordGeneratorButtonElement> generatorButton = PasswordGeneratorButtonElement::create(element().document());
    if (!isPasswordGenerationDecorationEnabled())
        generatorButton->setInlineStyleProperty(CSSPropertyDisplay, CSSValueNone);
    containerElement()->appendChild(generatorButton.release());
}

const AtomicString& PasswordInputType::formControlType() const
{
    return InputTypeNames::password;
}

bool PasswordInputType::shouldSaveAndRestoreFormControlState() const
{
    return false;
}

FormControlState PasswordInputType::saveFormControlState() const
{
    // Should never save/restore password fields.
    ASSERT_NOT_REACHED();
    return FormControlState();
}

void PasswordInputType::restoreFormControlState(const FormControlState&)
{
    // Should never save/restore password fields.
    ASSERT_NOT_REACHED();
}

bool PasswordInputType::shouldUseInputMethod() const
{
    // Input methods are disabled for the password field because otherwise
    // anyone can access the underlying password and display it in clear text.
    return false;
}

bool PasswordInputType::shouldResetOnDocumentActivation()
{
    return true;
}

bool PasswordInputType::shouldRespectListAttribute()
{
    return false;
}

bool PasswordInputType::shouldRespectSpeechAttribute()
{
    return true;
}

bool PasswordInputType::isPasswordField() const
{
    return true;
}

void PasswordInputType::enableSecureTextInput()
{
    if (element().document().frame())
        element().document().setUseSecureKeyboardEntryWhenActive(true);
}

void PasswordInputType::disableSecureTextInput()
{
    if (element().document().frame())
        element().document().setUseSecureKeyboardEntryWhenActive(false);
}

} // namespace WebCore
