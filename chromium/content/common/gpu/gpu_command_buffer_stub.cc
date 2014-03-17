// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "base/hash.h"
#include "base/memory/shared_memory.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/common/gpu/devtools_gpu_instrumentation.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_command_buffer_stub.h"
#include "content/common/gpu/gpu_memory_manager.h"
#include "content/common/gpu/gpu_memory_tracking.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/gpu_watchdog.h"
#include "content/common/gpu/image_transport_surface.h"
#include "content/common/gpu/media/gpu_video_decode_accelerator.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/public/common/content_client.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/gles2_cmd_utils.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/service/gl_context_virtual.h"
#include "gpu/command_buffer/service/gl_state_restorer_impl.h"
#include "gpu/command_buffer/service/gpu_control_service.h"
#include "gpu/command_buffer/service/image_manager.h"
#include "gpu/command_buffer/service/logger.h"
#include "gpu/command_buffer/service/memory_tracking.h"
#include "gpu/command_buffer/service/query_manager.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_switches.h"

#if defined(OS_WIN)
#include "content/public/common/sandbox_init.h"
#endif

#if defined(OS_ANDROID)
#include "content/common/gpu/stream_texture_manager_android.h"
#endif

namespace content {
namespace {

// The GpuCommandBufferMemoryTracker class provides a bridge between the
// ContextGroup's memory type managers and the GpuMemoryManager class.
class GpuCommandBufferMemoryTracker : public gpu::gles2::MemoryTracker {
 public:
  explicit GpuCommandBufferMemoryTracker(GpuChannel* channel) :
      tracking_group_(channel->gpu_channel_manager()->gpu_memory_manager()->
          CreateTrackingGroup(channel->renderer_pid(), this)) {
  }

  virtual void TrackMemoryAllocatedChange(
      size_t old_size,
      size_t new_size,
      gpu::gles2::MemoryTracker::Pool pool) OVERRIDE {
    tracking_group_->TrackMemoryAllocatedChange(
        old_size, new_size, pool);
  }

  virtual bool EnsureGPUMemoryAvailable(size_t size_needed) OVERRIDE {
    return tracking_group_->EnsureGPUMemoryAvailable(size_needed);
  };

 private:
  virtual ~GpuCommandBufferMemoryTracker() {
  }
  scoped_ptr<GpuMemoryTrackingGroup> tracking_group_;

  DISALLOW_COPY_AND_ASSIGN(GpuCommandBufferMemoryTracker);
};

// FastSetActiveURL will shortcut the expensive call to SetActiveURL when the
// url_hash matches.
void FastSetActiveURL(const GURL& url, size_t url_hash) {
  // Leave the previously set URL in the empty case -- empty URLs are given by
  // WebKitPlatformSupportImpl::createOffscreenGraphicsContext3D. Hopefully the
  // onscreen context URL was set previously and will show up even when a crash
  // occurs during offscreen command processing.
  if (url.is_empty())
    return;
  static size_t g_last_url_hash = 0;
  if (url_hash != g_last_url_hash) {
    g_last_url_hash = url_hash;
    GetContentClient()->SetActiveURL(url);
  }
}

// The first time polling a fence, delay some extra time to allow other
// stubs to process some work, or else the timing of the fences could
// allow a pattern of alternating fast and slow frames to occur.
const int64 kHandleMoreWorkPeriodMs = 2;
const int64 kHandleMoreWorkPeriodBusyMs = 1;

// Prevents idle work from being starved.
const int64 kMaxTimeSinceIdleMs = 10;

}  // namespace

GpuCommandBufferStub::GpuCommandBufferStub(
    GpuChannel* channel,
    GpuCommandBufferStub* share_group,
    const gfx::GLSurfaceHandle& handle,
    gpu::gles2::MailboxManager* mailbox_manager,
    gpu::gles2::ImageManager* image_manager,
    const gfx::Size& size,
    const gpu::gles2::DisallowedFeatures& disallowed_features,
    const std::vector<int32>& attribs,
    gfx::GpuPreference gpu_preference,
    bool use_virtualized_gl_context,
    int32 route_id,
    int32 surface_id,
    GpuWatchdog* watchdog,
    bool software,
    const GURL& active_url)
    : channel_(channel),
      handle_(handle),
      initial_size_(size),
      disallowed_features_(disallowed_features),
      requested_attribs_(attribs),
      gpu_preference_(gpu_preference),
      use_virtualized_gl_context_(use_virtualized_gl_context),
      route_id_(route_id),
      surface_id_(surface_id),
      software_(software),
      last_flush_count_(0),
      last_memory_allocation_valid_(false),
      watchdog_(watchdog),
      sync_point_wait_count_(0),
      delayed_work_scheduled_(false),
      previous_messages_processed_(0),
      active_url_(active_url),
      total_gpu_memory_(0) {
  active_url_hash_ = base::Hash(active_url.possibly_invalid_spec());
  FastSetActiveURL(active_url_, active_url_hash_);
  if (share_group) {
    context_group_ = share_group->context_group_;
  } else {
    gpu::StreamTextureManager* stream_texture_manager = NULL;
#if defined(OS_ANDROID)
    stream_texture_manager = channel_->stream_texture_manager();
#endif
    context_group_ = new gpu::gles2::ContextGroup(
        mailbox_manager,
        image_manager,
        new GpuCommandBufferMemoryTracker(channel),
        stream_texture_manager,
        NULL,
        true);
  }

  use_virtualized_gl_context_ |=
      context_group_->feature_info()->workarounds().use_virtualized_gl_contexts;
}

GpuCommandBufferStub::~GpuCommandBufferStub() {
  Destroy();

  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->Send(new GpuHostMsg_DestroyCommandBuffer(surface_id()));
}

GpuMemoryManager* GpuCommandBufferStub::GetMemoryManager() const {
    return channel()->gpu_channel_manager()->gpu_memory_manager();
}

bool GpuCommandBufferStub::OnMessageReceived(const IPC::Message& message) {
  devtools_gpu_instrumentation::ScopedGpuTask task(this);
  FastSetActiveURL(active_url_, active_url_hash_);

  // Ensure the appropriate GL context is current before handling any IPC
  // messages directed at the command buffer. This ensures that the message
  // handler can assume that the context is current (not necessary for
  // Echo, RetireSyncPoint, or WaitSyncPoint).
  if (decoder_.get() &&
      message.type() != GpuCommandBufferMsg_Echo::ID &&
      message.type() != GpuCommandBufferMsg_GetStateFast::ID &&
      message.type() != GpuCommandBufferMsg_RetireSyncPoint::ID &&
      message.type() != GpuCommandBufferMsg_SetLatencyInfo::ID) {
    if (!MakeCurrent())
      return false;
  }

  // Always use IPC_MESSAGE_HANDLER_DELAY_REPLY for synchronous message handlers
  // here. This is so the reply can be delayed if the scheduler is unscheduled.
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(GpuCommandBufferStub, message)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_Initialize,
                                    OnInitialize);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_SetGetBuffer,
                                    OnSetGetBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_ProduceFrontBuffer,
                        OnProduceFrontBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Echo, OnEcho);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetState, OnGetState);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetStateFast,
                                    OnGetStateFast);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_AsyncFlush, OnAsyncFlush);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetLatencyInfo, OnSetLatencyInfo);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_Rescheduled, OnRescheduled);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RegisterTransferBuffer,
                        OnRegisterTransferBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyTransferBuffer,
                        OnDestroyTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_GetTransferBuffer,
                                    OnGetTransferBuffer);
    IPC_MESSAGE_HANDLER_DELAY_REPLY(GpuCommandBufferMsg_CreateVideoDecoder,
                                    OnCreateVideoDecoder)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SetSurfaceVisible,
                        OnSetSurfaceVisible)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RetireSyncPoint,
                        OnRetireSyncPoint)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalSyncPoint,
                        OnSignalSyncPoint)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SignalQuery,
                        OnSignalQuery)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_SendClientManagedMemoryStats,
                        OnReceivedClientManagedMemoryStats)
    IPC_MESSAGE_HANDLER(
        GpuCommandBufferMsg_SetClientHasMemoryAllocationChangedCallback,
        OnSetClientHasMemoryAllocationChangedCallback)
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_RegisterGpuMemoryBuffer,
                        OnRegisterGpuMemoryBuffer);
    IPC_MESSAGE_HANDLER(GpuCommandBufferMsg_DestroyGpuMemoryBuffer,
                        OnDestroyGpuMemoryBuffer);
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  // Ensure that any delayed work that was created will be handled.
  ScheduleDelayedWork(kHandleMoreWorkPeriodMs);

  DCHECK(handled);
  return handled;
}

bool GpuCommandBufferStub::Send(IPC::Message* message) {
  return channel_->Send(message);
}

bool GpuCommandBufferStub::IsScheduled() {
  return (!scheduler_.get() || scheduler_->IsScheduled());
}

bool GpuCommandBufferStub::HasMoreWork() {
  return scheduler_.get() && scheduler_->HasMoreWork();
}

void GpuCommandBufferStub::PollWork() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::PollWork");
  delayed_work_scheduled_ = false;
  FastSetActiveURL(active_url_, active_url_hash_);
  if (decoder_.get() && !MakeCurrent())
    return;

  if (scheduler_) {
    bool fences_complete = scheduler_->PollUnscheduleFences();
    // Perform idle work if all fences are complete.
    if (fences_complete) {
      uint64 current_messages_processed =
          channel()->gpu_channel_manager()->MessagesProcessed();
      // We're idle when no messages were processed or scheduled.
      bool is_idle =
          (previous_messages_processed_ == current_messages_processed) &&
          !channel()->gpu_channel_manager()->HandleMessagesScheduled();
      if (!is_idle && !last_idle_time_.is_null()) {
        base::TimeDelta time_since_idle = base::TimeTicks::Now() -
            last_idle_time_;
        base::TimeDelta max_time_since_idle =
            base::TimeDelta::FromMilliseconds(kMaxTimeSinceIdleMs);

        // Force idle when it's been too long since last time we were idle.
        if (time_since_idle > max_time_since_idle)
          is_idle = true;
      }

      if (is_idle) {
        last_idle_time_ = base::TimeTicks::Now();
        scheduler_->PerformIdleWork();
      }
    }
  }
  ScheduleDelayedWork(kHandleMoreWorkPeriodBusyMs);
}

bool GpuCommandBufferStub::HasUnprocessedCommands() {
  if (command_buffer_) {
    gpu::CommandBuffer::State state = command_buffer_->GetLastState();
    return state.put_offset != state.get_offset &&
        !gpu::error::IsError(state.error);
  }
  return false;
}

void GpuCommandBufferStub::ScheduleDelayedWork(int64 delay) {
  if (!HasMoreWork()) {
    last_idle_time_ = base::TimeTicks();
    return;
  }

  if (delayed_work_scheduled_)
    return;
  delayed_work_scheduled_ = true;

  // Idle when no messages are processed between now and when
  // PollWork is called.
  previous_messages_processed_ =
      channel()->gpu_channel_manager()->MessagesProcessed();
  if (last_idle_time_.is_null())
    last_idle_time_ = base::TimeTicks::Now();

  // IsScheduled() returns true after passing all unschedule fences
  // and this is when we can start performing idle work. Idle work
  // is done synchronously so we can set delay to 0 and instead poll
  // for more work at the rate idle work is performed. This also ensures
  // that idle work is done as efficiently as possible without any
  // unnecessary delays.
  if (scheduler_.get() &&
      scheduler_->IsScheduled() &&
      scheduler_->HasMoreIdleWork()) {
    delay = 0;
  }

  base::MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&GpuCommandBufferStub::PollWork, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(delay));
}

void GpuCommandBufferStub::OnEcho(const IPC::Message& message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnEcho");
  Send(new IPC::Message(message));
}

bool GpuCommandBufferStub::MakeCurrent() {
  if (decoder_->MakeCurrent())
    return true;
  DLOG(ERROR) << "Context lost because MakeCurrent failed.";
  command_buffer_->SetContextLostReason(decoder_->GetContextLostReason());
  command_buffer_->SetParseError(gpu::error::kLostContext);
  CheckContextLost();
  return false;
}

void GpuCommandBufferStub::Destroy() {
  if (handle_.is_null() && !active_url_.is_empty()) {
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    gpu_channel_manager->Send(new GpuHostMsg_DidDestroyOffscreenContext(
        active_url_));
  }

  memory_manager_client_state_.reset();

  while (!sync_points_.empty())
    OnRetireSyncPoint(sync_points_.front());

  if (decoder_)
    decoder_->set_engine(NULL);

  // The scheduler has raw references to the decoder and the command buffer so
  // destroy it before those.
  scheduler_.reset();

  bool have_context = false;
  if (decoder_ && command_buffer_ &&
      command_buffer_->GetState().error != gpu::error::kLostContext)
    have_context = decoder_->MakeCurrent();
  FOR_EACH_OBSERVER(DestructionObserver,
                    destruction_observers_,
                    OnWillDestroyStub());

  if (decoder_) {
    decoder_->Destroy(have_context);
    decoder_.reset();
  }

  command_buffer_.reset();

  // Remove this after crbug.com/248395 is sorted out.
  surface_ = NULL;
}

void GpuCommandBufferStub::OnInitializeFailed(IPC::Message* reply_message) {
  Destroy();
  GpuCommandBufferMsg_Initialize::WriteReplyParams(
      reply_message, false, gpu::Capabilities());
  Send(reply_message);
}

void GpuCommandBufferStub::OnInitialize(
    base::SharedMemoryHandle shared_state_handle,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnInitialize");
  DCHECK(!command_buffer_.get());

  scoped_ptr<base::SharedMemory> shared_state_shm(
      new base::SharedMemory(shared_state_handle, false));

  command_buffer_.reset(new gpu::CommandBufferService(
      context_group_->transfer_buffer_manager()));

  if (!command_buffer_->Initialize()) {
    DLOG(ERROR) << "CommandBufferService failed to initialize.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  decoder_.reset(::gpu::gles2::GLES2Decoder::Create(context_group_.get()));

  scheduler_.reset(new gpu::GpuScheduler(command_buffer_.get(),
                                         decoder_.get(),
                                         decoder_.get()));
  if (preemption_flag_.get())
    scheduler_->SetPreemptByFlag(preemption_flag_);

  decoder_->set_engine(scheduler_.get());

  if (!handle_.is_null()) {
#if defined(OS_MACOSX) || defined(UI_COMPOSITOR_IMAGE_TRANSPORT)
    if (software_) {
      DLOG(ERROR) << "No software support.\n";
      OnInitializeFailed(reply_message);
      return;
    }
#endif

    surface_ = ImageTransportSurface::CreateSurface(
        channel_->gpu_channel_manager(),
        this,
        handle_);
  } else {
    GpuChannelManager* manager = channel_->gpu_channel_manager();
    surface_ = manager->GetDefaultOffscreenSurface();
  }

  if (!surface_.get()) {
    DLOG(ERROR) << "Failed to create surface.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  scoped_refptr<gfx::GLContext> context;
  if (use_virtualized_gl_context_ && channel_->share_group()) {
    context = channel_->share_group()->GetSharedContext();
    if (!context.get()) {
      context = gfx::GLContext::CreateGLContext(
          channel_->share_group(),
          channel_->gpu_channel_manager()->GetDefaultOffscreenSurface(),
          gpu_preference_);
      channel_->share_group()->SetSharedContext(context.get());
    }
    // This should be a non-virtual GL context.
    DCHECK(context->GetHandle());
    context = new gpu::GLContextVirtual(
        channel_->share_group(), context.get(), decoder_->AsWeakPtr());
    if (!context->Initialize(surface_.get(), gpu_preference_)) {
      // TODO(sievers): The real context created above for the default
      // offscreen surface might not be compatible with this surface.
      // Need to adjust at least GLX to be able to create the initial context
      // with a config that is compatible with onscreen and offscreen surfaces.
      context = NULL;

      DLOG(ERROR) << "Failed to initialize virtual GL context.";
      OnInitializeFailed(reply_message);
      return;
    }
  }
  if (!context.get()) {
    context = gfx::GLContext::CreateGLContext(
        channel_->share_group(), surface_.get(), gpu_preference_);
  }
  if (!context.get()) {
    DLOG(ERROR) << "Failed to create context.\n";
    OnInitializeFailed(reply_message);
    return;
  }

  if (!context->MakeCurrent(surface_.get())) {
    LOG(ERROR) << "Failed to make context current.";
    OnInitializeFailed(reply_message);
    return;
  }

  if (!context->GetGLStateRestorer()) {
    context->SetGLStateRestorer(
        new gpu::GLStateRestorerImpl(decoder_->AsWeakPtr()));
  }

  if (!context->GetTotalGpuMemory(&total_gpu_memory_))
    total_gpu_memory_ = 0;

  if (!context_group_->has_program_cache()) {
    context_group_->set_program_cache(
        channel_->gpu_channel_manager()->program_cache());
  }

  // Initialize the decoder with either the view or pbuffer GLContext.
  if (!decoder_->Initialize(surface_,
                            context,
                            !surface_id(),
                            initial_size_,
                            disallowed_features_,
                            requested_attribs_)) {
    DLOG(ERROR) << "Failed to initialize decoder.";
    OnInitializeFailed(reply_message);
    return;
  }

  gpu_control_.reset(
      new gpu::GpuControlService(context_group_->image_manager(),
                                 NULL,
                                 context_group_->mailbox_manager(),
                                 NULL,
                                 decoder_->GetCapabilities()));

  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableGPUServiceLogging)) {
    decoder_->set_log_commands(true);
  }

  decoder_->GetLogger()->SetMsgCallback(
      base::Bind(&GpuCommandBufferStub::SendConsoleMessage,
                 base::Unretained(this)));
  decoder_->SetShaderCacheCallback(
      base::Bind(&GpuCommandBufferStub::SendCachedShader,
                 base::Unretained(this)));
  decoder_->SetWaitSyncPointCallback(
      base::Bind(&GpuCommandBufferStub::OnWaitSyncPoint,
                 base::Unretained(this)));

  command_buffer_->SetPutOffsetChangeCallback(
      base::Bind(&GpuCommandBufferStub::PutChanged, base::Unretained(this)));
  command_buffer_->SetGetBufferChangeCallback(
      base::Bind(&gpu::GpuScheduler::SetGetBuffer,
                 base::Unretained(scheduler_.get())));
  command_buffer_->SetParseErrorCallback(
      base::Bind(&GpuCommandBufferStub::OnParseError, base::Unretained(this)));
  scheduler_->SetSchedulingChangedCallback(
      base::Bind(&GpuChannel::StubSchedulingChanged,
                 base::Unretained(channel_)));

  if (watchdog_) {
    scheduler_->SetCommandProcessedCallback(
        base::Bind(&GpuCommandBufferStub::OnCommandProcessed,
                   base::Unretained(this)));
  }

  if (!command_buffer_->SetSharedStateBuffer(shared_state_shm.Pass())) {
    DLOG(ERROR) << "Failed to map shared stae buffer.";
    OnInitializeFailed(reply_message);
    return;
  }

  GpuCommandBufferMsg_Initialize::WriteReplyParams(
      reply_message, true, gpu_control_->GetCapabilities());
  Send(reply_message);

  if (handle_.is_null() && !active_url_.is_empty()) {
    GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
    gpu_channel_manager->Send(new GpuHostMsg_DidCreateOffscreenContext(
        active_url_));
  }
}

void GpuCommandBufferStub::OnSetLatencyInfo(
    const ui::LatencyInfo& latency_info) {
  if (!latency_info_callback_.is_null())
    latency_info_callback_.Run(latency_info);
}

void GpuCommandBufferStub::SetLatencyInfoCallback(
    const LatencyInfoCallback& callback) {
  latency_info_callback_ = callback;
}

int32 GpuCommandBufferStub::GetRequestedAttribute(int attr) const {
  // The command buffer is pairs of enum, value
  // search for the requested attribute, return the value.
  for (std::vector<int32>::const_iterator it = requested_attribs_.begin();
       it != requested_attribs_.end(); ++it) {
    if (*it++ == attr) {
      return *it;
    }
  }
  return -1;
}

void GpuCommandBufferStub::OnSetGetBuffer(int32 shm_id,
                                          IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSetGetBuffer");
  if (command_buffer_)
    command_buffer_->SetGetBuffer(shm_id);
  Send(reply_message);
}

void GpuCommandBufferStub::OnProduceFrontBuffer(const gpu::Mailbox& mailbox) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnProduceFrontBuffer");
  if (!decoder_)
    LOG(ERROR) << "Can't produce front buffer before initialization.";

  if (!decoder_->ProduceFrontBuffer(mailbox))
    LOG(ERROR) << "Failed to produce front buffer.";
}

void GpuCommandBufferStub::OnGetState(IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnGetState");
  if (command_buffer_) {
    gpu::CommandBuffer::State state = command_buffer_->GetState();
    CheckContextLost();
    GpuCommandBufferMsg_GetState::WriteReplyParams(reply_message, state);
  } else {
    DLOG(ERROR) << "no command_buffer.";
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnParseError() {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnParseError");
  DCHECK(command_buffer_.get());
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  IPC::Message* msg = new GpuCommandBufferMsg_Destroyed(
      route_id_, state.context_lost_reason);
  msg->set_unblock(true);
  Send(msg);

  // Tell the browser about this context loss as well, so it can
  // determine whether client APIs like WebGL need to be immediately
  // blocked from automatically running.
  GpuChannelManager* gpu_channel_manager = channel_->gpu_channel_manager();
  gpu_channel_manager->Send(new GpuHostMsg_DidLoseContext(
      handle_.is_null(), state.context_lost_reason, active_url_));

  CheckContextLost();
}

void GpuCommandBufferStub::OnGetStateFast(IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnGetStateFast");
  DCHECK(command_buffer_.get());
  CheckContextLost();
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  GpuCommandBufferMsg_GetStateFast::WriteReplyParams(reply_message, state);
  Send(reply_message);
}

void GpuCommandBufferStub::OnAsyncFlush(int32 put_offset,
                                        uint32 flush_count) {
  TRACE_EVENT1("gpu", "GpuCommandBufferStub::OnAsyncFlush",
               "put_offset", put_offset);
  DCHECK(command_buffer_.get());
  if (flush_count - last_flush_count_ < 0x8000000U) {
    last_flush_count_ = flush_count;
    command_buffer_->Flush(put_offset);
  } else {
    // We received this message out-of-order. This should not happen but is here
    // to catch regressions. Ignore the message.
    NOTREACHED() << "Received a Flush message out-of-order";
  }

  ReportState();
}

void GpuCommandBufferStub::OnRescheduled() {
  gpu::CommandBuffer::State pre_state = command_buffer_->GetLastState();
  command_buffer_->Flush(pre_state.put_offset);
  gpu::CommandBuffer::State post_state = command_buffer_->GetLastState();

  if (pre_state.get_offset != post_state.get_offset)
    ReportState();
}

void GpuCommandBufferStub::OnRegisterTransferBuffer(
    int32 id,
    base::SharedMemoryHandle transfer_buffer,
    uint32 size) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnRegisterTransferBuffer");
  base::SharedMemory shared_memory(transfer_buffer, false);

  if (command_buffer_)
    command_buffer_->RegisterTransferBuffer(id, &shared_memory, size);
}

void GpuCommandBufferStub::OnDestroyTransferBuffer(int32 id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyTransferBuffer");

  if (command_buffer_)
    command_buffer_->DestroyTransferBuffer(id);
}

void GpuCommandBufferStub::OnGetTransferBuffer(
    int32 id,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnGetTransferBuffer");
  if (command_buffer_) {
    base::SharedMemoryHandle transfer_buffer = base::SharedMemoryHandle();
    uint32 size = 0;

    gpu::Buffer buffer = command_buffer_->GetTransferBuffer(id);
    if (buffer.shared_memory) {
#if defined(OS_WIN)
      transfer_buffer = NULL;
      BrokerDuplicateHandle(buffer.shared_memory->handle(),
          channel_->renderer_pid(), &transfer_buffer, FILE_MAP_READ |
          FILE_MAP_WRITE, 0);
      DCHECK(transfer_buffer != NULL);
#else
      buffer.shared_memory->ShareToProcess(channel_->renderer_pid(),
                                           &transfer_buffer);
#endif
      size = buffer.size;
    }

    GpuCommandBufferMsg_GetTransferBuffer::WriteReplyParams(reply_message,
                                                            transfer_buffer,
                                                            size);
  } else {
    reply_message->set_reply_error();
  }
  Send(reply_message);
}

void GpuCommandBufferStub::OnCommandProcessed() {
  if (watchdog_)
    watchdog_->CheckArmed();
}

void GpuCommandBufferStub::ReportState() {
  if (!CheckContextLost())
    command_buffer_->UpdateState();
}

void GpuCommandBufferStub::PutChanged() {
  FastSetActiveURL(active_url_, active_url_hash_);
  scheduler_->PutChanged();
}

void GpuCommandBufferStub::OnCreateVideoDecoder(
    media::VideoCodecProfile profile,
    IPC::Message* reply_message) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnCreateVideoDecoder");
  int decoder_route_id = channel_->GenerateRouteID();
  GpuVideoDecodeAccelerator* decoder = new GpuVideoDecodeAccelerator(
      decoder_route_id, this, channel_->io_message_loop());
  decoder->Initialize(profile, reply_message);
  // decoder is registered as a DestructionObserver of this stub and will
  // self-delete during destruction of this stub.
}

void GpuCommandBufferStub::OnSetSurfaceVisible(bool visible) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnSetSurfaceVisible");
  if (memory_manager_client_state_)
    memory_manager_client_state_->SetVisible(visible);
}

void GpuCommandBufferStub::AddSyncPoint(uint32 sync_point) {
  sync_points_.push_back(sync_point);
}

void GpuCommandBufferStub::OnRetireSyncPoint(uint32 sync_point) {
  DCHECK(!sync_points_.empty() && sync_points_.front() == sync_point);
  sync_points_.pop_front();
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->sync_point_manager()->RetireSyncPoint(sync_point);
}

bool GpuCommandBufferStub::OnWaitSyncPoint(uint32 sync_point) {
  if (sync_point_wait_count_ == 0) {
    TRACE_EVENT_ASYNC_BEGIN1("gpu", "WaitSyncPoint", this,
                             "GpuCommandBufferStub", this);
  }
  scheduler_->SetScheduled(false);
  ++sync_point_wait_count_;
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->sync_point_manager()->AddSyncPointCallback(
      sync_point,
      base::Bind(&GpuCommandBufferStub::OnSyncPointRetired,
                 this->AsWeakPtr()));
  return scheduler_->IsScheduled();
}

void GpuCommandBufferStub::OnSyncPointRetired() {
  --sync_point_wait_count_;
  if (sync_point_wait_count_ == 0) {
    TRACE_EVENT_ASYNC_END1("gpu", "WaitSyncPoint", this,
                           "GpuCommandBufferStub", this);
  }
  scheduler_->SetScheduled(true);
}

void GpuCommandBufferStub::OnSignalSyncPoint(uint32 sync_point, uint32 id) {
  GpuChannelManager* manager = channel_->gpu_channel_manager();
  manager->sync_point_manager()->AddSyncPointCallback(
      sync_point,
      base::Bind(&GpuCommandBufferStub::OnSignalSyncPointAck,
                 this->AsWeakPtr(),
                 id));
}

void GpuCommandBufferStub::OnSignalSyncPointAck(uint32 id) {
  Send(new GpuCommandBufferMsg_SignalSyncPointAck(route_id_, id));
}

void GpuCommandBufferStub::OnSignalQuery(uint32 query_id, uint32 id) {
  if (decoder_) {
    gpu::gles2::QueryManager* query_manager = decoder_->GetQueryManager();
    if (query_manager) {
      gpu::gles2::QueryManager::Query* query =
          query_manager->GetQuery(query_id);
      if (query) {
        query->AddCallback(
          base::Bind(&GpuCommandBufferStub::OnSignalSyncPointAck,
                     this->AsWeakPtr(),
                     id));
        return;
      }
    }
  }
  // Something went wrong, run callback immediately.
  OnSignalSyncPointAck(id);
}


void GpuCommandBufferStub::OnReceivedClientManagedMemoryStats(
    const gpu::ManagedMemoryStats& stats) {
  TRACE_EVENT0(
      "gpu",
      "GpuCommandBufferStub::OnReceivedClientManagedMemoryStats");
  if (memory_manager_client_state_)
    memory_manager_client_state_->SetManagedMemoryStats(stats);
}

void GpuCommandBufferStub::OnSetClientHasMemoryAllocationChangedCallback(
    bool has_callback) {
  TRACE_EVENT0(
      "gpu",
      "GpuCommandBufferStub::OnSetClientHasMemoryAllocationChangedCallback");
  if (has_callback) {
    if (!memory_manager_client_state_) {
      memory_manager_client_state_.reset(GetMemoryManager()->CreateClientState(
          this, surface_id_ != 0, true));
    }
  } else {
    memory_manager_client_state_.reset();
  }
}

void GpuCommandBufferStub::OnRegisterGpuMemoryBuffer(
    int32 id,
    gfx::GpuMemoryBufferHandle gpu_memory_buffer,
    uint32 width,
    uint32 height,
    uint32 internalformat) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnRegisterGpuMemoryBuffer");
  if (gpu_control_) {
    gpu_control_->RegisterGpuMemoryBuffer(id,
                                          gpu_memory_buffer,
                                          width,
                                          height,
                                          internalformat);
  }
}

void GpuCommandBufferStub::OnDestroyGpuMemoryBuffer(int32 id) {
  TRACE_EVENT0("gpu", "GpuCommandBufferStub::OnDestroyGpuMemoryBuffer");
  if (gpu_control_)
    gpu_control_->DestroyGpuMemoryBuffer(id);
}

void GpuCommandBufferStub::SendConsoleMessage(
    int32 id,
    const std::string& message) {
  GPUCommandBufferConsoleMessage console_message;
  console_message.id = id;
  console_message.message = message;
  IPC::Message* msg = new GpuCommandBufferMsg_ConsoleMsg(
      route_id_, console_message);
  msg->set_unblock(true);
  Send(msg);
}

void GpuCommandBufferStub::SendCachedShader(
    const std::string& key, const std::string& shader) {
  channel_->CacheShader(key, shader);
}

void GpuCommandBufferStub::AddDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void GpuCommandBufferStub::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

void GpuCommandBufferStub::SetPreemptByFlag(
    scoped_refptr<gpu::PreemptionFlag> flag) {
  preemption_flag_ = flag;
  if (scheduler_)
    scheduler_->SetPreemptByFlag(preemption_flag_);
}

bool GpuCommandBufferStub::GetTotalGpuMemory(uint64* bytes) {
  *bytes = total_gpu_memory_;
  return !!total_gpu_memory_;
}

gfx::Size GpuCommandBufferStub::GetSurfaceSize() const {
  if (!surface_.get())
    return gfx::Size();
  return surface_->GetSize();
}

gpu::gles2::MemoryTracker* GpuCommandBufferStub::GetMemoryTracker() const {
  return context_group_->memory_tracker();
}

void GpuCommandBufferStub::SetMemoryAllocation(
    const gpu::MemoryAllocation& allocation) {
  if (!last_memory_allocation_valid_ ||
      !allocation.Equals(last_memory_allocation_)) {
    Send(new GpuCommandBufferMsg_SetMemoryAllocation(
        route_id_, allocation));
  }

  last_memory_allocation_valid_ = true;
  last_memory_allocation_ = allocation;
}

void GpuCommandBufferStub::SuggestHaveFrontBuffer(
    bool suggest_have_frontbuffer) {
  // This can be called outside of OnMessageReceived, so the context needs
  // to be made current before calling methods on the surface.
  if (surface_.get() && MakeCurrent())
    surface_->SetFrontbufferAllocation(suggest_have_frontbuffer);
}

bool GpuCommandBufferStub::CheckContextLost() {
  DCHECK(command_buffer_);
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  bool was_lost = state.error == gpu::error::kLostContext;
  // Lose all other contexts if the reset was triggered by the robustness
  // extension instead of being synthetic.
  if (was_lost && decoder_ && decoder_->WasContextLostByRobustnessExtension() &&
      (gfx::GLContext::LosesAllContextsOnContextLost() ||
       use_virtualized_gl_context_))
    channel_->LoseAllContexts();
  return was_lost;
}

void GpuCommandBufferStub::MarkContextLost() {
  if (!command_buffer_ ||
      command_buffer_->GetState().error == gpu::error::kLostContext)
    return;

  command_buffer_->SetContextLostReason(gpu::error::kUnknown);
  if (decoder_)
    decoder_->LoseContext(GL_UNKNOWN_CONTEXT_RESET_ARB);
  command_buffer_->SetParseError(gpu::error::kLostContext);
}

uint64 GpuCommandBufferStub::GetMemoryUsage() const {
  return GetMemoryManager()->GetClientMemoryUsage(this);
}

}  // namespace content
