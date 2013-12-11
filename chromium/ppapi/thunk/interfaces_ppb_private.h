// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Please see inteface_ppb_public_stable for the documentation on the format of
// this file.

#include "ppapi/thunk/interfaces_preamble.h"

// See interfaces_ppb_private_no_permissions.h for other private interfaces.

PROXIED_API(PPB_X509Certificate_Private)

PROXIED_IFACE(PPB_X509Certificate_Private,
              PPB_X509CERTIFICATE_PRIVATE_INTERFACE_0_1,
              PPB_X509Certificate_Private_0_1)

#if !defined(OS_NACL)
PROXIED_API(PPB_Broker)

PROXIED_IFACE(PPB_Broker, PPB_BROKER_TRUSTED_INTERFACE_0_2,
              PPB_BrokerTrusted_0_2)
PROXIED_IFACE(PPB_Broker, PPB_BROKER_TRUSTED_INTERFACE_0_3,
              PPB_BrokerTrusted_0_3)
PROXIED_IFACE(PPB_Instance, PPB_BROWSERFONT_TRUSTED_INTERFACE_1_0,
              PPB_BrowserFont_Trusted_1_0)
PROXIED_IFACE(PPB_Instance,
              PPB_CONTENTDECRYPTOR_PRIVATE_INTERFACE_0_7,
              PPB_ContentDecryptor_Private_0_7)
PROXIED_IFACE(PPB_Instance, PPB_CHARSET_TRUSTED_INTERFACE_1_0,
              PPB_CharSet_Trusted_1_0)
PROXIED_IFACE(NoAPIName, PPB_FILECHOOSER_TRUSTED_INTERFACE_0_5,
              PPB_FileChooserTrusted_0_5)
PROXIED_IFACE(NoAPIName, PPB_FILECHOOSER_TRUSTED_INTERFACE_0_6,
              PPB_FileChooserTrusted_0_6)
PROXIED_IFACE(NoAPIName, PPB_FILEREFPRIVATE_INTERFACE_0_1,
              PPB_FileRefPrivate_0_1)
// TODO(xhwang): Move PPB_Flash_DeviceID back to interfaces_ppb_private_flash.h.
PROXIED_IFACE(NoAPIName, PPB_FLASH_DEVICEID_INTERFACE_1_0,
              PPB_Flash_DeviceID_1_0)
PROXIED_IFACE(PPB_Instance, PPB_FLASHFULLSCREEN_INTERFACE_0_1,
              PPB_FlashFullscreen_0_1)
PROXIED_IFACE(PPB_Instance, PPB_FLASHFULLSCREEN_INTERFACE_1_0,
              PPB_FlashFullscreen_0_1)
PROXIED_IFACE(NoAPIName, PPB_PDF_INTERFACE,
              PPB_PDF)
#if defined(OS_CHROMEOS)
PROXIED_IFACE(NoAPIName, PPB_PLATFORMVERIFICATION_PRIVATE_INTERFACE_0_1,
              PPB_PlatformVerification_Private_0_1)
#endif
PROXIED_IFACE(NoAPIName, PPB_TALK_PRIVATE_INTERFACE_1_0,
              PPB_Talk_Private_1_0)
PROXIED_IFACE(NoAPIName, PPB_TALK_PRIVATE_INTERFACE_2_0,
              PPB_Talk_Private_2_0)
// This uses the FileIO API which is declared in the public stable file.
PROXIED_IFACE(NoAPIName, PPB_FILEIOTRUSTED_INTERFACE_0_4, PPB_FileIOTrusted_0_4)

PROXIED_IFACE(NoAPIName, PPB_URLLOADERTRUSTED_INTERFACE_0_3,
              PPB_URLLoaderTrusted_0_3)

// Hack to keep font working. The Font 0.6 API is binary compatible with
// BrowserFont 1.0, so just map the string to the same thing.
// TODO(brettw) remove support for the old Font API.
PROXIED_IFACE(PPB_Instance, PPB_FONT_DEV_INTERFACE_0_6,
              PPB_BrowserFont_Trusted_1_0)
#endif  // !defined(OS_NACL)

#include "ppapi/thunk/interfaces_postamble.h"
