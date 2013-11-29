// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
#define CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_

#include <string>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/common/gpu/gpu_process_launch_causes.h"
#include "content/common/message_router.h"
#include "gpu/config/gpu_info.h"
#include "ipc/ipc_channel_handle.h"
#include "ipc/ipc_channel_proxy.h"
#include "ipc/ipc_sync_channel.h"
#include "media/video/video_decode_accelerator.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gl/gpu_preference.h"

class GURL;
class TransportTextureService;
struct GPUCreateCommandBufferConfig;

namespace base {
class MessageLoop;
class MessageLoopProxy;
}

namespace gpu {
struct Mailbox;
}

namespace IPC {
class SyncMessageFilter;
}

namespace content {
class CommandBufferProxyImpl;
class GpuChannelHost;
struct GpuRenderingStats;

struct GpuListenerInfo {
  GpuListenerInfo();
  ~GpuListenerInfo();

  base::WeakPtr<IPC::Listener> listener;
  scoped_refptr<base::MessageLoopProxy> loop;
};

class CONTENT_EXPORT GpuChannelHostFactory {
 public:
  typedef base::Callback<void(const gfx::Size)> CreateImageCallback;

  virtual ~GpuChannelHostFactory() {}

  virtual bool IsMainThread() = 0;
  virtual base::MessageLoop* GetMainLoop() = 0;
  virtual scoped_refptr<base::MessageLoopProxy> GetIOLoopProxy() = 0;
  virtual base::WaitableEvent* GetShutDownEvent() = 0;
  virtual scoped_ptr<base::SharedMemory> AllocateSharedMemory(size_t size) = 0;
  virtual int32 CreateViewCommandBuffer(
      int32 surface_id, const GPUCreateCommandBufferConfig& init_params) = 0;
  virtual GpuChannelHost* EstablishGpuChannelSync(CauseForGpuLaunch) = 0;
  virtual void CreateImage(
      gfx::PluginWindowHandle window,
      int32 image_id,
      const CreateImageCallback& callback) = 0;
  virtual void DeleteImage(int32 image_id, int32 sync_point) = 0;
};

// Encapsulates an IPC channel between the client and one GPU process.
// On the GPU process side there's a corresponding GpuChannel.
// Every method can be called on any thread with a message loop, except for the
// IO thread.
class GpuChannelHost : public IPC::Sender,
                       public base::RefCountedThreadSafe<GpuChannelHost> {
 public:
  // Must be called on the main thread (as defined by the factory).
  static scoped_refptr<GpuChannelHost> Create(
      GpuChannelHostFactory* factory,
      int gpu_host_id,
      int client_id,
      const gpu::GPUInfo& gpu_info,
      const IPC::ChannelHandle& channel_handle);

  bool IsLost() const {
    DCHECK(channel_filter_.get());
    return channel_filter_->IsLost();
  }

  // The GPU stats reported by the GPU process.
  const gpu::GPUInfo& gpu_info() const { return gpu_info_; }

  // IPC::Sender implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxyImpl* CreateViewCommandBuffer(
      int32 surface_id,
      CommandBufferProxyImpl* share_group,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Create and connect to a command buffer in the GPU process.
  CommandBufferProxyImpl* CreateOffscreenCommandBuffer(
      const gfx::Size& size,
      CommandBufferProxyImpl* share_group,
      const std::string& allowed_extensions,
      const std::vector<int32>& attribs,
      const GURL& active_url,
      gfx::GpuPreference gpu_preference);

  // Creates a video decoder in the GPU process.
  scoped_ptr<media::VideoDecodeAccelerator> CreateVideoDecoder(
      int command_buffer_route_id,
      media::VideoCodecProfile profile,
      media::VideoDecodeAccelerator::Client* client);

  // Destroy a command buffer created by this channel.
  void DestroyCommandBuffer(CommandBufferProxyImpl* command_buffer);

  // Collect rendering stats from GPU process.
  bool CollectRenderingStatsForSurface(
      int surface_id, GpuRenderingStats* stats);

  // Add a route for the current message loop.
  void AddRoute(int route_id, base::WeakPtr<IPC::Listener> listener);
  void RemoveRoute(int route_id);

  GpuChannelHostFactory* factory() const { return factory_; }
  int gpu_host_id() const { return gpu_host_id_; }

  int client_id() const { return client_id_; }

  // Returns a handle to the shared memory that can be sent via IPC to the
  // GPU process. The caller is responsible for ensuring it is closed. Returns
  // an invalid handle on failure.
  base::SharedMemoryHandle ShareToGpuProcess(
      base::SharedMemoryHandle source_handle);

  // Generates |num| unique mailbox names that can be used with
  // GL_texture_mailbox_CHROMIUM. Unlike genMailboxCHROMIUM, this IPC is
  // handled only on the GPU process' IO thread, and so is not effectively
  // a finish.
  bool GenerateMailboxNames(unsigned num, std::vector<gpu::Mailbox>* names);

  // Reserve one unused transfer buffer ID.
  int32 ReserveTransferBufferId();

 private:
  friend class base::RefCountedThreadSafe<GpuChannelHost>;
  GpuChannelHost(GpuChannelHostFactory* factory,
                 int gpu_host_id,
                 int client_id,
                 const gpu::GPUInfo& gpu_info);
  virtual ~GpuChannelHost();
  void Connect(const IPC::ChannelHandle& channel_handle);

  // A filter used internally to route incoming messages from the IO thread
  // to the correct message loop. It also maintains some shared state between
  // all the contexts.
  class MessageFilter : public IPC::ChannelProxy::MessageFilter {
   public:
    MessageFilter();

    // Called on the IO thread.
    void AddRoute(int route_id,
                  base::WeakPtr<IPC::Listener> listener,
                  scoped_refptr<base::MessageLoopProxy> loop);
    // Called on the IO thread.
    void RemoveRoute(int route_id);

    // IPC::ChannelProxy::MessageFilter implementation
    // (called on the IO thread):
    virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;
    virtual void OnChannelError() OVERRIDE;

    // The following methods can be called on any thread.

    // Whether the channel is lost.
    bool IsLost() const;

    // Gets mailboxes from the pool, and return the number of mailboxes to ask
    // the GPU process to maintain a good pool size. The caller is responsible
    // for sending the GpuChannelMsg_GenerateMailboxNamesAsync message.
    size_t GetMailboxNames(size_t num, std::vector<gpu::Mailbox>* names);

   private:
    virtual ~MessageFilter();
    bool OnControlMessageReceived(const IPC::Message& msg);

    // Message handlers.
    void OnGenerateMailboxNamesReply(const std::vector<gpu::Mailbox>& names);

    // Threading notes: |listeners_| is only accessed on the IO thread. Every
    // other field is protected by |lock_|.
    typedef base::hash_map<int, GpuListenerInfo> ListenerMap;
    ListenerMap listeners_;

    // Protexts all fields below this one.
    mutable base::Lock lock_;

    // Whether the channel has been lost.
    bool lost_;

    // A pool of valid mailbox names.
    std::vector<gpu::Mailbox> mailbox_name_pool_;

    // Number of pending mailbox requested from the GPU process.
    size_t requested_mailboxes_;
  };

  // Threading notes: all fields are constant during the lifetime of |this|
  // except:
  // - |next_transfer_buffer_id_|, atomic type
  // - |proxies_|, protected by |context_lock_|
  GpuChannelHostFactory* const factory_;
  const int client_id_;
  const int gpu_host_id_;

  const gpu::GPUInfo gpu_info_;

  scoped_ptr<IPC::SyncChannel> channel_;
  scoped_refptr<MessageFilter> channel_filter_;

  // A filter for sending messages from thread other than the main thread.
  scoped_refptr<IPC::SyncMessageFilter> sync_filter_;

  // Transfer buffer IDs are allocated in sequence.
  base::AtomicSequenceNumber next_transfer_buffer_id_;

  // Protects proxies_.
  mutable base::Lock context_lock_;
  // Used to look up a proxy from its routing id.
  typedef base::hash_map<int, CommandBufferProxyImpl*> ProxyMap;
  ProxyMap proxies_;

  DISALLOW_COPY_AND_ASSIGN(GpuChannelHost);
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GPU_CHANNEL_HOST_H_
