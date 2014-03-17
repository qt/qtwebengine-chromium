// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace blink {
class WebString;
}

namespace content {

// Returns whether |key_system| is a real supported key system that can be
// instantiated.
// Abstract parent |key_system| strings will return false.
// Call IsSupportedKeySystemWithMediaMimeType() to determine whether a
// |key_system| supports a specific type of media or to check parent key
// systems.
CONTENT_EXPORT bool IsConcreteSupportedKeySystem(
    const blink::WebString& key_system);

// Returns whether |key_sytem| supports the specified media type and codec(s).
CONTENT_EXPORT bool IsSupportedKeySystemWithMediaMimeType(
    const std::string& mime_type,
    const std::vector<std::string>& codecs,
    const std::string& key_system);

// Returns a name for |key_system| suitable to UMA logging.
CONTENT_EXPORT std::string KeySystemNameForUMA(
    const blink::WebString& key_system);
CONTENT_EXPORT std::string KeySystemNameForUMA(const std::string& key_system);

// Returns whether AesDecryptor can be used for the given |concrete_key_system|.
CONTENT_EXPORT bool CanUseAesDecryptor(const std::string& concrete_key_system);

#if defined(ENABLE_PEPPER_CDMS)
// Returns the Pepper MIME type for |concrete_key_system|.
// Returns empty string if |concrete_key_system| is unknown or not Pepper-based.
CONTENT_EXPORT std::string GetPepperType(
    const std::string& concrete_key_system);
#elif defined(OS_ANDROID)
// Convert |concrete_key_system| to 16-byte Android UUID.
CONTENT_EXPORT std::vector<uint8> GetUUID(
    const std::string& concrete_key_system);
#endif

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_H_
