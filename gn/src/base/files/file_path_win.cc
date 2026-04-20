// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"

#include <shlwapi.h>

#include "base/win/win_util.h"

namespace base {

bool FilePath::IsMatchingPattern(const FilePath::StringType& pattern) const {
  return PathMatchSpec(ToWCharT(&value()), ToWCharT(&pattern)) == TRUE;
}

}  // namespace base
