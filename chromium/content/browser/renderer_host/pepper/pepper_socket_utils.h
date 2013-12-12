// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
#define CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_

#include "content/public/common/socket_permission_request.h"
#include "ppapi/c/pp_stdint.h"

struct PP_NetAddress_Private;

namespace net {
class X509Certificate;
}

namespace ppapi {
class PPB_X509Certificate_Fields;
}

namespace content {

class RenderViewHost;

namespace pepper_socket_utils {

SocketPermissionRequest CreateSocketPermissionRequest(
    SocketPermissionRequest::OperationType type,
    const PP_NetAddress_Private& net_addr);

// Returns true if the socket operation specified by |params| is allowed.
// If |params| is NULL, this method checks the basic "socket" permission, which
// is for those operations that don't require a specific socket permission rule.
bool CanUseSocketAPIs(bool external_plugin,
                      bool private_api,
                      const SocketPermissionRequest* params,
                      int render_process_id,
                      int render_view_id);

// TODO (ygorshenin@): remove this method.
bool CanUseSocketAPIs(bool external_plugin,
                      bool private_api,
                      const SocketPermissionRequest* params,
                      RenderViewHost* render_view_host);

// Extracts the certificate field data from a net::X509Certificate into
// PPB_X509Certificate_Fields.
bool GetCertificateFields(const net::X509Certificate& cert,
                          ppapi::PPB_X509Certificate_Fields* fields);

// Extracts the certificate field data from the DER representation of a
// certificate into PPB_X509Certificate_Fields.
bool GetCertificateFields(const char* der,
                          uint32_t length,
                          ppapi::PPB_X509Certificate_Fields* fields);

}  // namespace pepper_socket_utils

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PEPPER_PEPPER_SOCKET_UTILS_H_
