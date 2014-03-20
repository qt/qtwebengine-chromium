// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_TOOLTIP_MANAGER_AURA_H_
#define UI_VIEWS_WIDGET_TOOLTIP_MANAGER_AURA_H_

#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/gfx/point.h"
#include "ui/views/widget/tooltip_manager.h"

namespace aura {
class Window;
}

namespace gfx {
class FontList;
}

namespace views {

class Widget;

// TooltipManager implementation for Aura.
class TooltipManagerAura : public TooltipManager {
 public:
  explicit TooltipManagerAura(Widget* widget);
  virtual ~TooltipManagerAura();

  // If |source| has capture this finds the Widget under the mouse and invokes
  // UpdateTooltip() on it's TooltipManager. This is necessary as when capture
  // is held mouse events are only delivered to the Window that has capture even
  // though we may show tooltips for the Window under the mouse.
  static void UpdateTooltipManagerForCapture(Widget* source);

  // Returns the FontList used by all TooltipManagerAuras.
  static const gfx::FontList& GetDefaultFontList();

  // TooltipManager:
  virtual const gfx::FontList& GetFontList() const OVERRIDE;
  virtual void UpdateTooltip() OVERRIDE;
  virtual void TooltipTextChanged(View* view) OVERRIDE;

 private:
  View* GetViewUnderPoint(const gfx::Point& point);
  void UpdateTooltipForTarget(View* target,
                              const gfx::Point& point,
                              aura::Window* root_window);

  // Returns the Window the tooltip text is installed on.
  aura::Window* GetWindow();

  Widget* widget_;
  string16 tooltip_text_;

  DISALLOW_COPY_AND_ASSIGN(TooltipManagerAura);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_TOOLTIP_MANAGER_AURA_H_
