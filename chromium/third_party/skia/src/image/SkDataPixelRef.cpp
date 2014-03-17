/*
 * Copyright 2012 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkDataPixelRef.h"
#include "SkData.h"
#include "SkFlattenableBuffers.h"

SkDataPixelRef::SkDataPixelRef(const SkImageInfo& info, SkData* data)
    : INHERITED(info)
    , fData(data)
{
    fData->ref();
    this->setPreLocked(const_cast<void*>(fData->data()), NULL);
}

SkDataPixelRef::~SkDataPixelRef() {
    fData->unref();
}

void* SkDataPixelRef::onLockPixels(SkColorTable** ct) {
    *ct = NULL;
    return const_cast<void*>(fData->data());
}

void SkDataPixelRef::onUnlockPixels() {
    // nothing to do
}

size_t SkDataPixelRef::getAllocatedSizeInBytes() const {
    return fData ? fData->size() : 0;
}

void SkDataPixelRef::flatten(SkFlattenableWriteBuffer& buffer) const {
    this->INHERITED::flatten(buffer);
    buffer.writeDataAsByteArray(fData);
}

SkDataPixelRef::SkDataPixelRef(SkFlattenableReadBuffer& buffer)
        : INHERITED(buffer, NULL) {
    fData = buffer.readByteArrayAsData();
    this->setPreLocked(const_cast<void*>(fData->data()), NULL);
}
