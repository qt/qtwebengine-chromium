// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_host_manager.h"

#include <utility>

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/render_view_devtools_agent_host.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_controller_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_factory.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/webui/web_ui_controller_factory_registry.h"
#include "content/browser/webui/web_ui_impl.h"
#include "content/common/view_messages.h"
#include "content/port/browser/render_widget_host_view_port.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"

namespace content {

RenderFrameHostManager::PendingNavigationParams::PendingNavigationParams()
    : is_transfer(false), frame_id(-1), should_replace_current_entry(false) {
}

RenderFrameHostManager::PendingNavigationParams::PendingNavigationParams(
    const GlobalRequestID& global_request_id,
    bool is_transfer,
    const std::vector<GURL>& transfer_url_chain,
    Referrer referrer,
    PageTransition page_transition,
    int64 frame_id,
    bool should_replace_current_entry)
    : global_request_id(global_request_id),
      is_transfer(is_transfer),
      transfer_url_chain(transfer_url_chain),
      referrer(referrer),
      page_transition(page_transition),
      frame_id(frame_id),
      should_replace_current_entry(should_replace_current_entry) {
}

RenderFrameHostManager::PendingNavigationParams::~PendingNavigationParams() {}

RenderFrameHostManager::RenderFrameHostManager(
    RenderFrameHostDelegate* render_frame_delegate,
    RenderViewHostDelegate* render_view_delegate,
    RenderWidgetHostDelegate* render_widget_delegate,
    Delegate* delegate)
    : delegate_(delegate),
      cross_navigation_pending_(false),
      render_frame_delegate_(render_frame_delegate),
      render_view_delegate_(render_view_delegate),
      render_widget_delegate_(render_widget_delegate),
      render_view_host_(NULL),
      pending_render_view_host_(NULL),
      interstitial_page_(NULL) {
}

RenderFrameHostManager::~RenderFrameHostManager() {
  if (pending_render_view_host_)
    CancelPending();

  // We should always have a main RenderViewHost except in some tests.
  RenderViewHostImpl* render_view_host = render_view_host_;
  render_view_host_ = NULL;
  if (render_view_host)
    render_view_host->Shutdown();

  // Shut down any swapped out RenderViewHosts.
  for (RenderViewHostMap::iterator iter = swapped_out_hosts_.begin();
       iter != swapped_out_hosts_.end();
       ++iter) {
    iter->second->Shutdown();
  }
}

void RenderFrameHostManager::Init(BrowserContext* browser_context,
                                  SiteInstance* site_instance,
                                  int routing_id,
                                  int main_frame_routing_id) {
  // Create a RenderViewHost, once we have an instance.  It is important to
  // immediately give this SiteInstance to a RenderViewHost so that it is
  // ref counted.
  if (!site_instance)
    site_instance = SiteInstance::Create(browser_context);
  render_view_host_ = static_cast<RenderViewHostImpl*>(
      RenderViewHostFactory::Create(
          site_instance, render_view_delegate_, render_frame_delegate_,
          render_widget_delegate_, routing_id, main_frame_routing_id, false,
          delegate_->IsHidden()));
  render_view_host_->AttachToFrameTree();

  // Keep track of renderer processes as they start to shut down or are
  // crashed/killed.
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSED,
                 NotificationService::AllSources());
  registrar_.Add(this, NOTIFICATION_RENDERER_PROCESS_CLOSING,
                 NotificationService::AllSources());
}

RenderViewHostImpl* RenderFrameHostManager::current_host() const {
  return render_view_host_;
}

RenderViewHostImpl* RenderFrameHostManager::pending_render_view_host() const {
  return pending_render_view_host_;
}

RenderWidgetHostView* RenderFrameHostManager::GetRenderWidgetHostView() const {
  if (interstitial_page_)
    return interstitial_page_->GetView();
  if (!render_view_host_)
    return NULL;
  return render_view_host_->GetView();
}

void RenderFrameHostManager::SetPendingWebUI(const NavigationEntryImpl& entry) {
  pending_web_ui_.reset(
      delegate_->CreateWebUIForRenderManager(entry.GetURL()));
  pending_and_current_web_ui_.reset();

  // If we have assigned (zero or more) bindings to this NavigationEntry in the
  // past, make sure we're not granting it different bindings than it had
  // before.  If so, note it and don't give it any bindings, to avoid a
  // potential privilege escalation.
  if (pending_web_ui_.get() &&
      entry.bindings() != NavigationEntryImpl::kInvalidBindings &&
      pending_web_ui_->GetBindings() != entry.bindings()) {
    RecordAction(UserMetricsAction("ProcessSwapBindingsMismatch_RVHM"));
    pending_web_ui_.reset();
  }
}

RenderViewHostImpl* RenderFrameHostManager::Navigate(
    const NavigationEntryImpl& entry) {
  TRACE_EVENT0("browser", "RenderFrameHostManager:Navigate");
  // Create a pending RenderViewHost. It will give us the one we should use
  RenderViewHostImpl* dest_render_view_host =
      static_cast<RenderViewHostImpl*>(UpdateRendererStateForNavigate(entry));
  if (!dest_render_view_host)
    return NULL;  // We weren't able to create a pending render view host.

  // If the current render_view_host_ isn't live, we should create it so
  // that we don't show a sad tab while the dest_render_view_host fetches
  // its first page.  (Bug 1145340)
  if (dest_render_view_host != render_view_host_ &&
      !render_view_host_->IsRenderViewLive()) {
    // Note: we don't call InitRenderView here because we are navigating away
    // soon anyway, and we don't have the NavigationEntry for this host.
    delegate_->CreateRenderViewForRenderManager(render_view_host_,
                                                MSG_ROUTING_NONE);
  }

  // If the renderer crashed, then try to create a new one to satisfy this
  // navigation request.
  if (!dest_render_view_host->IsRenderViewLive()) {
    // Recreate the opener chain.
    int opener_route_id = delegate_->CreateOpenerRenderViewsForRenderManager(
        dest_render_view_host->GetSiteInstance());
    if (!InitRenderView(dest_render_view_host, opener_route_id))
      return NULL;

    // Now that we've created a new renderer, be sure to hide it if it isn't
    // our primary one.  Otherwise, we might crash if we try to call Show()
    // on it later.
    if (dest_render_view_host != render_view_host_ &&
        dest_render_view_host->GetView()) {
      dest_render_view_host->GetView()->Hide();
    } else {
      // This is our primary renderer, notify here as we won't be calling
      // CommitPending (which does the notify).
      delegate_->NotifySwappedFromRenderManager(NULL, render_view_host_);
    }
  }

  return dest_render_view_host;
}

void RenderFrameHostManager::Stop() {
  render_view_host_->Stop();

  // If we are cross-navigating, we should stop the pending renderers.  This
  // will lead to a DidFailProvisionalLoad, which will properly destroy them.
  if (cross_navigation_pending_) {
    pending_render_view_host_->Send(
        new ViewMsg_Stop(pending_render_view_host_->GetRoutingID()));
  }
}

void RenderFrameHostManager::SetIsLoading(bool is_loading) {
  render_view_host_->SetIsLoading(is_loading);
  if (pending_render_view_host_)
    pending_render_view_host_->SetIsLoading(is_loading);
}

bool RenderFrameHostManager::ShouldCloseTabOnUnresponsiveRenderer() {
  if (!cross_navigation_pending_)
    return true;

  // We should always have a pending RVH when there's a cross-process navigation
  // in progress.  Sanity check this for http://crbug.com/276333.
  CHECK(pending_render_view_host_);

  // If the tab becomes unresponsive during {before}unload while doing a
  // cross-site navigation, proceed with the navigation.  (This assumes that
  // the pending RenderViewHost is still responsive.)
  if (render_view_host_->is_waiting_for_unload_ack()) {
    // The request has been started and paused while we're waiting for the
    // unload handler to finish.  We'll pretend that it did.  The pending
    // renderer will then be swapped in as part of the usual DidNavigate logic.
    // (If the unload handler later finishes, this call will be ignored because
    // the pending_nav_params_ state will already be cleaned up.)
    current_host()->OnSwappedOut(true);
  } else if (render_view_host_->is_waiting_for_beforeunload_ack()) {
    // Haven't gotten around to starting the request, because we're still
    // waiting for the beforeunload handler to finish.  We'll pretend that it
    // did finish, to let the navigation proceed.  Note that there's a danger
    // that the beforeunload handler will later finish and possibly return
    // false (meaning the navigation should not proceed), but we'll ignore it
    // in this case because it took too long.
    if (pending_render_view_host_->are_navigations_suspended())
      pending_render_view_host_->SetNavigationsSuspended(
          false, base::TimeTicks::Now());
  }
  return false;
}

void RenderFrameHostManager::SwappedOut(RenderViewHost* render_view_host) {
  // Make sure this is from our current RVH, and that we have a pending
  // navigation from OnCrossSiteResponse.  (There may be no pending navigation
  // for data URLs that don't make network requests, for example.)   If not,
  // just return early and ignore.
  if (render_view_host != render_view_host_ || !pending_nav_params_.get()) {
    pending_nav_params_.reset();
    return;
  }

  // Now that the unload handler has run, we need to either initiate the
  // pending transfer (if there is one) or resume the paused response (if not).
  // TODO(creis): The blank swapped out page is visible during this time, but
  // we can shorten this by delivering the response directly, rather than
  // forcing an identical request to be made.
  if (pending_nav_params_->is_transfer) {
    // Treat the last URL in the chain as the destination and the remainder as
    // the redirect chain.
    CHECK(pending_nav_params_->transfer_url_chain.size());
    GURL transfer_url = pending_nav_params_->transfer_url_chain.back();
    pending_nav_params_->transfer_url_chain.pop_back();

    // We don't know whether the original request had |user_action| set to true.
    // However, since we force the navigation to be in the current tab, it
    // doesn't matter.
    render_view_host->GetDelegate()->RequestTransferURL(
        transfer_url,
        pending_nav_params_->transfer_url_chain,
        pending_nav_params_->referrer,
        pending_nav_params_->page_transition,
        CURRENT_TAB,
        pending_nav_params_->frame_id,
        pending_nav_params_->global_request_id,
        pending_nav_params_->should_replace_current_entry,
        true);
  } else if (pending_render_view_host_) {
    RenderProcessHostImpl* pending_process =
        static_cast<RenderProcessHostImpl*>(
            pending_render_view_host_->GetProcess());
    pending_process->ResumeDeferredNavigation(
        pending_nav_params_->global_request_id);
  }
  pending_nav_params_.reset();
}

void RenderFrameHostManager::DidNavigateMainFrame(
    RenderViewHost* render_view_host) {
  if (!cross_navigation_pending_) {
    DCHECK(!pending_render_view_host_);

    // We should only hear this from our current renderer.
    DCHECK(render_view_host == render_view_host_);

    // Even when there is no pending RVH, there may be a pending Web UI.
    if (pending_web_ui())
      CommitPending();
    return;
  }

  if (render_view_host == pending_render_view_host_) {
    // The pending cross-site navigation completed, so show the renderer.
    // If it committed without sending network requests (e.g., data URLs),
    // then we still need to swap out the old RVH first and run its unload
    // handler.  OK for that to happen in the background.
    if (pending_render_view_host_->HasPendingCrossSiteRequest())
      SwapOutOldPage();

    CommitPending();
    cross_navigation_pending_ = false;
  } else if (render_view_host == render_view_host_) {
    // A navigation in the original page has taken place.  Cancel the pending
    // one.
    CancelPending();
    cross_navigation_pending_ = false;
  } else {
    // No one else should be sending us DidNavigate in this state.
    DCHECK(false);
  }
}

void RenderFrameHostManager::DidDisownOpener(RenderViewHost* render_view_host) {
  // Notify all swapped out hosts, including the pending RVH.
  for (RenderViewHostMap::iterator iter = swapped_out_hosts_.begin();
       iter != swapped_out_hosts_.end();
       ++iter) {
    DCHECK_NE(iter->second->GetSiteInstance(),
              current_host()->GetSiteInstance());
    iter->second->DisownOpener();
  }
}

void RenderFrameHostManager::RendererAbortedProvisionalLoad(
    RenderViewHost* render_view_host) {
  // We used to cancel the pending renderer here for cross-site downloads.
  // However, it's not safe to do that because the download logic repeatedly
  // looks for this WebContents based on a render view ID.  Instead, we just
  // leave the pending renderer around until the next navigation event
  // (Navigate, DidNavigate, etc), which will clean it up properly.
  // TODO(creis): All of this will go away when we move the cross-site logic
  // to ResourceDispatcherHost, so that we intercept responses rather than
  // navigation events.  (That's necessary to support onunload anyway.)  Once
  // we've made that change, we won't create a pending renderer until we know
  // the response is not a download.
}

void RenderFrameHostManager::RendererProcessClosing(
    RenderProcessHost* render_process_host) {
  // Remove any swapped out RVHs from this process, so that we don't try to
  // swap them back in while the process is exiting.  Start by finding them,
  // since there could be more than one.
  std::list<int> ids_to_remove;
  for (RenderViewHostMap::iterator iter = swapped_out_hosts_.begin();
       iter != swapped_out_hosts_.end();
       ++iter) {
    if (iter->second->GetProcess() == render_process_host)
      ids_to_remove.push_back(iter->first);
  }

  // Now delete them.
  while (!ids_to_remove.empty()) {
    swapped_out_hosts_[ids_to_remove.back()]->Shutdown();
    swapped_out_hosts_.erase(ids_to_remove.back());
    ids_to_remove.pop_back();
  }
}

void RenderFrameHostManager::ShouldClosePage(
    bool for_cross_site_transition,
    bool proceed,
    const base::TimeTicks& proceed_time) {
  if (for_cross_site_transition) {
    // Ignore if we're not in a cross-site navigation.
    if (!cross_navigation_pending_)
      return;

    if (proceed) {
      // Ok to unload the current page, so proceed with the cross-site
      // navigation.  Note that if navigations are not currently suspended, it
      // might be because the renderer was deemed unresponsive and this call was
      // already made by ShouldCloseTabOnUnresponsiveRenderer.  In that case, it
      // is ok to do nothing here.
      if (pending_render_view_host_ &&
          pending_render_view_host_->are_navigations_suspended()) {
        pending_render_view_host_->SetNavigationsSuspended(false, proceed_time);
      }
    } else {
      // Current page says to cancel.
      CancelPending();
      cross_navigation_pending_ = false;
    }
  } else {
    // Non-cross site transition means closing the entire tab.
    bool proceed_to_fire_unload;
    delegate_->BeforeUnloadFiredFromRenderManager(proceed, proceed_time,
                                                  &proceed_to_fire_unload);

    if (proceed_to_fire_unload) {
      // If we're about to close the tab and there's a pending RVH, cancel it.
      // Otherwise, if the navigation in the pending RVH completes before the
      // close in the current RVH, we'll lose the tab close.
      if (pending_render_view_host_) {
        CancelPending();
        cross_navigation_pending_ = false;
      }

      // This is not a cross-site navigation, the tab is being closed.
      render_view_host_->ClosePage();
    }
  }
}

void RenderFrameHostManager::OnCrossSiteResponse(
    RenderViewHost* pending_render_view_host,
    const GlobalRequestID& global_request_id,
    bool is_transfer,
    const std::vector<GURL>& transfer_url_chain,
    const Referrer& referrer,
    PageTransition page_transition,
    int64 frame_id,
    bool should_replace_current_entry) {
  // This should be called either when the pending RVH is ready to commit or
  // when we realize that the current RVH's request requires a transfer.
  DCHECK(
      pending_render_view_host == pending_render_view_host_ ||
      pending_render_view_host == render_view_host_);

  // TODO(creis): Eventually we will want to check all navigation responses
  // here, but currently we pass information for a transfer if
  // ShouldSwapProcessesForRedirect returned true in the network stack.
  // In that case, we should set up a transfer after the unload handler runs.
  // If is_transfer is false, we will just run the unload handler and resume.
  pending_nav_params_.reset(new PendingNavigationParams(
      global_request_id, is_transfer, transfer_url_chain, referrer,
      page_transition, frame_id, should_replace_current_entry));

  // Run the unload handler of the current page.
  SwapOutOldPage();
}

void RenderFrameHostManager::SwapOutOldPage() {
  // Should only see this while we have a pending renderer or transfer.
  CHECK(cross_navigation_pending_ || pending_nav_params_.get());

  // Tell the renderer to suppress any further modal dialogs so that we can swap
  // it out.  This must be done before canceling any current dialog, in case
  // there is a loop creating additional dialogs.
  render_view_host_->SuppressDialogsUntilSwapOut();

  // Now close any modal dialogs that would prevent us from swapping out.  This
  // must be done separately from SwapOut, so that the PageGroupLoadDeferrer is
  // no longer on the stack when we send the SwapOut message.
  delegate_->CancelModalDialogsForRenderManager();

  // Tell the old renderer it is being swapped out.  This will fire the unload
  // handler (without firing the beforeunload handler a second time).  When the
  // unload handler finishes and the navigation completes, we will send a
  // message to the ResourceDispatcherHost, allowing the pending RVH's response
  // to resume.
  render_view_host_->SwapOut();

  // ResourceDispatcherHost has told us to run the onunload handler, which
  // means it is not a download or unsafe page, and we are going to perform the
  // navigation.  Thus, we no longer need to remember that the RenderViewHost
  // is part of a pending cross-site request.
  if (pending_render_view_host_)
    pending_render_view_host_->SetHasPendingCrossSiteRequest(false);
}

void RenderFrameHostManager::Observe(
    int type,
    const NotificationSource& source,
    const NotificationDetails& details) {
  switch (type) {
    case NOTIFICATION_RENDERER_PROCESS_CLOSED:
    case NOTIFICATION_RENDERER_PROCESS_CLOSING:
      RendererProcessClosing(
          Source<RenderProcessHost>(source).ptr());
      break;

    default:
      NOTREACHED();
  }
}

bool RenderFrameHostManager::ShouldTransitionCrossSite() {
  // False in the single-process mode, as it makes RVHs to accumulate
  // in swapped_out_hosts_.
  // True if we are using process-per-site-instance (default) or
  // process-per-site (kProcessPerSite).
  return
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerTab);
}

bool RenderFrameHostManager::ShouldSwapBrowsingInstancesForNavigation(
    const NavigationEntry* current_entry,
    const NavigationEntryImpl* new_entry) const {
  DCHECK(new_entry);

  // If new_entry already has a SiteInstance, assume it is correct and use it.
  if (new_entry->site_instance())
    return false;

  // Check for reasons to swap processes even if we are in a process model that
  // doesn't usually swap (e.g., process-per-tab).  Any time we return true,
  // the new_entry will be rendered in a new SiteInstance AND BrowsingInstance.

  // We use the effective URL here, since that's what is used in the
  // SiteInstance's site and when we later call IsSameWebSite.  If there is no
  // current_entry, check the current SiteInstance's site, which might already
  // be committed to a Web UI URL (such as the NTP).
  BrowserContext* browser_context =
      delegate_->GetControllerForRenderManager().GetBrowserContext();
  const GURL& current_url = (current_entry) ?
      SiteInstanceImpl::GetEffectiveURL(browser_context,
                                        current_entry->GetURL()) :
      render_view_host_->GetSiteInstance()->GetSiteURL();
  const GURL& new_url = SiteInstanceImpl::GetEffectiveURL(browser_context,
                                                          new_entry->GetURL());

  // Don't force a new BrowsingInstance for debug URLs that are handled in the
  // renderer process, like javascript: or chrome://crash.
  if (IsRendererDebugURL(new_url))
    return false;

  // For security, we should transition between processes when one is a Web UI
  // page and one isn't.
  if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
          browser_context, current_url)) {
    // If so, force a swap if destination is not an acceptable URL for Web UI.
    // Here, data URLs are never allowed.
    if (!WebUIControllerFactoryRegistry::GetInstance()->IsURLAcceptableForWebUI(
            browser_context, new_url, false)) {
      return true;
    }
  } else {
    // Force a swap if it's a Web UI URL.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, new_url)) {
      return true;
    }
  }

  // Check with the content client as well.  Important to pass current_url here,
  // which uses the SiteInstance's site if there is no current_entry.
  if (GetContentClient()->browser()->ShouldSwapBrowsingInstancesForNavigation(
          render_view_host_->GetSiteInstance(), current_url, new_url)) {
    return true;
  }

  // We can't switch a RenderView between view source and non-view source mode
  // without screwing up the session history sometimes (when navigating between
  // "view-source:http://foo.com/" and "http://foo.com/", Blink doesn't treat
  // it as a new navigation). So require a BrowsingInstance switch.
  if (current_entry &&
      current_entry->IsViewSourceMode() != new_entry->IsViewSourceMode())
    return true;

  return false;
}

bool RenderFrameHostManager::ShouldReuseWebUI(
    const NavigationEntry* current_entry,
    const NavigationEntryImpl* new_entry) const {
  NavigationControllerImpl& controller =
      delegate_->GetControllerForRenderManager();
  return current_entry && web_ui_.get() &&
      (WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          controller.GetBrowserContext(), current_entry->GetURL()) ==
       WebUIControllerFactoryRegistry::GetInstance()->GetWebUIType(
          controller.GetBrowserContext(), new_entry->GetURL()));
}

SiteInstance* RenderFrameHostManager::GetSiteInstanceForEntry(
    const NavigationEntryImpl& entry,
    SiteInstance* current_instance,
    bool force_browsing_instance_swap) {
  // Determine which SiteInstance to use for navigating to |entry|.
  const GURL& dest_url = entry.GetURL();
  NavigationControllerImpl& controller =
      delegate_->GetControllerForRenderManager();
  BrowserContext* browser_context = controller.GetBrowserContext();

  // If a swap is required, we need to force the SiteInstance AND
  // BrowsingInstance to be different ones, using CreateForURL.
  if (force_browsing_instance_swap) {
    // We shouldn't be forcing a swap if an entry already has a SiteInstance.
    CHECK(!entry.site_instance());
    return SiteInstance::CreateForURL(browser_context, dest_url);
  }

  // If the entry has an instance already we should use it.
  if (entry.site_instance())
    return entry.site_instance();

  // (UGLY) HEURISTIC, process-per-site only:
  //
  // If this navigation is generated, then it probably corresponds to a search
  // query.  Given that search results typically lead to users navigating to
  // other sites, we don't really want to use the search engine hostname to
  // determine the site instance for this navigation.
  //
  // NOTE: This can be removed once we have a way to transition between
  //       RenderViews in response to a link click.
  //
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kProcessPerSite) &&
      PageTransitionCoreTypeIs(entry.GetTransitionType(),
                               PAGE_TRANSITION_GENERATED)) {
    return current_instance;
  }

  SiteInstanceImpl* current_site_instance =
      static_cast<SiteInstanceImpl*>(current_instance);

  // If we haven't used our SiteInstance (and thus RVH) yet, then we can use it
  // for this entry.  We won't commit the SiteInstance to this site until the
  // navigation commits (in DidNavigate), unless the navigation entry was
  // restored or it's a Web UI as described below.
  if (!current_site_instance->HasSite()) {
    // If we've already created a SiteInstance for our destination, we don't
    // want to use this unused SiteInstance; use the existing one.  (We don't
    // do this check if the current_instance has a site, because for now, we
    // want to compare against the current URL and not the SiteInstance's site.
    // In this case, there is no current URL, so comparing against the site is
    // ok.  See additional comments below.)
    //
    // Also, if the URL should use process-per-site mode and there is an
    // existing process for the site, we should use it.  We can call
    // GetRelatedSiteInstance() for this, which will eagerly set the site and
    // thus use the correct process.
    bool use_process_per_site =
        RenderProcessHost::ShouldUseProcessPerSite(browser_context, dest_url) &&
        RenderProcessHostImpl::GetProcessHostForSite(browser_context, dest_url);
    if (current_site_instance->HasRelatedSiteInstance(dest_url) ||
        use_process_per_site) {
      return current_site_instance->GetRelatedSiteInstance(dest_url);
    }

    // For extensions, Web UI URLs (such as the new tab page), and apps we do
    // not want to use the current_instance if it has no site, since it will
    // have a RenderProcessHost of PRIV_NORMAL.  Create a new SiteInstance for
    // this URL instead (with the correct process type).
    if (current_site_instance->HasWrongProcessForURL(dest_url))
      return current_site_instance->GetRelatedSiteInstance(dest_url);

    // View-source URLs must use a new SiteInstance and BrowsingInstance.
    // TODO(nasko): This is the same condition as later in the function. This
    // should be taken into account when refactoring this method as part of
    // http://crbug.com/123007.
    if (entry.IsViewSourceMode())
      return SiteInstance::CreateForURL(browser_context, dest_url);

    // If we are navigating from a blank SiteInstance to a WebUI, make sure we
    // create a new SiteInstance.
    if (WebUIControllerFactoryRegistry::GetInstance()->UseWebUIForURL(
            browser_context, dest_url)) {
        return SiteInstance::CreateForURL(browser_context, dest_url);
    }

    // Normally the "site" on the SiteInstance is set lazily when the load
    // actually commits. This is to support better process sharing in case
    // the site redirects to some other site: we want to use the destination
    // site in the site instance.
    //
    // In the case of session restore, as it loads all the pages immediately
    // we need to set the site first, otherwise after a restore none of the
    // pages would share renderers in process-per-site.
    if (entry.restore_type() != NavigationEntryImpl::RESTORE_NONE)
      current_site_instance->SetSite(dest_url);

    return current_site_instance;
  }

  // Otherwise, only create a new SiteInstance for a cross-site navigation.

  // TODO(creis): Once we intercept links and script-based navigations, we
  // will be able to enforce that all entries in a SiteInstance actually have
  // the same site, and it will be safe to compare the URL against the
  // SiteInstance's site, as follows:
  // const GURL& current_url = current_instance->site();
  // For now, though, we're in a hybrid model where you only switch
  // SiteInstances if you type in a cross-site URL.  This means we have to
  // compare the entry's URL to the last committed entry's URL.
  NavigationEntry* current_entry = controller.GetLastCommittedEntry();
  if (interstitial_page_) {
    // The interstitial is currently the last committed entry, but we want to
    // compare against the last non-interstitial entry.
    current_entry = controller.GetEntryAtOffset(-1);
  }
  // If there is no last non-interstitial entry (and current_instance already
  // has a site), then we must have been opened from another tab.  We want
  // to compare against the URL of the page that opened us, but we can't
  // get to it directly.  The best we can do is check against the site of
  // the SiteInstance.  This will be correct when we intercept links and
  // script-based navigations, but for now, it could place some pages in a
  // new process unnecessarily.  We should only hit this case if a page tries
  // to open a new tab to an interstitial-inducing URL, and then navigates
  // the page to a different same-site URL.  (This seems very unlikely in
  // practice.)
  const GURL& current_url = (current_entry) ? current_entry->GetURL() :
      current_instance->GetSiteURL();

  // View-source URLs must use a new SiteInstance and BrowsingInstance.
  // We don't need a swap when going from view-source to a debug URL like
  // chrome://crash, however.
  // TODO(creis): Refactor this method so this duplicated code isn't needed.
  // See http://crbug.com/123007.
  if (current_entry &&
      current_entry->IsViewSourceMode() != entry.IsViewSourceMode() &&
      !IsRendererDebugURL(dest_url)) {
    return SiteInstance::CreateForURL(browser_context, dest_url);
  }

  // Use the current SiteInstance for same site navigations, as long as the
  // process type is correct.  (The URL may have been installed as an app since
  // the last time we visited it.)
  if (SiteInstance::IsSameWebSite(browser_context, current_url, dest_url) &&
      !current_site_instance->HasWrongProcessForURL(dest_url)) {
    return current_instance;
  }

  // Start the new renderer in a new SiteInstance, but in the current
  // BrowsingInstance.  It is important to immediately give this new
  // SiteInstance to a RenderViewHost (if it is different than our current
  // SiteInstance), so that it is ref counted.  This will happen in
  // CreateRenderView.
  return current_instance->GetRelatedSiteInstance(dest_url);
}

int RenderFrameHostManager::CreateRenderView(
    SiteInstance* instance,
    int opener_route_id,
    bool swapped_out,
    bool hidden) {
  CHECK(instance);
  DCHECK(!swapped_out || hidden); // Swapped out views should always be hidden.

  // We are creating a pending or swapped out RVH here.  We should never create
  // it in the same SiteInstance as our current RVH.
  CHECK_NE(render_view_host_->GetSiteInstance(), instance);

  // Check if we've already created an RVH for this SiteInstance.  If so, try
  // to re-use the existing one, which has already been initialized.  We'll
  // remove it from the list of swapped out hosts if it commits.
  RenderViewHostImpl* new_render_view_host = static_cast<RenderViewHostImpl*>(
      GetSwappedOutRenderViewHost(instance));
  if (new_render_view_host) {
    // Prevent the process from exiting while we're trying to use it.
    if (!swapped_out)
      new_render_view_host->GetProcess()->AddPendingView();
  } else {
    // Create a new RenderViewHost if we don't find an existing one.
    new_render_view_host = static_cast<RenderViewHostImpl*>(
        RenderViewHostFactory::Create(instance,
                                      render_view_delegate_,
                                      render_frame_delegate_,
                                      render_widget_delegate_,
                                      MSG_ROUTING_NONE,
                                      MSG_ROUTING_NONE,
                                      swapped_out,
                                      hidden));

    // If the new RVH is swapped out already, store it.  Otherwise prevent the
    // process from exiting while we're trying to navigate in it.
    if (swapped_out) {
      swapped_out_hosts_[instance->GetId()] = new_render_view_host;
    } else {
      new_render_view_host->GetProcess()->AddPendingView();
    }

    bool success = InitRenderView(new_render_view_host, opener_route_id);
    if (success) {
      // Don't show the view until we get a DidNavigate from it.
      new_render_view_host->GetView()->Hide();
    } else if (!swapped_out) {
      CancelPending();
    }
  }

  // Use this as our new pending RVH if it isn't swapped out.
  if (!swapped_out)
    pending_render_view_host_ = new_render_view_host;

  return new_render_view_host->GetRoutingID();
}

bool RenderFrameHostManager::InitRenderView(RenderViewHost* render_view_host,
                                            int opener_route_id) {
  // If the pending navigation is to a WebUI and the RenderView is not in a
  // guest process, tell the RenderView about any bindings it will need enabled.
  if (pending_web_ui() && !render_view_host->GetProcess()->IsGuest()) {
    render_view_host->AllowBindings(pending_web_ui()->GetBindings());
  } else {
    // Ensure that we don't create an unprivileged RenderView in a WebUI-enabled
    // process unless it's swapped out.
    RenderViewHostImpl* rvh_impl =
        static_cast<RenderViewHostImpl*>(render_view_host);
    if (!rvh_impl->is_swapped_out()) {
      CHECK(!ChildProcessSecurityPolicyImpl::GetInstance()->HasWebUIBindings(
                render_view_host->GetProcess()->GetID()));
    }
  }

  return delegate_->CreateRenderViewForRenderManager(render_view_host,
                                                     opener_route_id);
}

void RenderFrameHostManager::CommitPending() {
  // First check whether we're going to want to focus the location bar after
  // this commit.  We do this now because the navigation hasn't formally
  // committed yet, so if we've already cleared |pending_web_ui_| the call chain
  // this triggers won't be able to figure out what's going on.
  bool will_focus_location_bar = delegate_->FocusLocationBarByDefault();

  // We expect SwapOutOldPage to have canceled any modal dialogs and told the
  // renderer to suppress any further dialogs until it is swapped out.  However,
  // crash reports indicate that it's still possible for modal dialogs to exist
  // at this point, which poses a risk if we delete their RenderViewHost below.
  // Cancel them again to be safe.  http://crbug.com/324320.
  delegate_->CancelModalDialogsForRenderManager();

  // Next commit the Web UI, if any. Either replace |web_ui_| with
  // |pending_web_ui_|, or clear |web_ui_| if there is no pending WebUI, or
  // leave |web_ui_| as is if reusing it.
  DCHECK(!(pending_web_ui_.get() && pending_and_current_web_ui_.get()));
  if (pending_web_ui_) {
    web_ui_.reset(pending_web_ui_.release());
  } else if (!pending_and_current_web_ui_.get()) {
    web_ui_.reset();
  } else {
    DCHECK_EQ(pending_and_current_web_ui_.get(), web_ui_.get());
    pending_and_current_web_ui_.reset();
  }

  // It's possible for the pending_render_view_host_ to be NULL when we aren't
  // crossing process boundaries. If so, we just needed to handle the Web UI
  // committing above and we're done.
  if (!pending_render_view_host_) {
    if (will_focus_location_bar)
      delegate_->SetFocusToLocationBar(false);
    return;
  }

  // Remember if the page was focused so we can focus the new renderer in
  // that case.
  bool focus_render_view = !will_focus_location_bar &&
      render_view_host_->GetView() && render_view_host_->GetView()->HasFocus();

  // Swap in the pending view and make it active. Also ensure the FrameTree
  // stays in sync.
  RenderViewHostImpl* old_render_view_host = render_view_host_;
  render_view_host_ = pending_render_view_host_;
  pending_render_view_host_ = NULL;
  render_view_host_->AttachToFrameTree();

  // The process will no longer try to exit, so we can decrement the count.
  render_view_host_->GetProcess()->RemovePendingView();

  // If the view is gone, then this RenderViewHost died while it was hidden.
  // We ignored the RenderProcessGone call at the time, so we should send it now
  // to make sure the sad tab shows up, etc.
  if (!render_view_host_->GetView())
    delegate_->RenderProcessGoneFromRenderManager(render_view_host_);
  else if (!delegate_->IsHidden())
    render_view_host_->GetView()->Show();

  // Hide the old view now that the new one is visible.
  if (old_render_view_host->GetView()) {
    old_render_view_host->GetView()->Hide();
    old_render_view_host->WasSwappedOut();
  }

  // Make sure the size is up to date.  (Fix for bug 1079768.)
  delegate_->UpdateRenderViewSizeForRenderManager();

  if (will_focus_location_bar)
    delegate_->SetFocusToLocationBar(false);
  else if (focus_render_view && render_view_host_->GetView())
    RenderWidgetHostViewPort::FromRWHV(render_view_host_->GetView())->Focus();

  // Notify that we've swapped RenderViewHosts. We do this
  // before shutting down the RVH so that we can clean up
  // RendererResources related to the RVH first.
  delegate_->NotifySwappedFromRenderManager(old_render_view_host,
                                            render_view_host_);

  // If the pending view was on the swapped out list, we can remove it.
  swapped_out_hosts_.erase(render_view_host_->GetSiteInstance()->GetId());

  // If there are no active RVHs in this SiteInstance, it means that
  // this RVH was the last active one in the SiteInstance. Now that we
  // know that all RVHs are swapped out, we can delete all the RVHs in
  // this SiteInstance.
  if (!static_cast<SiteInstanceImpl*>(old_render_view_host->GetSiteInstance())->
          active_view_count()) {
    ShutdownRenderViewHostsInSiteInstance(
        old_render_view_host->GetSiteInstance()->GetId());
    // This is deleted while cleaning up the SitaInstance's views.
    old_render_view_host = NULL;
  } else if (old_render_view_host->IsRenderViewLive()) {
    // If the old RVH is live, we are swapping it out and should keep track of
    // it in case we navigate back to it.
    DCHECK(old_render_view_host->is_swapped_out());
    // Temp fix for http://crbug.com/90867 until we do a better cleanup to make
    // sure we don't get different rvh instances for the same site instance
    // in the same rvhmgr.
    // TODO(creis): Clean this up.
    int32 old_site_instance_id =
        old_render_view_host->GetSiteInstance()->GetId();
    RenderViewHostMap::iterator iter =
        swapped_out_hosts_.find(old_site_instance_id);
    if (iter != swapped_out_hosts_.end() &&
        iter->second != old_render_view_host) {
      // Shutdown the RVH that will be replaced in the map to avoid a leak.
      iter->second->Shutdown();
    }
    swapped_out_hosts_[old_site_instance_id] = old_render_view_host;
  } else {
    old_render_view_host->Shutdown();
    old_render_view_host = NULL;  // Shutdown() deletes it.
  }
}

void RenderFrameHostManager::ShutdownRenderViewHostsInSiteInstance(
    int32 site_instance_id) {
  // First remove any swapped out RVH for this SiteInstance from our
  // list.
  swapped_out_hosts_.erase(site_instance_id);

  scoped_ptr<RenderWidgetHostIterator> widgets(
      RenderWidgetHostImpl::GetAllRenderWidgetHosts());
  while (RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (!widget->IsRenderView())
      continue;
    RenderViewHostImpl* rvh =
        static_cast<RenderViewHostImpl*>(RenderViewHost::From(widget));
    if (site_instance_id == rvh->GetSiteInstance()->GetId())
      rvh->Shutdown();
  }
}

RenderViewHostImpl* RenderFrameHostManager::UpdateRendererStateForNavigate(
    const NavigationEntryImpl& entry) {
  // If we are currently navigating cross-process, we want to get back to normal
  // and then navigate as usual.
  if (cross_navigation_pending_) {
    if (pending_render_view_host_)
      CancelPending();
    cross_navigation_pending_ = false;
  }

  // render_view_host_'s SiteInstance and new_instance will not be deleted
  // before the end of this method, so we don't have to worry about their ref
  // counts dropping to zero.
  SiteInstance* current_instance = render_view_host_->GetSiteInstance();
  SiteInstance* new_instance = current_instance;

  // We do not currently swap processes for navigations in webview tag guests.
  bool is_guest_scheme = current_instance->GetSiteURL().SchemeIs(kGuestScheme);

  // Determine if we need a new BrowsingInstance for this entry.  If true, this
  // implies that it will get a new SiteInstance (and likely process), and that
  // other tabs in the current BrosingInstance will be unalbe to script it.
  // This is used for cases that require a process swap even in the
  // process-per-tab model, such as WebUI pages.
  const NavigationEntry* current_entry =
      delegate_->GetLastCommittedNavigationEntryForRenderManager();
  bool force_swap = !is_guest_scheme &&
      ShouldSwapBrowsingInstancesForNavigation(current_entry, &entry);
  if (!is_guest_scheme && (ShouldTransitionCrossSite() || force_swap))
    new_instance = GetSiteInstanceForEntry(entry, current_instance, force_swap);

  // If force_swap is true, we must use a different SiteInstance.  If we didn't,
  // we would have two RenderViewHosts in the same SiteInstance and the same
  // tab, resulting in page_id conflicts for their NavigationEntries.
  if (force_swap)
    CHECK_NE(new_instance, current_instance);

  if (new_instance != current_instance) {
    // New SiteInstance: create a pending RVH to navigate.
    DCHECK(!cross_navigation_pending_);

    // This will possibly create (set to NULL) a Web UI object for the pending
    // page. We'll use this later to give the page special access. This must
    // happen before the new renderer is created below so it will get bindings.
    // It must also happen after the above conditional call to CancelPending(),
    // otherwise CancelPending may clear the pending_web_ui_ and the page will
    // not have its bindings set appropriately.
    SetPendingWebUI(entry);

    // Ensure that we have created RVHs for the new RVH's opener chain if
    // we are staying in the same BrowsingInstance. This allows the pending RVH
    // to send cross-process script calls to its opener(s).
    int opener_route_id = MSG_ROUTING_NONE;
    if (new_instance->IsRelatedSiteInstance(current_instance)) {
      opener_route_id =
          delegate_->CreateOpenerRenderViewsForRenderManager(new_instance);
    }

    // Create a non-swapped-out pending RVH with the given opener and navigate
    // it.
    int route_id = CreateRenderView(new_instance, opener_route_id, false,
                                    delegate_->IsHidden());
    if (route_id == MSG_ROUTING_NONE)
      return NULL;

    // Check if our current RVH is live before we set up a transition.
    if (!render_view_host_->IsRenderViewLive()) {
      if (!cross_navigation_pending_) {
        // The current RVH is not live.  There's no reason to sit around with a
        // sad tab or a newly created RVH while we wait for the pending RVH to
        // navigate.  Just switch to the pending RVH now and go back to non
        // cross-navigating (Note that we don't care about on{before}unload
        // handlers if the current RVH isn't live.)
        CommitPending();
        return render_view_host_;
      } else {
        NOTREACHED();
        return render_view_host_;
      }
    }
    // Otherwise, it's safe to treat this as a pending cross-site transition.

    // We need to wait until the beforeunload handler has run, unless we are
    // transferring an existing request (in which case it has already run).
    // Suspend the new render view (i.e., don't let it send the cross-site
    // Navigate message) until we hear back from the old renderer's
    // beforeunload handler.  If the handler returns false, we'll have to
    // cancel the request.
    DCHECK(!pending_render_view_host_->are_navigations_suspended());
    bool is_transfer =
        entry.transferred_global_request_id() != GlobalRequestID();
    if (is_transfer) {
      // We don't need to stop the old renderer or run beforeunload/unload
      // handlers, because those have already been done.
      DCHECK(pending_nav_params_->global_request_id ==
                entry.transferred_global_request_id());
    } else {
      // Also make sure the old render view stops, in case a load is in
      // progress.  (We don't want to do this for transfers, since it will
      // interrupt the transfer with an unexpected DidStopLoading.)
      render_view_host_->Send(
          new ViewMsg_Stop(render_view_host_->GetRoutingID()));

      pending_render_view_host_->SetNavigationsSuspended(true,
                                                         base::TimeTicks());

      // Tell the CrossSiteRequestManager that this RVH has a pending cross-site
      // request, so that ResourceDispatcherHost will know to tell us to run the
      // old page's unload handler before it sends the response.
      pending_render_view_host_->SetHasPendingCrossSiteRequest(true);
    }

    // We now have a pending RVH.
    DCHECK(!cross_navigation_pending_);
    cross_navigation_pending_ = true;

    // Unless we are transferring an existing request, we should now
    // tell the old render view to run its beforeunload handler, since it
    // doesn't otherwise know that the cross-site request is happening.  This
    // will trigger a call to ShouldClosePage with the reply.
    if (!is_transfer)
      render_view_host_->FirePageBeforeUnload(true);

    return pending_render_view_host_;
  }

  // Otherwise the same SiteInstance can be used.  Navigate render_view_host_.
  DCHECK(!cross_navigation_pending_);
  if (ShouldReuseWebUI(current_entry, &entry)) {
    pending_web_ui_.reset();
    pending_and_current_web_ui_ = web_ui_->AsWeakPtr();
  } else {
    SetPendingWebUI(entry);

    // Make sure the new RenderViewHost has the right bindings.
    if (pending_web_ui() && !render_view_host_->GetProcess()->IsGuest())
      render_view_host_->AllowBindings(pending_web_ui()->GetBindings());
  }

  if (pending_web_ui() && render_view_host_->IsRenderViewLive())
    pending_web_ui()->GetController()->RenderViewReused(render_view_host_);

  // The renderer can exit view source mode when any error or cancellation
  // happen. We must overwrite to recover the mode.
  if (entry.IsViewSourceMode()) {
    render_view_host_->Send(
        new ViewMsg_EnableViewSourceMode(render_view_host_->GetRoutingID()));
  }

  return render_view_host_;
}

void RenderFrameHostManager::CancelPending() {
  RenderViewHostImpl* pending_render_view_host = pending_render_view_host_;
  pending_render_view_host_ = NULL;

  RenderViewDevToolsAgentHost::OnCancelPendingNavigation(
      pending_render_view_host,
      render_view_host_);

  // We no longer need to prevent the process from exiting.
  pending_render_view_host->GetProcess()->RemovePendingView();

  // The pending RVH may already be on the swapped out list if we started to
  // swap it back in and then canceled.  If so, make sure it gets swapped out
  // again.  If it's not on the swapped out list (e.g., aborting a pending
  // load), then it's safe to shut down.
  if (IsOnSwappedOutList(pending_render_view_host)) {
    // Any currently suspended navigations are no longer needed.
    pending_render_view_host->CancelSuspendedNavigations();

    pending_render_view_host->SwapOut();
  } else {
    // We won't be coming back, so shut this one down.
    pending_render_view_host->Shutdown();
  }

  pending_web_ui_.reset();
  pending_and_current_web_ui_.reset();
}

void RenderFrameHostManager::RenderViewDeleted(RenderViewHost* rvh) {
  // We are doing this in order to work around and to track a crasher
  // (http://crbug.com/23411) where it seems that pending_render_view_host_ is
  // deleted (not sure from where) but not NULLed.
  if (rvh == pending_render_view_host_) {
    // If you hit this NOTREACHED, please report it in the following bug
    // http://crbug.com/23411 Make sure to include what you were doing when it
    // happened  (navigating to a new page, closing a tab...) and if you can
    // reproduce.
    NOTREACHED();
    pending_render_view_host_ = NULL;
  }

  // Make sure deleted RVHs are not kept in the swapped out list while we are
  // still alive.  (If render_view_host_ is null, we're already being deleted.)
  if (!render_view_host_)
    return;
  // We can't look it up by SiteInstance ID, which may no longer be valid.
  for (RenderViewHostMap::iterator iter = swapped_out_hosts_.begin();
       iter != swapped_out_hosts_.end();
       ++iter) {
    if (iter->second == rvh) {
      swapped_out_hosts_.erase(iter);
      break;
    }
  }
}

bool RenderFrameHostManager::IsOnSwappedOutList(RenderViewHost* rvh) const {
  if (!rvh->GetSiteInstance())
    return false;

  RenderViewHostMap::const_iterator iter = swapped_out_hosts_.find(
      rvh->GetSiteInstance()->GetId());
  if (iter == swapped_out_hosts_.end())
    return false;

  return iter->second == rvh;
}

RenderViewHostImpl* RenderFrameHostManager::GetSwappedOutRenderViewHost(
    SiteInstance* instance) {
  RenderViewHostMap::iterator iter = swapped_out_hosts_.find(instance->GetId());
  if (iter != swapped_out_hosts_.end())
    return iter->second;

  return NULL;
}

}  // namespace content
