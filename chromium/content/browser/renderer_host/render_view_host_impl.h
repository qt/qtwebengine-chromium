// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/process/kill.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/accessibility_node_data.h"
#include "content/common/drag_event_source_info.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/common/javascript_message_type.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/window_container_type.h"
#include "net/base/load_states.h"
#include "third_party/WebKit/public/web/WebAXEnums.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "third_party/WebKit/public/web/WebPopupType.h"
#include "third_party/WebKit/public/web/WebTextDirection.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/window_open_disposition.h"

class SkBitmap;
class ViewMsg_Navigate;
struct AccessibilityHostMsg_EventParams;
struct MediaPlayerAction;
struct ViewHostMsg_CreateWindow_Params;
struct ViewHostMsg_DidFailProvisionalLoadWithError_Params;
struct ViewHostMsg_OpenURL_Params;
struct ViewHostMsg_SelectionBounds_Params;
struct ViewHostMsg_ShowPopup_Params;
struct ViewMsg_Navigate_Params;
struct ViewMsg_PostMessage_Params;
struct ViewMsg_StopFinding_Params;

namespace base {
class ListValue;
}

namespace gfx {
class Range;
}

namespace ui {
struct SelectedFileInfo;
}

namespace content {

class BrowserMediaPlayerManager;
class ChildProcessSecurityPolicyImpl;
class PageState;
class RenderFrameHostDelegate;
class RenderFrameHostImpl;
class RenderWidgetHostDelegate;
class SessionStorageNamespace;
class SessionStorageNamespaceImpl;
class TestRenderViewHost;
struct ContextMenuParams;
struct FileChooserParams;
struct Referrer;
struct ShowDesktopNotificationHostMsgParams;

#if defined(COMPILER_MSVC)
// RenderViewHostImpl is the bottom of a diamond-shaped hierarchy,
// with RenderWidgetHost at the root. VS warns when methods from the
// root are overridden in only one of the base classes and not both
// (in this case, RenderWidgetHostImpl provides implementations of
// many of the methods).  This is a silly warning when dealing with
// pure virtual methods that only have a single implementation in the
// hierarchy above this class, and is safe to ignore in this case.
#pragma warning(push)
#pragma warning(disable: 4250)
#endif

// This implements the RenderViewHost interface that is exposed to
// embedders of content, and adds things only visible to content.
//
// The exact API of this object needs to be more thoroughly designed. Right
// now it mimics what WebContentsImpl exposed, which is a fairly large API and
// may contain things that are not relevant to a common subset of views. See
// also the comment in render_view_host_delegate.h about the size and scope of
// the delegate API.
//
// Right now, the concept of page navigation (both top level and frame) exists
// in the WebContentsImpl still, so if you instantiate one of these elsewhere,
// you will not be able to traverse pages back and forward. We need to determine
// if we want to bring that and other functionality down into this object so it
// can be shared by others.
class CONTENT_EXPORT RenderViewHostImpl
    : public RenderViewHost,
      public RenderWidgetHostImpl {
 public:
  // Convenience function, just like RenderViewHost::FromID.
  static RenderViewHostImpl* FromID(int render_process_id, int render_view_id);

  // |routing_id| could be a valid route id, or it could be MSG_ROUTING_NONE, in
  // which case RenderWidgetHost will create a new one.  |swapped_out| indicates
  // whether the view should initially be swapped out (e.g., for an opener
  // frame being rendered by another process). |hidden| indicates whether the
  // view is initially hidden or visible.
  //
  // The |session_storage_namespace| parameter allows multiple render views and
  // WebContentses to share the same session storage (part of the WebStorage
  // spec) space. This is useful when restoring contentses, but most callers
  // should pass in NULL which will cause a new SessionStorageNamespace to be
  // created.
  RenderViewHostImpl(
      SiteInstance* instance,
      RenderViewHostDelegate* delegate,
      RenderFrameHostDelegate* frame_delegate,
      RenderWidgetHostDelegate* widget_delegate,
      int routing_id,
      int main_frame_routing_id,
      bool swapped_out,
      bool hidden);
  virtual ~RenderViewHostImpl();

  // RenderViewHost implementation.
  virtual void AllowBindings(int binding_flags) OVERRIDE;
  virtual void ClearFocusedNode() OVERRIDE;
  virtual void ClosePage() OVERRIDE;
  virtual void CopyImageAt(int x, int y) OVERRIDE;
  virtual void DesktopNotificationPermissionRequestDone(
      int callback_context) OVERRIDE;
  virtual void DesktopNotificationPostDisplay(int callback_context) OVERRIDE;
  virtual void DesktopNotificationPostError(
      int notification_id,
      const base::string16& message) OVERRIDE;
  virtual void DesktopNotificationPostClose(int notification_id,
                                            bool by_user) OVERRIDE;
  virtual void DesktopNotificationPostClick(int notification_id) OVERRIDE;
  virtual void DirectoryEnumerationFinished(
      int request_id,
      const std::vector<base::FilePath>& files) OVERRIDE;
  virtual void DisableScrollbarsForThreshold(const gfx::Size& size) OVERRIDE;
  virtual void DragSourceEndedAt(
      int client_x, int client_y, int screen_x, int screen_y,
      blink::WebDragOperation operation) OVERRIDE;
  virtual void DragSourceMovedTo(
      int client_x, int client_y, int screen_x, int screen_y) OVERRIDE;
  virtual void DragSourceSystemDragEnded() OVERRIDE;
  virtual void DragTargetDragEnter(
      const DropData& drop_data,
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) OVERRIDE;
  virtual void DragTargetDragOver(
      const gfx::Point& client_pt,
      const gfx::Point& screen_pt,
      blink::WebDragOperationsMask operations_allowed,
      int key_modifiers) OVERRIDE;
  virtual void DragTargetDragLeave() OVERRIDE;
  virtual void DragTargetDrop(const gfx::Point& client_pt,
                              const gfx::Point& screen_pt,
                              int key_modifiers) OVERRIDE;
  virtual void EnableAutoResize(const gfx::Size& min_size,
                                const gfx::Size& max_size) OVERRIDE;
  virtual void DisableAutoResize(const gfx::Size& new_size) OVERRIDE;
  virtual void EnablePreferredSizeMode() OVERRIDE;
  virtual void ExecuteCustomContextMenuCommand(
      int action, const CustomContextMenuContext& context) OVERRIDE;
  virtual void ExecuteMediaPlayerActionAtLocation(
      const gfx::Point& location,
      const blink::WebMediaPlayerAction& action) OVERRIDE;
  virtual void ExecuteJavascriptInWebFrame(
      const base::string16& frame_xpath,
      const base::string16& jscript) OVERRIDE;
  virtual void ExecuteJavascriptInWebFrameCallbackResult(
      const base::string16& frame_xpath,
      const base::string16& jscript,
      const JavascriptResultCallback& callback) OVERRIDE;
  virtual void ExecutePluginActionAtLocation(
      const gfx::Point& location,
      const blink::WebPluginAction& action) OVERRIDE;
  virtual void ExitFullscreen() OVERRIDE;
  virtual void Find(int request_id, const base::string16& search_text,
                    const blink::WebFindOptions& options) OVERRIDE;
  virtual void StopFinding(StopFindAction action) OVERRIDE;
  virtual void FirePageBeforeUnload(bool for_cross_site_transition) OVERRIDE;
  virtual void FilesSelectedInChooser(
      const std::vector<ui::SelectedFileInfo>& files,
      FileChooserParams::Mode permissions) OVERRIDE;
  virtual RenderViewHostDelegate* GetDelegate() const OVERRIDE;
  virtual int GetEnabledBindings() const OVERRIDE;
  virtual SiteInstance* GetSiteInstance() const OVERRIDE;
  virtual void InsertCSS(const base::string16& frame_xpath,
                         const std::string& css) OVERRIDE;
  virtual bool IsRenderViewLive() const OVERRIDE;
  virtual bool IsSubframe() const OVERRIDE;
  virtual void NotifyContextMenuClosed(
      const CustomContextMenuContext& context) OVERRIDE;
  virtual void NotifyMoveOrResizeStarted() OVERRIDE;
  virtual void ReloadFrame() OVERRIDE;
  virtual void SetAltErrorPageURL(const GURL& url) OVERRIDE;
  virtual void SetWebUIProperty(const std::string& name,
                                const std::string& value) OVERRIDE;
  virtual void Zoom(PageZoom zoom) OVERRIDE;
  virtual void SyncRendererPrefs() OVERRIDE;
  virtual void ToggleSpeechInput() OVERRIDE;
  virtual WebPreferences GetWebkitPreferences() OVERRIDE;
  virtual void UpdateWebkitPreferences(
      const WebPreferences& prefs) OVERRIDE;
  virtual void NotifyTimezoneChange() OVERRIDE;
  virtual void GetAudioOutputControllers(
      const GetAudioOutputControllersCallback& callback) const OVERRIDE;

#if defined(OS_ANDROID)
  virtual void ActivateNearestFindResult(int request_id,
                                         float x,
                                         float y) OVERRIDE;
  virtual void RequestFindMatchRects(int current_version) OVERRIDE;
  virtual void DisableFullscreenEncryptedMediaPlayback() OVERRIDE;
#endif

  void set_delegate(RenderViewHostDelegate* d) {
    CHECK(d);  // http://crbug.com/82827
    delegate_ = d;
  }

  // Set up the RenderView child process. Virtual because it is overridden by
  // TestRenderViewHost. If the |frame_name| parameter is non-empty, it is used
  // as the name of the new top-level frame.
  // The |opener_route_id| parameter indicates which RenderView created this
  // (MSG_ROUTING_NONE if none). If |max_page_id| is larger than -1, the
  // RenderView is told to start issuing page IDs at |max_page_id| + 1.
  virtual bool CreateRenderView(const base::string16& frame_name,
                                int opener_route_id,
                                int32 max_page_id);

  base::TerminationStatus render_view_termination_status() const {
    return render_view_termination_status_;
  }

  // Returns the content specific prefs for this RenderViewHost.
  WebPreferences GetWebkitPrefs(const GURL& url);

  // Sends the given navigation message. Use this rather than sending it
  // yourself since this does the internal bookkeeping described below. This
  // function takes ownership of the provided message pointer.
  //
  // If a cross-site request is in progress, we may be suspended while waiting
  // for the onbeforeunload handler, so this function might buffer the message
  // rather than sending it.
  void Navigate(const ViewMsg_Navigate_Params& message);

  // Load the specified URL, this is a shortcut for Navigate().
  void NavigateToURL(const GURL& url);

  // Returns whether navigation messages are currently suspended for this
  // RenderViewHost.  Only true during a cross-site navigation, while waiting
  // for the onbeforeunload handler.
  bool are_navigations_suspended() const { return navigations_suspended_; }

  // Suspends (or unsuspends) any navigation messages from being sent from this
  // RenderViewHost.  This is called when a pending RenderViewHost is created
  // for a cross-site navigation, because we must suspend any navigations until
  // we hear back from the old renderer's onbeforeunload handler.  Note that it
  // is important that only one navigation event happen after calling this
  // method with |suspend| equal to true.  If |suspend| is false and there is
  // a suspended_nav_message_, this will send the message.  This function
  // should only be called to toggle the state; callers should check
  // are_navigations_suspended() first. If |suspend| is false, the time that the
  // user decided the navigation should proceed should be passed as
  // |proceed_time|.
  void SetNavigationsSuspended(bool suspend,
                               const base::TimeTicks& proceed_time);

  // Clears any suspended navigation state after a cross-site navigation is
  // canceled or suspended.  This is important if we later return to this
  // RenderViewHost.
  void CancelSuspendedNavigations();

  // Whether the initial empty page of this view has been accessed by another
  // page, making it unsafe to show the pending URL.  Always false after the
  // first commit.
  bool has_accessed_initial_document() {
    return has_accessed_initial_document_;
  }

  // Whether this RenderViewHost has been swapped out to be displayed by a
  // different process.
  bool is_swapped_out() const { return is_swapped_out_; }

  // Called on the pending RenderViewHost when the network response is ready to
  // commit.  We should ensure that the old RenderViewHost runs its unload
  // handler and determine whether a transfer to a different RenderViewHost is
  // needed.
  void OnCrossSiteResponse(
      const GlobalRequestID& global_request_id,
      bool is_transfer,
      const std::vector<GURL>& transfer_url_chain,
      const Referrer& referrer,
      PageTransition page_transition,
      int64 frame_id,
      bool should_replace_current_entry);

  // Tells the renderer that this RenderView will soon be swapped out, and thus
  // not to create any new modal dialogs until it happens.  This must be done
  // separately so that the PageGroupLoadDeferrers of any current dialogs are no
  // longer on the stack when we attempt to swap it out.
  void SuppressDialogsUntilSwapOut();

  // Tells the renderer that this RenderView is being swapped out for one in a
  // different renderer process.  It should run its unload handler and move to
  // a blank document.  The renderer should preserve the Frame object until it
  // exits, in case we come back.  The renderer can exit if it has no other
  // active RenderViews, but not until WasSwappedOut is called (when it is no
  // longer visible).
  void SwapOut();

  // Called when either the SwapOut request has been acknowledged or has timed
  // out.
  void OnSwappedOut(bool timed_out);

  // Called to notify the renderer that it has been visibly swapped out and
  // replaced by another RenderViewHost, after an earlier call to SwapOut.
  // It is now safe for the process to exit if there are no other active
  // RenderViews.
  void WasSwappedOut();

  // Close the page ignoring whether it has unload events registers.
  // This is called after the beforeunload and unload events have fired
  // and the user has agreed to continue with closing the page.
  void ClosePageIgnoringUnloadEvents();

  // Returns whether this RenderViewHost has an outstanding cross-site request.
  // Cleared when we hear the response and start to swap out the old
  // RenderViewHost, or if we hear a commit here without a network request.
  bool HasPendingCrossSiteRequest();

  // Sets whether this RenderViewHost has an outstanding cross-site request,
  // for which another renderer will need to run an onunload event handler.
  // This is called before the first navigation event for this RenderViewHost,
  // and cleared when we hear the response or commit.
  void SetHasPendingCrossSiteRequest(bool has_pending_request);

  // Notifies the RenderView that the JavaScript message that was shown was
  // closed by the user.
  void JavaScriptDialogClosed(IPC::Message* reply_msg,
                              bool success,
                              const base::string16& user_input);

  // Tells the renderer view to focus the first (last if reverse is true) node.
  void SetInitialFocus(bool reverse);

  // Get html data by serializing all frames of current page with lists
  // which contain all resource links that have local copy.
  // The parameter links contain original URLs of all saved links.
  // The parameter local_paths contain corresponding local file paths of
  // all saved links, which matched with vector:links one by one.
  // The parameter local_directory_name is relative path of directory which
  // contain all saved auxiliary files included all sub frames and resouces.
  void GetSerializedHtmlDataForCurrentPageWithLocalLinks(
      const std::vector<GURL>& links,
      const std::vector<base::FilePath>& local_paths,
      const base::FilePath& local_directory_name);

  // Notifies the RenderViewHost that its load state changed.
  void LoadStateChanged(const GURL& url,
                        const net::LoadStateWithParam& load_state,
                        uint64 upload_position,
                        uint64 upload_size);

  bool SuddenTerminationAllowed() const;
  void set_sudden_termination_allowed(bool enabled) {
    sudden_termination_allowed_ = enabled;
  }

  // RenderWidgetHost public overrides.
  virtual void Init() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual bool IsRenderView() const OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void GotFocus() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  virtual void LostMouseLock() OVERRIDE;
  virtual void ForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) OVERRIDE;
  virtual void OnPointerEventActivate() OVERRIDE;
  virtual void ForwardKeyboardEvent(
      const NativeWebKeyboardEvent& key_event) OVERRIDE;
  virtual gfx::Rect GetRootWindowResizerRect() const OVERRIDE;

  // Creates a new RenderView with the given route id.
  void CreateNewWindow(
      int route_id,
      int main_frame_route_id,
      const ViewHostMsg_CreateWindow_Params& params,
      SessionStorageNamespace* session_storage_namespace);

  // Creates a new RenderWidget with the given route id.  |popup_type| indicates
  // if this widget is a popup and what kind of popup it is (select, autofill).
  void CreateNewWidget(int route_id, blink::WebPopupType popup_type);

  // Creates a full screen RenderWidget.
  void CreateNewFullscreenWidget(int route_id);

#if defined(OS_MACOSX)
  // Select popup menu related methods (for external popup menus).
  void DidSelectPopupMenuItem(int selected_index);
  void DidCancelPopupMenu();
#endif

#if defined(OS_ANDROID)
  BrowserMediaPlayerManager* media_player_manager() {
    return media_player_manager_.get();
  }

  void DidSelectPopupMenuItems(const std::vector<int>& selected_indices);
  void DidCancelPopupMenu();
#endif

  // User rotated the screen. Calls the "onorientationchange" Javascript hook.
  void SendOrientationChangeEvent(int orientation);

  // Sets a bit indicating whether the RenderView is responsible for displaying
  // a subframe in a different process from its parent page.
  void set_is_subframe(bool is_subframe) {
    is_subframe_ = is_subframe;
  }

  // TODO(creis): Remove this when we replace frame IDs with RenderFrameHost
  // routing IDs.
  int64 main_frame_id() const {
    return main_frame_id_;
  }

  // Set the opener to null in the renderer process.
  void DisownOpener();

  // Turn on accessibility testing. The given callback will be run
  // every time an accessibility notification is received from the
  // renderer process, and the accessibility tree it sent can be
  // retrieved using accessibility_tree_for_testing().
  void SetAccessibilityCallbackForTesting(
      const base::Callback<void(blink::WebAXEvent)>& callback);

  // Only valid if SetAccessibilityCallbackForTesting was called and
  // the callback was run at least once. Returns a snapshot of the
  // accessibility tree received from the renderer as of the last time
  // a LoadComplete or LayoutComplete accessibility notification was received.
  const AccessibilityNodeDataTreeNode& accessibility_tree_for_testing() {
    return accessibility_tree_;
  }

  // Set accessibility callbacks.
  void SetAccessibilityLayoutCompleteCallbackForTesting(
      const base::Closure& callback);
  void SetAccessibilityLoadCompleteCallbackForTesting(
      const base::Closure& callback);
  void SetAccessibilityOtherCallbackForTesting(
      const base::Closure& callback);

  bool is_waiting_for_beforeunload_ack() {
    return is_waiting_for_beforeunload_ack_;
  }

  bool is_waiting_for_unload_ack() {
    return is_waiting_for_unload_ack_;
  }

  // Returns whether the given URL is allowed to commit in the current process.
  // This is a more conservative check than FilterURL, since it will be used to
  // kill processes that commit unauthorized URLs.
  bool CanCommitURL(const GURL& url);

  // Checks that the given renderer can request |url|, if not it sets it to
  // about:blank.
  // empty_allowed must be set to false for navigations for security reasons.
  static void FilterURL(ChildProcessSecurityPolicyImpl* policy,
                        const RenderProcessHost* process,
                        bool empty_allowed,
                        GURL* url);

  // Update the FrameTree to use this RenderViewHost's main frame
  // RenderFrameHost. Called when the RenderViewHost is committed.
  //
  // TODO(ajwong): Remove once RenderViewHost no longer owns the main frame
  // RenderFrameHost.
  void AttachToFrameTree();

  // The following IPC handlers are public so RenderFrameHost can call them,
  // while we transition the code to not use RenderViewHost.
  //
  // TODO(nasko): Remove those methods once we are done moving navigation
  // into RenderFrameHost.
  void OnDidStartProvisionalLoadForFrame(int64 frame_id,
                                         int64 parent_frame_id,
                                         bool main_frame,
                                         const GURL& url);

  // NOTE: Do not add functions that just send an IPC message that are called in
  // one or two places. Have the caller send the IPC message directly (unless
  // the caller places are in different platforms, in which case it's better
  // to keep them consistent).

 protected:
  // RenderWidgetHost protected overrides.
  virtual void OnUserGesture() OVERRIDE;
  virtual void NotifyRendererUnresponsive() OVERRIDE;
  virtual void NotifyRendererResponsive() OVERRIDE;
  virtual void OnRenderAutoResized(const gfx::Size& size) OVERRIDE;
  virtual void RequestToLockMouse(bool user_gesture,
                                  bool last_unlocked_by_target) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // IPC message handlers.
  void OnShowView(int route_id,
                  WindowOpenDisposition disposition,
                  const gfx::Rect& initial_pos,
                  bool user_gesture);
  void OnShowWidget(int route_id, const gfx::Rect& initial_pos);
  void OnShowFullscreenWidget(int route_id);
  void OnRunModal(int opener_id, IPC::Message* reply_msg);
  void OnRenderViewReady();
  void OnRenderProcessGone(int status, int error_code);
  void OnDidRedirectProvisionalLoad(int32 page_id,
                                    const GURL& source_url,
                                    const GURL& target_url);
  void OnDidFailProvisionalLoadWithError(
      const ViewHostMsg_DidFailProvisionalLoadWithError_Params& params);
  void OnNavigate(const IPC::Message& msg);
  void OnUpdateState(int32 page_id, const PageState& state);
  void OnUpdateTitle(int32 page_id,
                     const base::string16& title,
                     blink::WebTextDirection title_direction);
  void OnUpdateEncoding(const std::string& encoding);
  void OnUpdateTargetURL(int32 page_id, const GURL& url);
  void OnClose();
  void OnRequestMove(const gfx::Rect& pos);
  void OnDidStartLoading();
  void OnDidStopLoading();
  void OnDidChangeLoadProgress(double load_progress);
  void OnDidDisownOpener();
  void OnDocumentAvailableInMainFrame();
  void OnDocumentOnLoadCompletedInMainFrame(int32 page_id);
  void OnContextMenu(const ContextMenuParams& params);
  void OnToggleFullscreen(bool enter_fullscreen);
  void OnOpenURL(const ViewHostMsg_OpenURL_Params& params);
  void OnDidContentsPreferredSizeChange(const gfx::Size& new_size);
  void OnDidChangeScrollOffset();
  void OnDidChangeScrollbarsForMainFrame(bool has_horizontal_scrollbar,
                                         bool has_vertical_scrollbar);
  void OnDidChangeScrollOffsetPinningForMainFrame(bool is_pinned_to_left,
                                                  bool is_pinned_to_right);
  void OnDidChangeNumWheelEvents(int count);
  void OnSelectionChanged(const base::string16& text,
                          size_t offset,
                          const gfx::Range& range);
  void OnSelectionBoundsChanged(
      const ViewHostMsg_SelectionBounds_Params& params);
  void OnPasteFromSelectionClipboard();
  void OnRouteCloseEvent();
  void OnRouteMessageEvent(const ViewMsg_PostMessage_Params& params);
  void OnRunJavaScriptMessage(const base::string16& message,
                              const base::string16& default_prompt,
                              const GURL& frame_url,
                              JavaScriptMessageType type,
                              IPC::Message* reply_msg);
  void OnRunBeforeUnloadConfirm(const GURL& frame_url,
                                const base::string16& message,
                                bool is_reload,
                                IPC::Message* reply_msg);
  void OnStartDragging(const DropData& drop_data,
                       blink::WebDragOperationsMask operations_allowed,
                       const SkBitmap& bitmap,
                       const gfx::Vector2d& bitmap_offset_in_dip,
                       const DragEventSourceInfo& event_info);
  void OnUpdateDragCursor(blink::WebDragOperation drag_operation);
  void OnTargetDropACK();
  void OnTakeFocus(bool reverse);
  void OnFocusedNodeChanged(bool is_editable_node);
  void OnAddMessageToConsole(int32 level,
                             const base::string16& message,
                             int32 line_no,
                             const base::string16& source_id);
  void OnUpdateInspectorSetting(const std::string& key,
                                const std::string& value);
  void OnShouldCloseACK(
      bool proceed,
      const base::TimeTicks& renderer_before_unload_start_time,
      const base::TimeTicks& renderer_before_unload_end_time);
  void OnClosePageACK();
  void OnSwapOutACK();
  void OnAccessibilityEvents(
      const std::vector<AccessibilityHostMsg_EventParams>& params);
  void OnScriptEvalResponse(int id, const base::ListValue& result);
  void OnDidZoomURL(double zoom_level, bool remember, const GURL& url);
  void OnRequestDesktopNotificationPermission(const GURL& origin,
                                              int callback_id);
  void OnShowDesktopNotification(
      const ShowDesktopNotificationHostMsgParams& params);
  void OnCancelDesktopNotification(int notification_id);
  void OnRunFileChooser(const FileChooserParams& params);
  void OnDidAccessInitialDocument();
  void OnDomOperationResponse(const std::string& json_string,
                              int automation_id);
  void OnFocusedNodeTouched(bool editable);

#if defined(OS_MACOSX) || defined(OS_ANDROID)
  void OnShowPopup(const ViewHostMsg_ShowPopup_Params& params);
#endif

  // TODO(nasko): Remove this accessor once RenderFrameHost moves into the frame
  // tree.
  RenderFrameHostImpl* main_render_frame_host() const {
    return main_render_frame_host_.get();
  }

 private:
  friend class TestRenderViewHost;
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, BasicRenderFrameHost);
  FRIEND_TEST_ALL_PREFIXES(RenderViewHostTest, RoutingIdSane);

  // Sets whether this RenderViewHost is swapped out in favor of another,
  // and clears any waiting state that is no longer relevant.
  void SetSwappedOut(bool is_swapped_out);

  bool CanAccessFilesOfPageState(const PageState& state) const;

  // All RenderViewHosts must have a RenderFrameHost for its main frame.
  // Currently the RenderFrameHost is created in lock step on construction
  // and a pointer to the main frame is given to the FrameTreeNode
  // when the RenderViewHost commits (see AttachToFrameTree()).
  //
  // TODO(ajwong): Make this reference non-owning. The root FrameTreeNode of
  // the FrameTree should be responsible for owning the main frame's
  // RenderFrameHost.
  scoped_ptr<RenderFrameHostImpl> main_render_frame_host_;

  // Our delegate, which wants to know about changes in the RenderView.
  RenderViewHostDelegate* delegate_;

  // The SiteInstance associated with this RenderViewHost.  All pages drawn
  // in this RenderViewHost are part of this SiteInstance.  Should not change
  // over time.
  scoped_refptr<SiteInstanceImpl> instance_;

  // true if we are currently waiting for a response for drag context
  // information.
  bool waiting_for_drag_context_response_;

  // A bitwise OR of bindings types that have been enabled for this RenderView.
  // See BindingsPolicy for details.
  int enabled_bindings_;

  // Whether we should buffer outgoing Navigate messages rather than sending
  // them.  This will be true when a RenderViewHost is created for a cross-site
  // request, until we hear back from the onbeforeunload handler of the old
  // RenderViewHost.
  bool navigations_suspended_;

  // We only buffer the params for a suspended navigation while we have a
  // pending RVH for a WebContentsImpl.  There will only ever be one suspended
  // navigation, because WebContentsImpl will destroy the pending RVH and create
  // a new one if a second navigation occurs.
  scoped_ptr<ViewMsg_Navigate_Params> suspended_nav_params_;

  // Whether the initial empty page of this view has been accessed by another
  // page, making it unsafe to show the pending URL.  Usually false unless
  // another window tries to modify the blank page.  Always false after the
  // first commit.
  bool has_accessed_initial_document_;

  // Whether this RenderViewHost is currently swapped out, such that the view is
  // being rendered by another process.
  bool is_swapped_out_;

  // Whether this RenderView is responsible for displaying a subframe in a
  // different process from its parent page.
  bool is_subframe_;

  // The frame id of the main (top level) frame. This value is set on the
  // initial navigation of a RenderView and reset when the RenderView's
  // process is terminated (in RenderProcessGone).
  int64 main_frame_id_;

  // If we were asked to RunModal, then this will hold the reply_msg that we
  // must return to the renderer to unblock it.
  IPC::Message* run_modal_reply_msg_;
  // This will hold the routing id of the RenderView that opened us.
  int run_modal_opener_id_;

  // Set to true when there is a pending ViewMsg_ShouldClose message.  This
  // ensures we don't spam the renderer with multiple beforeunload requests.
  // When either this value or is_waiting_for_unload_ack_ is true, the value of
  // unload_ack_is_for_cross_site_transition_ indicates whether this is for a
  // cross-site transition or a tab close attempt.
  bool is_waiting_for_beforeunload_ack_;

  // Set to true when there is a pending ViewMsg_Close message.  Also see
  // is_waiting_for_beforeunload_ack_, unload_ack_is_for_cross_site_transition_.
  bool is_waiting_for_unload_ack_;

  // Set to true when waiting for ViewHostMsg_SwapOut_ACK has timed out.
  bool has_timed_out_on_unload_;

  // Valid only when is_waiting_for_beforeunload_ack_ or
  // is_waiting_for_unload_ack_ is true.  This tells us if the unload request
  // is for closing the entire tab ( = false), or only this RenderViewHost in
  // the case of a cross-site transition ( = true).
  bool unload_ack_is_for_cross_site_transition_;

  bool are_javascript_messages_suppressed_;

  // The mapping of pending javascript calls created by
  // ExecuteJavascriptInWebFrameCallbackResult and their corresponding
  // callbacks.
  std::map<int, JavascriptResultCallback> javascript_callbacks_;

  // Accessibility callback for testing.
  base::Callback<void(blink::WebAXEvent)> accessibility_testing_callback_;

  // The most recently received accessibility tree - for testing only.
  AccessibilityNodeDataTreeNode accessibility_tree_;

  // True if the render view can be shut down suddenly.
  bool sudden_termination_allowed_;

  // The termination status of the last render view that terminated.
  base::TerminationStatus render_view_termination_status_;

  // When the last ShouldClose message was sent.
  base::TimeTicks send_should_close_start_time_;

  // Set to true if we requested the on screen keyboard to be displayed.
  bool virtual_keyboard_requested_;

#if defined(OS_ANDROID)
  // Manages all the android mediaplayer objects and handling IPCs for video.
  scoped_ptr<BrowserMediaPlayerManager> media_player_manager_;
#endif

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostImpl);
};

#if defined(COMPILER_MSVC)
#pragma warning(pop)
#endif

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_RENDER_VIEW_HOST_IMPL_H_
