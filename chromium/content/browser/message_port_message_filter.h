// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MESSAGE_PORT_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_MESSAGE_PORT_MESSAGE_FILTER_H_

#include "base/callback.h"
#include "content/public/browser/browser_message_filter.h"

namespace content {

// Filter for MessagePort related IPC messages (creating and destroying a
// MessagePort, sending a message via a MessagePort etc).
class MessagePortMessageFilter : public BrowserMessageFilter {
 public:
  typedef base::Callback<int(void)> NextRoutingIDCallback;

  // |next_routing_id| is owned by this object.  It can be used up until
  // OnChannelClosing.
  explicit MessagePortMessageFilter(const NextRoutingIDCallback& callback);

  // BrowserMessageFilter implementation.
  virtual void OnChannelClosing() OVERRIDE;
  virtual bool OnMessageReceived(const IPC::Message& message,
                                 bool* message_was_ok) OVERRIDE;
  virtual void OnDestruct() const OVERRIDE;

  int GetNextRoutingID();

 private:
  friend class BrowserThread;
  friend class base::DeleteHelper<MessagePortMessageFilter>;

  virtual ~MessagePortMessageFilter();

  // Message handlers.
  void OnCreateMessagePort(int* route_id, int* message_port_id);

  // This is guaranteed to be valid until OnChannelClosing is invoked, and it's
  // not used after.
  NextRoutingIDCallback next_routing_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(MessagePortMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_MESSAGE_FILTER_H_
