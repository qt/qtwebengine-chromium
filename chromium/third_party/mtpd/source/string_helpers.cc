// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "string_helpers.h"

#include "build_config.h"

// TODO(thestig) Merge these once libchrome catches up to Chromium's base,
// or when mtpd moves into its own repo. http://crbug.com/221123
#if defined(CROS_BUILD)
#include <base/string_util.h>
#else
#include <base/strings/string_util.h>
#endif

namespace mtpd {

std::string EnsureUTF8String(const std::string& str) {
  return IsStringUTF8(str) ? str : "";
}

}  // namespace
