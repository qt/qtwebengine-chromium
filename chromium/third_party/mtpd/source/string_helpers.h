// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STRING_HELPERS_H_
#define STRING_HELPERS_H_

#include <string>

namespace mtpd {

// Returns |str| if |str| is a valid UTF8 string (determined by
// base::IsStringUTF8) or an empty string otherwise.
std::string EnsureUTF8String(const std::string& str);

}  // namespace

#endif  // STRING_HELPERS_H_
