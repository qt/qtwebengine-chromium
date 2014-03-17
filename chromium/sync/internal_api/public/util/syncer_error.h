// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_PUBLIC_UTIL_SYNCER_ERROR_H_
#define SYNC_INTERNAL_API_PUBLIC_UTIL_SYNCER_ERROR_H_

#include "sync/base/sync_export.h"

namespace syncer {

// This enum describes all the possible results of a sync cycle.
enum SYNC_EXPORT_PRIVATE SyncerError {
  UNSET = 0,       // Default value.
  CANNOT_DO_WORK,  // A model worker could not process a work item.

  NETWORK_CONNECTION_UNAVAILABLE,  // Connectivity failure.
  NETWORK_IO_ERROR,                // Response buffer read error.
  SYNC_SERVER_ERROR,               // Non auth HTTP error.
  SYNC_AUTH_ERROR,                 // HTTP auth error.

  // Based on values returned by server.  Most are defined in sync.proto.
  SERVER_RETURN_INVALID_CREDENTIAL,
  SERVER_RETURN_UNKNOWN_ERROR,
  SERVER_RETURN_THROTTLED,
  SERVER_RETURN_TRANSIENT_ERROR,
  SERVER_RETURN_MIGRATION_DONE,
  SERVER_RETURN_CLEAR_PENDING,
  SERVER_RETURN_NOT_MY_BIRTHDAY,
  SERVER_RETURN_CONFLICT,
  SERVER_RESPONSE_VALIDATION_FAILED,
  SERVER_RETURN_DISABLED_BY_ADMIN,

  SERVER_MORE_TO_DOWNLOAD,

  SYNCER_OK
};

SYNC_EXPORT const char* GetSyncerErrorString(SyncerError);

// Helper to check that |error| is set to something (not UNSET) and is not
// SYNCER_OK.
bool SyncerErrorIsError(SyncerError error);

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_UTIL_SYNCER_ERROR_H_
