// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_IMPL_H_
#define CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_IMPL_H_

#include <map>
#include <queue>
#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/process/process.h"
#include "base/timer/timer.h"
#include "content/browser/child_process_launcher.h"
#include "content/browser/geolocation/geolocation_dispatcher_host.h"
#include "content/browser/power_monitor_message_broadcaster.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/gpu_data_manager_observer.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_channel_proxy.h"
#include "ui/surface/transport_dib.h"

class CommandLine;
struct ViewHostMsg_CompositorSurfaceBuffersSwapped_Params;

namespace base {
class MessageLoop;
}

namespace gfx {
class Size;
}

namespace content {
class AudioRendererHost;
class BrowserDemuxerAndroid;
class GeolocationDispatcherHost;
class GpuMessageFilter;
class MessagePortMessageFilter;
class PeerConnectionTrackerHost;
class RendererMainThread;
class RenderWidgetHelper;
class RenderWidgetHost;
class RenderWidgetHostImpl;
class RenderWidgetHostViewFrameSubscriber;
class StoragePartition;
class StoragePartitionImpl;

// Implements a concrete RenderProcessHost for the browser process for talking
// to actual renderer processes (as opposed to mocks).
//
// Represents the browser side of the browser <--> renderer communication
// channel. There will be one RenderProcessHost per renderer process.
//
// This object is refcounted so that it can release its resources when all
// hosts using it go away.
//
// This object communicates back and forth with the RenderProcess object
// running in the renderer process. Each RenderProcessHost and RenderProcess
// keeps a list of RenderView (renderer) and WebContentsImpl (browser) which
// are correlated with IDs. This way, the Views and the corresponding ViewHosts
// communicate through the two process objects.
//
// A RenderProcessHost is also associated with one and only one
// StoragePartition.  This allows us to implement strong storage isolation
// because all the IPCs from the RenderViews (renderer) will only ever be able
// to access the partition they are assigned to.
class CONTENT_EXPORT RenderProcessHostImpl
    : public RenderProcessHost,
      public ChildProcessLauncher::Client,
      public GpuDataManagerObserver {
 public:
  RenderProcessHostImpl(BrowserContext* browser_context,
                        StoragePartitionImpl* storage_partition_impl,
                        bool supports_browser_plugin,
                        bool is_guest);
  virtual ~RenderProcessHostImpl();

  // RenderProcessHost implementation (public portion).
  virtual void EnableSendQueue() OVERRIDE;
  virtual bool Init() OVERRIDE;
  virtual int GetNextRoutingID() OVERRIDE;
  virtual void AddRoute(int32 routing_id, IPC::Listener* listener) OVERRIDE;
  virtual void RemoveRoute(int32 routing_id) OVERRIDE;
  virtual void AddObserver(RenderProcessHostObserver* observer) OVERRIDE;
  virtual void RemoveObserver(RenderProcessHostObserver* observer) OVERRIDE;
  virtual bool WaitForBackingStoreMsg(int render_widget_id,
                                      const base::TimeDelta& max_delay,
                                      IPC::Message* msg) OVERRIDE;
  virtual void ReceivedBadMessage() OVERRIDE;
  virtual void WidgetRestored() OVERRIDE;
  virtual void WidgetHidden() OVERRIDE;
  virtual int VisibleWidgetCount() const OVERRIDE;
  virtual bool IsGuest() const OVERRIDE;
  virtual StoragePartition* GetStoragePartition() const OVERRIDE;
  virtual bool FastShutdownIfPossible() OVERRIDE;
  virtual void DumpHandles() OVERRIDE;
  virtual base::ProcessHandle GetHandle() const OVERRIDE;
  virtual TransportDIB* GetTransportDIB(TransportDIB::Id dib_id) OVERRIDE;
  virtual TransportDIB* MapTransportDIB(TransportDIB::Id dib_id) OVERRIDE;
  virtual BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual bool InSameStoragePartition(
      StoragePartition* partition) const OVERRIDE;
  virtual int GetID() const OVERRIDE;
  virtual bool HasConnection() const OVERRIDE;
  virtual void SetIgnoreInputEvents(bool ignore_input_events) OVERRIDE;
  virtual bool IgnoreInputEvents() const OVERRIDE;
  virtual void Cleanup() OVERRIDE;
  virtual void AddPendingView() OVERRIDE;
  virtual void RemovePendingView() OVERRIDE;
  virtual void SetSuddenTerminationAllowed(bool enabled) OVERRIDE;
  virtual bool SuddenTerminationAllowed() const OVERRIDE;
  virtual IPC::ChannelProxy* GetChannel() OVERRIDE;
  virtual void AddFilter(BrowserMessageFilter* filter) OVERRIDE;
  virtual bool FastShutdownForPageCount(size_t count) OVERRIDE;
  virtual bool FastShutdownStarted() const OVERRIDE;
  virtual base::TimeDelta GetChildProcessIdleTime() const OVERRIDE;
  virtual void SurfaceUpdated(int32 surface_id) OVERRIDE;
  virtual void ResumeRequestsForView(int route_id) OVERRIDE;

  // IPC::Sender via RenderProcessHost.
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // IPC::Listener via RenderProcessHost.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
  virtual void OnChannelConnected(int32 peer_pid) OVERRIDE;
  virtual void OnChannelError() OVERRIDE;

  // ChildProcessLauncher::Client implementation.
  virtual void OnProcessLaunched() OVERRIDE;

  scoped_refptr<AudioRendererHost> audio_renderer_host() const;

  // Tells the ResourceDispatcherHost to resume a deferred navigation without
  // transferring it to a new renderer process.
  void ResumeDeferredNavigation(const GlobalRequestID& request_id);

  // Call this function when it is evident that the child process is actively
  // performing some operation, for example if we just received an IPC message.
  void mark_child_process_activity_time() {
    child_process_activity_time_ = base::TimeTicks::Now();
  }

  // Returns the current number of active views in this process.  Excludes
  // any RenderViewHosts that are swapped out.
  int GetActiveViewCount();

  // Start and end frame subscription for a specific renderer.
  // This API only supports subscription to accelerated composited frames.
  void BeginFrameSubscription(
      int route_id,
      scoped_ptr<RenderWidgetHostViewFrameSubscriber> subscriber);
  void EndFrameSubscription(int route_id);

  scoped_refptr<GeolocationDispatcherHost>
      geolocation_dispatcher_host() const {
    return make_scoped_refptr(geolocation_dispatcher_host_);
  }

  // Register/unregister the host identified by the host id in the global host
  // list.
  static void RegisterHost(int host_id, RenderProcessHost* host);
  static void UnregisterHost(int host_id);

  // Returns true if |host| is suitable for launching a new view with |site_url|
  // in the given |browser_context|.
  static bool IsSuitableHost(RenderProcessHost* host,
                             BrowserContext* browser_context,
                             const GURL& site_url);

  // Returns an existing RenderProcessHost for |url| in |browser_context|,
  // if one exists.  Otherwise a new RenderProcessHost should be created and
  // registered using RegisterProcessHostForSite().
  // This should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static RenderProcessHost* GetProcessHostForSite(
      BrowserContext* browser_context,
      const GURL& url);

  // Registers the given |process| to be used for any instance of |url|
  // within |browser_context|.
  // This should only be used for process-per-site mode, which can be enabled
  // globally with a command line flag or per-site, as determined by
  // SiteInstanceImpl::ShouldUseProcessPerSite.
  static void RegisterProcessHostForSite(
      BrowserContext* browser_context,
      RenderProcessHost* process,
      const GURL& url);

  static base::MessageLoop* GetInProcessRendererThreadForTesting();

  // This forces a renderer that is running "in process" to shut down.
  static void ShutDownInProcessRenderer();

#if defined(OS_ANDROID)
  const scoped_refptr<BrowserDemuxerAndroid>& browser_demuxer_android() {
    return browser_demuxer_android_;
  }
#endif

  MessagePortMessageFilter* message_port_message_filter() const {
    return message_port_message_filter_;
  }

  void SetIsGuestForTesting(bool is_guest) {
    is_guest_ = is_guest;
  }

 protected:
  // A proxy for our IPC::Channel that lives on the IO thread (see
  // browser_process.h)
  scoped_ptr<IPC::ChannelProxy> channel_;

  // True if fast shutdown has been performed on this RPH.
  bool fast_shutdown_started_;

  // True if we've posted a DeleteTask and will be deleted soon.
  bool deleting_soon_;

#ifndef NDEBUG
  // True if this object has deleted itself.
  bool is_self_deleted_;
#endif

  // The count of currently swapped out but pending RenderViews.  We have
  // started to swap these in, so the renderer process should not exit if
  // this count is non-zero.
  int32 pending_views_;

 private:
  friend class VisitRelayingRenderProcessHost;

  // Creates and adds the IO thread message filters.
  void CreateMessageFilters();

  // Control message handlers.
  void OnShutdownRequest();
  void OnDumpHandlesDone();
  void SuddenTerminationChanged(bool enabled);
  void OnUserMetricsRecordAction(const std::string& action);
  void OnSavedPageAsMHTML(int job_id, int64 mhtml_file_size);

  // CompositorSurfaceBuffersSwapped handler when there's no RWH.
  void OnCompositorSurfaceBuffersSwappedNoHost(
      const ViewHostMsg_CompositorSurfaceBuffersSwapped_Params& params);

  // Generates a command line to be used to spawn a renderer and appends the
  // results to |*command_line|.
  void AppendRendererCommandLine(CommandLine* command_line) const;

  // Copies applicable command line switches from the given |browser_cmd| line
  // flags to the output |renderer_cmd| line flags. Not all switches will be
  // copied over.
  void PropagateBrowserCommandLineToRenderer(const CommandLine& browser_cmd,
                                             CommandLine* renderer_cmd) const;

  // Callers can reduce the RenderProcess' priority.
  void SetBackgrounded(bool backgrounded);

  // Handle termination of our process.
  void ProcessDied(bool already_dead);

  virtual void OnGpuSwitching() OVERRIDE;

  // The registered IPC listener objects. When this list is empty, we should
  // delete ourselves.
  IDMap<IPC::Listener> listeners_;

  // The count of currently visible widgets.  Since the host can be a container
  // for multiple widgets, it uses this count to determine when it should be
  // backgrounded.
  int32 visible_widgets_;

  // Does this process have backgrounded priority.
  bool backgrounded_;

  // Used to allow a RenderWidgetHost to intercept various messages on the
  // IO thread.
  scoped_refptr<RenderWidgetHelper> widget_helper_;

  // The filter for GPU-related messages coming from the renderer.
  // Thread safety note: this field is to be accessed from the UI thread.
  // We don't keep a reference to it, to avoid it being destroyed on the UI
  // thread, but we clear this field when we clear channel_. When channel_ goes
  // away, it posts a task to the IO thread to destroy it there, so we know that
  // it's valid if non-NULL.
  GpuMessageFilter* gpu_message_filter_;

  // The filter for MessagePort messages coming from the renderer.
  scoped_refptr<MessagePortMessageFilter> message_port_message_filter_;

  // A map of transport DIB ids to cached TransportDIBs
  std::map<TransportDIB::Id, TransportDIB*> cached_dibs_;

  enum {
    // This is the maximum size of |cached_dibs_|
    MAX_MAPPED_TRANSPORT_DIBS = 3,
  };

  void ClearTransportDIBCache();
  // This is used to clear our cache five seconds after the last use.
  base::DelayTimer<RenderProcessHostImpl> cached_dibs_cleaner_;

  // Used in single-process mode.
  scoped_ptr<base::Thread> in_process_renderer_;

  // True after Init() has been called. We can't just check channel_ because we
  // also reset that in the case of process termination.
  bool is_initialized_;

  // Used to launch and terminate the process without blocking the UI thread.
  scoped_ptr<ChildProcessLauncher> child_process_launcher_;

  // Messages we queue while waiting for the process handle.  We queue them here
  // instead of in the channel so that we ensure they're sent after init related
  // messages that are sent once the process handle is available.  This is
  // because the queued messages may have dependencies on the init messages.
  std::queue<IPC::Message*> queued_messages_;

  // The globally-unique identifier for this RPH.
  int id_;

  BrowserContext* browser_context_;

  // Owned by |browser_context_|.
  StoragePartitionImpl* storage_partition_impl_;

  // The observers watching our lifetime.
  ObserverList<RenderProcessHostObserver> observers_;

  // True if the process can be shut down suddenly.  If this is true, then we're
  // sure that all the RenderViews in the process can be shutdown suddenly.  If
  // it's false, then specific RenderViews might still be allowed to be shutdown
  // suddenly by checking their SuddenTerminationAllowed() flag.  This can occur
  // if one WebContents has an unload event listener but another WebContents in
  // the same process doesn't.
  bool sudden_termination_allowed_;

  // Set to true if we shouldn't send input events.  We actually do the
  // filtering for this at the render widget level.
  bool ignore_input_events_;

  // Records the last time we regarded the child process active.
  base::TimeTicks child_process_activity_time_;

  // Indicates whether this is a RenderProcessHost that has permission to embed
  // Browser Plugins.
  bool supports_browser_plugin_;

  // Indicates whether this is a RenderProcessHost of a Browser Plugin guest
  // renderer.
  bool is_guest_;

  // Forwards messages between WebRTCInternals in the browser process
  // and PeerConnectionTracker in the renderer process.
  scoped_refptr<PeerConnectionTrackerHost> peer_connection_tracker_host_;

  // Prevents the class from being added as a GpuDataManagerImpl observer more
  // than once.
  bool gpu_observer_registered_;

  // Forwards power state messages to the renderer process.
  PowerMonitorMessageBroadcaster power_monitor_broadcaster_;

  scoped_refptr<AudioRendererHost> audio_renderer_host_;

#if defined(OS_ANDROID)
  scoped_refptr<BrowserDemuxerAndroid> browser_demuxer_android_;
#endif

  // Message filter for geolocation messages.
  GeolocationDispatcherHost* geolocation_dispatcher_host_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_BROWSER_RENDER_PROCESS_HOST_IMPL_H_
