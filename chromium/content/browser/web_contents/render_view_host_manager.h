// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_
#define CONTENT_BROWSER_WEB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/site_instance_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"


namespace content {
class BrowserContext;
class InterstitialPageImpl;
class NavigationControllerImpl;
class NavigationEntry;
class NavigationEntryImpl;
class RenderViewHost;
class RenderViewHostImpl;
class RenderViewHostManagerTest;
class RenderWidgetHostDelegate;
class RenderWidgetHostView;
class TestWebContents;
class WebUIImpl;

// Manages RenderViewHosts for a WebContentsImpl. Normally there is only one and
// it is easy to do. But we can also have transitions of processes (and hence
// RenderViewHosts) that can get complex.
class CONTENT_EXPORT RenderViewHostManager
    : public RenderViewHostDelegate::RendererManagement,
      public NotificationObserver {
 public:
  // Functions implemented by our owner that we need.
  //
  // TODO(brettw) Clean this up! These are all the functions in WebContentsImpl
  // that are required to run this class. The design should probably be better
  // such that these are more clear.
  //
  // There is additional complexity that some of the functions we need in
  // WebContentsImpl are inherited and non-virtual. These are named with
  // "RenderManager" so that the duplicate implementation of them will be clear.
  class CONTENT_EXPORT Delegate {
   public:
    // Initializes the given renderer if necessary and creates the view ID
    // corresponding to this view host. If this method is not called and the
    // process is not shared, then the WebContentsImpl will act as though the
    // renderer is not running (i.e., it will render "sad tab"). This method is
    // automatically called from LoadURL.
    //
    // If you are attaching to an already-existing RenderView, you should call
    // InitWithExistingID.
    virtual bool CreateRenderViewForRenderManager(
        RenderViewHost* render_view_host, int opener_route_id) = 0;
    virtual void BeforeUnloadFiredFromRenderManager(
        bool proceed, const base::TimeTicks& proceed_time,
        bool* proceed_to_fire_unload) = 0;
    virtual void RenderProcessGoneFromRenderManager(
        RenderViewHost* render_view_host) = 0;
    virtual void UpdateRenderViewSizeForRenderManager() = 0;
    virtual void CancelModalDialogsForRenderManager() = 0;
    virtual void NotifySwappedFromRenderManager(
        RenderViewHost* old_render_view_host) = 0;
    virtual NavigationControllerImpl&
        GetControllerForRenderManager() = 0;

    // Create swapped out RenderViews in the given SiteInstance for each tab in
    // the opener chain of this tab, if any.  This allows the current tab to
    // make cross-process script calls to its opener(s).  Returns the route ID
    // of the immediate opener, if one exists (otherwise MSG_ROUTING_NONE).
    virtual int CreateOpenerRenderViewsForRenderManager(
        SiteInstance* instance) = 0;

    // Creates a WebUI object for the given URL if one applies. Ownership of the
    // returned pointer will be passed to the caller. If no WebUI applies,
    // returns NULL.
    virtual WebUIImpl* CreateWebUIForRenderManager(const GURL& url) = 0;

    // Returns the navigation entry of the current navigation, or NULL if there
    // is none.
    virtual NavigationEntry*
        GetLastCommittedNavigationEntryForRenderManager() = 0;

    // Returns true if the location bar should be focused by default rather than
    // the page contents. The view calls this function when the tab is focused
    // to see what it should do.
    virtual bool FocusLocationBarByDefault() = 0;

    // Focuses the location bar.
    virtual void SetFocusToLocationBar(bool select_all) = 0;

    // Creates a view and sets the size for the specified RVH.
    virtual void CreateViewAndSetSizeForRVH(RenderViewHost* rvh) = 0;

    // Returns true if views created for this delegate should be created in a
    // hidden state.
    virtual bool IsHidden() = 0;

   protected:
    virtual ~Delegate() {}
  };

  // All three delegate pointers must be non-NULL and are not owned by this
  // class.  They must outlive this class. The RenderViewHostDelegate and
  // RenderWidgetHostDelegate are what will be installed into all
  // RenderViewHosts that are created.
  //
  // You must call Init() before using this class.
  RenderViewHostManager(
      RenderViewHostDelegate* render_view_delegate,
      RenderWidgetHostDelegate* render_widget_delegate,
      Delegate* delegate);
  virtual ~RenderViewHostManager();

  // For arguments, see WebContentsImpl constructor.
  void Init(BrowserContext* browser_context,
            SiteInstance* site_instance,
            int routing_id,
            int main_frame_routing_id);

  // Returns the currently active RenderViewHost.
  //
  // This will be non-NULL between Init() and Shutdown(). You may want to NULL
  // check it in many cases, however. Windows can send us messages during the
  // destruction process after it has been shut down.
  RenderViewHostImpl* current_host() const;

  // Returns the view associated with the current RenderViewHost, or NULL if
  // there is no current one.
  RenderWidgetHostView* GetRenderWidgetHostView() const;

  // Returns the pending render view host, or NULL if there is no pending one.
  RenderViewHostImpl* pending_render_view_host() const;

  // Returns the current committed Web UI or NULL if none applies.
  WebUIImpl* web_ui() const { return web_ui_.get(); }

  // Returns the Web UI for the pending navigation, or NULL of none applies.
  WebUIImpl* pending_web_ui() const {
    return pending_web_ui_.get() ? pending_web_ui_.get() :
                                   pending_and_current_web_ui_.get();
  }

  // Sets the pending Web UI for the pending navigation, ensuring that the
  // bindings are appropriate for the given NavigationEntry.
  void SetPendingWebUI(const NavigationEntryImpl& entry);

  // Called when we want to instruct the renderer to navigate to the given
  // navigation entry. It may create a new RenderViewHost or re-use an existing
  // one. The RenderViewHost to navigate will be returned. Returns NULL if one
  // could not be created.
  RenderViewHostImpl* Navigate(const NavigationEntryImpl& entry);

  // Instructs the various live views to stop. Called when the user directed the
  // page to stop loading.
  void Stop();

  // Notifies the regular and pending RenderViewHosts that a load is or is not
  // happening. Even though the message is only for one of them, we don't know
  // which one so we tell both.
  void SetIsLoading(bool is_loading);

  // Whether to close the tab or not when there is a hang during an unload
  // handler. If we are mid-crosssite navigation, then we should proceed
  // with the navigation instead of closing the tab.
  bool ShouldCloseTabOnUnresponsiveRenderer();

  // The RenderViewHost has been swapped out, so we should resume the pending
  // network response and allow the pending RenderViewHost to commit.
  void SwappedOut(RenderViewHost* render_view_host);

  // Called when a renderer's main frame navigates.
  void DidNavigateMainFrame(RenderViewHost* render_view_host);

  // Called when a renderer sets its opener to null.
  void DidDisownOpener(RenderViewHost* render_view_host);

  // Helper method to create a RenderViewHost.  If |swapped_out| is true, it
  // will be initially placed on the swapped out hosts list.  Otherwise, it
  // will be used for a pending cross-site navigation.
  int CreateRenderView(SiteInstance* instance,
                       int opener_route_id,
                       bool swapped_out,
                       bool hidden);

  // Called when a provisional load on the given renderer is aborted.
  void RendererAbortedProvisionalLoad(RenderViewHost* render_view_host);

  // Sets the passed passed interstitial as the currently showing interstitial.
  // |interstitial_page| should be non NULL (use the remove_interstitial_page
  // method to unset the interstitial) and no interstitial page should be set
  // when there is already a non NULL interstitial page set.
  void set_interstitial_page(InterstitialPageImpl* interstitial_page) {
    DCHECK(!interstitial_page_ && interstitial_page);
    interstitial_page_ = interstitial_page;
  }

  // Unsets the currently showing interstitial.
  void remove_interstitial_page() {
    DCHECK(interstitial_page_);
    interstitial_page_ = NULL;
  }

  // Returns the currently showing interstitial, NULL if no interstitial is
  // showing.
  InterstitialPageImpl* interstitial_page() const { return interstitial_page_; }

  // RenderViewHostDelegate::RendererManagement implementation.
  virtual void ShouldClosePage(
      bool for_cross_site_transition,
      bool proceed,
      const base::TimeTicks& proceed_time) OVERRIDE;
  virtual void OnCrossSiteResponse(
      RenderViewHost* pending_render_view_host,
      const GlobalRequestID& global_request_id) OVERRIDE;

  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // Called when a RenderViewHost is about to be deleted.
  void RenderViewDeleted(RenderViewHost* rvh);

  // Returns whether the given RenderViewHost is on the list of swapped out
  // RenderViewHosts.
  bool IsOnSwappedOutList(RenderViewHost* rvh) const;

  // Returns the swapped out RenderViewHost for the given SiteInstance, if any.
  RenderViewHostImpl* GetSwappedOutRenderViewHost(SiteInstance* instance);

  // Runs the unload handler in the current page, when we know that a pending
  // cross-process navigation is going to commit.
  void SwapOutOldPage();

 private:
  friend class RenderViewHostManagerTest;
  friend class TestWebContents;

  // Tracks information about a navigation while a cross-process transition is
  // in progress.
  // TODO(creis): Add transfer navigation params for http://crbug.com/238331.
  struct PendingNavigationParams {
    PendingNavigationParams();
    explicit PendingNavigationParams(const GlobalRequestID& global_request_id);

    GlobalRequestID global_request_id;
  };

  // Returns whether this tab should transition to a new renderer for
  // cross-site URLs.  Enabled unless we see the --process-per-tab command line
  // switch.  Can be overridden in unit tests.
  bool ShouldTransitionCrossSite();

  // Returns true if the two navigation entries are incompatible in some way
  // other than site instances. Cases where this can happen include Web UI
  // to regular web pages. It will cause us to swap RenderViewHosts (and hence
  // RenderProcessHosts) even if the site instance would otherwise be the same.
  // As part of this, we'll also force new SiteInstances and BrowsingInstances.
  // Either of the entries may be NULL.
  bool ShouldSwapProcessesForNavigation(
      const NavigationEntry* curr_entry,
      const NavigationEntryImpl* new_entry) const;

  bool ShouldReuseWebUI(
      const NavigationEntry* curr_entry,
      const NavigationEntryImpl* new_entry) const;

  // Returns an appropriate SiteInstance object for the given NavigationEntry,
  // possibly reusing the current SiteInstance.
  // Never called if --process-per-tab is used.
  SiteInstance* GetSiteInstanceForEntry(
      const NavigationEntryImpl& entry,
      SiteInstance* curr_instance);

  // Sets up the necessary state for a new RenderViewHost with the given opener.
  bool InitRenderView(RenderViewHost* render_view_host, int opener_route_id);

  // Sets the pending RenderViewHost/WebUI to be the active one. Note that this
  // doesn't require the pending render_view_host_ pointer to be non-NULL, since
  // there could be Web UI switching as well. Call this for every commit.
  void CommitPending();

  // Shutdown all RenderViewHosts in a SiteInstance. This is called
  // to shutdown views when all the views in a SiteInstance are
  // confirmed to be swapped out.
  void ShutdownRenderViewHostsInSiteInstance(int32 site_instance_id);

  // Helper method to terminate the pending RenderViewHost.
  void CancelPending();

  RenderViewHostImpl* UpdateRendererStateForNavigate(
      const NavigationEntryImpl& entry);

  // Called when a renderer process is starting to close.  We should not
  // schedule new navigations in its swapped out RenderViewHosts after this.
  void RendererProcessClosing(RenderProcessHost* render_process_host);

  // Our delegate, not owned by us. Guaranteed non-NULL.
  Delegate* delegate_;

  // Whether a navigation requiring different RenderView's is pending. This is
  // either cross-site request is (in the new process model), or when required
  // for the view type (like view source versus not).
  bool cross_navigation_pending_;

  // Implemented by the owner of this class, these delegates are installed into
  // all the RenderViewHosts that we create.
  RenderViewHostDelegate* render_view_delegate_;
  RenderWidgetHostDelegate* render_widget_delegate_;

  // Our RenderView host and its associated Web UI (if any, will be NULL for
  // non-DOM-UI pages). This object is responsible for all communication with
  // a child RenderView instance.
  RenderViewHostImpl* render_view_host_;
  scoped_ptr<WebUIImpl> web_ui_;

  // A RenderViewHost used to load a cross-site page. This remains hidden
  // while a cross-site request is pending until it calls DidNavigate. It may
  // have an associated Web UI, in which case the Web UI pointer will be non-
  // NULL.
  //
  // The |pending_web_ui_| may be non-NULL even when the
  // |pending_render_view_host_| is NULL. This will happen when we're
  // transitioning between two Web UI pages: the RVH won't be swapped, so the
  // pending pointer will be unused, but there will be a pending Web UI
  // associated with the navigation.
  RenderViewHostImpl* pending_render_view_host_;

  // Tracks information about any current pending cross-process navigation.
  scoped_ptr<PendingNavigationParams> pending_nav_params_;

  // If either of these is non-NULL, the pending navigation is to a chrome:
  // page. The scoped_ptr is used if pending_web_ui_ != web_ui_, the WeakPtr is
  // used for when they reference the same object. If either is non-NULL, the
  // other should be NULL.
  scoped_ptr<WebUIImpl> pending_web_ui_;
  base::WeakPtr<WebUIImpl> pending_and_current_web_ui_;

  // A map of site instance ID to swapped out RenderViewHosts.  This may include
  // pending_render_view_host_ for navigations to existing entries.
  typedef base::hash_map<int32, RenderViewHostImpl*> RenderViewHostMap;
  RenderViewHostMap swapped_out_hosts_;

  // The intersitial page currently shown if any, not own by this class
  // (the InterstitialPage is self-owned, it deletes itself when hidden).
  InterstitialPageImpl* interstitial_page_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(RenderViewHostManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_RENDER_VIEW_HOST_MANAGER_H_
