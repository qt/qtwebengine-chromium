// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/date/tray_date.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/date/date_view.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "ash/system/tray/tray_popup_header_button.h"
#include "base/i18n/time_formatting.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/fieldpos.h"
#include "third_party/icu/source/i18n/unicode/fmtable.h"
#include "third_party/skia/include/core/SkRect.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/painter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/system_clock_observer.h"
#endif

namespace {

const int kPaddingVertical = 19;

}  // namespace

namespace ash {
namespace internal {

class DateDefaultView : public views::View,
                        public views::ButtonListener {
 public:
  explicit DateDefaultView(ash::user::LoginStatus login)
      : help_(NULL),
        shutdown_(NULL),
        lock_(NULL),
        date_view_(NULL) {
    SetLayoutManager(new views::FillLayout);

    date_view_ = new tray::DateView();
    date_view_->set_border(views::Border::CreateEmptyBorder(kPaddingVertical,
        ash::kTrayPopupPaddingHorizontal,
        0,
        0));
    SpecialPopupRow* view = new SpecialPopupRow();
    view->SetContent(date_view_);
    AddChildView(view);

    if (login == ash::user::LOGGED_IN_LOCKED ||
        login == ash::user::LOGGED_IN_NONE)
      return;

    date_view_->SetActionable(true);

    help_ = new TrayPopupHeaderButton(this,
        IDR_AURA_UBER_TRAY_HELP,
        IDR_AURA_UBER_TRAY_HELP,
        IDR_AURA_UBER_TRAY_HELP_HOVER,
        IDR_AURA_UBER_TRAY_HELP_HOVER,
        IDS_ASH_STATUS_TRAY_HELP);
    help_->SetTooltipText(l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_HELP));
    view->AddButton(help_);

#if !defined(OS_WIN)
    if (login != ash::user::LOGGED_IN_LOCKED &&
        login != ash::user::LOGGED_IN_RETAIL_MODE) {
      shutdown_ = new TrayPopupHeaderButton(this,
          IDR_AURA_UBER_TRAY_SHUTDOWN,
          IDR_AURA_UBER_TRAY_SHUTDOWN,
          IDR_AURA_UBER_TRAY_SHUTDOWN_HOVER,
          IDR_AURA_UBER_TRAY_SHUTDOWN_HOVER,
          IDS_ASH_STATUS_TRAY_SHUTDOWN);
      shutdown_->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_SHUTDOWN));
      view->AddButton(shutdown_);
    }

    if (ash::Shell::GetInstance()->session_state_delegate()->CanLockScreen()) {
      lock_ = new TrayPopupHeaderButton(this,
          IDR_AURA_UBER_TRAY_LOCKSCREEN,
          IDR_AURA_UBER_TRAY_LOCKSCREEN,
          IDR_AURA_UBER_TRAY_LOCKSCREEN_HOVER,
          IDR_AURA_UBER_TRAY_LOCKSCREEN_HOVER,
          IDS_ASH_STATUS_TRAY_LOCK);
      lock_->SetTooltipText(
          l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_LOCK));
      view->AddButton(lock_);
    }
#endif  // !defined(OS_WIN)
  }

  virtual ~DateDefaultView() {}

  views::View* GetHelpButtonView() const {
    return help_;
  }

  tray::DateView* GetDateView() const {
    return date_view_;
  }

 private:
  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    ash::Shell* shell = ash::Shell::GetInstance();
    ash::SystemTrayDelegate* tray_delegate = shell->system_tray_delegate();
    if (sender == help_) {
      shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_HELP);
      tray_delegate->ShowHelp();
    } else if (sender == shutdown_) {
      shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_SHUT_DOWN);
      tray_delegate->ShutDown();
    } else if (sender == lock_) {
      shell->metrics()->RecordUserMetricsAction(ash::UMA_TRAY_LOCK_SCREEN);
      tray_delegate->RequestLockScreen();
    } else {
      NOTREACHED();
    }
  }

  TrayPopupHeaderButton* help_;
  TrayPopupHeaderButton* shutdown_;
  TrayPopupHeaderButton* lock_;
  tray::DateView* date_view_;

  DISALLOW_COPY_AND_ASSIGN(DateDefaultView);
};

TrayDate::TrayDate(SystemTray* system_tray)
    : SystemTrayItem(system_tray),
      time_tray_(NULL),
      default_view_(NULL) {
#if defined(OS_CHROMEOS)
  system_clock_observer_.reset(new SystemClockObserver());
#endif
  Shell::GetInstance()->system_tray_notifier()->AddClockObserver(this);
}

TrayDate::~TrayDate() {
  Shell::GetInstance()->system_tray_notifier()->RemoveClockObserver(this);
}

views::View* TrayDate::GetHelpButtonView() const {
  if (!default_view_)
    return NULL;
  return default_view_->GetHelpButtonView();
}

views::View* TrayDate::CreateTrayView(user::LoginStatus status) {
  CHECK(time_tray_ == NULL);
  ClockLayout clock_layout =
      (system_tray()->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM ||
       system_tray()->shelf_alignment() == SHELF_ALIGNMENT_TOP) ?
          HORIZONTAL_CLOCK : VERTICAL_CLOCK;
  time_tray_ = new tray::TimeView(clock_layout);
  views::View* view = new TrayItemView(this);
  view->AddChildView(time_tray_);
  return view;
}

views::View* TrayDate::CreateDefaultView(user::LoginStatus status) {
  default_view_ = new DateDefaultView(status);
  return default_view_;
}

views::View* TrayDate::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayDate::DestroyTrayView() {
  time_tray_ = NULL;
}

void TrayDate::DestroyDefaultView() {
  default_view_ = NULL;
}

void TrayDate::DestroyDetailedView() {
}

void TrayDate::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayDate::UpdateAfterShelfAlignmentChange(ShelfAlignment alignment) {
  if (time_tray_) {
    ClockLayout clock_layout = (alignment == SHELF_ALIGNMENT_BOTTOM ||
        alignment == SHELF_ALIGNMENT_TOP) ?
            HORIZONTAL_CLOCK : VERTICAL_CLOCK;
    time_tray_->UpdateClockLayout(clock_layout);
  }
}

void TrayDate::OnDateFormatChanged() {
  if (time_tray_)
    time_tray_->UpdateTimeFormat();
  if (default_view_)
    default_view_->GetDateView()->UpdateTimeFormat();
}

void TrayDate::OnSystemClockTimeUpdated() {
  if (time_tray_)
    time_tray_->UpdateTimeFormat();
  if (default_view_)
    default_view_->GetDateView()->UpdateTimeFormat();
}

void TrayDate::Refresh() {
  if (time_tray_)
    time_tray_->UpdateText();
}

}  // namespace internal
}  // namespace ash
