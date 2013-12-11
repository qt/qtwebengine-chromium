/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef CSSAnimationDataList_h
#define CSSAnimationDataList_h

#include "core/platform/animation/CSSAnimationData.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class CSSAnimationDataList {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CSSAnimationDataList() { }
    explicit CSSAnimationDataList(const CSSAnimationDataList&);

    void fillUnsetProperties();
    bool operator==(const CSSAnimationDataList& o) const;
    bool operator!=(const CSSAnimationDataList& o) const
    {
        return !(*this == o);
    }

    size_t size() const { return m_animations.size(); }
    bool isEmpty() const { return m_animations.isEmpty(); }

    void resize(size_t n) { m_animations.resize(n); }
    void remove(size_t i) { m_animations.remove(i); }
    void append(PassRefPtr<CSSAnimationData> anim) { m_animations.append(anim); }

    CSSAnimationData* animation(size_t i) { return m_animations[i].get(); }
    const CSSAnimationData* animation(size_t i) const { return m_animations[i].get(); }

private:
    CSSAnimationDataList& operator=(const CSSAnimationDataList&);

    Vector<RefPtr<CSSAnimationData> > m_animations;
};


} // namespace WebCore

#endif // CSSAnimationDataList_h
