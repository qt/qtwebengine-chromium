// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
#define UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/views/accessibility/native_view_accessibility.h"
#include "ui/views/controls/webview/webview_export.h"
#include "ui/views/view.h"

namespace content {
class SiteInstance;
}

namespace views {

class NativeViewHost;

class WEBVIEW_EXPORT WebView : public View,
                               public content::NotificationObserver,
                               public content::WebContentsDelegate,
                               public content::WebContentsObserver {
 public:
  static const char kViewClassName[];

  explicit WebView(content::BrowserContext* browser_context);
  virtual ~WebView();

  // This creates a WebContents if none is yet associated with this WebView. The
  // WebView owns this implicitly created WebContents.
  content::WebContents* GetWebContents();

  // Creates a WebContents if none is yet assocaited with this WebView, with the
  // specified site instance. The WebView owns this WebContents.
  void CreateWebContentsWithSiteInstance(content::SiteInstance* site_instance);

  // WebView does not assume ownership of WebContents set via this method, only
  // those it implicitly creates via GetWebContents() above.
  void SetWebContents(content::WebContents* web_contents);

  // If |mode| is true, WebView will register itself with WebContents as a
  // WebContentsObserver, monitor for the showing/destruction of fullscreen
  // render widgets, and alter its child view hierarchy to embed the fullscreen
  // widget or restore the normal WebContentsView.
  void SetEmbedFullscreenWidgetMode(bool mode);

  content::WebContents* web_contents() { return web_contents_; }

  content::BrowserContext* browser_context() { return browser_context_; }

  // Loads the initial URL to display in the attached WebContents. Creates the
  // WebContents if none is attached yet. Note that this is intended as a
  // convenience for loading the initial URL, and so URLs are navigated with
  // PAGE_TRANSITION_AUTO_TOPLEVEL, so this is not intended as a general purpose
  // navigation method - use WebContents' API directly.
  void LoadInitialURL(const GURL& url);

  // Controls how the attached WebContents is resized.
  // false = WebContents' views' bounds are updated continuously as the
  //         WebView's bounds change (default).
  // true  = WebContents' views' position is updated continuously but its size
  //         is not (which may result in some clipping or under-painting) until
  //         a continuous size operation completes. This allows for smoother
  //         resizing performance during interactive resizes and animations.
  void SetFastResize(bool fast_resize);

  // Called when the WebContents is focused.
  // TODO(beng): This view should become a WebContentsViewObserver when a
  //             WebContents is attached, and not rely on the delegate to
  //             forward this notification.
  void OnWebContentsFocused(content::WebContents* web_contents);

  // When used to host UI, we need to explicitly allow accelerators to be
  // processed. Default is false.
  void set_allow_accelerators(bool allow_accelerators) {
    allow_accelerators_ = allow_accelerators;
  }

  // Sets the preferred size. If empty, View's implementation of
  // GetPreferredSize() is used.
  void SetPreferredSize(const gfx::Size& preferred_size);

  // Overridden from View:
  virtual const char* GetClassName() const OVERRIDE;

 private:
  // Overridden from View:
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE;
  virtual bool SkipDefaultKeyEventProcessing(
      const ui::KeyEvent& event) OVERRIDE;
  virtual bool IsFocusable() const OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual void WebContentsFocused(content::WebContents* web_contents) OVERRIDE;
  virtual bool EmbedsFullscreenWidget() const OVERRIDE;

  // Overridden from content::WebContentsObserver:
  virtual void DidShowFullscreenWidget(int routing_id) OVERRIDE;
  virtual void DidDestroyFullscreenWidget(int routing_id) OVERRIDE;
  // Workaround for MSVC++ linker bug/feature that requires
  // instantiation of the inline IPC::Listener methods in all translation units.
  virtual void OnChannelConnected(int32 peer_id) OVERRIDE {}
  virtual void OnChannelError() OVERRIDE {}

  void AttachWebContents();
  void DetachWebContents();
  void ReattachForFullscreenChange(bool enter_fullscreen);

  // Create a regular or test web contents (based on whether we're running
  // in a unit test or not).
  content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance);

  NativeViewHost* wcv_holder_;
  scoped_ptr<content::WebContents> wc_owner_;
  content::WebContents* web_contents_;
  // When true, WebView observes WebContents and auto-embeds fullscreen widgets
  // as a child view.
  bool embed_fullscreen_widget_mode_enabled_;
  // Set to true while WebView is embedding a fullscreen widget view as a child
  // view instead of the normal WebContentsView render view.
  bool is_embedding_fullscreen_widget_;
  content::BrowserContext* browser_context_;
  content::NotificationRegistrar registrar_;
  bool allow_accelerators_;
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(WebView);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_WEBVIEW_WEBVIEW_H_
