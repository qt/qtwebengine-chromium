// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_aura.h"

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/overscroll_controller.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/web_contents/aura/image_window_delegate.h"
#include "content/browser/web_contents/aura/shadow_layer_delegate.h"
#include "content/browser/web_contents/aura/window_slider.h"
#include "content/browser/web_contents/interstitial_page_impl.h"
#include "content/browser/web_contents/navigation_entry_impl.h"
#include "content/browser/web_contents/touch_editable_impl_aura.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/overscroll_configuration.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/browser/web_drag_dest_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/drop_data.h"
#include "net/base/net_util.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/drag_drop_delegate.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drag_utils.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_png_rep.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"

namespace content {
WebContentsViewPort* CreateWebContentsView(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate,
    RenderViewHostDelegateView** render_view_host_delegate_view) {
  WebContentsViewAura* rv = new WebContentsViewAura(web_contents, delegate);
  *render_view_host_delegate_view = rv;
  return rv;
}

namespace {

bool IsScrollEndEffectEnabled() {
  return CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kScrollEndEffect) == "1";
}

bool ShouldNavigateForward(const NavigationController& controller,
                           OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_EAST : OVERSCROLL_WEST) &&
         controller.CanGoForward();
}

bool ShouldNavigateBack(const NavigationController& controller,
                        OverscrollMode mode) {
  return mode == (base::i18n::IsRTL() ? OVERSCROLL_WEST : OVERSCROLL_EAST) &&
         controller.CanGoBack();
}

RenderWidgetHostViewAura* ToRenderWidgetHostViewAura(
    RenderWidgetHostView* view) {
  if (!view || RenderViewHostFactory::has_factory())
    return NULL;  // Can't cast to RenderWidgetHostViewAura in unit tests.
  RenderProcessHostImpl* process = static_cast<RenderProcessHostImpl*>(
      view->GetRenderWidgetHost()->GetProcess());
  if (process->IsGuest())
    return NULL;
  return static_cast<RenderWidgetHostViewAura*>(view);
}

// The window delegate for the overscroll window. This redirects trackpad events
// to the web-contents window. The delegate destroys itself when the window is
// destroyed.
class OverscrollWindowDelegate : public ImageWindowDelegate {
 public:
  OverscrollWindowDelegate(WebContentsImpl* web_contents,
                           OverscrollMode overscroll_mode)
      : web_contents_(web_contents),
        forward_events_(true) {
    const NavigationControllerImpl& controller = web_contents->GetController();
    const NavigationEntryImpl* entry = NULL;
    if (ShouldNavigateForward(controller, overscroll_mode)) {
      entry = NavigationEntryImpl::FromNavigationEntry(
          controller.GetEntryAtOffset(1));
    } else if (ShouldNavigateBack(controller, overscroll_mode)) {
      entry = NavigationEntryImpl::FromNavigationEntry(
          controller.GetEntryAtOffset(-1));
    }

    gfx::Image image;
    if (entry && entry->screenshot().get()) {
      std::vector<gfx::ImagePNGRep> image_reps;
      image_reps.push_back(gfx::ImagePNGRep(entry->screenshot(),
            ui::GetScaleFactorForNativeView(web_contents_window())));
      image = gfx::Image(image_reps);
    }
    SetImage(image);
  }

  void stop_forwarding_events() { forward_events_ = false; }

 private:
  virtual ~OverscrollWindowDelegate() {}

  aura::Window* web_contents_window() {
    return web_contents_->GetView()->GetContentNativeView();
  }

  // Overridden from ui::EventHandler.
  virtual void OnScrollEvent(ui::ScrollEvent* event) OVERRIDE {
    if (forward_events_ && web_contents_window())
      web_contents_window()->delegate()->OnScrollEvent(event);
  }

  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE {
    if (forward_events_ && web_contents_window())
      web_contents_window()->delegate()->OnGestureEvent(event);
  }

  WebContents* web_contents_;

  // The window is displayed both during the gesture, and after the gesture
  // while the navigation is in progress. During the gesture, it is necessary to
  // forward input events to the content page (e.g. when the overscroll window
  // slides under the cursor and starts receiving scroll events). However, once
  // the gesture is complete, and the window is being displayed as an overlay
  // window during navigation, events should not be forwarded anymore.
  bool forward_events_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollWindowDelegate);
};

// Listens to all mouse drag events during a drag and drop and sends them to
// the renderer.
class WebDragSourceAura : public base::MessageLoopForUI::Observer,
                          public NotificationObserver {
 public:
  WebDragSourceAura(aura::Window* window, WebContentsImpl* contents)
      : window_(window),
        contents_(contents) {
    base::MessageLoopForUI::current()->AddObserver(this);
    registrar_.Add(this,
                   NOTIFICATION_WEB_CONTENTS_DISCONNECTED,
                   Source<WebContents>(contents));
  }

  virtual ~WebDragSourceAura() {
    base::MessageLoopForUI::current()->RemoveObserver(this);
  }

  // MessageLoop::Observer implementation:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    return base::EVENT_CONTINUE;
  }
  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
    if (!contents_)
      return;
    ui::EventType type = ui::EventTypeFromNative(event);
    RenderViewHost* rvh = NULL;
    switch (type) {
      case ui::ET_MOUSE_DRAGGED:
        rvh = contents_->GetRenderViewHost();
        if (rvh) {
          gfx::Point screen_loc_in_pixel = ui::EventLocationFromNative(event);
          gfx::Point screen_loc = ConvertViewPointToDIP(rvh->GetView(),
              screen_loc_in_pixel);
          gfx::Point client_loc = screen_loc;
          aura::Window* window = rvh->GetView()->GetNativeView();
          aura::Window::ConvertPointToTarget(window->GetRootWindow(),
              window, &client_loc);
          contents_->DragSourceMovedTo(client_loc.x(), client_loc.y(),
              screen_loc.x(), screen_loc.y());
        }
        break;
      default:
        break;
    }
  }

  virtual void Observe(int type,
      const NotificationSource& source,
      const NotificationDetails& details) OVERRIDE {
    if (type != NOTIFICATION_WEB_CONTENTS_DISCONNECTED)
      return;

    // Cancel the drag if it is still in progress.
    aura::client::DragDropClient* dnd_client =
        aura::client::GetDragDropClient(window_->GetRootWindow());
    if (dnd_client && dnd_client->IsDragDropInProgress())
      dnd_client->DragCancel();

    window_ = NULL;
    contents_ = NULL;
  }

  aura::Window* window() const { return window_; }

 private:
  aura::Window* window_;
  WebContentsImpl* contents_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(WebDragSourceAura);
};

#if defined(OS_WIN)
// Fill out the OSExchangeData with a file contents, synthesizing a name if
// necessary.
void PrepareDragForFileContents(const DropData& drop_data,
                                ui::OSExchangeData::Provider* provider) {
  base::FilePath file_name(drop_data.file_description_filename);
  // Images without ALT text will only have a file extension so we need to
  // synthesize one from the provided extension and URL.
  if (file_name.BaseName().RemoveExtension().empty()) {
    const string16 extension = file_name.Extension();
    // Retrieve the name from the URL.
    file_name = base::FilePath(net::GetSuggestedFilename(
        drop_data.url, "", "", "", "", "")).ReplaceExtension(extension);
  }
  provider->SetFileContents(file_name, drop_data.file_contents);
}
#endif

// Utility to fill a ui::OSExchangeDataProvider object from DropData.
void PrepareDragData(const DropData& drop_data,
                     ui::OSExchangeData::Provider* provider) {
#if defined(OS_WIN)
  // We set the file contents before the URL because the URL also sets file
  // contents (to a .URL shortcut).  We want to prefer file content data over
  // a shortcut so we add it first.
  if (!drop_data.file_contents.empty())
    PrepareDragForFileContents(drop_data, provider);
#endif
  if (!drop_data.text.string().empty())
    provider->SetString(drop_data.text.string());
  if (drop_data.url.is_valid())
    provider->SetURL(drop_data.url, drop_data.url_title);
  if (!drop_data.html.string().empty())
    provider->SetHtml(drop_data.html.string(), drop_data.html_base_url);
  if (!drop_data.filenames.empty()) {
    std::vector<ui::OSExchangeData::FileInfo> filenames;
    for (std::vector<DropData::FileInfo>::const_iterator it =
             drop_data.filenames.begin();
         it != drop_data.filenames.end(); ++it) {
      filenames.push_back(
          ui::OSExchangeData::FileInfo(
              base::FilePath::FromUTF8Unsafe(UTF16ToUTF8(it->path)),
              base::FilePath::FromUTF8Unsafe(UTF16ToUTF8(it->display_name))));
    }
    provider->SetFilenames(filenames);
  }
  if (!drop_data.custom_data.empty()) {
    Pickle pickle;
    ui::WriteCustomDataToPickle(drop_data.custom_data, &pickle);
    provider->SetPickledData(ui::Clipboard::GetWebCustomDataFormatType(),
                             pickle);
  }
}

// Utility to fill a DropData object from ui::OSExchangeData.
void PrepareDropData(DropData* drop_data, const ui::OSExchangeData& data) {
  string16 plain_text;
  data.GetString(&plain_text);
  if (!plain_text.empty())
    drop_data->text = base::NullableString16(plain_text, false);

  GURL url;
  string16 url_title;
  data.GetURLAndTitle(&url, &url_title);
  if (url.is_valid()) {
    drop_data->url = url;
    drop_data->url_title = url_title;
  }

  string16 html;
  GURL html_base_url;
  data.GetHtml(&html, &html_base_url);
  if (!html.empty())
    drop_data->html = base::NullableString16(html, false);
  if (html_base_url.is_valid())
    drop_data->html_base_url = html_base_url;

  std::vector<ui::OSExchangeData::FileInfo> files;
  if (data.GetFilenames(&files) && !files.empty()) {
    for (std::vector<ui::OSExchangeData::FileInfo>::const_iterator
             it = files.begin(); it != files.end(); ++it) {
      drop_data->filenames.push_back(
          DropData::FileInfo(
              UTF8ToUTF16(it->path.AsUTF8Unsafe()),
              UTF8ToUTF16(it->display_name.AsUTF8Unsafe())));
    }
  }

  Pickle pickle;
  if (data.GetPickledData(ui::Clipboard::GetWebCustomDataFormatType(), &pickle))
    ui::ReadCustomDataIntoMap(
        pickle.data(), pickle.size(), &drop_data->custom_data);
}

// Utilities to convert between WebKit::WebDragOperationsMask and
// ui::DragDropTypes.
int ConvertFromWeb(WebKit::WebDragOperationsMask ops) {
  int drag_op = ui::DragDropTypes::DRAG_NONE;
  if (ops & WebKit::WebDragOperationCopy)
    drag_op |= ui::DragDropTypes::DRAG_COPY;
  if (ops & WebKit::WebDragOperationMove)
    drag_op |= ui::DragDropTypes::DRAG_MOVE;
  if (ops & WebKit::WebDragOperationLink)
    drag_op |= ui::DragDropTypes::DRAG_LINK;
  return drag_op;
}

WebKit::WebDragOperationsMask ConvertToWeb(int drag_op) {
  int web_drag_op = WebKit::WebDragOperationNone;
  if (drag_op & ui::DragDropTypes::DRAG_COPY)
    web_drag_op |= WebKit::WebDragOperationCopy;
  if (drag_op & ui::DragDropTypes::DRAG_MOVE)
    web_drag_op |= WebKit::WebDragOperationMove;
  if (drag_op & ui::DragDropTypes::DRAG_LINK)
    web_drag_op |= WebKit::WebDragOperationLink;
  return (WebKit::WebDragOperationsMask) web_drag_op;
}

int ConvertAuraEventFlagsToWebInputEventModifiers(int aura_event_flags) {
  int web_input_event_modifiers = 0;
  if (aura_event_flags & ui::EF_SHIFT_DOWN)
    web_input_event_modifiers |= WebKit::WebInputEvent::ShiftKey;
  if (aura_event_flags & ui::EF_CONTROL_DOWN)
    web_input_event_modifiers |= WebKit::WebInputEvent::ControlKey;
  if (aura_event_flags & ui::EF_ALT_DOWN)
    web_input_event_modifiers |= WebKit::WebInputEvent::AltKey;
  if (aura_event_flags & ui::EF_COMMAND_DOWN)
    web_input_event_modifiers |= WebKit::WebInputEvent::MetaKey;
  return web_input_event_modifiers;
}

// A LayerDelegate that paints an image for the layer.
class ImageLayerDelegate : public ui::LayerDelegate {
 public:
  ImageLayerDelegate() {}

  virtual ~ImageLayerDelegate() {}

  void SetImage(const gfx::Image& image) {
    image_ = image;
    image_size_ = image.AsImageSkia().size();
  }
  const gfx::Image& image() const { return image_; }

 private:
  // Overridden from ui::LayerDelegate:
  virtual void OnPaintLayer(gfx::Canvas* canvas) OVERRIDE {
    if (image_.IsEmpty()) {
      canvas->DrawColor(SK_ColorGRAY);
    } else {
      SkISize size = canvas->sk_canvas()->getDeviceSize();
      if (size.width() != image_size_.width() ||
          size.height() != image_size_.height()) {
        canvas->DrawColor(SK_ColorWHITE);
      }
      canvas->DrawImageInt(image_.AsImageSkia(), 0, 0);
    }
  }

  // Called when the layer's device scale factor has changed.
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE {
  }

  // Invoked prior to the bounds changing. The returned closured is run after
  // the bounds change.
  virtual base::Closure PrepareForLayerBoundsChange() OVERRIDE {
    return base::Closure();
  }

  gfx::Image image_;
  gfx::Size image_size_;

  DISALLOW_COPY_AND_ASSIGN(ImageLayerDelegate);
};

}  // namespace

// When a history navigation is triggered at the end of an overscroll
// navigation, it is necessary to show the history-screenshot until the page is
// done navigating and painting. This class accomplishes this by showing the
// screenshot window on top of the page until the page has completed loading and
// painting.
class OverscrollNavigationOverlay :
    public RenderWidgetHostViewAura::PaintObserver,
    public WindowSlider::Delegate {
 public:
  explicit OverscrollNavigationOverlay(WebContentsImpl* web_contents)
      : web_contents_(web_contents),
        image_delegate_(NULL),
        view_(NULL),
        loading_complete_(false),
        received_paint_update_(false),
        compositor_updated_(false),
        slide_direction_(SLIDE_UNKNOWN),
        need_paint_update_(true) {
  }

  virtual ~OverscrollNavigationOverlay() {
    if (view_)
      view_->set_paint_observer(NULL);
  }

  bool has_window() const { return !!window_.get(); }

  void StartObservingView(RenderWidgetHostViewAura* view) {
    if (view_)
      view_->set_paint_observer(NULL);

    loading_complete_ = false;
    received_paint_update_ = false;
    compositor_updated_ = false;
    view_ = view;
    if (view_)
      view_->set_paint_observer(this);

    // Make sure the overlay window is on top.
    if (window_.get() && window_->parent())
      window_->parent()->StackChildAtTop(window_.get());
  }

  void SetOverlayWindow(scoped_ptr<aura::Window> window,
                        ImageWindowDelegate* delegate) {
    window_ = window.Pass();
    if (window_.get() && window_->parent())
      window_->parent()->StackChildAtTop(window_.get());
    image_delegate_ = delegate;

    if (window_.get() && delegate->has_image()) {
      window_slider_.reset(new WindowSlider(this,
                                            window_->parent(),
                                            window_.get()));
      slide_direction_ = SLIDE_UNKNOWN;
    } else {
      window_slider_.reset();
    }
  }

  void SetupForTesting() {
    need_paint_update_ = false;
  }

 private:
  // Stop observing the page if the page-load has completed and the page has
  // been painted, and a window-slide isn't in progress.
  void StopObservingIfDone() {
    // If there is a screenshot displayed in the overlay window, then wait for
    // the navigated page to complete loading and some paint update before
    // hiding the overlay.
    // If there is no screenshot in the overlay window, then hide this view
    // as soon as there is any new painting notification.
    if ((need_paint_update_ && !received_paint_update_) ||
        (image_delegate_->has_image() && !loading_complete_)) {
      return;
    }

    // If a slide is in progress, then do not destroy the window or the slide.
    if (window_slider_.get() && window_slider_->IsSlideInProgress())
      return;

    window_slider_.reset();
    window_.reset();
    image_delegate_ = NULL;
    if (view_) {
      view_->set_paint_observer(NULL);
      view_ = NULL;
    }
  }

  // Creates a layer to be used for window-slide. |offset| is the offset of the
  // NavigationEntry for the screenshot image to display.
  ui::Layer* CreateSlideLayer(int offset) {
    const NavigationControllerImpl& controller = web_contents_->GetController();
    const NavigationEntryImpl* entry = NavigationEntryImpl::FromNavigationEntry(
        controller.GetEntryAtOffset(offset));

    gfx::Image image;
    if (entry && entry->screenshot().get()) {
      std::vector<gfx::ImagePNGRep> image_reps;
      image_reps.push_back(gfx::ImagePNGRep(entry->screenshot(),
            ui::GetScaleFactorForNativeView(window_.get())));
      image = gfx::Image(image_reps);
    }
    layer_delegate_.SetImage(image);

    ui::Layer* layer = new ui::Layer(ui::LAYER_TEXTURED);
    layer->set_delegate(&layer_delegate_);
    return layer;
  }

  // Overridden from WindowSlider::Delegate:
  virtual ui::Layer* CreateBackLayer() OVERRIDE {
    if (!web_contents_->GetController().CanGoBack())
      return NULL;
    slide_direction_ = SLIDE_BACK;
    return CreateSlideLayer(-1);
  }

  virtual ui::Layer* CreateFrontLayer() OVERRIDE {
    if (!web_contents_->GetController().CanGoForward())
      return NULL;
    slide_direction_ = SLIDE_FRONT;
    return CreateSlideLayer(1);
  }

  virtual void OnWindowSlideComplete() OVERRIDE {
    if (slide_direction_ == SLIDE_UNKNOWN) {
      window_slider_.reset();
      StopObservingIfDone();
      return;
    }

    // Change the image used for the overlay window.
    image_delegate_->SetImage(layer_delegate_.image());
    window_->layer()->SetTransform(gfx::Transform());
    window_->SchedulePaintInRect(gfx::Rect(window_->bounds().size()));

    SlideDirection direction = slide_direction_;
    slide_direction_ = SLIDE_UNKNOWN;

    // Reset state and wait for the new navigation page to complete
    // loading/painting.
    StartObservingView(ToRenderWidgetHostViewAura(
        web_contents_->GetRenderWidgetHostView()));

    // Perform the navigation.
    if (direction == SLIDE_BACK)
      web_contents_->GetController().GoBack();
    else if (direction == SLIDE_FRONT)
      web_contents_->GetController().GoForward();
    else
      NOTREACHED();
  }

  virtual void OnWindowSlideAborted() OVERRIDE {
    StopObservingIfDone();
  }

  virtual void OnWindowSliderDestroyed() OVERRIDE {
    // The slider has just been destroyed. Release the ownership.
    WindowSlider* slider ALLOW_UNUSED = window_slider_.release();
    StopObservingIfDone();
  }

  // Overridden from RenderWidgetHostViewAura::PaintObserver:
  virtual void OnPaintComplete() OVERRIDE {
    received_paint_update_ = true;
    StopObservingIfDone();
  }

  virtual void OnCompositingComplete() OVERRIDE {
    received_paint_update_ = compositor_updated_;
    StopObservingIfDone();
  }

  virtual void OnUpdateCompositorContent() OVERRIDE {
    compositor_updated_ = true;
  }

  virtual void OnPageLoadComplete() OVERRIDE {
    loading_complete_ = true;
    StopObservingIfDone();
  }

  virtual void OnViewDestroyed() OVERRIDE {
    DCHECK(view_);
    view_->set_paint_observer(NULL);
    view_ = NULL;
  }

  // The WebContents which is being navigated.
  WebContentsImpl* web_contents_;

  scoped_ptr<aura::Window> window_;

  // This is the WindowDelegate of |window_|. The delegate manages its own
  // lifetime (destroys itself when |window_| is destroyed).
  ImageWindowDelegate* image_delegate_;

  RenderWidgetHostViewAura* view_;
  bool loading_complete_;
  bool received_paint_update_;
  bool compositor_updated_;

  enum SlideDirection {
    SLIDE_UNKNOWN,
    SLIDE_BACK,
    SLIDE_FRONT
  };

  // The |WindowSlider| that allows sliding history layers while the page is
  // being reloaded.
  scoped_ptr<WindowSlider> window_slider_;

  // The direction of the in-progress slide (if any).
  SlideDirection slide_direction_;

  // The LayerDelegate used for the back/front layers during a slide.
  ImageLayerDelegate layer_delegate_;

  // During tests, the aura windows don't get any paint updates. So the overlay
  // container keeps waiting for a paint update it never receives, causing a
  // timeout. So during tests, disable the wait for paint updates.
  bool need_paint_update_;

  DISALLOW_COPY_AND_ASSIGN(OverscrollNavigationOverlay);
};

class WebContentsViewAura::WindowObserver
    : public aura::WindowObserver, public aura::RootWindowObserver {
 public:
  explicit WindowObserver(WebContentsViewAura* view)
      : view_(view),
        parent_(NULL) {
    view_->window_->AddObserver(this);
  }

  virtual ~WindowObserver() {
    view_->window_->RemoveObserver(this);
    if (view_->window_->GetRootWindow())
      view_->window_->GetRootWindow()->RemoveRootWindowObserver(this);
    if (parent_)
      parent_->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowParentChanged(aura::Window* window,
                                     aura::Window* parent) OVERRIDE {
    if (window == parent_)
      return;
    if (parent_)
      parent_->RemoveObserver(this);
    parent_ = parent;
    if (parent)
      parent->AddObserver(this);
  }

  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    SendScreenRects();
    if (view_->touch_editable_)
      view_->touch_editable_->UpdateEditingController();
  }

  virtual void OnWindowAddedToRootWindow(aura::Window* window) OVERRIDE {
    if (window != parent_)
      window->GetRootWindow()->AddRootWindowObserver(this);
  }

  virtual void OnWindowRemovingFromRootWindow(aura::Window* window) OVERRIDE {
    if (window != parent_)
      window->GetRootWindow()->RemoveRootWindowObserver(this);
  }

  // Overridden RootWindowObserver:
  virtual void OnRootWindowHostMoved(const aura::RootWindow* root,
                                     const gfx::Point& new_origin) OVERRIDE {
    // This is for the desktop case (i.e. Aura desktop).
    SendScreenRects();
  }

 private:
  void SendScreenRects() {
    RenderWidgetHostImpl::From(view_->web_contents_->GetRenderViewHost())->
        SendScreenRects();
  }

  WebContentsViewAura* view_;

  // We cache the old parent so that we can unregister when it's not the parent
  // anymore.
  aura::Window* parent_;

  DISALLOW_COPY_AND_ASSIGN(WindowObserver);
};

#if defined(OS_WIN)
// Constrained windows are added as children of the WebContent's view which may
// overlap with windowed NPAPI plugins. In that case, tell the RWHV so that it
// can update the plugins' cutout rects accordingly.
class WebContentsViewAura::ChildWindowObserver : public aura::WindowObserver,
                                                 public WebContentsObserver {
 public:
  explicit ChildWindowObserver(WebContentsViewAura* view)
      : WebContentsObserver(view->web_contents_),
        view_(view),
        web_contents_destroyed_(false) {
    view_->window_->AddObserver(this);
  }

  virtual ~ChildWindowObserver() {
    view_->window_->RemoveObserver(this);
    const aura::Window::Windows& children = view_->window_->children();
    for (size_t i = 0; i < children.size(); ++i)
      children[i]->RemoveObserver(this);
  }

  // Overridden from aura::WindowObserver:
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE {
    // If new child windows are added to the WebContent's view, tell the RWHV.
    // We also start watching them to know when their size is updated. Of
    // course, ignore the shadow window that contains the RWHV and child windows
    // of the child windows that we are watching.
    RenderWidgetHostViewAura* view = ToRenderWidgetHostViewAura(
        view_->web_contents_->GetRenderWidgetHostView());
    aura::Window* content_window = view ? view->GetNativeView() : NULL;
    if (new_window->parent() == view_->window_ &&
        new_window != content_window) {
      new_window->AddObserver(this);
      UpdateConstrainedWindows(NULL);
    }
  }

  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE {
    RenderWidgetHostViewAura* view = ToRenderWidgetHostViewAura(
        view_->web_contents_->GetRenderWidgetHostView());
    aura::Window* content_window = view ? view->GetNativeView() : NULL;
    if (window->parent() == view_->window_ &&
        window != content_window) {
      window->RemoveObserver(this);
      UpdateConstrainedWindows(window);
    }
  }

  virtual void OnWindowBoundsChanged(aura::Window* window,
                                     const gfx::Rect& old_bounds,
                                     const gfx::Rect& new_bounds) OVERRIDE {
    if (window->parent() == view_->window_ &&
        window != view_->GetContentNativeView()) {
      UpdateConstrainedWindows(NULL);
    }
  }

  // Overridden from WebContentsObserver:
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE {
    web_contents_destroyed_ = true;
  }

 private:
  void UpdateConstrainedWindows(aura::Window* exclude) {
    if (web_contents_destroyed_)
      return;

    RenderWidgetHostViewAura* view = ToRenderWidgetHostViewAura(
        view_->web_contents_->GetRenderWidgetHostView());
    if (!view)
      return;

    std::vector<gfx::Rect> constrained_windows;
    const aura::Window::Windows& children = view_->window_->children();
    aura::Window* content = view_->GetContentNativeView();
    for (size_t i = 0; i < children.size(); ++i) {
      if (children[i] != content && children[i] != exclude)
        constrained_windows.push_back(children[i]->GetBoundsInRootWindow());
    }

    view->UpdateConstrainedWindowRects(constrained_windows);
  }

  WebContentsViewAura* view_;
  bool web_contents_destroyed_;

  DISALLOW_COPY_AND_ASSIGN(ChildWindowObserver);
};
#endif

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, public:

WebContentsViewAura::WebContentsViewAura(
    WebContentsImpl* web_contents,
    WebContentsViewDelegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      current_drag_op_(WebKit::WebDragOperationNone),
      drag_dest_delegate_(NULL),
      current_rvh_for_drag_(NULL),
      overscroll_change_brightness_(false),
      current_overscroll_gesture_(OVERSCROLL_NONE),
      completed_overscroll_gesture_(OVERSCROLL_NONE),
      touch_editable_(TouchEditableImplAura::Create()) {
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, private:

WebContentsViewAura::~WebContentsViewAura() {
  if (!window_)
    return;

  window_observer_.reset();
#if defined(OS_WIN)
  child_window_observer_.reset();
#endif
  // Window needs a valid delegate during its destructor, so we explicitly
  // delete it here.
  window_.reset();
}

void WebContentsViewAura::SetupOverlayWindowForTesting() {
  if (navigation_overlay_)
    navigation_overlay_->SetupForTesting();
}

void WebContentsViewAura::SetTouchEditableForTest(
    TouchEditableImplAura* touch_editable) {
  touch_editable_.reset(touch_editable);
  AttachTouchEditableToRenderView();
}

void WebContentsViewAura::SizeChangedCommon(const gfx::Size& size) {
  if (web_contents_->GetInterstitialPage())
    web_contents_->GetInterstitialPage()->SetSize(size);
  RenderWidgetHostView* rwhv =
      web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->SetSize(size);
}

void WebContentsViewAura::EndDrag(WebKit::WebDragOperationsMask ops) {
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  gfx::Point screen_loc =
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint();
  gfx::Point client_loc = screen_loc;
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  aura::Window* window = rvh->GetView()->GetNativeView();
  aura::Window::ConvertPointToTarget(root_window, window, &client_loc);
  if (!web_contents_)
    return;
  web_contents_->DragSourceEndedAt(client_loc.x(), client_loc.y(),
      screen_loc.x(), screen_loc.y(), ops);
}

void WebContentsViewAura::PrepareOverscrollWindow() {
  // If there is an existing |overscroll_window_| which is in the middle of an
  // animation, then destroying the window here causes the animation to be
  // completed immidiately, which triggers |OnImplicitAnimationsCompleted()|
  // callback, and that tries to reset |overscroll_window_| again, causing a
  // double-free. So use a temporary variable here.
  if (overscroll_window_) {
    base::AutoReset<OverscrollMode> reset_state(&current_overscroll_gesture_,
                                                current_overscroll_gesture_);
    scoped_ptr<aura::Window> reset_window(overscroll_window_.release());
  }

  OverscrollWindowDelegate* overscroll_delegate = new OverscrollWindowDelegate(
      web_contents_,
      current_overscroll_gesture_);
  overscroll_window_.reset(new aura::Window(overscroll_delegate));
  overscroll_window_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  overscroll_window_->SetTransparent(true);
  overscroll_window_->Init(ui::LAYER_TEXTURED);
  overscroll_window_->layer()->SetMasksToBounds(false);
  overscroll_window_->SetName("OverscrollOverlay");

  overscroll_change_brightness_ = overscroll_delegate->has_image();
  window_->AddChild(overscroll_window_.get());

  gfx::Rect bounds = gfx::Rect(window_->bounds().size());
  if (ShouldNavigateForward(web_contents_->GetController(),
                            current_overscroll_gesture_)) {
    // The overlay will be sliding in from the right edge towards the left in
    // non-RTL, or sliding in from the left edge towards the right in RTL.
    // So position the overlay window accordingly.
    bounds.Offset(base::i18n::IsRTL() ? -bounds.width() : bounds.width(), 0);
  }

  aura::Window* animate_window = GetWindowToAnimateForOverscroll();
  if (animate_window == overscroll_window_)
    window_->StackChildAbove(overscroll_window_.get(), GetContentNativeView());
  else
    window_->StackChildBelow(overscroll_window_.get(), GetContentNativeView());

  UpdateOverscrollWindowBrightness(0.f);

  overscroll_window_->SetBounds(bounds);
  overscroll_window_->Show();

  overscroll_shadow_.reset(new ShadowLayerDelegate(animate_window->layer()));
}

void WebContentsViewAura::PrepareContentWindowForOverscroll() {
  StopObservingImplicitAnimations();
  aura::Window* content = GetContentNativeView();
  content->layer()->GetAnimator()->AbortAllAnimations();
  content->SetTransform(gfx::Transform());
  content->layer()->SetLayerBrightness(0.f);
}

void WebContentsViewAura::ResetOverscrollTransform() {
  if (!web_contents_->GetRenderWidgetHostView())
    return;
  aura::Window* target = GetWindowToAnimateForOverscroll();
  if (!target)
    return;
  {
    ui::ScopedLayerAnimationSettings settings(target->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    settings.AddObserver(this);
    target->SetTransform(gfx::Transform());
  }
  {
    ui::ScopedLayerAnimationSettings settings(target->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.SetTweenType(gfx::Tween::EASE_OUT);
    UpdateOverscrollWindowBrightness(0.f);
  }
}

void WebContentsViewAura::CompleteOverscrollNavigation(OverscrollMode mode) {
  if (!web_contents_->GetRenderWidgetHostView())
    return;

  // Animate out the current view first. Navigate to the requested history at
  // the end of the animation.
  if (current_overscroll_gesture_ == OVERSCROLL_NONE)
    return;

  UMA_HISTOGRAM_ENUMERATION("Overscroll.Navigated",
                            current_overscroll_gesture_, OVERSCROLL_COUNT);
  OverscrollWindowDelegate* delegate = static_cast<OverscrollWindowDelegate*>(
      overscroll_window_->delegate());
  delegate->stop_forwarding_events();

  completed_overscroll_gesture_ = mode;
  aura::Window* target = GetWindowToAnimateForOverscroll();
  ui::ScopedLayerAnimationSettings settings(target->layer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTweenType(gfx::Tween::EASE_OUT);
  settings.AddObserver(this);
  gfx::Transform transform;
  int content_width =
      web_contents_->GetRenderWidgetHostView()->GetViewBounds().width();
  int translate_x = mode == OVERSCROLL_WEST ? -content_width : content_width;
  transform.Translate(translate_x, 0);
  target->SetTransform(transform);
  UpdateOverscrollWindowBrightness(translate_x);
}

aura::Window* WebContentsViewAura::GetWindowToAnimateForOverscroll() {
  if (current_overscroll_gesture_ == OVERSCROLL_NONE)
    return NULL;

  return ShouldNavigateForward(web_contents_->GetController(),
                               current_overscroll_gesture_) ?
      overscroll_window_.get() : GetContentNativeView();
}

gfx::Vector2d WebContentsViewAura::GetTranslationForOverscroll(int delta_x,
                                                               int delta_y) {
  if (current_overscroll_gesture_ == OVERSCROLL_NORTH ||
      current_overscroll_gesture_ == OVERSCROLL_SOUTH) {
    return gfx::Vector2d(0, delta_y);
  }
  // For horizontal overscroll, scroll freely if a navigation is possible. Do a
  // resistive scroll otherwise.
  const NavigationControllerImpl& controller = web_contents_->GetController();
  const gfx::Rect& bounds = GetViewBounds();
  if (ShouldNavigateForward(controller, current_overscroll_gesture_))
    return gfx::Vector2d(std::max(-bounds.width(), delta_x), 0);
  else if (ShouldNavigateBack(controller, current_overscroll_gesture_))
    return gfx::Vector2d(std::min(bounds.width(), delta_x), 0);
  return gfx::Vector2d();
}

void WebContentsViewAura::PrepareOverscrollNavigationOverlay() {
  OverscrollWindowDelegate* delegate = static_cast<OverscrollWindowDelegate*>(
      overscroll_window_->delegate());
  overscroll_window_->SchedulePaintInRect(
      gfx::Rect(overscroll_window_->bounds().size()));
  overscroll_window_->SetBounds(gfx::Rect(window_->bounds().size()));
  overscroll_window_->SetTransform(gfx::Transform());
  navigation_overlay_->SetOverlayWindow(overscroll_window_.Pass(),
                                        delegate);
  navigation_overlay_->StartObservingView(ToRenderWidgetHostViewAura(
      web_contents_->GetRenderWidgetHostView()));
}

void WebContentsViewAura::UpdateOverscrollWindowBrightness(float delta_x) {
  if (!overscroll_change_brightness_)
    return;

  const float kBrightnessMin = -.1f;
  const float kBrightnessMax = -.01f;

  float ratio = fabs(delta_x) / GetViewBounds().width();
  ratio = std::min(1.f, ratio);
  if (base::i18n::IsRTL())
    ratio = 1.f - ratio;
  float brightness = current_overscroll_gesture_ == OVERSCROLL_WEST ?
      kBrightnessMin + ratio * (kBrightnessMax - kBrightnessMin) :
      kBrightnessMax - ratio * (kBrightnessMax - kBrightnessMin);
  brightness = std::max(kBrightnessMin, brightness);
  brightness = std::min(kBrightnessMax, brightness);
  aura::Window* window = GetWindowToAnimateForOverscroll();
  window->layer()->SetLayerBrightness(brightness);
}

void WebContentsViewAura::AttachTouchEditableToRenderView() {
  if (!touch_editable_)
    return;
  RenderWidgetHostViewAura* rwhva = ToRenderWidgetHostViewAura(
      web_contents_->GetRenderWidgetHostView());
  touch_editable_->AttachToView(rwhva);
}

void WebContentsViewAura::OverscrollUpdateForWebContentsDelegate(int delta_y) {
  if (web_contents_->GetDelegate() && IsScrollEndEffectEnabled())
    web_contents_->GetDelegate()->OverscrollUpdate(delta_y);
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, WebContentsView implementation:

gfx::NativeView WebContentsViewAura::GetNativeView() const {
  return window_.get();
}

gfx::NativeView WebContentsViewAura::GetContentNativeView() const {
  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  return rwhv ? rwhv->GetNativeView() : NULL;
}

gfx::NativeWindow WebContentsViewAura::GetTopLevelNativeWindow() const {
  return window_->GetToplevelWindow();
}

void WebContentsViewAura::GetContainerBounds(gfx::Rect *out) const {
  *out = window_->GetBoundsInScreen();
}

void WebContentsViewAura::OnTabCrashed(base::TerminationStatus status,
                                       int error_code) {
  // Set the focus to the parent because neither the view window nor this
  // window can handle key events.
  if (window_->HasFocus() && window_->parent())
    window_->parent()->Focus();
}

void WebContentsViewAura::SizeContents(const gfx::Size& size) {
  gfx::Rect bounds = window_->bounds();
  if (bounds.size() != size) {
    bounds.set_size(size);
    window_->SetBounds(bounds);
  } else {
    // Our size matches what we want but the renderers size may not match.
    // Pretend we were resized so that the renderers size is updated too.
    SizeChangedCommon(size);
  }
}

void WebContentsViewAura::Focus() {
  if (web_contents_->GetInterstitialPage()) {
    web_contents_->GetInterstitialPage()->Focus();
    return;
  }

  if (delegate_.get() && delegate_->Focus())
    return;

  RenderWidgetHostView* rwhv = web_contents_->GetRenderWidgetHostView();
  if (rwhv)
    rwhv->Focus();
}

void WebContentsViewAura::SetInitialFocus() {
  if (web_contents_->FocusLocationBarByDefault())
    web_contents_->SetFocusToLocationBar(false);
  else
    Focus();
}

void WebContentsViewAura::StoreFocus() {
  if (delegate_)
    delegate_->StoreFocus();
}

void WebContentsViewAura::RestoreFocus() {
  if (delegate_)
    delegate_->RestoreFocus();
}

DropData* WebContentsViewAura::GetDropData() const {
  return current_drop_data_.get();
}

gfx::Rect WebContentsViewAura::GetViewBounds() const {
  return window_->GetBoundsInScreen();
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, WebContentsViewPort implementation:

void WebContentsViewAura::CreateView(
    const gfx::Size& initial_size, gfx::NativeView context) {
  // NOTE: we ignore |initial_size| since in some cases it's wrong (such as
  // if the bookmark bar is not shown and you create a new tab). The right
  // value is set shortly after this, so its safe to ignore.

  window_.reset(new aura::Window(this));
  window_->set_owned_by_parent(false);
  window_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  window_->SetTransparent(false);
  window_->Init(ui::LAYER_NOT_DRAWN);
  aura::RootWindow* root_window = context ? context->GetRootWindow() : NULL;
  if (root_window) {
    // There are places where there is no context currently because object
    // hierarchies are built before they're attached to a Widget. (See
    // views::WebView as an example; GetWidget() returns NULL at the point
    // where we are created.)
    //
    // It should be OK to not set a default parent since such users will
    // explicitly add this WebContentsViewAura to their tree after they create
    // us.
    if (root_window) {
      window_->SetDefaultParentByRootWindow(
          root_window, root_window->GetBoundsInScreen());
    }
  }
  window_->layer()->SetMasksToBounds(true);
  window_->SetName("WebContentsViewAura");

  window_observer_.reset(new WindowObserver(this));
#if defined(OS_WIN)
  child_window_observer_.reset(new ChildWindowObserver(this));
#endif

  // delegate_->GetDragDestDelegate() creates a new delegate on every call.
  // Hence, we save a reference to it locally. Similar model is used on other
  // platforms as well.
  if (delegate_)
    drag_dest_delegate_ = delegate_->GetDragDestDelegate();
}

RenderWidgetHostView* WebContentsViewAura::CreateViewForWidget(
    RenderWidgetHost* render_widget_host) {
  if (render_widget_host->GetView()) {
    // During testing, the view will already be set up in most cases to the
    // test view, so we don't want to clobber it with a real one. To verify that
    // this actually is happening (and somebody isn't accidentally creating the
    // view twice), we check for the RVH Factory, which will be set when we're
    // making special ones (which go along with the special views).
    DCHECK(RenderViewHostFactory::has_factory());
    return render_widget_host->GetView();
  }

  RenderWidgetHostView* view =
      RenderWidgetHostView::CreateViewForWidget(render_widget_host);
  view->InitAsChild(NULL);
  GetNativeView()->AddChild(view->GetNativeView());

  if (navigation_overlay_.get() && navigation_overlay_->has_window()) {
    navigation_overlay_->StartObservingView(ToRenderWidgetHostViewAura(view));
  }

  RenderWidgetHostImpl* host_impl =
      RenderWidgetHostImpl::From(render_widget_host);

  if (!host_impl->is_hidden())
    view->Show();

  // We listen to drag drop events in the newly created view's window.
  aura::client::SetDragDropDelegate(view->GetNativeView(), this);

  if (host_impl->overscroll_controller() &&
      (!web_contents_->GetDelegate() ||
       web_contents_->GetDelegate()->CanOverscrollContent())) {
    host_impl->overscroll_controller()->set_delegate(this);
    if (!navigation_overlay_)
      navigation_overlay_.reset(new OverscrollNavigationOverlay(web_contents_));
  }

  AttachTouchEditableToRenderView();
  return view;
}

RenderWidgetHostView* WebContentsViewAura::CreateViewForPopupWidget(
    RenderWidgetHost* render_widget_host) {
  return RenderWidgetHostViewPort::CreateViewForWidget(render_widget_host);
}

void WebContentsViewAura::SetPageTitle(const string16& title) {
  window_->set_title(title);
}

void WebContentsViewAura::RenderViewCreated(RenderViewHost* host) {
}

void WebContentsViewAura::RenderViewSwappedIn(RenderViewHost* host) {
  if (navigation_overlay_.get() && navigation_overlay_->has_window()) {
    navigation_overlay_->StartObservingView(
        ToRenderWidgetHostViewAura(host->GetView()));
  }
  AttachTouchEditableToRenderView();
}

void WebContentsViewAura::SetOverscrollControllerEnabled(bool enabled) {
  RenderViewHostImpl* host = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  if (host) {
    host->SetOverscrollControllerEnabled(enabled);
    if (enabled)
      host->overscroll_controller()->set_delegate(this);
  }

  if (!enabled)
    navigation_overlay_.reset();
  else if (!navigation_overlay_)
    navigation_overlay_.reset(new OverscrollNavigationOverlay(web_contents_));
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, RenderViewHostDelegateView implementation:

void WebContentsViewAura::ShowContextMenu(const ContextMenuParams& params) {
  if (touch_editable_)
    touch_editable_->EndTouchEditing();
  if (delegate_) {
    delegate_->ShowContextMenu(params);
    // WARNING: we may have been deleted during the call to ShowContextMenu().
  }
}

void WebContentsViewAura::ShowPopupMenu(const gfx::Rect& bounds,
                                        int item_height,
                                        double item_font_size,
                                        int selected_item,
                                        const std::vector<MenuItem>& items,
                                        bool right_aligned,
                                        bool allow_multiple_selection) {
  // External popup menus are only used on Mac and Android.
  NOTIMPLEMENTED();
}

void WebContentsViewAura::StartDragging(
    const DropData& drop_data,
    WebKit::WebDragOperationsMask operations,
    const gfx::ImageSkia& image,
    const gfx::Vector2d& image_offset,
    const DragEventSourceInfo& event_info) {
  aura::RootWindow* root_window = GetNativeView()->GetRootWindow();
  if (!aura::client::GetDragDropClient(root_window)) {
    web_contents_->SystemDragEnded();
    return;
  }

  if (touch_editable_)
    touch_editable_->EndTouchEditing();

  ui::OSExchangeData::Provider* provider = ui::OSExchangeData::CreateProvider();
  PrepareDragData(drop_data, provider);

  ui::OSExchangeData data(provider);  // takes ownership of |provider|.

  if (!image.isNull()) {
    drag_utils::SetDragImageOnDataObject(image,
        gfx::Size(image.width(), image.height()), image_offset, &data);
  }

  scoped_ptr<WebDragSourceAura> drag_source(
      new WebDragSourceAura(GetNativeView(), web_contents_));

  // We need to enable recursive tasks on the message loop so we can get
  // updates while in the system DoDragDrop loop.
  int result_op = 0;
  {
    gfx::NativeView content_native_view = GetContentNativeView();
    base::MessageLoop::ScopedNestableTaskAllower allow(
        base::MessageLoop::current());
    result_op = aura::client::GetDragDropClient(root_window)
        ->StartDragAndDrop(data,
                           root_window,
                           content_native_view,
                           event_info.event_location,
                           ConvertFromWeb(operations),
                           event_info.event_source);
  }

  // Bail out immediately if the contents view window is gone. Note that it is
  // not safe to access any class members in this case since |this| may already
  // be destroyed. The local variable |drag_source| will still be valid though,
  // so we can use it to determine if the window is gone.
  if (!drag_source->window()) {
    // Note that in this case, we don't need to call SystemDragEnded() since the
    // renderer is going away.
    return;
  }

  EndDrag(ConvertToWeb(result_op));
  web_contents_->SystemDragEnded();
}

void WebContentsViewAura::UpdateDragCursor(WebKit::WebDragOperation operation) {
  current_drag_op_ = operation;
}

void WebContentsViewAura::GotFocus() {
  if (web_contents_->GetDelegate())
    web_contents_->GetDelegate()->WebContentsFocused(web_contents_);
}

void WebContentsViewAura::TakeFocus(bool reverse) {
  if (web_contents_->GetDelegate() &&
      !web_contents_->GetDelegate()->TakeFocus(web_contents_, reverse) &&
      delegate_.get()) {
    delegate_->TakeFocus(reverse);
  }
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, OverscrollControllerDelegate implementation:

void WebContentsViewAura::OnOverscrollUpdate(float delta_x, float delta_y) {
  if (current_overscroll_gesture_ == OVERSCROLL_NONE)
    return;

  aura::Window* target = GetWindowToAnimateForOverscroll();
  ui::ScopedLayerAnimationSettings settings(target->layer()->GetAnimator());
  settings.SetPreemptionStrategy(ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
  gfx::Vector2d translate = GetTranslationForOverscroll(delta_x, delta_y);
  gfx::Transform transform;

  // Vertical overscrolls don't participate in the navigation gesture.
  if (current_overscroll_gesture_ != OVERSCROLL_NORTH &&
      current_overscroll_gesture_ != OVERSCROLL_SOUTH) {
    transform.Translate(translate.x(), translate.y());
    target->SetTransform(transform);
    UpdateOverscrollWindowBrightness(delta_x);
  }

  OverscrollUpdateForWebContentsDelegate(translate.y());
}

void WebContentsViewAura::OnOverscrollComplete(OverscrollMode mode) {
  UMA_HISTOGRAM_ENUMERATION("Overscroll.Completed", mode, OVERSCROLL_COUNT);
  OverscrollUpdateForWebContentsDelegate(0);
  NavigationControllerImpl& controller = web_contents_->GetController();
  if (ShouldNavigateForward(controller, mode) ||
      ShouldNavigateBack(controller, mode)) {
    CompleteOverscrollNavigation(mode);
    return;
  }

  ResetOverscrollTransform();
}

void WebContentsViewAura::OnOverscrollModeChange(OverscrollMode old_mode,
                                                 OverscrollMode new_mode) {
  // Reset any in-progress overscroll animation first.
  ResetOverscrollTransform();

  if (new_mode != OVERSCROLL_NONE && touch_editable_)
    touch_editable_->OverscrollStarted();

  if (new_mode == OVERSCROLL_NONE ||
      !GetContentNativeView() ||
      ((new_mode == OVERSCROLL_EAST || new_mode == OVERSCROLL_WEST) &&
       navigation_overlay_.get() && navigation_overlay_->has_window())) {
    current_overscroll_gesture_ = OVERSCROLL_NONE;
    OverscrollUpdateForWebContentsDelegate(0);
  } else {
    aura::Window* target = GetWindowToAnimateForOverscroll();
    if (target) {
      StopObservingImplicitAnimations();
      target->layer()->GetAnimator()->AbortAllAnimations();
    }
    // Cleanup state of the content window first, because that can reset the
    // value of |current_overscroll_gesture_|.
    PrepareContentWindowForOverscroll();

    current_overscroll_gesture_ = new_mode;
    if (current_overscroll_gesture_ == OVERSCROLL_EAST ||
        current_overscroll_gesture_ == OVERSCROLL_WEST)
      PrepareOverscrollWindow();

    UMA_HISTOGRAM_ENUMERATION("Overscroll.Started", new_mode, OVERSCROLL_COUNT);
  }
  completed_overscroll_gesture_ = OVERSCROLL_NONE;
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, ui::ImplicitAnimationObserver implementation:

void WebContentsViewAura::OnImplicitAnimationsCompleted() {
  overscroll_shadow_.reset();

  if (ShouldNavigateForward(web_contents_->GetController(),
                            completed_overscroll_gesture_)) {
    PrepareOverscrollNavigationOverlay();
    web_contents_->GetController().GoForward();
  } else if (ShouldNavigateBack(web_contents_->GetController(),
                                completed_overscroll_gesture_)) {
    PrepareOverscrollNavigationOverlay();
    web_contents_->GetController().GoBack();
  } else {
    if (touch_editable_)
      touch_editable_->OverscrollCompleted();
  }

  aura::Window* content = GetContentNativeView();
  if (content) {
    content->SetTransform(gfx::Transform());
    content->layer()->SetLayerBrightness(0.f);
  }
  current_overscroll_gesture_ = OVERSCROLL_NONE;
  completed_overscroll_gesture_ = OVERSCROLL_NONE;
  overscroll_window_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, aura::WindowDelegate implementation:

gfx::Size WebContentsViewAura::GetMinimumSize() const {
  return gfx::Size();
}

gfx::Size WebContentsViewAura::GetMaximumSize() const {
  return gfx::Size();
}

void WebContentsViewAura::OnBoundsChanged(const gfx::Rect& old_bounds,
                                          const gfx::Rect& new_bounds) {
  SizeChangedCommon(new_bounds.size());
  if (delegate_)
    delegate_->SizeChanged(new_bounds.size());

  // Constrained web dialogs, need to be kept centered over our content area.
  for (size_t i = 0; i < window_->children().size(); i++) {
    if (window_->children()[i]->GetProperty(
            aura::client::kConstrainedWindowKey)) {
      gfx::Rect bounds = window_->children()[i]->bounds();
      bounds.set_origin(
          gfx::Point((new_bounds.width() - bounds.width()) / 2,
                     (new_bounds.height() - bounds.height()) / 2));
      window_->children()[i]->SetBounds(bounds);
    }
  }
}

gfx::NativeCursor WebContentsViewAura::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int WebContentsViewAura::GetNonClientComponent(const gfx::Point& point) const {
  return HTCLIENT;
}

bool WebContentsViewAura::ShouldDescendIntoChildForEventHandling(
    aura::Window* child,
    const gfx::Point& location) {
  return true;
}

bool WebContentsViewAura::CanFocus() {
  // Do not take the focus if the render widget host view is gone because
  // neither the view window nor this window can handle key events.
  return web_contents_->GetRenderWidgetHostView() != NULL;
}

void WebContentsViewAura::OnCaptureLost() {
}

void WebContentsViewAura::OnPaint(gfx::Canvas* canvas) {
}

void WebContentsViewAura::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void WebContentsViewAura::OnWindowDestroying() {
  // This means the destructor is going to be called soon. If there is an
  // overscroll gesture in progress (i.e. |overscroll_window_| is not NULL),
  // then destroying it in the WebContentsViewAura destructor can trigger other
  // virtual functions to be called (e.g. OnImplicitAnimationsCompleted()). So
  // destroy the overscroll window here.
  navigation_overlay_.reset();
  overscroll_window_.reset();
}

void WebContentsViewAura::OnWindowDestroyed() {
}

void WebContentsViewAura::OnWindowTargetVisibilityChanged(bool visible) {
  if (visible)
    web_contents_->WasShown();
  else
    web_contents_->WasHidden();
}

bool WebContentsViewAura::HasHitTestMask() const {
  return false;
}

void WebContentsViewAura::GetHitTestMask(gfx::Path* mask) const {
}

void WebContentsViewAura::DidRecreateLayer(ui::Layer *old_layer,
                                           ui::Layer *new_layer) {
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, ui::EventHandler implementation:

void WebContentsViewAura::OnKeyEvent(ui::KeyEvent* event) {
}

void WebContentsViewAura::OnMouseEvent(ui::MouseEvent* event) {
  if (!web_contents_->GetDelegate())
    return;

  switch (event->type()) {
    case ui::ET_MOUSE_PRESSED:
      web_contents_->GetDelegate()->ActivateContents(web_contents_);
      break;
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_EXITED:
      web_contents_->GetDelegate()->ContentsMouseEvent(
          web_contents_,
          gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint(),
          event->type() == ui::ET_MOUSE_MOVED);
      break;
    default:
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// WebContentsViewAura, aura::client::DragDropDelegate implementation:

void WebContentsViewAura::OnDragEntered(const ui::DropTargetEvent& event) {
  if (drag_dest_delegate_)
    drag_dest_delegate_->DragInitialize(web_contents_);

  current_drop_data_.reset(new DropData());

  PrepareDropData(current_drop_data_.get(), event.data());
  WebKit::WebDragOperationsMask op = ConvertToWeb(event.source_operations());

  gfx::Point screen_pt =
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint();
  current_rvh_for_drag_ = web_contents_->GetRenderViewHost();
  web_contents_->GetRenderViewHost()->DragTargetDragEnter(
      *current_drop_data_.get(), event.location(), screen_pt, op,
      ConvertAuraEventFlagsToWebInputEventModifiers(event.flags()));

  if (drag_dest_delegate_) {
    drag_dest_delegate_->OnReceiveDragData(event.data());
    drag_dest_delegate_->OnDragEnter();
  }
}

int WebContentsViewAura::OnDragUpdated(const ui::DropTargetEvent& event) {
  DCHECK(current_rvh_for_drag_);
  if (current_rvh_for_drag_ != web_contents_->GetRenderViewHost())
    OnDragEntered(event);

  WebKit::WebDragOperationsMask op = ConvertToWeb(event.source_operations());
  gfx::Point screen_pt =
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint();
  web_contents_->GetRenderViewHost()->DragTargetDragOver(
      event.location(), screen_pt, op,
      ConvertAuraEventFlagsToWebInputEventModifiers(event.flags()));

  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDragOver();

  return ConvertFromWeb(current_drag_op_);
}

void WebContentsViewAura::OnDragExited() {
  DCHECK(current_rvh_for_drag_);
  if (current_rvh_for_drag_ != web_contents_->GetRenderViewHost())
    return;

  web_contents_->GetRenderViewHost()->DragTargetDragLeave();
  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDragLeave();

  current_drop_data_.reset();
}

int WebContentsViewAura::OnPerformDrop(const ui::DropTargetEvent& event) {
  DCHECK(current_rvh_for_drag_);
  if (current_rvh_for_drag_ != web_contents_->GetRenderViewHost())
    OnDragEntered(event);

  web_contents_->GetRenderViewHost()->DragTargetDrop(
      event.location(),
      gfx::Screen::GetScreenFor(GetNativeView())->GetCursorScreenPoint(),
      ConvertAuraEventFlagsToWebInputEventModifiers(event.flags()));
  if (drag_dest_delegate_)
    drag_dest_delegate_->OnDrop();
  current_drop_data_.reset();
  return current_drag_op_;
}

}  // namespace content
