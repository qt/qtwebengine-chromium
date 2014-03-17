// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_FACTORY_OZONE_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_FACTORY_OZONE_H_

#include "ui/views/views_export.h"

namespace gfx {
class Rect;
}

namespace views {
class DesktopNativeWidgetAura;
class DesktopRootWindowHost;

namespace internal {
class NativeWidgetDelegate;
}

class VIEWS_EXPORT DesktopFactoryOzone {
 public:
  DesktopFactoryOzone();
  virtual ~DesktopFactoryOzone();

  // Returns the instance.
  static DesktopFactoryOzone* GetInstance();

  // Sets the implementation delegate. Ownership is retained by the caller.
  static void SetInstance(DesktopFactoryOzone* impl);

  // Delegates implementation of DesktopRootWindowHost::Create externally to
  // Ozone implementation.
  virtual DesktopRootWindowHost* CreateRootWindowHost(
      internal::NativeWidgetDelegate* native_widget_delegate,
      DesktopNativeWidgetAura* desktop_native_widget_aura) = 0;

 private:
  static DesktopFactoryOzone* impl_; // not owned
};

}  // namespace views

#endif // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_FACTORY_OZONE_H_
