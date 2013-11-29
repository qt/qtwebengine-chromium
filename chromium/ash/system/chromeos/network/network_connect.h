// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_CONNECT_H
#define ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_CONNECT_H

#include <string>

#include "ash/ash_export.h"
#include "base/strings/string16.h"
#include "ui/gfx/native_widget_types.h"  // gfx::NativeWindow

namespace base {
class DictionaryValue;
}

namespace ash {
namespace network_connect {

ASH_EXPORT extern const char kErrorActivateFailed[];

// Requests a network connection and handles any errors and notifications.
// |owning_window| is used to parent any UI on failure (e.g. for certificate
// enrollment). If NULL, the default window will be used.
ASH_EXPORT void ConnectToNetwork(const std::string& service_path,
                                 gfx::NativeWindow owning_window);

// Requests network activation and handles any errors and notifications.
ASH_EXPORT void ActivateCellular(const std::string& service_path);

// Configures a network with a dictionary of Shill properties, then sends a
// connect request. The profile is set according to 'shared' if allowed.
ASH_EXPORT void ConfigureNetworkAndConnect(
    const std::string& service_path,
    const base::DictionaryValue& properties,
    bool shared);

// Requests a new network configuration to be created from a dictionary of
// Shill properties. The profile used is determined by |shared|.
ASH_EXPORT void CreateConfigurationAndConnect(base::DictionaryValue* properties,
                                              bool shared);

// Returns the localized string for shill error string |error|.
ASH_EXPORT base::string16 ErrorString(const std::string& error);

}  // network_connect
}  // ash

#endif  // ASH_SYSTEM_CHROMEOS_NETWORK_NETWORK_CONNECT_H
