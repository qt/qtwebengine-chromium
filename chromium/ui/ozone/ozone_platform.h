// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_OZONE_PLATFORM_H_
#define UI_OZONE_OZONE_PLATFORM_H_

#include "base/memory/scoped_ptr.h"
#include "ui/events/ozone/event_factory_ozone.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"
#include "ui/ozone/ozone_export.h"

namespace ui {

// Base class for Ozone platform implementations.
//
// Ozone platforms must override this class and implement the virtual
// GetFooFactoryOzone() methods to provide implementations of the
// various ozone interfaces.
//
// The OzonePlatform subclass can own any state needed by the
// implementation that is shared between the various ozone interfaces,
// such as a connection to the windowing system.
//
// A platform is free to use different implementations of each
// interface depending on the context. You can, for example, create
// different objects depending on the underlying hardware, command
// line flags, or whatever is appropriate for the platform.
class OZONE_EXPORT OzonePlatform {
 public:
  OzonePlatform();
  virtual ~OzonePlatform();

  // Initialize the platform. Once complete, SurfaceFactoryOzone &
  // EventFactoryOzone will be set.
  static void Initialize();

  // Factory getters to override in subclasses. The returned objects will be
  // injected into the appropriate layer at startup. Subclasses should not
  // inject these objects themselves. Ownership is retained by OzonePlatform.
  virtual gfx::SurfaceFactoryOzone* GetSurfaceFactoryOzone() = 0;
  virtual ui::EventFactoryOzone* GetEventFactoryOzone() = 0;

 private:
  static OzonePlatform* instance_;

  DISALLOW_COPY_AND_ASSIGN(OzonePlatform);
};

}  // namespace ui

#endif  // UI_OZONE_OZONE_PLATFORM_H_
