// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_
#define CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_

#include <string>

namespace WebKit {
class WebString;
}

namespace content {

// Returns true if canPlayType should return an empty string for |key_system|.
bool IsCanPlayTypeSuppressed(const std::string& key_system);

// Returns the name that UMA will use for the given |key_system|.
// This function can be called frequently. Hence this function should be
// implemented not to impact performance and does not rely on the main
// key system map.
std::string KeySystemNameForUMAInternal(const WebKit::WebString& key_system);

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CRYPTO_KEY_SYSTEMS_INFO_H_
