// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gl_in_process_context.h"

#include <set>
#include <utility>
#include <vector>

#include <GLES2/gl2.h>
#ifndef GL_GLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES 1
#endif
#include <GLES2/gl2ext.h>
#include <GLES2/gl2extchromium.h>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/service/in_process_command_buffer.h"
#include "ui/gfx/size.h"
#include "ui/gl/gl_image.h"

#if defined(OS_ANDROID)
#include "ui/gl/android/surface_texture.h"
#endif

namespace gpu {

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const size_t kStartTransferBufferSize = 4 * 1024 * 1024;
const size_t kMinTransferBufferSize = 1 * 256 * 1024;
const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;

class GLInProcessContextImpl
    : public GLInProcessContext,
      public base::SupportsWeakPtr<GLInProcessContextImpl> {
 public:
  explicit GLInProcessContextImpl();
  virtual ~GLInProcessContextImpl();

  bool Initialize(scoped_refptr<gfx::GLSurface> surface,
                  bool is_offscreen,
                  bool share_resources,
                  gfx::AcceleratedWidget window,
                  const gfx::Size& size,
                  const GLInProcessContextAttribs& attribs,
                  gfx::GpuPreference gpu_preference);

  // GLInProcessContext implementation:
  virtual void SetContextLostCallback(const base::Closure& callback) OVERRIDE;
  virtual void SignalSyncPoint(unsigned sync_point,
                               const base::Closure& callback) OVERRIDE;
  virtual void SignalQuery(unsigned query, const base::Closure& callback)
      OVERRIDE;
  virtual gles2::GLES2Implementation* GetImplementation() OVERRIDE;

#if defined(OS_ANDROID)
  virtual scoped_refptr<gfx::SurfaceTexture> GetSurfaceTexture(
      uint32 stream_id) OVERRIDE;
#endif

 private:
  void Destroy();
  void PollQueryCallbacks();
  void CallQueryCallback(size_t index);
  void OnContextLost();
  void OnSignalSyncPoint(const base::Closure& callback);

  scoped_ptr<gles2::GLES2CmdHelper> gles2_helper_;
  scoped_ptr<TransferBuffer> transfer_buffer_;
  scoped_ptr<gles2::GLES2Implementation> gles2_implementation_;
  scoped_ptr<InProcessCommandBuffer> command_buffer_;

  typedef std::pair<unsigned, base::Closure> QueryCallback;
  std::vector<QueryCallback> query_callbacks_;

  unsigned int share_group_id_;
  bool context_lost_;
  base::Closure context_lost_callback_;

  DISALLOW_COPY_AND_ASSIGN(GLInProcessContextImpl);
};

base::LazyInstance<base::Lock> g_all_shared_contexts_lock =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::set<GLInProcessContextImpl*> > g_all_shared_contexts =
    LAZY_INSTANCE_INITIALIZER;

size_t SharedContextCount() {
  base::AutoLock lock(g_all_shared_contexts_lock.Get());
  return g_all_shared_contexts.Get().size();
}

GLInProcessContextImpl::GLInProcessContextImpl()
    : share_group_id_(0), context_lost_(false) {}

GLInProcessContextImpl::~GLInProcessContextImpl() {
  {
    base::AutoLock lock(g_all_shared_contexts_lock.Get());
    g_all_shared_contexts.Get().erase(this);
  }
  Destroy();
}

void GLInProcessContextImpl::SignalSyncPoint(unsigned sync_point,
                                             const base::Closure& callback) {
  DCHECK(!callback.is_null());
  base::Closure wrapped_callback = base::Bind(
      &GLInProcessContextImpl::OnSignalSyncPoint, AsWeakPtr(), callback);
  command_buffer_->SignalSyncPoint(sync_point, wrapped_callback);
}

gles2::GLES2Implementation* GLInProcessContextImpl::GetImplementation() {
  return gles2_implementation_.get();
}

void GLInProcessContextImpl::SetContextLostCallback(
    const base::Closure& callback) {
  context_lost_callback_ = callback;
}

void GLInProcessContextImpl::OnContextLost() {
  context_lost_ = true;
  if (!context_lost_callback_.is_null()) {
    context_lost_callback_.Run();
  }
}

void GLInProcessContextImpl::OnSignalSyncPoint(const base::Closure& callback) {
  // TODO: Should it always trigger callbacks?
  if (!context_lost_)
    callback.Run();
}

bool GLInProcessContextImpl::Initialize(
    scoped_refptr<gfx::GLSurface> surface,
    bool is_offscreen,
    bool share_resources,
    gfx::AcceleratedWidget window,
    const gfx::Size& size,
    const GLInProcessContextAttribs& attribs,
    gfx::GpuPreference gpu_preference) {
  DCHECK(size.width() >= 0 && size.height() >= 0);

  const int32 ALPHA_SIZE     = 0x3021;
  const int32 BLUE_SIZE      = 0x3022;
  const int32 GREEN_SIZE     = 0x3023;
  const int32 RED_SIZE       = 0x3024;
  const int32 DEPTH_SIZE     = 0x3025;
  const int32 STENCIL_SIZE   = 0x3026;
  const int32 SAMPLES        = 0x3031;
  const int32 SAMPLE_BUFFERS = 0x3032;
  const int32 NONE           = 0x3038;

  std::vector<int32> attrib_vector;
  if (attribs.alpha_size >= 0) {
    attrib_vector.push_back(ALPHA_SIZE);
    attrib_vector.push_back(attribs.alpha_size);
  }
  if (attribs.blue_size >= 0) {
    attrib_vector.push_back(BLUE_SIZE);
    attrib_vector.push_back(attribs.blue_size);
  }
  if (attribs.green_size >= 0) {
    attrib_vector.push_back(GREEN_SIZE);
    attrib_vector.push_back(attribs.green_size);
  }
  if (attribs.red_size >= 0) {
    attrib_vector.push_back(RED_SIZE);
    attrib_vector.push_back(attribs.red_size);
  }
  if (attribs.depth_size >= 0) {
    attrib_vector.push_back(DEPTH_SIZE);
    attrib_vector.push_back(attribs.depth_size);
  }
  if (attribs.stencil_size >= 0) {
    attrib_vector.push_back(STENCIL_SIZE);
    attrib_vector.push_back(attribs.stencil_size);
  }
  if (attribs.samples >= 0) {
    attrib_vector.push_back(SAMPLES);
    attrib_vector.push_back(attribs.samples);
  }
  if (attribs.sample_buffers >= 0) {
    attrib_vector.push_back(SAMPLE_BUFFERS);
    attrib_vector.push_back(attribs.sample_buffers);
  }
  attrib_vector.push_back(NONE);

  base::Closure wrapped_callback =
      base::Bind(&GLInProcessContextImpl::OnContextLost, AsWeakPtr());
  command_buffer_.reset(new InProcessCommandBuffer());

  scoped_ptr<base::AutoLock> scoped_shared_context_lock;
  scoped_refptr<gles2::ShareGroup> share_group;
  if (share_resources) {
    scoped_shared_context_lock.reset(
        new base::AutoLock(g_all_shared_contexts_lock.Get()));
    for (std::set<GLInProcessContextImpl*>::const_iterator it =
             g_all_shared_contexts.Get().begin();
         it != g_all_shared_contexts.Get().end();
         it++) {
      const GLInProcessContextImpl* context = *it;
      if (!context->context_lost_) {
        share_group = context->gles2_implementation_->share_group();
        DCHECK(share_group);
        share_group_id_ = context->share_group_id_;
        break;
      }
      share_group_id_ = std::max(share_group_id_, context->share_group_id_);
    }
    if (!share_group && !++share_group_id_)
        ++share_group_id_;
  }
  if (!command_buffer_->Initialize(surface,
                                   is_offscreen,
                                   share_resources,
                                   window,
                                   size,
                                   attrib_vector,
                                   gpu_preference,
                                   wrapped_callback,
                                   share_group_id_)) {
    LOG(INFO) << "Failed to initialize InProcessCommmandBuffer";
    return false;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_.reset(new gles2::GLES2CmdHelper(command_buffer_.get()));
  if (!gles2_helper_->Initialize(kCommandBufferSize)) {
    LOG(INFO) << "Failed to initialize GLES2CmdHelper";
    Destroy();
    return false;
  }

  // Create a transfer buffer.
  transfer_buffer_.reset(new TransferBuffer(gles2_helper_.get()));

  // Create the object exposing the OpenGL API.
  gles2_implementation_.reset(new gles2::GLES2Implementation(
      gles2_helper_.get(),
      share_group,
      transfer_buffer_.get(),
      false,
      command_buffer_.get()));

  if (share_resources) {
    g_all_shared_contexts.Get().insert(this);
    scoped_shared_context_lock.reset();
  }

  if (!gles2_implementation_->Initialize(
      kStartTransferBufferSize,
      kMinTransferBufferSize,
      kMaxTransferBufferSize,
      gles2::GLES2Implementation::kNoLimit)) {
    return false;
  }

  return true;
}

void GLInProcessContextImpl::Destroy() {
  while (!query_callbacks_.empty()) {
    CallQueryCallback(0);
  }

  if (gles2_implementation_) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gles2_implementation_->Flush();

    gles2_implementation_.reset();
  }

  transfer_buffer_.reset();
  gles2_helper_.reset();
  command_buffer_.reset();
}

void GLInProcessContextImpl::CallQueryCallback(size_t index) {
  DCHECK_LT(index, query_callbacks_.size());
  QueryCallback query_callback = query_callbacks_[index];
  query_callbacks_[index] = query_callbacks_.back();
  query_callbacks_.pop_back();
  query_callback.second.Run();
}

// TODO(sievers): Move this to the service side
void GLInProcessContextImpl::PollQueryCallbacks() {
  for (size_t i = 0; i < query_callbacks_.size();) {
    unsigned query = query_callbacks_[i].first;
    GLuint param = 0;
    gles2::GLES2Implementation* gl = GetImplementation();
    if (gl->IsQueryEXT(query)) {
      gl->GetQueryObjectuivEXT(query, GL_QUERY_RESULT_AVAILABLE_EXT, &param);
    } else {
      param = 1;
    }
    if (param) {
      CallQueryCallback(i);
    } else {
      i++;
    }
  }
  if (!query_callbacks_.empty()) {
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&GLInProcessContextImpl::PollQueryCallbacks,
                   this->AsWeakPtr()),
        base::TimeDelta::FromMilliseconds(5));
  }
}

void GLInProcessContextImpl::SignalQuery(
    unsigned query,
    const base::Closure& callback) {
  query_callbacks_.push_back(std::make_pair(query, callback));
  // If size > 1, there is already a poll callback pending.
  if (query_callbacks_.size() == 1) {
    PollQueryCallbacks();
  }
}

#if defined(OS_ANDROID)
scoped_refptr<gfx::SurfaceTexture>
GLInProcessContextImpl::GetSurfaceTexture(uint32 stream_id) {
  return command_buffer_->GetSurfaceTexture(stream_id);
}
#endif

}  // anonymous namespace

GLInProcessContextAttribs::GLInProcessContextAttribs()
    : alpha_size(-1),
      blue_size(-1),
      green_size(-1),
      red_size(-1),
      depth_size(-1),
      stencil_size(-1),
      samples(-1),
      sample_buffers(-1) {}

// static
GLInProcessContext* GLInProcessContext::CreateContext(
    bool is_offscreen,
    gfx::AcceleratedWidget window,
    const gfx::Size& size,
    bool share_resources,
    const GLInProcessContextAttribs& attribs,
    gfx::GpuPreference gpu_preference) {
  scoped_ptr<GLInProcessContextImpl> context(
      new GLInProcessContextImpl());
  if (!context->Initialize(
      NULL /* surface */,
      is_offscreen,
      share_resources,
      window,
      size,
      attribs,
      gpu_preference))
    return NULL;

  return context.release();
}

// static
GLInProcessContext* GLInProcessContext::CreateWithSurface(
    scoped_refptr<gfx::GLSurface> surface,
    bool share_resources,
    const GLInProcessContextAttribs& attribs,
    gfx::GpuPreference gpu_preference) {
  scoped_ptr<GLInProcessContextImpl> context(
      new GLInProcessContextImpl());
  if (!context->Initialize(
      surface,
      surface->IsOffscreen(),
      share_resources,
      gfx::kNullAcceleratedWidget,
      surface->GetSize(),
      attribs,
      gpu_preference))
    return NULL;

  return context.release();
}

}  // namespace gpu
