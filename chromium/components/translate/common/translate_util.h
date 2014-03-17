// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_COMMON_TRANSLATE_UTIL_H_
#define COMPONENTS_TRANSLATE_COMMON_TRANSLATE_UTIL_H_

#include <string>

class GURL;

namespace translate {

// Isolated world sets following security-origin by default.
extern const char kSecurityOrigin[];

// Converts language code synonym to use at Translate server.
void ToTranslateLanguageSynonym(std::string* language);

// Converts language code synonym to use at Chrome internal.
void ToChromeLanguageSynonym(std::string* language);

// Get Security origin with which Translate runs.
GURL GetTranslateSecurityOrigin();

}  // namespace translate

#endif  // COMPONENTS_TRANSLATE_COMMON_TRANSLATE_UTIL_H_
