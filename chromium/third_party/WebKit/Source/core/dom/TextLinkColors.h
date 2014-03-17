/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef TextLinkColors_h
#define TextLinkColors_h

#include "platform/graphics/Color.h"
#include "wtf/Noncopyable.h"

namespace WebCore {

class CSSPrimitiveValue;
class Element;

class TextLinkColors {
WTF_MAKE_NONCOPYABLE(TextLinkColors);
public:
    TextLinkColors();

    void setTextColor(const Color& color) { m_textColor = color; }
    Color textColor() const { return m_textColor; }

    const Color& linkColor() const { return m_linkColor; }
    const Color& visitedLinkColor() const { return m_visitedLinkColor; }
    const Color& activeLinkColor() const { return m_activeLinkColor; }
    void setLinkColor(const Color& color) { m_linkColor = color; }
    void setVisitedLinkColor(const Color& color) { m_visitedLinkColor = color; }
    void setActiveLinkColor(const Color& color) { m_activeLinkColor = color; }
    void resetLinkColor();
    void resetVisitedLinkColor();
    void resetActiveLinkColor();
    Color colorFromPrimitiveValue(const CSSPrimitiveValue*, Color currentColor, bool forVisitedLink = false) const;
private:

    Color m_textColor;
    Color m_linkColor;
    Color m_visitedLinkColor;
    Color m_activeLinkColor;
};

}

#endif
