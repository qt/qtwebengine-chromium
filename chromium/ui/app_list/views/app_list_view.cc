// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_view.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/pagination_model.h"
#include "ui/app_list/signin_delegate.h"
#include "ui/app_list/speech_ui_model.h"
#include "ui/app_list/views/app_list_background.h"
#include "ui/app_list/views/app_list_main_view.h"
#include "ui/app_list/views/app_list_view_observer.h"
#include "ui/app_list/views/search_box_view.h"
#include "ui/app_list/views/signin_view.h"
#include "ui/app_list/views/speech_view.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/aura/root_window.h"
#if defined(OS_WIN)
#include "ui/base/win/shell.h"
#endif
#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif
#endif  // defined(USE_AURA)

namespace app_list {

namespace {

void (*g_next_paint_callback)();

// The margin from the edge to the speech UI.
const int kSpeechUIMargin = 12;

// The vertical position for the appearing animation of the speech UI.
const float kSpeechUIApearingPosition =12;

// The distance between the arrow tip and edge of the anchor view.
const int kArrowOffset = 10;

#if defined(OS_LINUX)
// The WM_CLASS name for the app launcher window on Linux.
const char* kAppListWMClass = "chrome_app_list";
#endif

// Determines whether the current environment supports shadows bubble borders.
bool SupportsShadow() {
#if defined(USE_AURA) && defined(OS_WIN)
  // Shadows are not supported on Windows Aura without Aero Glass.
  if (!ui::win::IsAeroGlassEnabled() ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDwmComposition)) {
    return false;
  }
#elif defined(OS_LINUX) && !defined(USE_ASH)
  // Shadows are not supported on (non-ChromeOS) Linux.
  return false;
#endif
  return true;
}

}  // namespace

// An animation observer to hide the view at the end of the animation.
class HideViewAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  HideViewAnimationObserver() : target_(NULL) {
  }

  virtual ~HideViewAnimationObserver() {
    if (target_)
      StopObservingImplicitAnimations();
  }

  void SetTarget(views::View* target) {
    if (!target_)
      StopObservingImplicitAnimations();
    target_ = target;
  }

 private:
  // Overridden from ui::ImplicitAnimationObserver:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    if (target_) {
      target_->SetVisible(false);
      target_ = NULL;
    }
  }

  views::View* target_;

  DISALLOW_COPY_AND_ASSIGN(HideViewAnimationObserver);
};

////////////////////////////////////////////////////////////////////////////////
// AppListView:

AppListView::AppListView(AppListViewDelegate* delegate)
    : delegate_(delegate),
      app_list_main_view_(NULL),
      signin_view_(NULL),
      speech_view_(NULL),
      animation_observer_(new HideViewAnimationObserver()) {
  CHECK(delegate);

  delegate_->AddObserver(this);
  delegate_->GetSpeechUI()->AddObserver(this);
}

AppListView::~AppListView() {
  delegate_->GetSpeechUI()->RemoveObserver(this);
  delegate_->RemoveObserver(this);
  animation_observer_.reset();
  // Remove child views first to ensure no remaining dependencies on delegate_.
  RemoveAllChildViews(true);
}

void AppListView::InitAsBubbleAttachedToAnchor(
    gfx::NativeView parent,
    PaginationModel* pagination_model,
    views::View* anchor,
    const gfx::Vector2d& anchor_offset,
    views::BubbleBorder::Arrow arrow,
    bool border_accepts_events) {
  SetAnchorView(anchor);
  InitAsBubbleInternal(
      parent, pagination_model, arrow, border_accepts_events, anchor_offset);
}

void AppListView::InitAsBubbleAtFixedLocation(
    gfx::NativeView parent,
    PaginationModel* pagination_model,
    const gfx::Point& anchor_point_in_screen,
    views::BubbleBorder::Arrow arrow,
    bool border_accepts_events) {
  SetAnchorView(NULL);
  SetAnchorRect(gfx::Rect(anchor_point_in_screen, gfx::Size()));
  InitAsBubbleInternal(
      parent, pagination_model, arrow, border_accepts_events, gfx::Vector2d());
}

void AppListView::SetBubbleArrow(views::BubbleBorder::Arrow arrow) {
  GetBubbleFrameView()->bubble_border()->set_arrow(arrow);
  SizeToContents();  // Recalcuates with new border.
  GetBubbleFrameView()->SchedulePaint();
}

void AppListView::SetAnchorPoint(const gfx::Point& anchor_point) {
  SetAnchorRect(gfx::Rect(anchor_point, gfx::Size()));
}

void AppListView::SetDragAndDropHostOfCurrentAppList(
    ApplicationDragAndDropHost* drag_and_drop_host) {
  app_list_main_view_->SetDragAndDropHostOfCurrentAppList(drag_and_drop_host);
}

void AppListView::ShowWhenReady() {
  app_list_main_view_->ShowAppListWhenReady();
}

void AppListView::Close() {
  app_list_main_view_->Close();
  delegate_->Dismiss();
}

void AppListView::UpdateBounds() {
  SizeToContents();
}

gfx::Size AppListView::GetPreferredSize() {
  return app_list_main_view_->GetPreferredSize();
}

void AppListView::Paint(gfx::Canvas* canvas) {
  views::BubbleDelegateView::Paint(canvas);
  if (g_next_paint_callback) {
    g_next_paint_callback();
    g_next_paint_callback = NULL;
  }
}

bool AppListView::ShouldHandleSystemCommands() const {
  return true;
}

void AppListView::Prerender() {
  app_list_main_view_->Prerender();
}

void AppListView::OnProfilesChanged() {
  SigninDelegate* signin_delegate =
      delegate_ ? delegate_->GetSigninDelegate() : NULL;
  bool show_signin_view = signin_delegate && signin_delegate->NeedSignin();

  signin_view_->SetVisible(show_signin_view);
  app_list_main_view_->SetVisible(!show_signin_view);
  app_list_main_view_->search_box_view()->InvalidateMenu();
}

void AppListView::SetProfileByPath(const base::FilePath& profile_path) {
  delegate_->SetProfileByPath(profile_path);
  app_list_main_view_->ModelChanged();
}

void AppListView::AddObserver(AppListViewObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListView::RemoveObserver(AppListViewObserver* observer) {
  observers_.RemoveObserver(observer);
}

// static
void AppListView::SetNextPaintCallback(void (*callback)()) {
  g_next_paint_callback = callback;
}

#if defined(OS_WIN)
HWND AppListView::GetHWND() const {
#if defined(USE_AURA)
  gfx::NativeWindow window =
      GetWidget()->GetTopLevelWidget()->GetNativeWindow();
  return window->GetDispatcher()->host()->GetAcceleratedWidget();
#else
  return GetWidget()->GetTopLevelWidget()->GetNativeWindow();
#endif
}
#endif

void AppListView::InitAsBubbleInternal(gfx::NativeView parent,
                                       PaginationModel* pagination_model,
                                       views::BubbleBorder::Arrow arrow,
                                       bool border_accepts_events,
                                       const gfx::Vector2d& anchor_offset) {
  app_list_main_view_ = new AppListMainView(delegate_.get(),
                                            pagination_model,
                                            parent);
  AddChildView(app_list_main_view_);
#if defined(USE_AURA)
  app_list_main_view_->SetPaintToLayer(true);
  app_list_main_view_->SetFillsBoundsOpaquely(false);
  app_list_main_view_->layer()->SetMasksToBounds(true);
#endif

  signin_view_ =
      new SigninView(delegate_->GetSigninDelegate(),
                     app_list_main_view_->GetPreferredSize().width());
  AddChildView(signin_view_);

  // Speech recognition is available only when the start page exists.
  if (delegate_ && delegate_->GetStartPageContents()) {
    speech_view_ = new SpeechView(delegate_.get());
    speech_view_->SetVisible(false);
#if defined(USE_AURA)
    speech_view_->SetPaintToLayer(true);
    speech_view_->SetFillsBoundsOpaquely(false);
    speech_view_->layer()->SetOpacity(0.0f);
#endif
    AddChildView(speech_view_);
  }

  OnProfilesChanged();
  set_color(kContentsBackgroundColor);
  set_margins(gfx::Insets());
  set_move_with_anchor(true);
  set_parent_window(parent);
  set_close_on_deactivate(false);
  set_close_on_esc(false);
  set_anchor_view_insets(gfx::Insets(kArrowOffset + anchor_offset.y(),
                                     kArrowOffset + anchor_offset.x(),
                                     kArrowOffset - anchor_offset.y(),
                                     kArrowOffset - anchor_offset.x()));
  set_border_accepts_events(border_accepts_events);
  set_shadow(SupportsShadow() ? views::BubbleBorder::BIG_SHADOW
                              : views::BubbleBorder::NO_SHADOW_OPAQUE_BORDER);
  views::BubbleDelegateView::CreateBubble(this);
  SetBubbleArrow(arrow);

#if defined(USE_AURA)
  GetWidget()->GetNativeWindow()->layer()->SetMasksToBounds(true);
  GetBubbleFrameView()->set_background(new AppListBackground(
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius(),
      app_list_main_view_));
  set_background(NULL);
#else
  set_background(new AppListBackground(
      GetBubbleFrameView()->bubble_border()->GetBorderCornerRadius(),
      app_list_main_view_));

  // On non-aura the bubble has two widgets, and it's possible for the border
  // to be shown independently in odd situations. Explicitly hide the bubble
  // widget to ensure that any WM_WINDOWPOSCHANGED messages triggered by the
  // window manager do not have the SWP_SHOWWINDOW flag set which would cause
  // the border to be shown. See http://crbug.com/231687 .
  GetWidget()->Hide();
#endif
}

void AppListView::OnBeforeBubbleWidgetInit(
    views::Widget::InitParams* params,
    views::Widget* widget) const {
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  if (delegate_ && delegate_->ForceNativeDesktop())
    params->native_widget = new views::DesktopNativeWidgetAura(widget);
#endif
#if defined(OS_LINUX)
  // Set up a custom WM_CLASS for the app launcher window. This allows task
  // switchers in X11 environments to distinguish it from main browser windows.
  params->wm_class_name = kAppListWMClass;
#endif
}

views::View* AppListView::GetInitiallyFocusedView() {
  return app_list_main_view_->search_box_view()->search_box();
}

gfx::ImageSkia AppListView::GetWindowIcon() {
  if (delegate_)
    return delegate_->GetWindowIcon();

  return gfx::ImageSkia();
}

bool AppListView::WidgetHasHitTestMask() const {
  return true;
}

void AppListView::GetWidgetHitTestMask(gfx::Path* mask) const {
  DCHECK(mask);
  mask->addRect(gfx::RectToSkRect(
      GetBubbleFrameView()->GetContentsBounds()));
}

bool AppListView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  // The accelerator is added by BubbleDelegateView.
  if (accelerator.key_code() == ui::VKEY_ESCAPE) {
    if (app_list_main_view_->search_box_view()->HasSearch()) {
      app_list_main_view_->search_box_view()->ClearSearch();
    } else {
      GetWidget()->Deactivate();
      Close();
    }
    return true;
  }

  return false;
}

void AppListView::Layout() {
  const gfx::Rect contents_bounds = GetContentsBounds();
  app_list_main_view_->SetBoundsRect(contents_bounds);
  signin_view_->SetBoundsRect(contents_bounds);

  if (speech_view_) {
    gfx::Rect speech_bounds = contents_bounds;
    int preferred_height = speech_view_->GetPreferredSize().height();
    speech_bounds.Inset(kSpeechUIMargin, kSpeechUIMargin);
    speech_bounds.set_height(std::min(speech_bounds.height(),
                                      preferred_height));
    speech_bounds.Inset(-speech_view_->GetInsets());
    speech_view_->SetBoundsRect(speech_bounds);
  }
}

void AppListView::OnWidgetDestroying(views::Widget* widget) {
  BubbleDelegateView::OnWidgetDestroying(widget);
  if (delegate_ && widget == GetWidget())
    delegate_->ViewClosing();
}

void AppListView::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  // Do not called inherited function as the bubble delegate auto close
  // functionality is not used.
  if (widget == GetWidget())
    FOR_EACH_OBSERVER(AppListViewObserver, observers_,
                      OnActivationChanged(widget, active));
}

void AppListView::OnWidgetVisibilityChanged(views::Widget* widget,
                                            bool visible) {
  BubbleDelegateView::OnWidgetVisibilityChanged(widget, visible);

  if (widget != GetWidget())
    return;

  // We clear the search when hiding so the next time the app list appears it is
  // not showing search results.
  if (!visible)
    app_list_main_view_->search_box_view()->ClearSearch();

  // Whether we need to signin or not may have changed since last time we were
  // shown.
  Layout();
}

void AppListView::OnSpeechRecognitionStateChanged(
    SpeechRecognitionState new_state) {
  DCHECK(!signin_view_->visible());

  bool recognizing = new_state != SPEECH_RECOGNITION_NOT_STARTED;
  // No change for this class.
  if (speech_view_->visible() == recognizing)
    return;

  if (recognizing)
    speech_view_->Reset();

#if defined(USE_AURA)
  gfx::Transform speech_transform;
  speech_transform.Translate(
      0, SkFloatToMScalar(kSpeechUIApearingPosition));
  if (recognizing)
    speech_view_->layer()->SetTransform(speech_transform);

  {
    ui::ScopedLayerAnimationSettings main_settings(
        app_list_main_view_->layer()->GetAnimator());
    if (recognizing) {
      animation_observer_->SetTarget(app_list_main_view_);
      main_settings.AddObserver(animation_observer_.get());
    }
    app_list_main_view_->layer()->SetOpacity(recognizing ? 0.0f : 1.0f);
  }

  {
    ui::ScopedLayerAnimationSettings speech_settings(
        speech_view_->layer()->GetAnimator());
    if (!recognizing) {
      animation_observer_->SetTarget(speech_view_);
      speech_settings.AddObserver(animation_observer_.get());
    }

    speech_view_->layer()->SetOpacity(recognizing ? 1.0f : 0.0f);
    if (recognizing)
      speech_view_->layer()->SetTransform(gfx::Transform());
    else
      speech_view_->layer()->SetTransform(speech_transform);
  }

  if (recognizing)
    speech_view_->SetVisible(true);
  else
    app_list_main_view_->SetVisible(true);
#else
  speech_view_->SetVisible(recognizing);
  app_list_main_view_->SetVisible(!recognizing);
#endif

  // Needs to schedule paint of AppListView itself, to repaint the background.
  SchedulePaint();
}

}  // namespace app_list
