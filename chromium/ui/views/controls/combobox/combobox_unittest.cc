// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/combobox/combobox.h"

#include <set>

#include "base/basictypes.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/combobox_model.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/controls/combobox/combobox_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/menu_runner_handler.h"
#include "ui/views/ime/mock_input_method.h"
#include "ui/views/test/menu_runner_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace views {

namespace {

// An dummy implementation of MenuRunnerHandler to check if the dropdown menu is
// shown or not.
class TestMenuRunnerHandler : public MenuRunnerHandler {
 public:
  TestMenuRunnerHandler()
      : executed_(false) {}

  bool executed() const { return executed_; }

  virtual MenuRunner::RunResult RunMenuAt(Widget* parent,
                                          MenuButton* button,
                                          const gfx::Rect& bounds,
                                          MenuItemView::AnchorPosition anchor,
                                          ui::MenuSourceType source_type,
                                          int32 types) OVERRIDE {
    executed_ = true;
    return MenuRunner::NORMAL_EXIT;
  }

 private:
  bool executed_;

  DISALLOW_COPY_AND_ASSIGN(TestMenuRunnerHandler);
};

// A wrapper of Combobox to intercept the result of OnKeyPressed() and
// OnKeyReleased() methods.
class TestCombobox : public Combobox {
 public:
  explicit TestCombobox(ui::ComboboxModel* model)
      : Combobox(model),
        key_handled_(false),
        key_received_(false) {
  }

  virtual bool OnKeyPressed(const ui::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = Combobox::OnKeyPressed(e);
    return key_handled_;
  }

  virtual bool OnKeyReleased(const ui::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = Combobox::OnKeyReleased(e);
    return key_handled_;
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() {
    key_received_ = key_handled_ = false;
  }

 private:
  bool key_handled_;
  bool key_received_;

  DISALLOW_COPY_AND_ASSIGN(TestCombobox);
};

// A concrete class is needed to test the combobox.
class TestComboboxModel : public ui::ComboboxModel {
 public:
  TestComboboxModel() {}
  virtual ~TestComboboxModel() {}

  // ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE {
    return 10;
  }
  virtual string16 GetItemAt(int index) OVERRIDE {
    if (IsItemSeparatorAt(index)) {
      NOTREACHED();
      return ASCIIToUTF16("SEPARATOR");
    }
    return ASCIIToUTF16(index % 2 == 0 ? "PEANUT BUTTER" : "JELLY");
  }
  virtual bool IsItemSeparatorAt(int index) OVERRIDE {
    return separators_.find(index) != separators_.end();
  }

  void SetSeparators(const std::set<int>& separators) {
    separators_ = separators;
  }

 private:
  std::set<int> separators_;

  DISALLOW_COPY_AND_ASSIGN(TestComboboxModel);
};

class EvilListener : public ComboboxListener {
 public:
  EvilListener() : deleted_(false) {};
  virtual ~EvilListener() {};

  // ComboboxListener:
  virtual void OnSelectedIndexChanged(Combobox* combobox) OVERRIDE {
    delete combobox;
    deleted_ = true;
  }

  bool deleted() const { return deleted_; }

 private:
  bool deleted_;

  DISALLOW_COPY_AND_ASSIGN(EvilListener);
};

class TestComboboxListener : public views::ComboboxListener {
 public:
  TestComboboxListener()
      : on_selected_index_changed_called_(false),
        on_combobox_text_button_clicked_called_(false) {
  }
  virtual ~TestComboboxListener() {}

  virtual void OnSelectedIndexChanged(views::Combobox* combobox) OVERRIDE {
    on_selected_index_changed_called_ = true;
  }

  virtual void OnComboboxTextButtonClicked(views::Combobox* combobox) OVERRIDE {
    on_combobox_text_button_clicked_called_ = true;
  }

  bool on_selected_index_changed_called() const {
    return on_selected_index_changed_called_;
  }

  bool on_combobox_text_button_clicked_called() const {
    return on_combobox_text_button_clicked_called_;
  }

 private:
  bool on_selected_index_changed_called_;
  bool on_combobox_text_button_clicked_called_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestComboboxListener);
};

}  // namespace

class ComboboxTest : public ViewsTestBase {
 public:
  ComboboxTest() : widget_(NULL), combobox_(NULL), input_method_(NULL) {}

  virtual void TearDown() OVERRIDE {
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  void InitCombobox() {
    model_.reset(new TestComboboxModel());

    ASSERT_FALSE(combobox_);
    combobox_ = new TestCombobox(model_.get());
    combobox_->set_id(1);

    widget_ = new Widget;
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(200, 200, 200, 200);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(combobox_);

    input_method_ = new MockInputMethod();
    widget_->ReplaceInputMethod(input_method_);

    // Assumes the Widget is always focused.
    input_method_->OnFocus();

    combobox_->RequestFocus();
    combobox_->SizeToPreferredSize();
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code) {
    SendKeyEventWithType(key_code, ui::ET_KEY_PRESSED);
  }

  void SendKeyEventWithType(ui::KeyboardCode key_code, ui::EventType type) {
    ui::KeyEvent event(type, key_code, 0, false);
    input_method_->DispatchKeyEvent(event);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  void PerformClick(const gfx::Point& point) {
    ui::MouseEvent pressed_event = ui::MouseEvent(ui::ET_MOUSE_PRESSED, point,
                                                  point,
                                                  ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&pressed_event);
    ui::MouseEvent released_event = ui::MouseEvent(ui::ET_MOUSE_RELEASED, point,
                                                   point,
                                                   ui::EF_LEFT_MOUSE_BUTTON);
    widget_->OnMouseEvent(&released_event);
  }

  // We need widget to populate wrapper class.
  Widget* widget_;

  // |combobox_| will be allocated InitCombobox() and then owned by |widget_|.
  TestCombobox* combobox_;

  // Combobox does not take ownership of the model, hence it needs to be scoped.
  scoped_ptr<TestComboboxModel> model_;

  // For testing input method related behaviors.
  MockInputMethod* input_method_;
};

TEST_F(ComboboxTest, KeyTest) {
  InitCombobox();
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(combobox_->selected_index() + 1, model_->GetItemCount());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(combobox_->selected_index(), 0);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_EQ(combobox_->selected_index(), 2);
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(combobox_->selected_index(), 1);
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(combobox_->selected_index(), 0);
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(combobox_->selected_index(), model_->GetItemCount() - 1);
}

// Check that if a combobox is disabled before it has a native wrapper, then the
// native wrapper inherits the disabled state when it gets created.
TEST_F(ComboboxTest, DisabilityTest) {
  model_.reset(new TestComboboxModel());

  ASSERT_FALSE(combobox_);
  combobox_ = new TestCombobox(model_.get());
  combobox_->SetEnabled(false);

  widget_ = new Widget;
  Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
  params.bounds = gfx::Rect(100, 100, 100, 100);
  widget_->Init(params);
  View* container = new View();
  widget_->SetContentsView(container);
  container->AddChildView(combobox_);
  EXPECT_FALSE(combobox_->enabled());
}

// Verifies that we don't select a separator line in combobox when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipSeparatorSimple) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(2);
  model_->SetSeparators(separators);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
}

// Verifies that we never select the separator that is in the beginning of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorBeginning) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(0);
  model_->SetSeparators(separators);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(2, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(1, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
}

// Verifies that we never select the separator that is in the end of the
// combobox list when navigating through keyboard.
TEST_F(ComboboxTest, SkipSeparatorEnd) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(model_->GetItemCount() - 1);
  model_->SetSeparators(separators);
  combobox_->SetSelectedIndex(8);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(8, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(7, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(8, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the beginning of the combobox list when
// navigating through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtBeginning) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(2);
  model_->SetSeparators(separators);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(9, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(3, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(3, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the middle of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleAdjacentSeparatorsAtMiddle) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(4);
  separators.insert(5);
  separators.insert(6);
  model_->SetSeparators(separators);
  combobox_->SetSelectedIndex(3);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(7, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(3, combobox_->selected_index());
}

// Verifies that we never select any of the adjacent separators (multiple
// consecutive) that appear in the end of the combobox list when navigating
// through keyboard.
TEST_F(ComboboxTest, SkipMultipleSeparatorsAtEnd) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(7);
  separators.insert(8);
  separators.insert(9);
  model_->SetSeparators(separators);
  combobox_->SetSelectedIndex(6);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_EQ(6, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_UP);
  EXPECT_EQ(5, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_NEXT);
  EXPECT_EQ(6, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_PRIOR);
  EXPECT_EQ(0, combobox_->selected_index());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(6, combobox_->selected_index());
}

TEST_F(ComboboxTest, GetTextForRowTest) {
  InitCombobox();
  std::set<int> separators;
  separators.insert(0);
  separators.insert(1);
  separators.insert(9);
  model_->SetSeparators(separators);
  for (int i = 0; i < combobox_->GetRowCount(); ++i) {
    if (separators.count(i) != 0) {
      EXPECT_TRUE(combobox_->GetTextForRow(i).empty()) << i;
    } else {
      EXPECT_EQ(ASCIIToUTF16(i % 2 == 0 ? "PEANUT BUTTER" : "JELLY"),
                combobox_->GetTextForRow(i)) << i;
    }
  }
}

// Verifies selecting the first matching value (and returning whether found).
TEST_F(ComboboxTest, SelectValue) {
  InitCombobox();
  ASSERT_EQ(model_->GetDefaultIndex(), combobox_->selected_index());
  EXPECT_TRUE(combobox_->SelectValue(ASCIIToUTF16("PEANUT BUTTER")));
  EXPECT_EQ(0, combobox_->selected_index());
  EXPECT_TRUE(combobox_->SelectValue(ASCIIToUTF16("JELLY")));
  EXPECT_EQ(1, combobox_->selected_index());
  EXPECT_FALSE(combobox_->SelectValue(ASCIIToUTF16("BANANAS")));
  EXPECT_EQ(1, combobox_->selected_index());
}

TEST_F(ComboboxTest, ListenerHandlesDelete) {
  TestComboboxModel model;
  TestCombobox* combobox = new TestCombobox(&model);  // Deleted on change.
  EvilListener evil_listener;
  combobox->set_listener(&evil_listener);
  ASSERT_NO_FATAL_FAILURE(combobox->ExecuteCommand(2));
  EXPECT_TRUE(evil_listener.deleted());
}

TEST_F(ComboboxTest, Click) {
  InitCombobox();

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->Layout();

  // Click the left side. The menu is shown.
  TestMenuRunnerHandler* test_menu_runner_handler = new TestMenuRunnerHandler();
  scoped_ptr<MenuRunnerHandler> menu_runner_handler(test_menu_runner_handler);
  test::MenuRunnerTestAPI test_api(
      combobox_->dropdown_list_menu_runner_.get());
  test_api.SetMenuRunnerHandler(menu_runner_handler.Pass());
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_combobox_text_button_clicked_called());
  EXPECT_TRUE(test_menu_runner_handler->executed());
}

TEST_F(ComboboxTest, NotifyOnClickWithReturnKey) {
  InitCombobox();

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // With STYLE_SHOW_DROP_DOWN_ON_CLICK, the click event is ignored.
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_FALSE(listener.on_combobox_text_button_clicked_called());

  // With STYLE_NOTIFY_ON_CLICK, the click event is notified.
  combobox_->SetStyle(Combobox::STYLE_NOTIFY_ON_CLICK);
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_TRUE(listener.on_combobox_text_button_clicked_called());
}

TEST_F(ComboboxTest, NotifyOnClickWithSpaceKey) {
  InitCombobox();

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  // With STYLE_SHOW_DROP_DOWN_ON_CLICK, the click event is ignored.
  SendKeyEvent(ui::VKEY_SPACE);
  EXPECT_FALSE(listener.on_combobox_text_button_clicked_called());
  SendKeyEventWithType(ui::VKEY_SPACE, ui::ET_KEY_RELEASED);
  EXPECT_FALSE(listener.on_combobox_text_button_clicked_called());

  // With STYLE_NOTIFY_ON_CLICK, the click event is notified after releasing.
  combobox_->SetStyle(Combobox::STYLE_NOTIFY_ON_CLICK);
  SendKeyEvent(ui::VKEY_SPACE);
  EXPECT_FALSE(listener.on_combobox_text_button_clicked_called());
  SendKeyEventWithType(ui::VKEY_SPACE, ui::ET_KEY_RELEASED);
  EXPECT_TRUE(listener.on_combobox_text_button_clicked_called());
}

TEST_F(ComboboxTest, NotifyOnClickWithMouse) {
  InitCombobox();

  TestComboboxListener listener;
  combobox_->set_listener(&listener);

  combobox_->SetStyle(Combobox::STYLE_NOTIFY_ON_CLICK);
  combobox_->Layout();

  // Click the right side (arrow button). The menu is shown.
  TestMenuRunnerHandler* test_menu_runner_handler = new TestMenuRunnerHandler();
  scoped_ptr<MenuRunnerHandler> menu_runner_handler(test_menu_runner_handler);
  scoped_ptr<test::MenuRunnerTestAPI> test_api(
      new test::MenuRunnerTestAPI(combobox_->dropdown_list_menu_runner_.get()));
  test_api->SetMenuRunnerHandler(menu_runner_handler.Pass());

  PerformClick(gfx::Point(combobox_->x() + combobox_->width() - 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_FALSE(listener.on_combobox_text_button_clicked_called());
  EXPECT_TRUE(test_menu_runner_handler->executed());

  // Click the left side (text button). The click event is notified.
  test_menu_runner_handler = new TestMenuRunnerHandler();
  menu_runner_handler.reset(test_menu_runner_handler);
  test_api.reset(
      new test::MenuRunnerTestAPI(combobox_->dropdown_list_menu_runner_.get()));
  test_api->SetMenuRunnerHandler(menu_runner_handler.Pass());
  PerformClick(gfx::Point(combobox_->x() + 1,
                          combobox_->y() + combobox_->height() / 2));
  EXPECT_TRUE(listener.on_combobox_text_button_clicked_called());
  EXPECT_FALSE(test_menu_runner_handler->executed());
}

TEST_F(ComboboxTest, ConsumingPressKeyEvents) {
  InitCombobox();

  EXPECT_FALSE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0, false)));
  EXPECT_FALSE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0, false)));

  // When the combobox's style is STYLE_NOTIFY_ON_CLICK, pressing events of
  // a space key or an enter key will be consumed.
  combobox_->SetStyle(Combobox::STYLE_NOTIFY_ON_CLICK);
  EXPECT_TRUE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_RETURN, 0, false)));
  EXPECT_TRUE(combobox_->OnKeyPressed(
      ui::KeyEvent(ui::ET_KEY_PRESSED, ui::VKEY_SPACE, 0, false)));
}

}  // namespace views
