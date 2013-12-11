// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/tabbed_pane/tabbed_pane.h"

#include "base/logging.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/tabbed_pane/tabbed_pane_listener.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"

namespace {

// TODO(markusheintz|msw): Use NativeTheme colors.
const SkColor kTabTitleColor_Inactive = SkColorSetRGB(0x64, 0x64, 0x64);
const SkColor kTabTitleColor_Active = SK_ColorBLACK;
const SkColor kTabTitleColor_Hovered = SK_ColorBLACK;
const SkColor kTabBorderColor = SkColorSetRGB(0xC8, 0xC8, 0xC8);
const SkScalar kTabBorderThickness = 1.0f;

}  // namespace

namespace views {

// static
const char TabbedPane::kViewClassName[] = "TabbedPane";

// The tab view shown in the tab strip.
class Tab : public View {
 public:
  Tab(TabbedPane* tabbed_pane, const string16& title, View* contents);
  virtual ~Tab();

  View* contents() const { return contents_; }

  bool selected() const { return contents_->visible(); }
  void SetSelected(bool selected);

  // Overridden from View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseEntered(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  enum TabState {
    TAB_INACTIVE,
    TAB_ACTIVE,
    TAB_PRESSED,
    TAB_HOVERED,
  };

  void SetState(TabState tab_state);

  TabbedPane* tabbed_pane_;
  Label* title_;
  gfx::Size preferred_title_size_;
  TabState tab_state_;
  // The content view associated with this tab.
  View* contents_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

// The tab strip shown above the tab contents.
class TabStrip : public View {
 public:
  explicit TabStrip(TabbedPane* tabbed_pane);
  virtual ~TabStrip();

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  TabbedPane* tabbed_pane_;

  DISALLOW_COPY_AND_ASSIGN(TabStrip);
};

Tab::Tab(TabbedPane* tabbed_pane, const string16& title, View* contents)
    : tabbed_pane_(tabbed_pane),
      title_(new Label(title, gfx::Font().DeriveFont(0, gfx::Font::BOLD))),
      tab_state_(TAB_ACTIVE),
      contents_(contents) {
  // Calculate this now while the font is guaranteed to be bold.
  preferred_title_size_ = title_->GetPreferredSize();

  SetState(TAB_INACTIVE);
  AddChildView(title_);
}

Tab::~Tab() {}

void Tab::SetSelected(bool selected) {
  contents_->SetVisible(selected);
  SetState(selected ? TAB_ACTIVE : TAB_INACTIVE);
}

bool Tab::OnMousePressed(const ui::MouseEvent& event) {
  SetState(TAB_PRESSED);
  return true;
}

void Tab::OnMouseReleased(const ui::MouseEvent& event) {
  SetState(selected() ? TAB_ACTIVE : TAB_HOVERED);
  if (GetLocalBounds().Contains(event.location()))
    tabbed_pane_->SelectTab(this);
}

void Tab::OnMouseCaptureLost() {
  SetState(TAB_INACTIVE);
}

void Tab::OnMouseEntered(const ui::MouseEvent& event) {
  SetState(selected() ? TAB_ACTIVE : TAB_HOVERED);
}

void Tab::OnMouseExited(const ui::MouseEvent& event) {
  SetState(selected() ? TAB_ACTIVE : TAB_INACTIVE);
}

void Tab::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_TAP_DOWN:
      SetState(TAB_PRESSED);
      break;
    case ui::ET_GESTURE_TAP:
      // SelectTab also sets the right tab color.
      tabbed_pane_->SelectTab(this);
      break;
    case ui::ET_GESTURE_TAP_CANCEL:
      SetState(selected() ? TAB_ACTIVE : TAB_INACTIVE);
      break;
    default:
      break;
  }
  event->SetHandled();
}

gfx::Size Tab::GetPreferredSize() {
  gfx::Size size(preferred_title_size_);
  size.Enlarge(21, 9);
  const int kTabMinWidth = 54;
  if (size.width() < kTabMinWidth)
    size.set_width(kTabMinWidth);
  return size;
}

void Tab::Layout() {
  gfx::Rect bounds = GetLocalBounds();
  bounds.Inset(0, 1, 0, 0);
  bounds.ClampToCenteredSize(preferred_title_size_);
  title_->SetBoundsRect(bounds);
}

void Tab::SetState(TabState tab_state) {
  if (tab_state == tab_state_)
    return;
  tab_state_ = tab_state;

  switch (tab_state) {
    case TAB_INACTIVE:
      title_->SetEnabledColor(kTabTitleColor_Inactive);
      title_->SetFont(gfx::Font());
      break;
    case TAB_ACTIVE:
      title_->SetEnabledColor(kTabTitleColor_Active);
      title_->SetFont(gfx::Font().DeriveFont(0, gfx::Font::BOLD));
      break;
    case TAB_PRESSED:
      // No visual distinction for pressed state.
      break;
    case TAB_HOVERED:
      title_->SetEnabledColor(kTabTitleColor_Hovered);
      title_->SetFont(gfx::Font());
      break;
  }
  SchedulePaint();
}

TabStrip::TabStrip(TabbedPane* tabbed_pane) : tabbed_pane_(tabbed_pane) {}

TabStrip::~TabStrip() {}

gfx::Size TabStrip::GetPreferredSize() {
  gfx::Size size;
  for (int i = 0; i < child_count(); ++i) {
    const gfx::Size child_size = child_at(i)->GetPreferredSize();
    size.SetSize(size.width() + child_size.width(),
                 std::max(size.height(), child_size.height()));
  }
  return size;
}

void TabStrip::Layout() {
  const int kTabOffset = 9;
  int x = kTabOffset;  // Layout tabs with an offset to the tabstrip border.
  for (int i = 0; i < child_count(); ++i) {
    gfx::Size ps = child_at(i)->GetPreferredSize();
    child_at(i)->SetBounds(x, 0, ps.width(), ps.height());
    x = child_at(i)->bounds().right();
  }
}

void TabStrip::OnPaint(gfx::Canvas* canvas) {
  OnPaintBackground(canvas);

  // Draw the TabStrip border.
  SkPaint paint;
  paint.setColor(kTabBorderColor);
  paint.setStrokeWidth(kTabBorderThickness);
  SkScalar line_y = SkIntToScalar(height()) - (kTabBorderThickness / 2);
  SkScalar line_end = SkIntToScalar(width());
  int selected_tab_index = tabbed_pane_->selected_tab_index();
  if (selected_tab_index >= 0) {
    Tab* selected_tab = tabbed_pane_->GetTabAt(selected_tab_index);
    SkPath path;
    SkScalar tab_height =
        SkIntToScalar(selected_tab->height()) - kTabBorderThickness;
    SkScalar tab_width =
        SkIntToScalar(selected_tab->width()) - kTabBorderThickness;
    SkScalar tab_start = SkIntToScalar(selected_tab->GetMirroredX());
    path.moveTo(0, line_y);
    path.rLineTo(tab_start, 0);
    path.rLineTo(0, -tab_height);
    path.rLineTo(tab_width, 0);
    path.rLineTo(0, tab_height);
    path.lineTo(line_end, line_y);

    SkPaint paint;
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kTabBorderColor);
    paint.setStrokeWidth(kTabBorderThickness);
    canvas->DrawPath(path, paint);
  } else {
    canvas->sk_canvas()->drawLine(0, line_y, line_end, line_y, paint);
  }
}

TabbedPane::TabbedPane(bool draw_border)
  : listener_(NULL),
    tab_strip_(new TabStrip(this)),
    contents_(new View()),
    selected_tab_index_(-1) {
  set_focusable(true);
  AddChildView(tab_strip_);
  AddChildView(contents_);
  if (draw_border) {
    contents_->set_border(Border::CreateSolidSidedBorder(0,
                                                         kTabBorderThickness,
                                                         kTabBorderThickness,
                                                         kTabBorderThickness,
                                                         kTabBorderColor));
  }
}

TabbedPane::~TabbedPane() {}

int TabbedPane::GetTabCount() {
  DCHECK_EQ(tab_strip_->child_count(), contents_->child_count());
  return contents_->child_count();
}

View* TabbedPane::GetSelectedTab() {
  return selected_tab_index() < 0 ?
      NULL : GetTabAt(selected_tab_index())->contents();
}

void TabbedPane::AddTab(const string16& title, View* contents) {
  AddTabAtIndex(tab_strip_->child_count(), title, contents);
}

void TabbedPane::AddTabAtIndex(int index,
                               const string16& title,
                               View* contents) {
  DCHECK(index >= 0 && index <= GetTabCount());
  contents->SetVisible(false);

  tab_strip_->AddChildViewAt(new Tab(this, title, contents), index);
  contents_->AddChildViewAt(contents, index);
  if (selected_tab_index() < 0)
    SelectTabAt(index);

  PreferredSizeChanged();
}

void TabbedPane::SelectTabAt(int index) {
  DCHECK(index >= 0 && index < GetTabCount());
  if (index == selected_tab_index())
    return;

  if (selected_tab_index() >= 0)
    GetTabAt(selected_tab_index())->SetSelected(false);

  selected_tab_index_ = index;
  Tab* tab = GetTabAt(index);
  tab->SetSelected(true);
  tab_strip_->SchedulePaint();

  FocusManager* focus_manager = tab->contents()->GetFocusManager();
  if (focus_manager) {
    const View* focused_view = focus_manager->GetFocusedView();
    if (focused_view && contents_->Contains(focused_view) &&
        !tab->contents()->Contains(focused_view))
      focus_manager->SetFocusedView(tab->contents());
  }

  if (listener())
    listener()->TabSelectedAt(index);
}

void TabbedPane::SelectTab(Tab* tab) {
  const int index = tab_strip_->GetIndexOf(tab);
  if (index >= 0)
    SelectTabAt(index);
}

gfx::Size TabbedPane::GetPreferredSize() {
  gfx::Size size;
  for (int i = 0; i < contents_->child_count(); ++i)
    size.SetToMax(contents_->child_at(i)->GetPreferredSize());
  size.Enlarge(0, tab_strip_->GetPreferredSize().height());
  return size;
}

Tab* TabbedPane::GetTabAt(int index) {
  return static_cast<Tab*>(tab_strip_->child_at(index));
}

void TabbedPane::Layout() {
  const gfx::Size size = tab_strip_->GetPreferredSize();
  tab_strip_->SetBounds(0, 0, width(), size.height());
  contents_->SetBounds(0, tab_strip_->bounds().bottom(), width(),
                       std::max(0, height() - size.height()));
  for (int i = 0; i < contents_->child_count(); ++i)
    contents_->child_at(i)->SetSize(contents_->size());
}

void TabbedPane::ViewHierarchyChanged(
    const ViewHierarchyChangedDetails& details) {
  if (details.is_add) {
    // Support navigating tabs by Ctrl+Tab and Ctrl+Shift+Tab.
    AddAccelerator(ui::Accelerator(ui::VKEY_TAB,
                                   ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN));
    AddAccelerator(ui::Accelerator(ui::VKEY_TAB, ui::EF_CONTROL_DOWN));
  }
}

bool TabbedPane::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // Handle Ctrl+Tab and Ctrl+Shift+Tab navigation of pages.
  DCHECK(accelerator.key_code() == ui::VKEY_TAB && accelerator.IsCtrlDown());
  const int tab_count = GetTabCount();
  if (tab_count <= 1)
    return false;
  const int increment = accelerator.IsShiftDown() ? -1 : 1;
  int next_tab_index = (selected_tab_index() + increment) % tab_count;
  // Wrap around.
  if (next_tab_index < 0)
    next_tab_index += tab_count;
  SelectTabAt(next_tab_index);
  return true;
}

const char* TabbedPane::GetClassName() const {
  return kViewClassName;
}

void TabbedPane::OnFocus() {
  View::OnFocus();

  View* selected_tab = GetSelectedTab();
  if (selected_tab) {
    selected_tab->NotifyAccessibilityEvent(
        ui::AccessibilityTypes::EVENT_FOCUS, true);
  }
}

void TabbedPane::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PAGETABLIST;
}

}  // namespace views
