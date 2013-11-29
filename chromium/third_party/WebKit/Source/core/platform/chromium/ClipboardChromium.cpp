/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
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

#include "config.h"
#include "core/platform/chromium/ClipboardChromium.h"

#include "HTMLNames.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/DataTransferItemList.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/StringCallback.h"
#include "core/editing/markup.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileList.h"
#include "core/loader/cache/ImageResource.h"
#include "core/page/Frame.h"
#include "core/platform/DragData.h"
#include "core/platform/MIMETypeRegistry.h"
#include "core/platform/chromium/ChromiumDataObject.h"
#include "core/platform/chromium/ChromiumDataObjectItem.h"
#include "core/platform/chromium/ClipboardMimeTypes.h"
#include "core/platform/chromium/ClipboardUtilitiesChromium.h"
#include "core/platform/graphics/Image.h"
#include "core/rendering/RenderImage.h"

#include "wtf/text/WTFString.h"

namespace WebCore {

namespace {

// A wrapper class that invalidates a DataTransferItemList when the associated Clipboard object goes out of scope.
class DataTransferItemListPolicyWrapper : public DataTransferItemList {
public:
    static PassRefPtr<DataTransferItemListPolicyWrapper> create(PassRefPtr<ClipboardChromium>, PassRefPtr<ChromiumDataObject>);
    virtual ~DataTransferItemListPolicyWrapper();

    virtual size_t length() const;
    virtual PassRefPtr<DataTransferItem> item(unsigned long index) OVERRIDE;
    virtual void deleteItem(unsigned long index, ExceptionState&) OVERRIDE;
    virtual void clear() OVERRIDE;
    virtual void add(const String& data, const String& type, ExceptionState&) OVERRIDE;
    virtual void add(PassRefPtr<File>) OVERRIDE;

private:
    DataTransferItemListPolicyWrapper(PassRefPtr<ClipboardChromium>, PassRefPtr<ChromiumDataObject>);

    RefPtr<ClipboardChromium> m_clipboard;
    RefPtr<ChromiumDataObject> m_dataObject;
};


PassRefPtr<DataTransferItemListPolicyWrapper> DataTransferItemListPolicyWrapper::create(
    PassRefPtr<ClipboardChromium> clipboard, PassRefPtr<ChromiumDataObject> list)
{
    return adoptRef(new DataTransferItemListPolicyWrapper(clipboard, list));
}

DataTransferItemListPolicyWrapper::~DataTransferItemListPolicyWrapper()
{
}

size_t DataTransferItemListPolicyWrapper::length() const
{
    if (!m_clipboard->canReadTypes())
        return 0;
    return m_dataObject->length();
}

PassRefPtr<DataTransferItem> DataTransferItemListPolicyWrapper::item(unsigned long index)
{
    if (!m_clipboard->canReadTypes())
        return 0;
    RefPtr<ChromiumDataObjectItem> item = m_dataObject->item(index);
    if (!item)
        return 0;

    return DataTransferItemPolicyWrapper::create(m_clipboard, item);
}

void DataTransferItemListPolicyWrapper::deleteItem(unsigned long index, ExceptionState& es)
{
    if (!m_clipboard->canWriteData()) {
        es.throwDOMException(InvalidStateError);
        return;
    }
    m_dataObject->deleteItem(index);
}

void DataTransferItemListPolicyWrapper::clear()
{
    if (!m_clipboard->canWriteData())
        return;
    m_dataObject->clearAll();
}

void DataTransferItemListPolicyWrapper::add(const String& data, const String& type, ExceptionState& es)
{
    if (!m_clipboard->canWriteData())
        return;
    m_dataObject->add(data, type, es);
}

void DataTransferItemListPolicyWrapper::add(PassRefPtr<File> file)
{
    if (!m_clipboard->canWriteData())
        return;
    m_dataObject->add(file, m_clipboard->frame()->document()->scriptExecutionContext());
}

DataTransferItemListPolicyWrapper::DataTransferItemListPolicyWrapper(
    PassRefPtr<ClipboardChromium> clipboard, PassRefPtr<ChromiumDataObject> dataObject)
    : m_clipboard(clipboard)
    , m_dataObject(dataObject)
{
}

} // namespace

PassRefPtr<DataTransferItemPolicyWrapper> DataTransferItemPolicyWrapper::create(
    PassRefPtr<ClipboardChromium> clipboard, PassRefPtr<ChromiumDataObjectItem> item)
{
    return adoptRef(new DataTransferItemPolicyWrapper(clipboard, item));
}

DataTransferItemPolicyWrapper::~DataTransferItemPolicyWrapper()
{
}

String DataTransferItemPolicyWrapper::kind() const
{
    if (!m_clipboard->canReadTypes())
        return String();
    return m_item->kind();
}

String DataTransferItemPolicyWrapper::type() const
{
    if (!m_clipboard->canReadTypes())
        return String();
    return m_item->type();
}

void DataTransferItemPolicyWrapper::getAsString(PassRefPtr<StringCallback> callback) const
{
    if (!m_clipboard->canReadData())
        return;

    m_item->getAsString(callback, m_clipboard->frame()->document()->scriptExecutionContext());
}

PassRefPtr<Blob> DataTransferItemPolicyWrapper::getAsFile() const
{
    if (!m_clipboard->canReadData())
        return 0;

    return m_item->getAsFile();
}

DataTransferItemPolicyWrapper::DataTransferItemPolicyWrapper(
    PassRefPtr<ClipboardChromium> clipboard, PassRefPtr<ChromiumDataObjectItem> item)
    : m_clipboard(clipboard)
    , m_item(item)
{
}

using namespace HTMLNames;

// We provide the IE clipboard types (URL and Text), and the clipboard types specified in the WHATWG Web Applications 1.0 draft
// see http://www.whatwg.org/specs/web-apps/current-work/ Section 6.3.5.3

static String normalizeType(const String& type, bool* convertToURL = 0)
{
    String cleanType = type.stripWhiteSpace().lower();
    if (cleanType == mimeTypeText || cleanType.startsWith(mimeTypeTextPlainEtc))
        return mimeTypeTextPlain;
    if (cleanType == mimeTypeURL) {
        if (convertToURL)
          *convertToURL = true;
        return mimeTypeTextURIList;
    }
    return cleanType;
}

PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy policy, DragData* dragData, Frame* frame)
{
    return ClipboardChromium::create(DragAndDrop, dragData->platformData(), policy, frame);
}

ClipboardChromium::ClipboardChromium(ClipboardType clipboardType,
                                     PassRefPtr<ChromiumDataObject> dataObject,
                                     ClipboardAccessPolicy policy,
                                     Frame* frame)
    : Clipboard(policy, clipboardType)
    , m_dataObject(dataObject)
    , m_frame(frame)
{
}

ClipboardChromium::~ClipboardChromium()
{
    if (m_dragImage)
        m_dragImage->removeClient(this);
}

PassRefPtr<ClipboardChromium> ClipboardChromium::create(ClipboardType clipboardType,
    PassRefPtr<ChromiumDataObject> dataObject, ClipboardAccessPolicy policy, Frame* frame)
{
    return adoptRef(new ClipboardChromium(clipboardType, dataObject, policy, frame));
}

void ClipboardChromium::clearData(const String& type)
{
    if (!canWriteData())
        return;

    m_dataObject->clearData(normalizeType(type));

    ASSERT_NOT_REACHED();
}

void ClipboardChromium::clearAllData()
{
    if (!canWriteData())
        return;

    m_dataObject->clearAll();
}

String ClipboardChromium::getData(const String& type) const
{
    if (!canReadData())
        return String();

    bool convertToURL = false;
    String data = m_dataObject->getData(normalizeType(type, &convertToURL));
    if (!convertToURL)
        return data;
    return convertURIListToURL(data);
}

bool ClipboardChromium::setData(const String& type, const String& data)
{
    if (!canWriteData())
        return false;

    return m_dataObject->setData(normalizeType(type), data);
}

// extensions beyond IE's API
ListHashSet<String> ClipboardChromium::types() const
{
    if (!canReadTypes())
        return ListHashSet<String>();

    return m_dataObject->types();
}

PassRefPtr<FileList> ClipboardChromium::files() const
{
    RefPtr<FileList> files = FileList::create();
    if (!canReadData())
        return files.release();

    for (size_t i = 0; i < m_dataObject->length(); ++i) {
        if (m_dataObject->item(i)->kind() == DataTransferItem::kindFile) {
            RefPtr<Blob> blob = m_dataObject->item(i)->getAsFile();
            if (blob && blob->isFile())
                files->append(toFile(blob.get()));
        }
    }

    return files.release();
}

void ClipboardChromium::setDragImage(ImageResource* image, Node* node, const IntPoint& loc)
{
    if (!canSetDragImage())
        return;

    if (m_dragImage)
        m_dragImage->removeClient(this);
    m_dragImage = image;
    if (m_dragImage)
        m_dragImage->addClient(this);

    m_dragLoc = loc;
    m_dragImageElement = node;
}

void ClipboardChromium::setDragImage(ImageResource* img, const IntPoint& loc)
{
    setDragImage(img, 0, loc);
}

void ClipboardChromium::setDragImageElement(Node* node, const IntPoint& loc)
{
    setDragImage(0, node, loc);
}

PassOwnPtr<DragImage> ClipboardChromium::createDragImage(IntPoint& loc) const
{
    if (m_dragImageElement) {
        if (m_frame) {
            loc = m_dragLoc;
            return m_frame->nodeImage(m_dragImageElement.get());
        }
    } else if (m_dragImage) {
        loc = m_dragLoc;
        return DragImage::create(m_dragImage->image());
    }
    return nullptr;
}

static ImageResource* getImageResource(Element* element)
{
    // Attempt to pull ImageResource from element
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;

    RenderImage* image = toRenderImage(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage();

    return 0;
}

static void writeImageToDataObject(ChromiumDataObject* dataObject, Element* element,
                                   const KURL& url)
{
    // Shove image data into a DataObject for use as a file
    ImageResource* cachedImage = getImageResource(element);
    if (!cachedImage || !cachedImage->imageForRenderer(element->renderer()) || !cachedImage->isLoaded())
        return;

    SharedBuffer* imageBuffer = cachedImage->imageForRenderer(element->renderer())->data();
    if (!imageBuffer || !imageBuffer->size())
        return;

    String imageExtension = cachedImage->image()->filenameExtension();
    ASSERT(!imageExtension.isEmpty());

    // Determine the filename for the file contents of the image.
    String filename = cachedImage->response().suggestedFilename();
    if (filename.isEmpty())
        filename = url.lastPathComponent();

    String fileExtension;
    if (filename.isEmpty()) {
        filename = element->getAttribute(altAttr);
    } else {
        // Strip any existing extension. Assume that alt text is usually not a filename.
        int extensionIndex = filename.reverseFind('.');
        if (extensionIndex != -1) {
            fileExtension = filename.substring(extensionIndex + 1);
            filename.truncate(extensionIndex);
        }
    }

    if (!fileExtension.isEmpty() && fileExtension != imageExtension) {
        String imageMimeType = MIMETypeRegistry::getMIMETypeForExtension(imageExtension);
        ASSERT(imageMimeType.startsWith("image/"));
        // Use the file extension only if it has imageMimeType: it's untrustworthy otherwise.
        if (imageMimeType == MIMETypeRegistry::getMIMETypeForExtension(fileExtension))
            imageExtension = fileExtension;
    }

    imageExtension = "." + imageExtension;
    ClipboardChromium::validateFilename(filename, imageExtension);

    dataObject->addSharedBuffer(filename + imageExtension, imageBuffer);
}

void ClipboardChromium::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    if (!m_dataObject)
        return;

    m_dataObject->setURLAndTitle(url, title);

    // Write the bytes in the image to the file format.
    writeImageToDataObject(m_dataObject.get(), element, url);

    // Put img tag on the clipboard referencing the image
    m_dataObject->setData(mimeTypeTextHTML, createMarkup(element, IncludeNode, 0, ResolveAllURLs));
}

void ClipboardChromium::writeURL(const KURL& url, const String& title, Frame*)
{
    if (!m_dataObject)
        return;
    ASSERT(!url.isEmpty());

    m_dataObject->setURLAndTitle(url, title);

    // The URL can also be used as plain text.
    m_dataObject->setData(mimeTypeTextPlain, url.string());

    // The URL can also be used as an HTML fragment.
    m_dataObject->setHTMLAndBaseURL(urlToMarkup(url, title), url);
}

void ClipboardChromium::writeRange(Range* selectedRange, Frame* frame)
{
    ASSERT(selectedRange);
    if (!m_dataObject)
         return;

    m_dataObject->setHTMLAndBaseURL(createMarkup(selectedRange, 0, AnnotateForInterchange, false, ResolveNonLocalURLs), frame->document()->url());

    String str = frame->selectedTextForClipboard();
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(str);
#endif
    replaceNBSPWithSpace(str);
    m_dataObject->setData(mimeTypeTextPlain, str);
}

void ClipboardChromium::writePlainText(const String& text)
{
    if (!m_dataObject)
        return;

    String str = text;
#if OS(WINDOWS)
    replaceNewlinesWithWindowsStyleNewlines(str);
#endif
    replaceNBSPWithSpace(str);

    m_dataObject->setData(mimeTypeTextPlain, str);
}

bool ClipboardChromium::hasData()
{
    ASSERT(isForDragAndDrop());

    return m_dataObject->length() > 0;
}

PassRefPtr<DataTransferItemList> ClipboardChromium::items()
{
    // FIXME: According to the spec, we are supposed to return the same collection of items each
    // time. We now return a wrapper that always wraps the *same* set of items, so JS shouldn't be
    // able to tell, but we probably still want to fix this.
    return DataTransferItemListPolicyWrapper::create(this, m_dataObject);
}

} // namespace WebCore
