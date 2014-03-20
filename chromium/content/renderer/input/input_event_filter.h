// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_INPUT_INPUT_EVENT_FILTER_H_
#define CONTENT_RENDERER_INPUT_INPUT_EVENT_FILTER_H_

#include <queue>
#include <set>

#include "base/callback_forward.h"
#include "base/synchronization/lock.h"
#include "content/common/content_export.h"
#include "content/port/common/input_event_ack_state.h"
#include "content/renderer/input/input_handler_manager_client.h"
#include "ipc/ipc_channel_proxy.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

// This class can be used to intercept InputMsg_HandleInputEvent messages
// and have them be delivered to a target thread.  Input events are filtered
// based on routing_id (see AddRoute and RemoveRoute).
//
// The user of this class provides an instance of InputEventFilter::Handler,
// which will be passed WebInputEvents on the target thread.
//

namespace content {

class CONTENT_EXPORT InputEventFilter
    : public InputHandlerManagerClient,
      public IPC::ChannelProxy::MessageFilter {
 public:
  InputEventFilter(IPC::Listener* main_listener,
                   const scoped_refptr<base::MessageLoopProxy>& target_loop);

  // The |handler| is invoked on the thread associated with |target_loop| to
  // handle input events matching the filtered routes.
  //
  // If INPUT_EVENT_ACK_STATE_NOT_CONSUMED is returned by the handler,
  // the original InputMsg_HandleInputEvent message will be delivered to
  // |main_listener| on the main thread.  (The "main thread" in this context is
  // the thread where the InputEventFilter was constructed.)  The responsibility
  // is left to the eventual handler to deliver the corresponding
  // InputHostMsg_HandleInputEvent_ACK.
  //
  virtual void SetBoundHandler(const Handler& handler) OVERRIDE;
  virtual void DidAddInputHandler(int routing_id,
                                  cc::InputHandler* input_handler) OVERRIDE;
  virtual void DidRemoveInputHandler(int routing_id) OVERRIDE;
  virtual void DidOverscroll(int routing_id,
                             const cc::DidOverscrollParams& params) OVERRIDE;

  // IPC::ChannelProxy::MessageFilter methods:
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  friend class IPC::ChannelProxy::MessageFilter;
  virtual ~InputEventFilter();

  void ForwardToMainListener(const IPC::Message& message);
  void ForwardToHandler(const IPC::Message& message);
  void SendACK(blink::WebInputEvent::Type type,
               InputEventAckState ack_result,
               const ui::LatencyInfo& latency_info,
               int routing_id);
  void SendMessageOnIOThread(const IPC::Message& message);

  scoped_refptr<base::MessageLoopProxy> main_loop_;
  IPC::Listener* main_listener_;

  // The sender_ only gets invoked on the thread corresponding to io_loop_.
  scoped_refptr<base::MessageLoopProxy> io_loop_;
  IPC::Sender* sender_;

  // The handler_ only gets Run on the thread corresponding to target_loop_.
  scoped_refptr<base::MessageLoopProxy> target_loop_;
  Handler handler_;

  // Protects access to routes_.
  base::Lock routes_lock_;

  // Indicates the routing_ids for which input events should be filtered.
  std::set<int> routes_;

  // Specifies whether overscroll notifications are forwarded to the host.
  bool overscroll_notifications_enabled_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_INPUT_INPUT_EVENT_FILTER_H_
