// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_SIMPLE_WEBMIMEREGISTRY_IMPL_H_
#define WEBKIT_GLUE_SIMPLE_WEBMIMEREGISTRY_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebMimeRegistry.h"
#include "webkit/glue/webkit_glue_export.h"

namespace webkit_glue {

class WEBKIT_GLUE_EXPORT SimpleWebMimeRegistryImpl :
    NON_EXPORTED_BASE(public WebKit::WebMimeRegistry) {
 public:
  SimpleWebMimeRegistryImpl() {}
  virtual ~SimpleWebMimeRegistryImpl() {}

  // Convert a WebString to ASCII, falling back on an empty string in the case
  // of a non-ASCII string.
  static std::string ToASCIIOrEmpty(const WebKit::WebString& string);

  // WebMimeRegistry methods:
  virtual WebKit::WebMimeRegistry::SupportsType supportsMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsImageMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsJavaScriptMIMEType(
      const WebKit::WebString&);
  // TODO(ddorwin): Remove after http://webk.it/82983 lands.
  virtual WebKit::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const WebKit::WebString&, const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsMediaMIMEType(
      const WebKit::WebString&,
      const WebKit::WebString&,
      const WebKit::WebString&);
  virtual bool supportsMediaSourceMIMEType(const WebKit::WebString&,
                                           const WebKit::WebString&);
  virtual WebKit::WebMimeRegistry::SupportsType supportsNonImageMIMEType(
      const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeForExtension(const WebKit::WebString&);
  virtual WebKit::WebString wellKnownMimeTypeForExtension(
      const WebKit::WebString&);
  virtual WebKit::WebString mimeTypeFromFile(const WebKit::WebString&);
};

}  // namespace webkit_glue

#endif  // WEBKIT_GLUE_SIMPLE_WEBMIMEREGISTRY_IMPL_H_
