// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_
#define CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_

#include "base/callback_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/common/file_chooser_params.h"
#include "content/public/common/page_zoom.h"
#include "content/public/common/stop_find_action.h"
#include "third_party/WebKit/public/web/WebDragOperation.h"

class GURL;
struct WebPreferences;

namespace gfx {
class Point;
}

namespace base {
class FilePath;
class Value;
}

namespace ui {
struct SelectedFileInfo;
}

namespace WebKit {
struct WebFindOptions;
struct WebMediaPlayerAction;
struct WebPluginAction;
}

namespace content {

class ChildProcessSecurityPolicy;
class RenderProcessHost;
class RenderViewHostDelegate;
class SessionStorageNamespace;
class SiteInstance;
struct CustomContextMenuContext;
struct DropData;

// A RenderViewHost is responsible for creating and talking to a RenderView
// object in a child process. It exposes a high level API to users, for things
// like loading pages, adjusting the display and other browser functionality,
// which it translates into IPC messages sent over the IPC channel with the
// RenderView. It responds to all IPC messages sent by that RenderView and
// cracks them, calling a delegate object back with higher level types where
// possible.
//
// The intent of this interface is to provide a view-agnostic communication
// conduit with a renderer. This is so we can build HTML views not only as
// WebContents (see WebContents for an example) but also as views, etc.
class CONTENT_EXPORT RenderViewHost : virtual public RenderWidgetHost {
 public:
  // Returns the RenderViewHost given its ID and the ID of its render process.
  // Returns NULL if the IDs do not correspond to a live RenderViewHost.
  static RenderViewHost* FromID(int render_process_id, int render_view_id);

  // Downcasts from a RenderWidgetHost to a RenderViewHost.  Required
  // because RenderWidgetHost is a virtual base class.
  static RenderViewHost* From(RenderWidgetHost* rwh);

  // Checks that the given renderer can request |url|, if not it sets it to
  // about:blank.
  // |empty_allowed| must be set to false for navigations for security reasons.
  static void FilterURL(const RenderProcessHost* process,
                        bool empty_allowed,
                        GURL* url);

  // Adds/removes a callback called on creation of each new RenderViewHost.
  typedef base::Callback<void(RenderViewHost*)> CreatedCallback;
  static void AddCreatedCallback(const CreatedCallback& callback);
  static void RemoveCreatedCallback(const CreatedCallback& callback);

  virtual ~RenderViewHost() {}

  // Tell the render view to enable a set of javascript bindings. The argument
  // should be a combination of values from BindingsPolicy.
  virtual void AllowBindings(int binding_flags) = 0;

  // Tells the renderer to clear the focused node (if any).
  virtual void ClearFocusedNode() = 0;

  // Causes the renderer to close the current page, including running its
  // onunload event handler.  A ClosePage_ACK message will be sent to the
  // ResourceDispatcherHost when it is finished.
  virtual void ClosePage() = 0;

  // Copies the image at location x, y to the clipboard (if there indeed is an
  // image at that location).
  virtual void CopyImageAt(int x, int y) = 0;

  // Notifies the renderer about the result of a desktop notification.
  virtual void DesktopNotificationPermissionRequestDone(
      int callback_context) = 0;
  virtual void DesktopNotificationPostDisplay(int callback_context) = 0;
  virtual void DesktopNotificationPostError(int notification_id,
                                    const string16& message) = 0;
  virtual void DesktopNotificationPostClose(int notification_id,
                                            bool by_user) = 0;
  virtual void DesktopNotificationPostClick(int notification_id) = 0;

  // Notifies the listener that a directory enumeration is complete.
  virtual void DirectoryEnumerationFinished(
      int request_id,
      const std::vector<base::FilePath>& files) = 0;

  // Tells the renderer not to add scrollbars with height and width below a
  // threshold.
  virtual void DisableScrollbarsForThreshold(const gfx::Size& size) = 0;

  // Notifies the renderer that a a drag operation that it started has ended,
  // either in a drop or by being cancelled.
  virtual void DragSourceEndedAt(
      int client_x, int client_y, int screen_x, int screen_y,
      WebKit::WebDragOperation operation) = 0;

  // Notifies the renderer that a drag and drop operation is in progress, with
  // droppable items positioned over the renderer's view.
  virtual void DragSourceMovedTo(
      int client_x, int client_y, int screen_x, int screen_y) = 0;

  // Notifies the renderer that we're done with the drag and drop operation.
  // This allows the renderer to reset some state.
  virtual void DragSourceSystemDragEnded() = 0;

  // D&d drop target messages that get sent to WebKit.
  virtual void DragTargetDragEnter(
      const DropData& drop_data,
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      WebKit::WebDragOperationsMask operations_allowed,
      int key_modifiers) = 0;
  virtual void DragTargetDragOver(
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      WebKit::WebDragOperationsMask operations_allowed,
      int key_modifiers) = 0;
  virtual void DragTargetDragLeave() = 0;
  virtual void DragTargetDrop(const gfx::Point& client_pt,
                              const gfx::Point& screen_pt,
                              int key_modifiers) = 0;

  // Instructs the RenderView to automatically resize and send back updates
  // for the new size.
  virtual void EnableAutoResize(const gfx::Size& min_size,
                                const gfx::Size& max_size) = 0;

  // Turns off auto-resize and gives a new size that the view should be.
  virtual void DisableAutoResize(const gfx::Size& new_size) = 0;

  // Instructs the RenderView to send back updates to the preferred size.
  virtual void EnablePreferredSizeMode() = 0;

  // Executes custom context menu action that was provided from WebKit.
  virtual void ExecuteCustomContextMenuCommand(
      int action, const CustomContextMenuContext& context) = 0;

  // Tells the renderer to perform the given action on the media player
  // located at the given point.
  virtual void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point& location,
      const WebKit::WebMediaPlayerAction& action) = 0;

  // Runs some javascript within the context of a frame in the page.
  virtual void ExecuteJavascriptInWebFrame(const string16& frame_xpath,
                                           const string16& jscript) = 0;

  // Runs some javascript within the context of a frame in the page. The result
  // is sent back via the provided callback.
  typedef base::Callback<void(const base::Value*)> JavascriptResultCallback;
  virtual void ExecuteJavascriptInWebFrameCallbackResult(
      const string16& frame_xpath,
      const string16& jscript,
      const JavascriptResultCallback& callback) = 0;

  // Tells the renderer to perform the given action on the plugin located at
  // the given point.
  virtual void ExecutePluginActionAtLocation(
      const gfx::Point& location, const WebKit::WebPluginAction& action) = 0;

  // Asks the renderer to exit fullscreen
  virtual void ExitFullscreen() = 0;

  // Finds text on a page.
  virtual void Find(int request_id, const string16& search_text,
                    const WebKit::WebFindOptions& options) = 0;

  // Notifies the renderer that the user has closed the FindInPage window
  // (and what action to take regarding the selection).
  virtual void StopFinding(StopFindAction action) = 0;

  // Causes the renderer to invoke the onbeforeunload event handler.  The
  // result will be returned via ViewMsg_ShouldClose. See also ClosePage and
  // SwapOut, which fire the PageUnload event.
  //
  // Set bool for_cross_site_transition when this close is just for the current
  // RenderView in the case of a cross-site transition. False means we're
  // closing the entire tab.
  virtual void FirePageBeforeUnload(bool for_cross_site_transition) = 0;

  // Notifies the Listener that one or more files have been chosen by the user
  // from a file chooser dialog for the form. |permissions| is the file
  // selection mode in which the chooser dialog was created.
  virtual void FilesSelectedInChooser(
      const std::vector<ui::SelectedFileInfo>& files,
      FileChooserParams::Mode permissions) = 0;

  virtual RenderViewHostDelegate* GetDelegate() const = 0;

  // Returns a bitwise OR of bindings types that have been enabled for this
  // RenderView. See BindingsPolicy for details.
  virtual int GetEnabledBindings() const = 0;

  virtual SiteInstance* GetSiteInstance() const = 0;

  // Requests the renderer to evaluate an xpath to a frame and insert css
  // into that frame's document.
  virtual void InsertCSS(const string16& frame_xpath,
                         const std::string& css) = 0;

  // Returns true if the RenderView is active and has not crashed. Virtual
  // because it is overridden by TestRenderViewHost.
  virtual bool IsRenderViewLive() const = 0;

  // Returns true if the RenderView is responsible for displaying a subframe
  // in a different process from its parent page.
  virtual bool IsSubframe() const = 0;

  // Let the renderer know that the menu has been closed.
  virtual void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) = 0;

  // Notification that a move or resize renderer's containing window has
  // started.
  virtual void NotifyMoveOrResizeStarted() = 0;

  // Reloads the current focused frame.
  virtual void ReloadFrame() = 0;

  // Sets the alternate error page URL (link doctor) for the renderer process.
  virtual void SetAltErrorPageURL(const GURL& url) = 0;

  // Sets a property with the given name and value on the Web UI binding object.
  // Must call AllowWebUIBindings() on this renderer first.
  virtual void SetWebUIProperty(const std::string& name,
                                const std::string& value) = 0;

  // Set the zoom level for the current main frame
  virtual void SetZoomLevel(double level) = 0;

  // Changes the zoom level for the current main frame.
  virtual void Zoom(PageZoom zoom) = 0;

  // Send the renderer process the current preferences supplied by the
  // RenderViewHostDelegate.
  virtual void SyncRendererPrefs() = 0;

  virtual void ToggleSpeechInput() = 0;

  // Returns the current WebKit preferences.
  virtual WebPreferences GetWebkitPreferences() = 0;

  // Passes a list of Webkit preferences to the renderer.
  virtual void UpdateWebkitPreferences(const WebPreferences& prefs) = 0;

  // Informs the renderer process of a change in timezone.
  virtual void NotifyTimezoneChange() = 0;

#if defined(OS_ANDROID)
  // Selects and zooms to the find result nearest to the point (x,y)
  // defined in find-in-page coordinates.
  virtual void ActivateNearestFindResult(int request_id, float x, float y) = 0;

  // Asks the renderer to send the rects of the current find matches.
  virtual void RequestFindMatchRects(int current_version) = 0;
#endif

 private:
  // This interface should only be implemented inside content.
  friend class RenderViewHostImpl;
  RenderViewHost() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_RENDER_VIEW_HOST_H_
