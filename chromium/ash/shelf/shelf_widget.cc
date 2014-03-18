// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_widget.h"

#include "ash/ash_switches.h"
#include "ash/focus_cycler.h"
#include "ash/root_window_controller.h"
#include "ash/session_state_delegate.h"
#include "ash/shelf/shelf_delegate.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_navigator.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/status_area_layout_manager.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/workspace_controller.h"
#include "grit/ash_resources.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
// Size of black border at bottom (or side) of launcher.
const int kNumBlackPixels = 3;
// Alpha to paint dimming image with.
const int kDimAlpha = 128;

// The time to dim and un-dim.
const int kTimeToDimMs = 3000;  // Slow in dimming.
const int kTimeToUnDimMs = 200;  // Fast in activating.

// Class used to slightly dim shelf items when maximized and visible.
class DimmerView : public views::View,
                   public views::WidgetDelegate,
                   ash::internal::BackgroundAnimatorDelegate {
 public:
  // If |disable_dimming_animations_for_test| is set, all alpha animations will
  // be performed instantly.
  DimmerView(ash::ShelfWidget* shelf_widget,
             bool disable_dimming_animations_for_test);
  virtual ~DimmerView();

  // Called by |DimmerEventFilter| when the mouse |hovered| state changes.
  void SetHovered(bool hovered);

  // Force the dimmer to be undimmed.
  void ForceUndimming(bool force);

  // views::WidgetDelegate overrides:
  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }

  // ash::internal::BackgroundAnimatorDelegate overrides:
  virtual void UpdateBackground(int alpha) OVERRIDE {
    alpha_ = alpha;
    SchedulePaint();
  }

  // views::View overrides:
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  // A function to test the current alpha used.
  int get_dimming_alpha_for_test() { return alpha_; }

 private:
  // This class monitors mouse events to see if it is on top of the launcher.
  class DimmerEventFilter : public ui::EventHandler {
   public:
    explicit DimmerEventFilter(DimmerView* owner);
    virtual ~DimmerEventFilter();

    // Overridden from ui::EventHandler:
    virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
    virtual void OnTouchEvent(ui::TouchEvent* event) OVERRIDE;

   private:
    // The owning class.
    DimmerView* owner_;

    // TRUE if the mouse is inside the shelf.
    bool mouse_inside_;

    // TRUE if a touch event is inside the shelf.
    bool touch_inside_;

    DISALLOW_COPY_AND_ASSIGN(DimmerEventFilter);
  };

  // The owning shelf.
  ash::ShelfWidget* shelf_;

  // The alpha to use for covering the shelf.
  int alpha_;

  // True if the event filter claims that we should not be dimmed.
  bool is_hovered_;

  // True if someone forces us not to be dimmed (e.g. a menu is open).
  bool force_hovered_;

  // True if animations should be suppressed for a test.
  bool disable_dimming_animations_for_test_;

  // The animator for the background transitions.
  ash::internal::BackgroundAnimator background_animator_;

  // Notification of entering / exiting of the shelf area by mouse.
  scoped_ptr<DimmerEventFilter> event_filter_;

  DISALLOW_COPY_AND_ASSIGN(DimmerView);
};

DimmerView::DimmerView(ash::ShelfWidget* shelf_widget,
                       bool disable_dimming_animations_for_test)
    : shelf_(shelf_widget),
      alpha_(kDimAlpha),
      is_hovered_(false),
      force_hovered_(false),
      disable_dimming_animations_for_test_(disable_dimming_animations_for_test),
      background_animator_(this, 0, kDimAlpha) {
  event_filter_.reset(new DimmerEventFilter(this));
  // Make sure it is undimmed at the beginning and then fire off the dimming
  // animation.
  background_animator_.SetPaintsBackground(false,
                                           ash::BACKGROUND_CHANGE_IMMEDIATE);
  SetHovered(false);
}

DimmerView::~DimmerView() {
}

void DimmerView::SetHovered(bool hovered) {
  // Remember the hovered state so that we can correct the state once a
  // possible force state has disappeared.
  is_hovered_ = hovered;
  // Undimm also if we were forced to by e.g. an open menu.
  hovered |= force_hovered_;
  background_animator_.SetDuration(hovered ? kTimeToUnDimMs : kTimeToDimMs);
  background_animator_.SetPaintsBackground(!hovered,
      disable_dimming_animations_for_test_ ?
          ash::BACKGROUND_CHANGE_IMMEDIATE : ash::BACKGROUND_CHANGE_ANIMATE);
}

void DimmerView::ForceUndimming(bool force) {
  bool previous = force_hovered_;
  force_hovered_ = force;
  // If the forced change does change the result we apply the change.
  if (is_hovered_ || force_hovered_ != is_hovered_ || previous)
    SetHovered(is_hovered_);
}

void DimmerView::OnPaintBackground(gfx::Canvas* canvas) {
  SkPaint paint;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia launcher_background =
      *rb.GetImageNamed(IDR_AURA_LAUNCHER_DIMMING).ToImageSkia();

  if (shelf_->GetAlignment() != ash::SHELF_ALIGNMENT_BOTTOM) {
    launcher_background = gfx::ImageSkiaOperations::CreateRotatedImage(
        launcher_background,
        shelf_->shelf_layout_manager()->SelectValueForShelfAlignment(
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_270_CW,
            SkBitmapOperations::ROTATION_180_CW));
  }
  paint.setAlpha(alpha_);
  canvas->DrawImageInt(
      launcher_background,
      0, 0, launcher_background.width(), launcher_background.height(),
      0, 0, width(), height(),
      false,
      paint);
}

DimmerView::DimmerEventFilter::DimmerEventFilter(DimmerView* owner)
    : owner_(owner),
      mouse_inside_(false),
      touch_inside_(false) {
  ash::Shell::GetInstance()->AddPreTargetHandler(this);
}

DimmerView::DimmerEventFilter::~DimmerEventFilter() {
  ash::Shell::GetInstance()->RemovePreTargetHandler(this);
}

void DimmerView::DimmerEventFilter::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_MOVED &&
      event->type() != ui::ET_MOUSE_DRAGGED)
    return;
  bool inside = owner_->GetBoundsInScreen().Contains(event->root_location());
  if (mouse_inside_ || touch_inside_ != inside || touch_inside_)
    owner_->SetHovered(inside || touch_inside_);
  mouse_inside_ = inside;
}

void DimmerView::DimmerEventFilter::OnTouchEvent(ui::TouchEvent* event) {
  bool touch_inside = false;
  if (event->type() != ui::ET_TOUCH_RELEASED &&
      event->type() != ui::ET_TOUCH_CANCELLED)
    touch_inside = owner_->GetBoundsInScreen().Contains(event->root_location());

  if (mouse_inside_ || touch_inside_ != mouse_inside_ || touch_inside)
    owner_->SetHovered(mouse_inside_ || touch_inside);
  touch_inside_ = touch_inside;
}

}  // namespace

namespace ash {

// The contents view of the Shelf. This view contains ShelfView and
// sizes it to the width of the shelf minus the size of the status area.
class ShelfWidget::DelegateView : public views::WidgetDelegate,
                                  public views::AccessiblePaneView,
                                  public internal::BackgroundAnimatorDelegate,
                                  public aura::WindowObserver {
 public:
  explicit DelegateView(ShelfWidget* shelf);
  virtual ~DelegateView();

  void set_focus_cycler(internal::FocusCycler* focus_cycler) {
    focus_cycler_ = focus_cycler;
  }
  internal::FocusCycler* focus_cycler() {
    return focus_cycler_;
  }

  ui::Layer* opaque_background() { return &opaque_background_; }

  // Set if the shelf area is dimmed (eg when a window is maximized).
  void SetDimmed(bool dimmed);
  bool GetDimmed() const;

  void SetParentLayer(ui::Layer* layer);

  // views::View overrides:
  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE;

  // views::WidgetDelegateView overrides:
  virtual views::Widget* GetWidget() OVERRIDE {
    return View::GetWidget();
  }
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return View::GetWidget();
  }

  virtual bool CanActivate() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void ReorderChildLayers(ui::Layer* parent_layer) OVERRIDE;
  // This will be called when the parent local bounds change.
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds) OVERRIDE;

  // aura::WindowObserver overrides:
  // This will be called when the shelf itself changes its absolute position.
  // Since the |dimmer_| panel needs to be placed in screen coordinates it needs
  // to be repositioned. The difference to the OnBoundsChanged call above is
  // that this gets also triggered when the shelf only moves.
  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // BackgroundAnimatorDelegate overrides:
  virtual void UpdateBackground(int alpha) OVERRIDE;

  // Force the shelf to be presented in an undimmed state.
  void ForceUndimming(bool force);

  // A function to test the current alpha used by the dimming bar. If there is
  // no dimmer active, the function will return -1.
  int GetDimmingAlphaForTest();

  // A function to test the bounds of the dimming bar. Returns gfx::Rect() if
  // the dimmer is inactive.
  gfx::Rect GetDimmerBoundsForTest();

  // Disable dimming animations for running tests. This needs to be called
  // prior to the creation of of the |dimmer_|.
  void disable_dimming_animations_for_test() {
    disable_dimming_animations_for_test_ = true;
  }

 private:
  ShelfWidget* shelf_;
  scoped_ptr<views::Widget> dimmer_;
  internal::FocusCycler* focus_cycler_;
  int alpha_;
  ui::Layer opaque_background_;

  // The view which does the dimming.
  DimmerView* dimmer_view_;

  // True if dimming animations should be turned off.
  bool disable_dimming_animations_for_test_;

  DISALLOW_COPY_AND_ASSIGN(DelegateView);
};

ShelfWidget::DelegateView::DelegateView(ShelfWidget* shelf)
    : shelf_(shelf),
      focus_cycler_(NULL),
      alpha_(0),
      opaque_background_(ui::LAYER_SOLID_COLOR),
      dimmer_view_(NULL),
      disable_dimming_animations_for_test_(false) {
  set_allow_deactivate_on_esc(true);
  opaque_background_.SetColor(SK_ColorBLACK);
  opaque_background_.SetBounds(GetLocalBounds());
  opaque_background_.SetOpacity(0.0f);
}

ShelfWidget::DelegateView::~DelegateView() {
  // Make sure that the dimmer goes away since it might have set an observer.
  SetDimmed(false);
}

void ShelfWidget::DelegateView::SetDimmed(bool value) {
  if (value == (dimmer_.get() != NULL))
    return;

  if (value) {
    dimmer_.reset(new views::Widget);
    views::Widget::InitParams params(
        views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
    params.can_activate = false;
    params.accept_events = false;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = shelf_->GetNativeView();
    dimmer_->Init(params);
    dimmer_->GetNativeWindow()->SetName("ShelfDimmer");
    dimmer_->SetBounds(shelf_->GetWindowBoundsInScreen());
    // The launcher should not take focus when it is initially shown.
    dimmer_->set_focus_on_creation(false);
    dimmer_view_ = new DimmerView(shelf_, disable_dimming_animations_for_test_);
    dimmer_->SetContentsView(dimmer_view_);
    dimmer_->GetNativeView()->SetName("ShelfDimmerView");
    dimmer_->Show();
    shelf_->GetNativeView()->AddObserver(this);
  } else {
    // Some unit tests will come here with a destroyed window.
    if (shelf_->GetNativeView())
      shelf_->GetNativeView()->RemoveObserver(this);
    dimmer_view_ = NULL;
    dimmer_.reset(NULL);
  }
}

bool ShelfWidget::DelegateView::GetDimmed() const {
  return dimmer_.get() && dimmer_->IsVisible();
}

void ShelfWidget::DelegateView::SetParentLayer(ui::Layer* layer) {
  layer->Add(&opaque_background_);
  ReorderLayers();
}

void ShelfWidget::DelegateView::OnPaintBackground(gfx::Canvas* canvas) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::ImageSkia launcher_background =
      *rb.GetImageSkiaNamed(IDR_AURA_LAUNCHER_BACKGROUND);
  if (SHELF_ALIGNMENT_BOTTOM != shelf_->GetAlignment())
    launcher_background = gfx::ImageSkiaOperations::CreateRotatedImage(
        launcher_background,
        shelf_->shelf_layout_manager()->SelectValueForShelfAlignment(
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_90_CW,
            SkBitmapOperations::ROTATION_270_CW,
            SkBitmapOperations::ROTATION_180_CW));
  const gfx::Rect dock_bounds(shelf_->shelf_layout_manager()->dock_bounds());
  SkPaint paint;
  paint.setAlpha(alpha_);
  canvas->DrawImageInt(
      launcher_background,
      0, 0, launcher_background.width(), launcher_background.height(),
      (SHELF_ALIGNMENT_BOTTOM == shelf_->GetAlignment() &&
       dock_bounds.x() == 0 && dock_bounds.width() > 0) ?
           dock_bounds.width() : 0, 0,
      SHELF_ALIGNMENT_BOTTOM == shelf_->GetAlignment() ?
          width() - dock_bounds.width() : width(), height(),
      false,
      paint);
  if (SHELF_ALIGNMENT_BOTTOM == shelf_->GetAlignment() &&
      dock_bounds.width() > 0) {
    // The part of the shelf background that is in the corner below the docked
    // windows close to the work area is an arched gradient that blends
    // vertically oriented docked background and horizontal shelf.
    gfx::ImageSkia launcher_corner =
        *rb.GetImageSkiaNamed(IDR_AURA_LAUNCHER_CORNER);
    if (dock_bounds.x() == 0) {
      launcher_corner = gfx::ImageSkiaOperations::CreateRotatedImage(
          launcher_corner, SkBitmapOperations::ROTATION_90_CW);
    }
    canvas->DrawImageInt(
        launcher_corner,
        0, 0, launcher_corner.width(), launcher_corner.height(),
        dock_bounds.x() > 0 ? dock_bounds.x() : dock_bounds.width() - height(),
        0,
        height(), height(),
        false,
        paint);
    // The part of the shelf background that is just below the docked windows
    // is drawn using the last (lowest) 1-pixel tall strip of the image asset.
    // This avoids showing the border 3D shadow between the shelf and the dock.
    canvas->DrawImageInt(
        launcher_background,
        0, launcher_background.height() - 1, launcher_background.width(), 1,
        dock_bounds.x() > 0 ? dock_bounds.x() + height() : 0, 0,
        dock_bounds.width() - height(), height(),
        false,
        paint);
  }
  gfx::Rect black_rect =
      shelf_->shelf_layout_manager()->SelectValueForShelfAlignment(
          gfx::Rect(0, height() - kNumBlackPixels, width(), kNumBlackPixels),
          gfx::Rect(0, 0, kNumBlackPixels, height()),
          gfx::Rect(width() - kNumBlackPixels, 0, kNumBlackPixels, height()),
          gfx::Rect(0, 0, width(), kNumBlackPixels));
  canvas->FillRect(black_rect, SK_ColorBLACK);
}

bool ShelfWidget::DelegateView::CanActivate() const {
  // Allow to activate as fallback.
  if (shelf_->activating_as_fallback_)
    return true;
  // Allow to activate from the focus cycler.
  if (focus_cycler_ && focus_cycler_->widget_activating() == GetWidget())
    return true;
  // Disallow activating in other cases, especially when using mouse.
  return false;
}

void ShelfWidget::DelegateView::Layout() {
  for(int i = 0; i < child_count(); ++i) {
    if (shelf_->shelf_layout_manager()->IsHorizontalAlignment()) {
      child_at(i)->SetBounds(child_at(i)->x(), child_at(i)->y(),
                             child_at(i)->width(), height());
    } else {
      child_at(i)->SetBounds(child_at(i)->x(), child_at(i)->y(),
                             width(), child_at(i)->height());
    }
  }
}

void ShelfWidget::DelegateView::ReorderChildLayers(ui::Layer* parent_layer) {
  views::View::ReorderChildLayers(parent_layer);
  parent_layer->StackAtBottom(&opaque_background_);
}

void ShelfWidget::DelegateView::OnBoundsChanged(const gfx::Rect& old_bounds) {
  opaque_background_.SetBounds(GetLocalBounds());
  if (dimmer_)
    dimmer_->SetBounds(GetBoundsInScreen());
}

void ShelfWidget::DelegateView::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds) {
  // Coming here the shelf got repositioned and since the |dimmer_| is placed
  // in screen coordinates and not relative to the parent it needs to be
  // repositioned accordingly.
  dimmer_->SetBounds(GetBoundsInScreen());
}

void ShelfWidget::DelegateView::ForceUndimming(bool force) {
  if (GetDimmed())
    dimmer_view_->ForceUndimming(force);
}

int ShelfWidget::DelegateView::GetDimmingAlphaForTest() {
  if (GetDimmed())
    return dimmer_view_->get_dimming_alpha_for_test();
  return -1;
}

gfx::Rect ShelfWidget::DelegateView::GetDimmerBoundsForTest() {
  if (GetDimmed())
    return dimmer_view_->GetBoundsInScreen();
  return gfx::Rect();
}

void ShelfWidget::DelegateView::UpdateBackground(int alpha) {
  alpha_ = alpha;
  SchedulePaint();
}

ShelfWidget::ShelfWidget(aura::Window* shelf_container,
                         aura::Window* status_container,
                         internal::WorkspaceController* workspace_controller)
    : delegate_view_(new DelegateView(this)),
      background_animator_(delegate_view_, 0, kLauncherBackgroundAlpha),
      activating_as_fallback_(false),
      window_container_(shelf_container) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = shelf_container;
  params.delegate = delegate_view_;
  Init(params);

  // The shelf should not take focus when initially shown.
  set_focus_on_creation(false);
  SetContentsView(delegate_view_);
  delegate_view_->SetParentLayer(GetLayer());

  status_area_widget_ = new internal::StatusAreaWidget(status_container);
  status_area_widget_->CreateTrayViews();
  if (Shell::GetInstance()->session_state_delegate()->
          IsActiveUserSessionStarted()) {
    status_area_widget_->Show();
  }
  Shell::GetInstance()->focus_cycler()->AddWidget(status_area_widget_);

  shelf_layout_manager_ = new internal::ShelfLayoutManager(this);
  shelf_layout_manager_->AddObserver(this);
  shelf_container->SetLayoutManager(shelf_layout_manager_);
  shelf_layout_manager_->set_workspace_controller(workspace_controller);
  workspace_controller->SetShelf(shelf_layout_manager_);

  status_container->SetLayoutManager(
      new internal::StatusAreaLayoutManager(this));

  views::Widget::AddObserver(this);
}

ShelfWidget::~ShelfWidget() {
  RemoveObserver(this);
}

void ShelfWidget::SetPaintsBackground(
    ShelfBackgroundType background_type,
    BackgroundAnimatorChangeType change_type) {
  ui::Layer* opaque_background = delegate_view_->opaque_background();
  float target_opacity =
      (background_type == SHELF_BACKGROUND_MAXIMIZED) ? 1.0f : 0.0f;
  scoped_ptr<ui::ScopedLayerAnimationSettings> opaque_background_animation;
  if (change_type != BACKGROUND_CHANGE_IMMEDIATE) {
    opaque_background_animation.reset(new ui::ScopedLayerAnimationSettings(
        opaque_background->GetAnimator()));
    opaque_background_animation->SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kTimeToSwitchBackgroundMs));
  }
  opaque_background->SetOpacity(target_opacity);

  // TODO(mukai): use ui::Layer on both opaque_background and normal background
  // retire background_animator_ at all. It would be simpler.
  // See also DockedBackgroundWidget::SetPaintsBackground.
  background_animator_.SetPaintsBackground(
      background_type != SHELF_BACKGROUND_DEFAULT,
      change_type);
  delegate_view_->SchedulePaint();
}

ShelfBackgroundType ShelfWidget::GetBackgroundType() const {
  if (delegate_view_->opaque_background()->GetTargetOpacity() == 1.0f)
    return SHELF_BACKGROUND_MAXIMIZED;
  if (background_animator_.paints_background())
    return SHELF_BACKGROUND_OVERLAP;

  return SHELF_BACKGROUND_DEFAULT;
}

// static
bool ShelfWidget::ShelfAlignmentAllowed() {
  if (!ash::switches::ShowShelfAlignmentMenu())
    return false;
  user::LoginStatus login_status =
      Shell::GetInstance()->system_tray_delegate()->GetUserLoginStatus();

  switch (login_status) {
    case user::LOGGED_IN_USER:
    case user::LOGGED_IN_OWNER:
      return true;
    case user::LOGGED_IN_LOCKED:
    case user::LOGGED_IN_PUBLIC:
    case user::LOGGED_IN_LOCALLY_MANAGED:
    case user::LOGGED_IN_GUEST:
    case user::LOGGED_IN_RETAIL_MODE:
    case user::LOGGED_IN_KIOSK_APP:
    case user::LOGGED_IN_NONE:
      return false;
  }

  DCHECK(false);
  return false;
}

ShelfAlignment ShelfWidget::GetAlignment() const {
  return shelf_layout_manager_->GetAlignment();
}

void ShelfWidget::SetAlignment(ShelfAlignment alignment) {
  if (launcher_)
    launcher_->SetAlignment(alignment);
  status_area_widget_->SetShelfAlignment(alignment);
  delegate_view_->SchedulePaint();
}

void ShelfWidget::SetDimsShelf(bool dimming) {
  delegate_view_->SetDimmed(dimming);
  // Repaint all children, allowing updates to reflect dimmed state eg:
  // status area background, app list button and overflow button.
  if (launcher_)
    launcher_->SchedulePaint();
  status_area_widget_->GetContentsView()->SchedulePaint();
}

bool ShelfWidget::GetDimsShelf() const {
  return delegate_view_->GetDimmed();
}

void ShelfWidget::CreateLauncher() {
  if (launcher_)
    return;

  Shell* shell = Shell::GetInstance();
  // This needs to be called before shelf_model().
  ShelfDelegate* shelf_delegate = shell->GetShelfDelegate();
  if (!shelf_delegate)
    return;  // Not ready to create Launcher

  launcher_.reset(new Launcher(shell->shelf_model(),
                               shell->GetShelfDelegate(),
                               this));
  SetFocusCycler(shell->focus_cycler());

  // Inform the root window controller.
  internal::RootWindowController::ForWindow(window_container_)->
      OnLauncherCreated();

  launcher_->SetVisible(
      shell->session_state_delegate()->IsActiveUserSessionStarted());
  shelf_layout_manager_->LayoutShelf();
  Show();
}

bool ShelfWidget::IsLauncherVisible() const {
  return launcher_.get() && launcher_->IsVisible();
}

void ShelfWidget::SetLauncherVisibility(bool visible) {
  if (launcher_)
    launcher_->SetVisible(visible);
}

void ShelfWidget::SetFocusCycler(internal::FocusCycler* focus_cycler) {
  delegate_view_->set_focus_cycler(focus_cycler);
  if (focus_cycler)
    focus_cycler->AddWidget(this);
}

internal::FocusCycler* ShelfWidget::GetFocusCycler() {
  return delegate_view_->focus_cycler();
}

void ShelfWidget::ShutdownStatusAreaWidget() {
  if (status_area_widget_)
    status_area_widget_->Shutdown();
  status_area_widget_ = NULL;
}

void ShelfWidget::ForceUndimming(bool force) {
  delegate_view_->ForceUndimming(force);
}

void ShelfWidget::OnWidgetActivationChanged(views::Widget* widget,
                                            bool active) {
  activating_as_fallback_ = false;
  if (active)
    delegate_view_->SetPaneFocusAndFocusDefault();
  else
    delegate_view_->GetFocusManager()->ClearFocus();
}

int ShelfWidget::GetDimmingAlphaForTest() {
  if (delegate_view_)
    return delegate_view_->GetDimmingAlphaForTest();
  return -1;
}

gfx::Rect ShelfWidget::GetDimmerBoundsForTest() {
  if (delegate_view_)
    return delegate_view_->GetDimmerBoundsForTest();
  return gfx::Rect();
}

void ShelfWidget::DisableDimmingAnimationsForTest() {
  DCHECK(delegate_view_);
  return delegate_view_->disable_dimming_animations_for_test();
}

void ShelfWidget::WillDeleteShelf() {
  shelf_layout_manager_->RemoveObserver(this);
  shelf_layout_manager_ = NULL;
}

}  // namespace ash
