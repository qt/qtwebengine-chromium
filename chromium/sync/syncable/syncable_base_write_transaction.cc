// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/syncable/syncable_base_write_transaction.h"

namespace syncer {
namespace syncable {

BaseWriteTransaction::BaseWriteTransaction(
    const tracked_objects::Location location,
    const char* name,
    WriterTag writer,
    Directory* directory)
    : BaseTransaction(location, name, writer, directory) {
}

BaseWriteTransaction::~BaseWriteTransaction() {}

}  // namespace syncable
}  // namespace syncer

