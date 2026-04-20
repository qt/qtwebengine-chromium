// Copyright 2026 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"

#include <fnmatch.h>

namespace base {

bool FilePath::IsMatchingPattern(const FilePath::StringType& pattern) const {
  return fnmatch(pattern.c_str(), value().c_str(), FNM_NOESCAPE) == 0;
}

}  // namespace base
