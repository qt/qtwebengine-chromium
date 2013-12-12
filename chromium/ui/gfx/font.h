// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_FONT_H_
#define UI_GFX_FONT_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {

class PlatformFont;

// Font provides a wrapper around an underlying font. Copy and assignment
// operators are explicitly allowed, and cheap.
class UI_EXPORT Font {
 public:
  // The following constants indicate the font style.
  enum FontStyle {
    NORMAL = 0,
    BOLD = 1,
    ITALIC = 2,
    UNDERLINE = 4,
  };

  // Creates a font with the default name and style.
  Font();

  // Creates a font that is a clone of another font object.
  Font(const Font& other);
  gfx::Font& operator=(const Font& other);

  // Creates a font from the specified native font.
  explicit Font(NativeFont native_font);

  // Constructs a Font object with the specified PlatformFont object. The Font
  // object takes ownership of the PlatformFont object.
  explicit Font(PlatformFont* platform_font);

  // Creates a font with the specified name in UTF-8 and size in pixels.
  Font(const std::string& font_name, int font_size);

  ~Font();

  // Returns a new Font derived from the existing font.
  // |size_deta| is the size in pixels to add to the current font. For example,
  // a value of 5 results in a font 5 pixels bigger than this font.
  Font DeriveFont(int size_delta) const;

  // Returns a new Font derived from the existing font.
  // |size_delta| is the size in pixels to add to the current font. See the
  // single argument version of this method for an example.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: BOLD, ITALIC and UNDERLINE.
  Font DeriveFont(int size_delta, int style) const;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.  This may include some leading, i.e. height may be
  // greater than just ascent + descent.  Specifically, the Windows and Mac
  // implementations include leading and the Linux one does not.  This may
  // need to be revisited in the future.
  int GetHeight() const;

  // Returns the baseline, or ascent, of the font.
  int GetBaseline() const;

  // Returns the average character width for the font.
  int GetAverageCharacterWidth() const;

  // Returns the number of horizontal pixels needed to display the specified
  // string.
  int GetStringWidth(const base::string16& text) const;

  // Returns the expected number of horizontal pixels needed to display the
  // specified length of characters. Call GetStringWidth() to retrieve the
  // actual number.
  int GetExpectedTextWidth(int length) const;

  // Returns the style of the font.
  int GetStyle() const;

  // Returns the font name in UTF-8.
  std::string GetFontName() const;

  // Returns the font size in pixels.
  int GetFontSize() const;

  // Returns the native font handle.
  // Lifetime lore:
  // Windows: This handle is owned by the Font object, and should not be
  //          destroyed by the caller.
  // Mac:     The object is owned by the system and should not be released.
  // Gtk:     This handle is created on demand, and must be freed by calling
  //          pango_font_description_free() when the caller is done using it or
  //          by using ScopedPangoFontDescription.
  NativeFont GetNativeFont() const;

  // Raw access to the underlying platform font implementation. Can be
  // static_cast to a known implementation type if needed.
  PlatformFont* platform_font() const { return platform_font_.get(); }

 private:
  // Wrapped platform font implementation.
  scoped_refptr<PlatformFont> platform_font_;
};

}  // namespace gfx

#endif  // UI_GFX_FONT_H_
