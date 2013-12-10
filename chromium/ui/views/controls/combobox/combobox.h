// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
#define UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_

#include <string>

#include "base/time/time.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/prefix_delegate.h"

namespace gfx {
class Font;
}

namespace ui {
class ComboboxModel;
}

namespace views {

class ComboboxListener;
class FocusableBorder;
class MenuRunner;
class PrefixSelector;

// A non-editable combobox (aka a drop-down list or selector).
class VIEWS_EXPORT Combobox : public MenuDelegate, public PrefixDelegate {
 public:
  // The combobox's class name.
  static const char kViewClassName[];

  // |model| is not owned by the combobox.
  explicit Combobox(ui::ComboboxModel* model);
  virtual ~Combobox();

  static const gfx::Font& GetFont();

  // Sets the listener which will be called when a selection has been made.
  void set_listener(ComboboxListener* listener) { listener_ = listener; }

  // Informs the combobox that its model changed.
  void ModelChanged();

  // Gets/Sets the selected index.
  int selected_index() const { return selected_index_; }
  void SetSelectedIndex(int index);

  ui::ComboboxModel* model() const { return model_; }

  // Set the accessible name of the combobox.
  void SetAccessibleName(const string16& name);

  // Visually marks the combobox as having an invalid value selected.
  // When invalid, it paints with white text on a red background.
  // Callers are responsible for restoring validity with selection changes.
  void SetInvalid(bool invalid);
  bool invalid() const { return invalid_; }

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual const char* GetClassName() const OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& e) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& mouse_event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& mouse_event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& e) OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& e) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* gesture) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;

  // Overridden from MenuDelegate:
  virtual bool IsItemChecked(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;
  virtual bool GetAccelerator(int id, ui::Accelerator* accelerator) OVERRIDE;

  // Overridden from PrefixDelegate:
  virtual int GetRowCount() OVERRIDE;
  virtual int GetSelectedRow() OVERRIDE;
  virtual void SetSelectedRow(int row) OVERRIDE;
  virtual string16 GetTextForRow(int row) OVERRIDE;

 private:
  // Updates the combobox's content from its model.
  void UpdateFromModel();

  // Given bounds within our View, this helper mirrors the bounds if necessary.
  void AdjustBoundsForRTLUI(gfx::Rect* rect) const;

  // Draw the selected value of the drop down list
  void PaintText(gfx::Canvas* canvas);

  // Show the drop down list
  void ShowDropDownMenu(ui::MenuSourceType source_type);

  // Called when the selection is changed by the user.
  void OnSelectionChanged();

  // Our model. Not owned.
  ui::ComboboxModel* model_;

  // Our listener. Not owned. Notified when the selected index change.
  ComboboxListener* listener_;

  // The current selected index; -1 and means no selection.
  int selected_index_;

  // True when the selection is visually denoted as invalid.
  bool invalid_;

  // The accessible name of this combobox.
  string16 accessible_name_;

  // A helper used to select entries by keyboard input.
  scoped_ptr<PrefixSelector> selector_;

  // The reference to the border class. The object is owned by View::border_.
  FocusableBorder* text_border_;

  // The disclosure arrow next to the currently selected item from the list.
  const gfx::ImageSkia* disclosure_arrow_;

  // Responsible for showing the context menu.
  scoped_ptr<MenuRunner> dropdown_list_menu_runner_;

  // Is the drop down list showing
  bool dropdown_open_;

  // Like MenuButton, we use a time object in order to keep track of when the
  // combobox was closed. The time is used for simulating menu behavior; that
  // is, if the menu is shown and the button is pressed, we need to close the
  // menu. There is no clean way to get the second click event because the
  // menu is displayed using a modal loop and, unlike regular menus in Windows,
  // the button is not part of the displayed menu.
  base::Time closed_time_;

  // The maximum dimensions of the content in the dropdown
  gfx::Size content_size_;

  DISALLOW_COPY_AND_ASSIGN(Combobox);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_COMBOBOX_COMBOBOX_H_
