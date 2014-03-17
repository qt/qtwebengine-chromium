// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_caps_lock.h"

#include "ash/caps_lock_delegate.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/system/tray/actionable_view.h"
#include "ash/system/tray/fixed_sized_image_view.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

class CapsLockDefaultView : public ActionableView {
 public:
  CapsLockDefaultView()
      : text_label_(new views::Label),
        shortcut_label_(new views::Label) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                          kTrayPopupPaddingHorizontal,
                                          0,
                                          kTrayPopupPaddingBetweenItems));

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    FixedSizedImageView* image =
        new FixedSizedImageView(0, kTrayPopupItemHeight);
    image->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_CAPS_LOCK_DARK).
        ToImageSkia());
    AddChildView(image);

    text_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    AddChildView(text_label_);

    shortcut_label_->SetEnabled(false);
    AddChildView(shortcut_label_);
  }

  virtual ~CapsLockDefaultView() {}

  // Updates the label text and the shortcut text.
  void Update(bool caps_lock_enabled, bool search_mapped_to_caps_lock) {
    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
    const int text_string_id = caps_lock_enabled ?
        IDS_ASH_STATUS_TRAY_CAPS_LOCK_ENABLED :
        IDS_ASH_STATUS_TRAY_CAPS_LOCK_DISABLED;
    text_label_->SetText(bundle.GetLocalizedString(text_string_id));

    int shortcut_string_id = 0;
    if (caps_lock_enabled) {
      shortcut_string_id = search_mapped_to_caps_lock ?
          IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_SEARCH_OR_SHIFT :
          IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_ALT_SEARCH_OR_SHIFT;
    } else {
      shortcut_string_id = search_mapped_to_caps_lock ?
          IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_SEARCH :
          IDS_ASH_STATUS_TRAY_CAPS_LOCK_SHORTCUT_ALT_SEARCH;
    }
    shortcut_label_->SetText(bundle.GetLocalizedString(shortcut_string_id));

    Layout();
  }

 private:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE {
    views::View::Layout();

    // Align the shortcut text with the right end
    const int old_x = shortcut_label_->x();
    const int new_x =
        width() - shortcut_label_->width() - kTrayPopupPaddingHorizontal;
    shortcut_label_->SetX(new_x);
    const gfx::Size text_size = text_label_->size();
    text_label_->SetSize(gfx::Size(text_size.width() + new_x - old_x,
                                   text_size.height()));
  }

  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE {
    state->role = ui::AccessibilityTypes::ROLE_PUSHBUTTON;
    state->name = text_label_->text();
  }

  // Overridden from ActionableView:
  virtual bool PerformAction(const ui::Event& event) OVERRIDE {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        Shell::GetInstance()->caps_lock_delegate()->IsCapsLockEnabled() ?
        ash::UMA_STATUS_AREA_CAPS_LOCK_DISABLED_BY_CLICK :
        ash::UMA_STATUS_AREA_CAPS_LOCK_ENABLED_BY_CLICK);
    Shell::GetInstance()->caps_lock_delegate()->ToggleCapsLock();
    return true;
  }

  views::Label* text_label_;
  views::Label* shortcut_label_;

  DISALLOW_COPY_AND_ASSIGN(CapsLockDefaultView);
};

TrayCapsLock::TrayCapsLock(SystemTray* system_tray)
    : TrayImageItem(system_tray, IDR_AURA_UBER_TRAY_CAPS_LOCK),
      default_(NULL),
      detailed_(NULL),
      search_mapped_to_caps_lock_(false),
      caps_lock_enabled_(
          Shell::GetInstance()->caps_lock_delegate()->IsCapsLockEnabled()),
      message_shown_(false) {
  Shell::GetInstance()->system_tray_notifier()->AddCapsLockObserver(this);
}

TrayCapsLock::~TrayCapsLock() {
  Shell::GetInstance()->system_tray_notifier()->RemoveCapsLockObserver(this);
}

bool TrayCapsLock::GetInitialVisibility() {
  return Shell::GetInstance()->caps_lock_delegate()->IsCapsLockEnabled();
}

views::View* TrayCapsLock::CreateDefaultView(user::LoginStatus status) {
  if (!caps_lock_enabled_)
    return NULL;
  DCHECK(default_ == NULL);
  default_ = new CapsLockDefaultView;
  default_->Update(caps_lock_enabled_, search_mapped_to_caps_lock_);
  return default_;
}

views::View* TrayCapsLock::CreateDetailedView(user::LoginStatus status) {
  DCHECK(detailed_ == NULL);
  detailed_ = new views::View;

  detailed_->SetLayoutManager(new
      views::BoxLayout(views::BoxLayout::kHorizontal,
      kTrayPopupPaddingHorizontal, 10, kTrayPopupPaddingBetweenItems));

  ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
  views::ImageView* image = new views::ImageView;
  image->SetImage(bundle.GetImageNamed(IDR_AURA_UBER_TRAY_CAPS_LOCK_DARK).
      ToImageSkia());

  detailed_->AddChildView(image);

  const int string_id = search_mapped_to_caps_lock_ ?
      IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_SEARCH :
      IDS_ASH_STATUS_TRAY_CAPS_LOCK_CANCEL_BY_ALT_SEARCH;
  views::Label* label = new views::Label(bundle.GetLocalizedString(string_id));
  label->SetMultiLine(true);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  detailed_->AddChildView(label);
  Shell::GetInstance()->metrics()->RecordUserMetricsAction(
      ash::UMA_STATUS_AREA_CAPS_LOCK_DETAILED);

  return detailed_;
}

void TrayCapsLock::DestroyDefaultView() {
  default_ = NULL;
}

void TrayCapsLock::DestroyDetailedView() {
  detailed_ = NULL;
}

void TrayCapsLock::OnCapsLockChanged(bool enabled,
                                     bool search_mapped_to_caps_lock) {
  if (tray_view())
    tray_view()->SetVisible(enabled);

  caps_lock_enabled_ = enabled;
  search_mapped_to_caps_lock_ = search_mapped_to_caps_lock;

  if (default_) {
    default_->Update(enabled, search_mapped_to_caps_lock);
  } else {
    if (enabled) {
      if (!message_shown_) {
        Shell::GetInstance()->metrics()->RecordUserMetricsAction(
            ash::UMA_STATUS_AREA_CAPS_LOCK_POPUP);
        PopupDetailedView(kTrayPopupAutoCloseDelayForTextInSeconds, false);
        message_shown_ = true;
      }
    } else if (detailed_) {
      detailed_->GetWidget()->Close();
    }
  }
}

}  // namespace internal
}  // namespace ash
