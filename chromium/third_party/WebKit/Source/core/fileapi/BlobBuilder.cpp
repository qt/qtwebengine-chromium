/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "core/fileapi/BlobBuilder.h"

#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "platform/text/LineEnding.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/PassRefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/TextEncoding.h"

namespace WebCore {

BlobBuilder::BlobBuilder()
    : m_size(0)
{
}

Vector<char>& BlobBuilder::getBuffer()
{
    // If the last item is not a data item, create one. Otherwise, we simply append the new string to the last data item.
    if (m_items.isEmpty() || m_items[m_items.size() - 1].type != BlobDataItem::Data)
        m_items.append(BlobDataItem(RawData::create()));

    return *m_items[m_items.size() - 1].data->mutableData();
}

void BlobBuilder::append(const String& text, const String& endingType)
{
    CString utf8Text = UTF8Encoding().normalizeAndEncode(text, WTF::EntitiesForUnencodables);

    Vector<char>& buffer = getBuffer();
    size_t oldSize = buffer.size();

    if (endingType == "native")
        normalizeLineEndingsToNative(utf8Text, buffer);
    else {
        ASSERT(endingType == "transparent");
        buffer.append(utf8Text.data(), utf8Text.length());
    }
    m_size += buffer.size() - oldSize;
}

void BlobBuilder::append(ArrayBuffer* arrayBuffer)
{
    if (!arrayBuffer)
        return;

    appendBytesData(arrayBuffer->data(), arrayBuffer->byteLength());
}

void BlobBuilder::append(ArrayBufferView* arrayBufferView)
{
    if (!arrayBufferView)
        return;

    appendBytesData(arrayBufferView->baseAddress(), arrayBufferView->byteLength());
}

void BlobBuilder::append(Blob* blob)
{
    if (!blob)
        return;
    if (blob->hasBackingFile()) {
        File* file = toFile(blob);
        // If the blob is file that is not snapshoted, capture the snapshot now.
        // FIXME: This involves synchronous file operation. We need to figure out how to make it asynchronous.
        long long snapshotSize;
        double snapshotModificationTime;
        file->captureSnapshot(snapshotSize, snapshotModificationTime);

        m_size += snapshotSize;
        if (!file->fileSystemURL().isEmpty())
            m_items.append(BlobDataItem(file->fileSystemURL(), 0, snapshotSize, snapshotModificationTime));
        else
            m_items.append(BlobDataItem(file->path(), 0, snapshotSize, snapshotModificationTime));
    } else {
        long long blobSize = static_cast<long long>(blob->size());
        m_size += blobSize;
        m_items.append(BlobDataItem(blob->blobDataHandle(), 0, blobSize));
    }
}

void BlobBuilder::appendBytesData(const void* data, size_t length)
{
    Vector<char>& buffer = getBuffer();
    size_t oldSize = buffer.size();
    buffer.append(static_cast<const char*>(data), length);
    m_size += buffer.size() - oldSize;
}

PassRefPtr<Blob> BlobBuilder::createBlob(const String& contentType)
{
    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->setContentType(contentType);
    blobData->swapItems(m_items);

    RefPtr<Blob> blob = Blob::create(BlobDataHandle::create(blobData.release(), m_size));

    // After creating a blob from the current blob data, we do not need to keep the data around any more.
    // Instead, we only need to keep a reference to the blob data just created.
    m_items.append(BlobDataItem(blob->blobDataHandle(), 0, m_size));

    return blob.release();
}

PassRefPtr<File> BlobBuilder::createFile(const String& contentType, const String& fileName, double modificationTime)
{
    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->setContentType(contentType);
    blobData->swapItems(m_items);

    RefPtr<File> file = File::create(fileName, modificationTime, BlobDataHandle::create(blobData.release(), m_size));

    // After creating a file from the current blob data, we do not need to keep the data around any more.
    // Instead, we only need to keep a reference to the blob data just created.
    m_items.append(BlobDataItem(file->blobDataHandle(), 0, m_size));

    return file.release();
}

} // namespace WebCore
