// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER
#define CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "cc/output/context_provider.h"
#include "content/common/content_export.h"
#include "content/common/gpu/client/webgraphicscontext3d_command_buffer_impl.h"

namespace webkit {
namespace gpu {
class GrContextForWebGraphicsContext3D;
}
}

namespace content {

// Implementation of cc::ContextProvider that provides a
// WebGraphicsContext3DCommandBufferImpl context and a GrContext.
class CONTENT_EXPORT ContextProviderCommandBuffer
    : NON_EXPORTED_BASE(public cc::ContextProvider) {
 public:
  static scoped_refptr<ContextProviderCommandBuffer> Create(
      scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
      const std::string& debug_name);

  virtual bool BindToCurrentThread() OVERRIDE;
  virtual WebGraphicsContext3DCommandBufferImpl* Context3d() OVERRIDE;
  virtual class GrContext* GrContext() OVERRIDE;
  virtual Capabilities ContextCapabilities() OVERRIDE;
  virtual void VerifyContexts() OVERRIDE;
  virtual bool DestroyedOnMainThread() OVERRIDE;
  virtual void SetLostContextCallback(
      const LostContextCallback& lost_context_callback) OVERRIDE;
  virtual void SetSwapBuffersCompleteCallback(
      const SwapBuffersCompleteCallback& swap_buffers_complete_callback)
      OVERRIDE;
  virtual void SetMemoryPolicyChangedCallback(
      const MemoryPolicyChangedCallback& memory_policy_changed_callback)
      OVERRIDE;

  void set_leak_on_destroy() {
    base::AutoLock lock(main_thread_lock_);
    leak_on_destroy_ = true;
  }

 protected:
  ContextProviderCommandBuffer(
      scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d,
      const std::string& debug_name);
  virtual ~ContextProviderCommandBuffer();

  void OnLostContext();
  void OnSwapBuffersComplete();
  void OnMemoryAllocationChanged(
      const WebKit::WebGraphicsMemoryAllocation& allocation);

 private:
  void InitializeCapabilities();

  base::ThreadChecker main_thread_checker_;
  base::ThreadChecker context_thread_checker_;

  scoped_ptr<WebGraphicsContext3DCommandBufferImpl> context3d_;
  scoped_ptr<webkit::gpu::GrContextForWebGraphicsContext3D> gr_context_;

  cc::ContextProvider::Capabilities capabilities_;
  std::string debug_name_;

  LostContextCallback lost_context_callback_;
  SwapBuffersCompleteCallback swap_buffers_complete_callback_;
  MemoryPolicyChangedCallback memory_policy_changed_callback_;

  base::Lock main_thread_lock_;
  bool leak_on_destroy_;
  bool destroyed_;

  class LostContextCallbackProxy;
  scoped_ptr<LostContextCallbackProxy> lost_context_callback_proxy_;

  class SwapBuffersCompleteCallbackProxy;
  scoped_ptr<SwapBuffersCompleteCallbackProxy>
      swap_buffers_complete_callback_proxy_;

  class MemoryAllocationCallbackProxy;
  scoped_ptr<MemoryAllocationCallbackProxy> memory_allocation_callback_proxy_;
};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_CONTEXT_PROVIDER_COMMAND_BUFFER
