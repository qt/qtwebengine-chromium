/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef Clipboard_h
#define Clipboard_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ClipboardAccessPolicy.h"
#include "core/dom/Node.h"
#include "core/loader/cache/ResourcePtr.h"
#include "core/page/DragActions.h"
#include "core/platform/DragImage.h"
#include "core/platform/graphics/IntPoint.h"
#include "wtf/Forward.h"

namespace WebCore {

class ImageResource;
class DataTransferItemList;
class DragData;
class DragImage;
class FileList;
class Frame;
class Range;

// State available during IE's events for drag and drop and copy/paste
class Clipboard : public RefCounted<Clipboard>, public ScriptWrappable {
public:
    // Whether this clipboard is serving a drag-drop or copy-paste request.
    enum ClipboardType {
        CopyAndPaste,
        DragAndDrop,
    };

    static PassRefPtr<Clipboard> create(ClipboardAccessPolicy, DragData*, Frame*);

    virtual ~Clipboard() { }

    bool isForCopyAndPaste() const { return m_clipboardType == CopyAndPaste; }
    bool isForDragAndDrop() const { return m_clipboardType == DragAndDrop; }

    String dropEffect() const { return dropEffectIsUninitialized() ? "none" : m_dropEffect; }
    void setDropEffect(const String&);
    bool dropEffectIsUninitialized() const { return m_dropEffect == "uninitialized"; }
    String effectAllowed() const { return m_effectAllowed; }
    void setEffectAllowed(const String&);

    virtual void clearData(const String& type) = 0;
    virtual void clearAllData() = 0;
    virtual String getData(const String& type) const = 0;
    virtual bool setData(const String& type, const String& data) = 0;

    // extensions beyond IE's API
    virtual ListHashSet<String> types() const = 0;
    virtual PassRefPtr<FileList> files() const = 0;

    IntPoint dragLocation() const { return m_dragLoc; }
    ImageResource* dragImage() const { return m_dragImage.get(); }
    virtual void setDragImage(ImageResource*, const IntPoint&) = 0;
    Node* dragImageElement() const { return m_dragImageElement.get(); }
    virtual void setDragImageElement(Node*, const IntPoint&) = 0;

    virtual PassOwnPtr<DragImage> createDragImage(IntPoint& dragLocation) const = 0;
    virtual void declareAndWriteDragImage(Element*, const KURL&, const String& title, Frame*) = 0;
    virtual void writeURL(const KURL&, const String&, Frame*) = 0;
    virtual void writeRange(Range*, Frame*) = 0;
    virtual void writePlainText(const String&) = 0;

    virtual bool hasData() = 0;

    void setAccessPolicy(ClipboardAccessPolicy);
    bool canReadTypes() const;
    bool canReadData() const;
    bool canWriteData() const;
    // Note that the spec doesn't actually allow drag image modification outside the dragstart
    // event. This capability is maintained for backwards compatiblity for ports that have
    // supported this in the past. On many ports, attempting to set a drag image outside the
    // dragstart operation is a no-op anyway.
    bool canSetDragImage() const;

    DragOperation sourceOperation() const;
    DragOperation destinationOperation() const;
    void setSourceOperation(DragOperation);
    void setDestinationOperation(DragOperation);

    bool hasDropZoneType(const String&);

    void setDragHasStarted() { m_dragStarted = true; }

    virtual PassRefPtr<DataTransferItemList> items() = 0;

protected:
    Clipboard(ClipboardAccessPolicy, ClipboardType);

    bool dragStarted() const { return m_dragStarted; }

private:
    bool hasFileOfType(const String&) const;
    bool hasStringOfType(const String&) const;

    // Instead of using this member directly, prefer to use the can*() methods above.
    ClipboardAccessPolicy m_policy;
    String m_dropEffect;
    String m_effectAllowed;
    bool m_dragStarted;
    ClipboardType m_clipboardType;

protected:
    IntPoint m_dragLoc;
    ResourcePtr<ImageResource> m_dragImage;
    RefPtr<Node> m_dragImageElement;
};

DragOperation convertDropZoneOperationToDragOperation(const String& dragOperation);
String convertDragOperationToDropZoneOperation(DragOperation);

} // namespace WebCore

#endif // Clipboard_h
