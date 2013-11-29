// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file declares a CoreLocation provider that runs on Mac OS X (10.6).
// Public for testing only - for normal usage this  header should not be
// required, as location_provider.h declares the needed factory function.

#ifndef CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_PROVIDER_MAC_H_
#define CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_PROVIDER_MAC_H_

#include "content/browser/geolocation/location_provider_base.h"
#include "content/public/common/geoposition.h"

namespace content {
class CoreLocationDataProviderMac;

class CoreLocationProviderMac : public LocationProviderBase {
 public:
  explicit CoreLocationProviderMac();
  virtual ~CoreLocationProviderMac();

  // LocationProvider
  virtual bool StartProvider(bool high_accuracy) OVERRIDE;
  virtual void StopProvider() OVERRIDE;
  virtual void GetPosition(Geoposition* position) OVERRIDE;
  virtual void OnPermissionGranted() OVERRIDE;

  // Receives new positions and calls UpdateListeners
  void SetPosition(Geoposition* position);

 private:
  bool is_updating_;
  CoreLocationDataProviderMac* data_provider_;
  Geoposition position_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_GEOLOCATION_CORE_LOCATION_PROVIDER_MAC_H_
