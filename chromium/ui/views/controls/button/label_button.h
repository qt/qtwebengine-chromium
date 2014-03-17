// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
#define UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/native_theme_delegate.h"

namespace views {

class Painter;

// LabelButton is an alternative to TextButton, it's not focusable by default.
class VIEWS_EXPORT LabelButton : public CustomButton,
                                 public NativeThemeDelegate {
 public:
  // The length of the hover fade animation.
  static const int kHoverAnimationDurationMs;

  static const char kViewClassName[];

  LabelButton(ButtonListener* listener, const string16& text);
  virtual ~LabelButton();

  // Get or set the image shown for the specified button state.
  // GetImage returns the image for STATE_NORMAL if the state's image is empty.
  virtual const gfx::ImageSkia& GetImage(ButtonState for_state);
  void SetImage(ButtonState for_state, const gfx::ImageSkia& image);

  // Get or set the text shown on the button.
  const string16& GetText() const;
  void SetText(const string16& text);

  // Set the text color shown for the specified button state.
  void SetTextColor(ButtonState for_state, SkColor color);

  // Get or set the text's multi-line property to break on '\n', etc.
  bool GetTextMultiLine() const;
  void SetTextMultiLine(bool text_multi_line);

  // Get or set the font used by this button.
  const gfx::Font& GetFont() const;
  void SetFont(const gfx::Font& font);

  // Set the elide behavior of this button.
  void SetElideBehavior(Label::ElideBehavior elide_behavior);

  // Get or set the horizontal alignment used for the button; reversed in RTL.
  // The optional image will lead the text, unless the button is right-aligned.
  gfx::HorizontalAlignment GetHorizontalAlignment() const;
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Call set_min_size(gfx::Size()) to clear the monotonically increasing size.
  void set_min_size(const gfx::Size& min_size) { min_size_ = min_size; }
  void set_max_size(const gfx::Size& max_size) { max_size_ = max_size; }

  // Get or set the option to handle the return key; false by default.
  bool is_default() const { return is_default_; }
  void SetIsDefault(bool is_default);

  // Get or set the button's overall style; the default is |STYLE_TEXTBUTTON|.
  ButtonStyle style() const { return style_; }
  void SetStyle(ButtonStyle style);

  void SetFocusPainter(scoped_ptr<Painter> focus_painter);

  // View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;

 protected:
  ImageView* image() const { return image_; }
  Label* label() const { return label_; }

  // View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // Fill |params| with information about the button.
  virtual void GetExtraParams(ui::NativeTheme::ExtraParams* params) const;

  // Resets colors from the NativeTheme, explicitly set colors are unchanged.
  virtual void ResetColorsFromNativeTheme();

  // Updates the image view to contain the appropriate button state image.
  void UpdateImage();

  // NativeThemeDelegate:
  virtual gfx::Rect GetThemePaintRect() const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Init);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Label);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Image);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, LabelAndImage);
  FRIEND_TEST_ALL_PREFIXES(LabelButtonTest, Font);

  // CustomButton:
  virtual void StateChanged() OVERRIDE;

  // View:
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

  // NativeThemeDelegate:
  virtual ui::NativeTheme::Part GetThemePart() const OVERRIDE;
  virtual ui::NativeTheme::State GetThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual const gfx::Animation* GetThemeAnimation() const OVERRIDE;
  virtual ui::NativeTheme::State GetBackgroundThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;
  virtual ui::NativeTheme::State GetForegroundThemeState(
      ui::NativeTheme::ExtraParams* params) const OVERRIDE;

  // The image and label shown in the button.
  ImageView* image_;
  Label* label_;

  // The images and colors for each button state.
  gfx::ImageSkia button_state_images_[STATE_COUNT];
  SkColor button_state_colors_[STATE_COUNT];

  // Used to track whether SetTextColor() has been invoked.
  bool explicitly_set_colors_[STATE_COUNT];

  // |min_size_| increases monotonically with the preferred size.
  gfx::Size min_size_;
  // |max_size_| may be set to clamp the preferred size.
  gfx::Size max_size_;

  // Flag indicating default handling of the return key via an accelerator.
  // Whether or not the button appears or behaves as the default button in its
  // current context;
  bool is_default_;

  // The button's overall style.
  ButtonStyle style_;

  scoped_ptr<Painter> focus_painter_;

  DISALLOW_COPY_AND_ASSIGN(LabelButton);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_BUTTON_LABEL_BUTTON_H_
