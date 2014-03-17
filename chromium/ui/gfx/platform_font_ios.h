// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_PLATFORM_FONT_IOS_H_
#define UI_GFX_PLATFORM_FONT_IOS_H_

#include "ui/gfx/platform_font.h"

namespace gfx {

class PlatformFontIOS : public PlatformFont {
 public:
  PlatformFontIOS();
  explicit PlatformFontIOS(NativeFont native_font);
  PlatformFontIOS(const std::string& font_name,
                  int font_size);

  // Overridden from PlatformFont:
  virtual Font DeriveFont(int size_delta, int style) const OVERRIDE;
  virtual int GetHeight() const OVERRIDE;
  virtual int GetBaseline() const OVERRIDE;
  virtual int GetCapHeight() const OVERRIDE;
  virtual int GetAverageCharacterWidth() const OVERRIDE;
  virtual int GetStringWidth(const base::string16& text) const OVERRIDE;
  virtual int GetExpectedTextWidth(int length) const OVERRIDE;
  virtual int GetStyle() const OVERRIDE;
  virtual std::string GetFontName() const OVERRIDE;
  virtual std::string GetActualFontNameForTesting() const OVERRIDE;
  virtual int GetFontSize() const OVERRIDE;
  virtual NativeFont GetNativeFont() const OVERRIDE;

 private:
  PlatformFontIOS(const std::string& font_name, int font_size, int style);
  virtual ~PlatformFontIOS() {}

  // Initialize the object with the specified parameters.
  void InitWithNameSizeAndStyle(const std::string& font_name,
                                int font_size,
                                int style);

  // Calculate and cache the font metrics.
  void CalculateMetrics();

  std::string font_name_;
  int font_size_;
  int style_;

  // Cached metrics, generated at construction.
  int height_;
  int ascent_;
  int cap_height_;
  int average_width_;

  DISALLOW_COPY_AND_ASSIGN(PlatformFontIOS);
};

}  // namespace gfx

#endif  // UI_GFX_PLATFORM_FONT_IOS_H_
