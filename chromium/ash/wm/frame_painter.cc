// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"

#include <vector>

#include "ash/ash_constants.h"
#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/caption_buttons/frame_caption_button_container_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"  // DCHECK
#include "grit/ash_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using aura::RootWindow;
using aura::Window;
using views::Widget;

namespace {
// TODO(jamescook): Border is specified to be a single pixel overlapping
// the web content and may need to be built into the shadow layers instead.
const int kBorderThickness = 0;
// Space between left edge of window and popup window icon.
const int kIconOffsetX = 9;
// Height and width of window icon.
const int kIconSize = 16;
// Space between the title text and the caption buttons.
const int kTitleLogoSpacing = 5;
// Space between window icon and title text.
const int kTitleIconOffsetX = 5;
// Space between window edge and title text, when there is no icon.
const int kTitleNoIconOffsetX = 8;
// Color for the non-maximized window title text.
const SkColor kNonMaximizedWindowTitleTextColor = SkColorSetRGB(40, 40, 40);
// Color for the maximized window title text.
const SkColor kMaximizedWindowTitleTextColor = SK_ColorWHITE;
// Size of header/content separator line below the header image.
const int kHeaderContentSeparatorSize = 1;
// Color of header bottom edge line.
const SkColor kHeaderContentSeparatorColor = SkColorSetRGB(128, 128, 128);
// In the pre-Ash era the web content area had a frame along the left edge, so
// user-generated theme images for the new tab page assume they are shifted
// right relative to the header.  Now that we have removed the left edge frame
// we need to copy the theme image for the window header from a few pixels
// inset to preserve alignment with the NTP image, or else we'll break a bunch
// of existing themes.  We do something similar on OS X for the same reason.
const int kThemeFrameImageInsetX = 5;
// Duration of crossfade animation for activating and deactivating frame.
const int kActivationCrossfadeDurationMs = 200;
// Alpha/opacity value for fully-opaque headers.
const int kFullyOpaque = 255;

// A flag to enable/disable solo window header.
bool solo_window_header_enabled = true;

// Tiles an image into an area, rounding the top corners. Samples |image|
// starting |image_inset_x| pixels from the left of the image.
void TileRoundRect(gfx::Canvas* canvas,
                   const gfx::ImageSkia& image,
                   const SkPaint& paint,
                   const gfx::Rect& bounds,
                   int top_left_corner_radius,
                   int top_right_corner_radius,
                   int image_inset_x) {
  SkRect rect = gfx::RectToSkRect(bounds);
  const SkScalar kTopLeftRadius = SkIntToScalar(top_left_corner_radius);
  const SkScalar kTopRightRadius = SkIntToScalar(top_right_corner_radius);
  SkScalar radii[8] = {
      kTopLeftRadius, kTopLeftRadius,  // top-left
      kTopRightRadius, kTopRightRadius,  // top-right
      0, 0,   // bottom-right
      0, 0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);
  canvas->DrawImageInPath(image, -image_inset_x, 0, path, paint);
}

// Tiles |frame_image| and |frame_overlay_image| into an area, rounding the top
// corners.
void PaintFrameImagesInRoundRect(gfx::Canvas* canvas,
                                 const gfx::ImageSkia* frame_image,
                                 const gfx::ImageSkia* frame_overlay_image,
                                 const SkPaint& paint,
                                 const gfx::Rect& bounds,
                                 int corner_radius,
                                 int image_inset_x) {
  SkXfermode::Mode normal_mode;
  SkXfermode::AsMode(NULL, &normal_mode);

  // If |paint| is using an unusual SkXfermode::Mode (this is the case while
  // crossfading), we must create a new canvas to overlay |frame_image| and
  // |frame_overlay_image| using |normal_mode| and then paint the result
  // using the unusual mode. We try to avoid this because creating a new
  // browser-width canvas is expensive.
  bool fast_path = (!frame_overlay_image ||
      SkXfermode::IsMode(paint.getXfermode(), normal_mode));
  if (fast_path) {
    TileRoundRect(canvas, *frame_image, paint, bounds, corner_radius,
        corner_radius, image_inset_x);

    if (frame_overlay_image) {
      // Adjust |bounds| such that |frame_overlay_image| is not tiled.
      gfx::Rect overlay_bounds = bounds;
      overlay_bounds.Intersect(
          gfx::Rect(bounds.origin(), frame_overlay_image->size()));
      int top_left_corner_radius = corner_radius;
      int top_right_corner_radius = corner_radius;
      if (overlay_bounds.width() < bounds.width() - corner_radius)
        top_right_corner_radius = 0;
      TileRoundRect(canvas, *frame_overlay_image, paint, overlay_bounds,
          top_left_corner_radius, top_right_corner_radius, 0);
    }
  } else {
    gfx::Canvas temporary_canvas(bounds.size(), canvas->scale_factor(), false);
    temporary_canvas.TileImageInt(*frame_image,
                                  image_inset_x, 0,
                                  0, 0,
                                  bounds.width(), bounds.height());
    temporary_canvas.DrawImageInt(*frame_overlay_image, 0, 0);
    TileRoundRect(canvas, gfx::ImageSkia(temporary_canvas.ExtractImageRep()),
        paint, bounds, corner_radius, corner_radius, 0);
  }
}

// Returns true if |child| and all ancestors are visible. Useful to ensure that
// a window is individually visible and is not part of a hidden workspace.
bool IsVisibleToRoot(Window* child) {
  for (Window* window = child; window; window = window->parent()) {
    // We must use TargetVisibility() because windows animate in and out and
    // IsVisible() also tracks the layer visibility state.
    if (!window->TargetVisibility())
      return false;
  }
  return true;
}

// Returns true if |window| is a "normal" window for purposes of solo window
// computations. Returns false for windows that are:
// * Not drawn (for example, DragDropTracker uses one for mouse capture)
// * Modal alerts (it looks odd for headers to change when an alert opens)
// * Constrained windows (ditto)
bool IsSoloWindowHeaderCandidate(aura::Window* window) {
  return window &&
      window->type() == aura::client::WINDOW_TYPE_NORMAL &&
      window->layer() &&
      window->layer()->type() != ui::LAYER_NOT_DRAWN &&
      window->GetProperty(aura::client::kModalKey) == ui::MODAL_TYPE_NONE &&
      !window->GetProperty(ash::kConstrainedWindowKey);
}

// Returns a list of windows in |root_window|| that potentially could have
// a transparent solo-window header.
std::vector<Window*> GetWindowsForSoloHeaderUpdate(RootWindow* root_window) {
  std::vector<Window*> windows;
  // During shutdown there may not be a workspace controller. In that case
  // we don't care about updating any windows.
  // Avoid memory allocations for typical window counts.
  windows.reserve(16);
  // Collect windows from the desktop.
  Window* desktop = ash::Shell::GetContainer(
      root_window, ash::internal::kShellWindowId_DefaultContainer);
  windows.insert(windows.end(),
                 desktop->children().begin(),
                 desktop->children().end());
  // Collect "always on top" windows.
  Window* top_container =
      ash::Shell::GetContainer(
          root_window, ash::internal::kShellWindowId_AlwaysOnTopContainer);
  windows.insert(windows.end(),
                 top_container->children().begin(),
                 top_container->children().end());
  return windows;
}
}  // namespace

namespace ash {

// static
int FramePainter::kActiveWindowOpacity = 255;  // 1.0
int FramePainter::kInactiveWindowOpacity = 255;  // 1.0
int FramePainter::kSoloWindowOpacity = 77;  // 0.3

///////////////////////////////////////////////////////////////////////////////
// FramePainter, public:

FramePainter::FramePainter()
    : frame_(NULL),
      window_icon_(NULL),
      caption_button_container_(NULL),
      window_(NULL),
      top_left_corner_(NULL),
      top_edge_(NULL),
      top_right_corner_(NULL),
      header_left_edge_(NULL),
      header_right_edge_(NULL),
      previous_theme_frame_id_(0),
      previous_theme_frame_overlay_id_(0),
      previous_opacity_(0),
      crossfade_theme_frame_id_(0),
      crossfade_theme_frame_overlay_id_(0),
      crossfade_opacity_(0) {}

FramePainter::~FramePainter() {
  // Sometimes we are destroyed before the window closes, so ensure we clean up.
  if (window_) {
    window_->RemoveObserver(this);
    wm::GetWindowState(window_)->RemoveObserver(this);
  }
}

void FramePainter::Init(
    views::Widget* frame,
    views::View* window_icon,
    FrameCaptionButtonContainerView* caption_button_container) {
  DCHECK(frame);
  // window_icon may be NULL.
  DCHECK(caption_button_container);
  frame_ = frame;
  window_icon_ = window_icon;
  caption_button_container_ = caption_button_container;

  // Window frame image parts.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  top_left_corner_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_LEFT).ToImageSkia();
  top_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP).ToImageSkia();
  top_right_corner_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_RIGHT).ToImageSkia();
  header_left_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_LEFT).ToImageSkia();
  header_right_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_RIGHT).ToImageSkia();

  window_ = frame->GetNativeWindow();
  gfx::Insets mouse_insets = gfx::Insets(-kResizeOutsideBoundsSize,
                                         -kResizeOutsideBoundsSize,
                                         -kResizeOutsideBoundsSize,
                                         -kResizeOutsideBoundsSize);
  gfx::Insets touch_insets = mouse_insets.Scale(
      kResizeOutsideBoundsScaleForTouch);
  // Ensure we get resize cursors for a few pixels outside our bounds.
  window_->SetHitTestBoundsOverrideOuter(mouse_insets, touch_insets);
  // Ensure we get resize cursors just inside our bounds as well.
  window_->set_hit_test_bounds_override_inner(mouse_insets);

  // Watch for maximize/restore/fullscreen state changes.  Observer removes
  // itself in OnWindowDestroying() below, or in the destructor if we go away
  // before the window.
  window_->AddObserver(this);
  wm::GetWindowState(window_)->AddObserver(this);

  // Solo-window header updates are handled by the workspace controller when
  // this window is added to the desktop.
}

// static
void FramePainter::SetSoloWindowHeadersEnabled(bool enabled) {
  solo_window_header_enabled = enabled;
}

// static
void FramePainter::UpdateSoloWindowHeader(RootWindow* root_window) {
  // Use a separate function here so callers outside of FramePainter don't need
  // to know about "ignorable_window".
  UpdateSoloWindowInRoot(root_window, NULL /* ignorable_window */);
}

gfx::Rect FramePainter::GetBoundsForClientView(
    int top_height,
    const gfx::Rect& window_bounds) const {
  return gfx::Rect(
      kBorderThickness,
      top_height,
      std::max(0, window_bounds.width() - (2 * kBorderThickness)),
      std::max(0, window_bounds.height() - top_height - kBorderThickness));
}

gfx::Rect FramePainter::GetWindowBoundsForClientBounds(
    int top_height,
    const gfx::Rect& client_bounds) const {
  return gfx::Rect(std::max(0, client_bounds.x() - kBorderThickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * kBorderThickness),
                   client_bounds.height() + top_height + kBorderThickness);
}

int FramePainter::NonClientHitTest(views::NonClientFrameView* view,
                                   const gfx::Point& point) {
  gfx::Rect expanded_bounds = view->bounds();
  int outside_bounds = kResizeOutsideBoundsSize;

  if (aura::Env::GetInstance()->is_touch_down())
    outside_bounds *= kResizeOutsideBoundsScaleForTouch;
  expanded_bounds.Inset(-outside_bounds, -outside_bounds);

  if (!expanded_bounds.Contains(point))
    return HTNOWHERE;

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  bool can_ever_resize = frame_->widget_delegate() ?
      frame_->widget_delegate()->CanResize() :
      false;
  // Don't allow overlapping resize handles when the window is maximized or
  // fullscreen, as it can't be resized in those states.
  int resize_border =
      frame_->IsMaximized() || frame_->IsFullscreen() ? 0 :
      kResizeInsideBoundsSize;
  int frame_component = view->GetHTComponentForFrame(point,
                                                     resize_border,
                                                     resize_border,
                                                     kResizeAreaCornerSize,
                                                     kResizeAreaCornerSize,
                                                     can_ever_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component = frame_->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  if (caption_button_container_->visible()) {
    gfx::Point point_in_caption_button_container(point);
    views::View::ConvertPointToTarget(view, caption_button_container_,
        &point_in_caption_button_container);
    client_component = caption_button_container_->NonClientHitTest(
        point_in_caption_button_container);
    if (client_component != HTNOWHERE)
      return client_component;
  }

  // Caption is a safe default.
  return HTCAPTION;
}

gfx::Size FramePainter::GetMinimumSize(views::NonClientFrameView* view) {
  gfx::Size min_size = frame_->client_view()->GetMinimumSize();
  // Ensure we can display the top of the caption area.
  gfx::Rect client_bounds = view->GetBoundsForClientView();
  min_size.Enlarge(0, client_bounds.y());
  // Ensure we have enough space for the window icon and buttons.  We allow
  // the title string to collapse to zero width.
  int title_width = GetTitleOffsetX() +
      caption_button_container_->GetMinimumSize().width();
  if (title_width > min_size.width())
    min_size.set_width(title_width);
  return min_size;
}

gfx::Size FramePainter::GetMaximumSize(views::NonClientFrameView* view) {
  return frame_->client_view()->GetMaximumSize();
}

int FramePainter::GetRightInset() const {
  return caption_button_container_->GetPreferredSize().width();
}

int FramePainter::GetThemeBackgroundXInset() const {
  return kThemeFrameImageInsetX;
}

bool FramePainter::ShouldUseMinimalHeaderStyle(Themed header_themed) const {
  // Use the minimalistic header style whenever |frame_| is maximized or
  // fullscreen EXCEPT:
  // - If the user has installed a theme with custom images for the header.
  // - For windows which are not tracked by the workspace code (which are used
  //   for tab dragging).
  return (frame_->IsMaximized() || frame_->IsFullscreen()) &&
      header_themed == THEMED_NO &&
      wm::GetWindowState(frame_->GetNativeWindow())->tracked_by_workspace();
}

void FramePainter::PaintHeader(views::NonClientFrameView* view,
                               gfx::Canvas* canvas,
                               HeaderMode header_mode,
                               int theme_frame_id,
                               int theme_frame_overlay_id) {
  bool initial_paint = (previous_theme_frame_id_ == 0);
  if (!initial_paint &&
      (previous_theme_frame_id_ != theme_frame_id ||
       previous_theme_frame_overlay_id_ != theme_frame_overlay_id)) {
    aura::Window* parent = frame_->GetNativeWindow()->parent();
    // Don't animate the header if the parent (a workspace) is already
    // animating. Doing so results in continually painting during the animation
    // and gives a slower frame rate.
    // TODO(sky): expose a better way to determine this rather than assuming
    // the parent is a workspace.
    bool parent_animating = parent &&
        (parent->layer()->GetAnimator()->IsAnimatingProperty(
            ui::LayerAnimationElement::OPACITY) ||
         parent->layer()->GetAnimator()->IsAnimatingProperty(
             ui::LayerAnimationElement::VISIBILITY));
    if (!parent_animating) {
      crossfade_animation_.reset(new gfx::SlideAnimation(this));
      crossfade_theme_frame_id_ = previous_theme_frame_id_;
      crossfade_theme_frame_overlay_id_ = previous_theme_frame_overlay_id_;
      crossfade_opacity_ = previous_opacity_;
      crossfade_animation_->SetSlideDuration(kActivationCrossfadeDurationMs);
      crossfade_animation_->Show();
    } else {
      crossfade_animation_.reset();
    }
  }

  int opacity =
      GetHeaderOpacity(header_mode, theme_frame_id, theme_frame_overlay_id);
  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  gfx::ImageSkia* theme_frame = theme_provider->GetImageSkiaNamed(
      theme_frame_id);
  gfx::ImageSkia* theme_frame_overlay = NULL;
  if (theme_frame_overlay_id != 0) {
    theme_frame_overlay = theme_provider->GetImageSkiaNamed(
        theme_frame_overlay_id);
  }
  header_frame_bounds_ = gfx::Rect(0, 0, view->width(), theme_frame->height());

  int corner_radius = GetHeaderCornerRadius();
  SkPaint paint;

  if (crossfade_animation_.get() && crossfade_animation_->is_animating()) {
    gfx::ImageSkia* crossfade_theme_frame =
        theme_provider->GetImageSkiaNamed(crossfade_theme_frame_id_);
    gfx::ImageSkia* crossfade_theme_frame_overlay = NULL;
    if (crossfade_theme_frame_overlay_id_ != 0) {
      crossfade_theme_frame_overlay = theme_provider->GetImageSkiaNamed(
          crossfade_theme_frame_overlay_id_);
    }
    if (!crossfade_theme_frame ||
        (crossfade_theme_frame_overlay_id_ != 0 &&
         !crossfade_theme_frame_overlay)) {
      // Reset the animation. This case occurs when the user switches the theme
      // that they are using.
      crossfade_animation_.reset();
      paint.setAlpha(opacity);
    } else {
      double current_value = crossfade_animation_->GetCurrentValue();
      int old_alpha = (1 - current_value) * crossfade_opacity_;
      int new_alpha = current_value * opacity;

      // Draw the old header background, clipping the corners to be rounded.
      paint.setAlpha(old_alpha);
      paint.setXfermodeMode(SkXfermode::kPlus_Mode);
      PaintFrameImagesInRoundRect(canvas,
                                  crossfade_theme_frame,
                                  crossfade_theme_frame_overlay,
                                  paint,
                                  header_frame_bounds_,
                                  corner_radius,
                                  GetThemeBackgroundXInset());

      paint.setAlpha(new_alpha);
    }
  } else {
    paint.setAlpha(opacity);
  }

  // Draw the header background, clipping the corners to be rounded.
  PaintFrameImagesInRoundRect(canvas,
                              theme_frame,
                              theme_frame_overlay,
                              paint,
                              header_frame_bounds_,
                              corner_radius,
                              GetThemeBackgroundXInset());

  previous_theme_frame_id_ = theme_frame_id;
  previous_theme_frame_overlay_id_ = theme_frame_overlay_id;
  previous_opacity_ = opacity;

  // We don't need the extra lightness in the edges when we're at the top edge
  // of the screen or when the header's corners are not rounded.
  //
  // TODO(sky): this isn't quite right. What we really want is a method that
  // returns bounds ignoring transforms on certain windows (such as workspaces)
  // and is relative to the root.
  if (frame_->GetNativeWindow()->bounds().y() == 0 || corner_radius == 0)
    return;

  // Draw the top corners and edge.
  int top_left_height = top_left_corner_->height();
  canvas->DrawImageInt(*top_left_corner_,
                       0, 0, top_left_corner_->width(), top_left_height,
                       0, 0, top_left_corner_->width(), top_left_height,
                       false);
  canvas->TileImageInt(*top_edge_,
      top_left_corner_->width(),
      0,
      view->width() - top_left_corner_->width() - top_right_corner_->width(),
      top_edge_->height());
  int top_right_height = top_right_corner_->height();
  canvas->DrawImageInt(*top_right_corner_,
                       0, 0,
                       top_right_corner_->width(), top_right_height,
                       view->width() - top_right_corner_->width(), 0,
                       top_right_corner_->width(), top_right_height,
                       false);

  // Header left edge.
  int header_left_height = theme_frame->height() - top_left_height;
  canvas->TileImageInt(*header_left_edge_,
                       0, top_left_height,
                       header_left_edge_->width(), header_left_height);

  // Header right edge.
  int header_right_height = theme_frame->height() - top_right_height;
  canvas->TileImageInt(*header_right_edge_,
                       view->width() - header_right_edge_->width(),
                       top_right_height,
                       header_right_edge_->width(),
                       header_right_height);

  // We don't draw edges around the content area.  Web content goes flush
  // to the edge of the window.
}

void FramePainter::PaintHeaderContentSeparator(views::NonClientFrameView* view,
                                               gfx::Canvas* canvas) {
  // Paint the line just above the content area.
  gfx::Rect client_bounds = view->GetBoundsForClientView();
  canvas->FillRect(gfx::Rect(client_bounds.x(),
                             client_bounds.y() - kHeaderContentSeparatorSize,
                             client_bounds.width(),
                             kHeaderContentSeparatorSize),
                   kHeaderContentSeparatorColor);
}

int FramePainter::HeaderContentSeparatorSize() const {
  return kHeaderContentSeparatorSize;
}

void FramePainter::PaintTitleBar(views::NonClientFrameView* view,
                                 gfx::Canvas* canvas,
                                 const gfx::Font& title_font) {
  // The window icon is painted by its own views::View.
  views::WidgetDelegate* delegate = frame_->widget_delegate();
  if (delegate && delegate->ShouldShowWindowTitle()) {
    gfx::Rect title_bounds = GetTitleBounds(title_font);
    SkColor title_color = frame_->IsMaximized() ?
        kMaximizedWindowTitleTextColor : kNonMaximizedWindowTitleTextColor;
    canvas->DrawStringInt(delegate->GetWindowTitle(),
                          title_font,
                          title_color,
                          view->GetMirroredXForRect(title_bounds),
                          title_bounds.y(),
                          title_bounds.width(),
                          title_bounds.height(),
                          gfx::Canvas::NO_SUBPIXEL_RENDERING);
  }
}

void FramePainter::LayoutHeader(views::NonClientFrameView* view,
                                bool shorter_layout) {
  caption_button_container_->set_header_style(shorter_layout ?
      FrameCaptionButtonContainerView::HEADER_STYLE_SHORT :
      FrameCaptionButtonContainerView::HEADER_STYLE_TALL);
  caption_button_container_->Layout();

  gfx::Size caption_button_container_size =
      caption_button_container_->GetPreferredSize();
  caption_button_container_->SetBounds(
      view->width() - caption_button_container_size.width(),
      0,
      caption_button_container_size.width(),
      caption_button_container_size.height());

  if (window_icon_) {
    // Vertically center the window icon with respect to the caption button
    // container.
    int icon_offset_y =
        GetCaptionButtonContainerCenterY() - window_icon_->height() / 2;
    window_icon_->SetBounds(kIconOffsetX, icon_offset_y, kIconSize, kIconSize);
  }
}

void FramePainter::SchedulePaintForTitle(const gfx::Font& title_font) {
  frame_->non_client_view()->SchedulePaintInRect(GetTitleBounds(title_font));
}

void FramePainter::OnThemeChanged() {
  // We do not cache the images for |previous_theme_frame_id_| and
  // |previous_theme_frame_overlay_id_|. Changing the theme changes the images
  // returned from ui::ThemeProvider for |previous_theme_frame_id_|
  // and |previous_theme_frame_overlay_id_|. Reset the image ids to prevent
  // starting a crossfade animation with these images.
  previous_theme_frame_id_ = 0;
  previous_theme_frame_overlay_id_ = 0;

  if (crossfade_animation_.get() && crossfade_animation_->is_animating()) {
    crossfade_animation_.reset();
    frame_->non_client_view()->SchedulePaintInRect(header_frame_bounds_);
  }
}

///////////////////////////////////////////////////////////////////////////////
// WindowState::Observer overrides:
void FramePainter::OnTrackedByWorkspaceChanged(aura::Window* window,
                                               bool old) {
  // When 'TrackedByWorkspace' changes, we are going to paint the header
  // differently. Schedule a paint to ensure everything is updated correctly.
  if (wm::GetWindowState(window)->tracked_by_workspace())
    frame_->non_client_view()->SchedulePaint();
}

///////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void FramePainter::OnWindowPropertyChanged(aura::Window* window,
                                           const void* key,
                                           intptr_t old) {
  if (key != aura::client::kShowStateKey)
    return;

  // Maximized and fullscreen windows don't want resize handles overlapping the
  // content area, because when the user moves the cursor to the right screen
  // edge we want them to be able to hit the scroll bar.
  wm::WindowState* window_state = wm::GetWindowState(window);
  if (window_state->IsMaximizedOrFullscreen()) {
    window->set_hit_test_bounds_override_inner(gfx::Insets());
  } else {
    window->set_hit_test_bounds_override_inner(
        gfx::Insets(kResizeInsideBoundsSize, kResizeInsideBoundsSize,
                    kResizeInsideBoundsSize, kResizeInsideBoundsSize));
  }
}

void FramePainter::OnWindowVisibilityChanged(aura::Window* window,
                                             bool visible) {
  // OnWindowVisibilityChanged can be called for the child windows of |window_|.
  if (window != window_)
    return;

  // Window visibility change may trigger the change of window solo-ness in a
  // different window.
  UpdateSoloWindowInRoot(window_->GetRootWindow(), visible ? NULL : window_);
}

void FramePainter::OnWindowDestroying(aura::Window* destroying) {
  DCHECK_EQ(window_, destroying);

  // Must be removed here and not in the destructor, as the aura::Window is
  // already destroyed when our destructor runs.
  window_->RemoveObserver(this);
  wm::GetWindowState(window_)->RemoveObserver(this);

  // If we have two or more windows open and we close this one, we might trigger
  // the solo window appearance for another window.
  UpdateSoloWindowInRoot(window_->GetRootWindow(), window_);

  window_ = NULL;
}

void FramePainter::OnWindowBoundsChanged(aura::Window* window,
                                         const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
  // TODO(sky): this isn't quite right. What we really want is a method that
  // returns bounds ignoring transforms on certain windows (such as workspaces).
  if ((!frame_->IsMaximized() && !frame_->IsFullscreen()) &&
      ((old_bounds.y() == 0 && new_bounds.y() != 0) ||
       (old_bounds.y() != 0 && new_bounds.y() == 0))) {
    SchedulePaintForHeader();
  }
}

void FramePainter::OnWindowAddedToRootWindow(aura::Window* window) {
  // Needs to trigger the window appearance change if the window moves across
  // root windows and a solo window is already in the new root.
  UpdateSoloWindowInRoot(window->GetRootWindow(), NULL /* ignore_window */);
}

void FramePainter::OnWindowRemovingFromRootWindow(aura::Window* window) {
  // Needs to trigger the window appearance change if the window moves across
  // root windows and only one window is left in the previous root.  Because
  // |window| is not yet moved, |window| has to be ignored.
  UpdateSoloWindowInRoot(window->GetRootWindow(), window);
}

///////////////////////////////////////////////////////////////////////////////
// gfx::AnimationDelegate overrides:

void FramePainter::AnimationProgressed(const gfx::Animation* animation) {
  frame_->non_client_view()->SchedulePaintInRect(header_frame_bounds_);
}

///////////////////////////////////////////////////////////////////////////////
// FramePainter, private:

int FramePainter::GetTitleOffsetX() const {
  return window_icon_ ?
      window_icon_->bounds().right() + kTitleIconOffsetX :
      kTitleNoIconOffsetX;
}

int FramePainter::GetCaptionButtonContainerCenterY() const {
  return caption_button_container_->y() +
      caption_button_container_->height() / 2;
}

int FramePainter::GetHeaderCornerRadius() const {
  // Use square corners for maximized and fullscreen windows when they are
  // tracked by the workspace code. (Windows which are not tracked by the
  // workspace code are used for tab dragging.)
  bool square_corners = ((frame_->IsMaximized() || frame_->IsFullscreen())) &&
      wm::GetWindowState(frame_->GetNativeWindow())->tracked_by_workspace();
  const int kCornerRadius = 2;
  return square_corners ? 0 : kCornerRadius;
}

int FramePainter::GetHeaderOpacity(
    HeaderMode header_mode,
    int theme_frame_id,
    int theme_frame_overlay_id) const {
  // User-provided themes are painted fully opaque.
  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  if (theme_provider->HasCustomImage(theme_frame_id) ||
      (theme_frame_overlay_id != 0 &&
       theme_provider->HasCustomImage(theme_frame_overlay_id))) {
    return kFullyOpaque;
  }

  // The header is fully opaque when using the minimalistic header style.
  if (ShouldUseMinimalHeaderStyle(THEMED_NO))
    return kFullyOpaque;

  // Single browser window is very transparent.
  if (UseSoloWindowHeader())
    return kSoloWindowOpacity;

  // Otherwise, change transparency based on window activation status.
  if (header_mode == ACTIVE)
    return kActiveWindowOpacity;
  return kInactiveWindowOpacity;
}

bool FramePainter::UseSoloWindowHeader() const {
  if (!solo_window_header_enabled)
    return false;
  // Don't use transparent headers for panels, pop-ups, etc.
  if (!IsSoloWindowHeaderCandidate(window_))
    return false;
  aura::RootWindow* root = window_->GetRootWindow();
  // Don't recompute every time, as it would require many window property
  // lookups.
  return internal::GetRootWindowSettings(root)->solo_window_header;
}

// static
bool FramePainter::UseSoloWindowHeaderInRoot(RootWindow* root_window,
                                             Window* ignore_window) {
  int visible_window_count = 0;
  std::vector<Window*> windows = GetWindowsForSoloHeaderUpdate(root_window);
  for (std::vector<Window*>::const_iterator it = windows.begin();
       it != windows.end();
       ++it) {
    Window* window = *it;
    // Various sorts of windows "don't count" for this computation.
    if (ignore_window == window ||
        !IsSoloWindowHeaderCandidate(window) ||
        !IsVisibleToRoot(window))
      continue;
    if (wm::GetWindowState(window)->IsMaximized())
      return false;
    ++visible_window_count;
    if (visible_window_count > 1)
      return false;
  }
  // Count must be tested because all windows might be "don't count" windows
  // in the loop above.
  return visible_window_count == 1;
}

// static
void FramePainter::UpdateSoloWindowInRoot(RootWindow* root,
                                          Window* ignore_window) {
#if defined(OS_WIN)
  // Non-Ash Windows doesn't do solo-window counting for transparency effects,
  // as the desktop background and window frames are managed by the OS.
  if (!ash::Shell::HasInstance())
    return;
#endif
  if (!root)
    return;
  internal::RootWindowSettings* root_window_settings =
      internal::GetRootWindowSettings(root);
  bool old_solo_header = root_window_settings->solo_window_header;
  bool new_solo_header = UseSoloWindowHeaderInRoot(root, ignore_window);
  if (old_solo_header == new_solo_header)
    return;
  root_window_settings->solo_window_header = new_solo_header;

  // Invalidate all the window frames in the desktop. There should only be
  // a few.
  std::vector<Window*> windows = GetWindowsForSoloHeaderUpdate(root);
  for (std::vector<Window*>::const_iterator it = windows.begin();
       it != windows.end();
       ++it) {
    Widget* widget = Widget::GetWidgetForNativeWindow(*it);
    if (widget && widget->non_client_view())
      widget->non_client_view()->SchedulePaint();
  }
}

void FramePainter::SchedulePaintForHeader() {
  int top_left_height = top_left_corner_->height();
  int top_right_height = top_right_corner_->height();
  frame_->non_client_view()->SchedulePaintInRect(
      gfx::Rect(0, 0, frame_->non_client_view()->width(),
                std::max(top_left_height, top_right_height)));
}

gfx::Rect FramePainter::GetTitleBounds(const gfx::Font& title_font) {
  int title_x = GetTitleOffsetX();
  // Center the text with respect to the caption button container. This way it
  // adapts to the caption button height and aligns exactly with the window
  // icon. Don't use |window_icon_| for this computation as it may be NULL.
  int title_y = GetCaptionButtonContainerCenterY() - title_font.GetHeight() / 2;
  return gfx::Rect(
      title_x,
      std::max(0, title_y),
      std::max(0, caption_button_container_->x() - kTitleLogoSpacing - title_x),
      title_font.GetHeight());
}

}  // namespace ash
