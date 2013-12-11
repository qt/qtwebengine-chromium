// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_MESSAGE_PORT_SERVICE_H_
#define CONTENT_BROWSER_WORKER_HOST_MESSAGE_PORT_SERVICE_H_

#include <map>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/singleton.h"
#include "base/strings/string16.h"
#include "ipc/ipc_message.h"

namespace content {
class WorkerMessageFilter;

class MessagePortService {
 public:
  typedef std::vector<std::pair<string16, std::vector<int> > > QueuedMessages;

  // Returns the MessagePortService singleton.
  static MessagePortService* GetInstance();

  // These methods correspond to the message port related IPCs.
  void Create(int route_id, WorkerMessageFilter* filter, int* message_port_id);
  void Destroy(int message_port_id);
  void Entangle(int local_message_port_id, int remote_message_port_id);
  void PostMessage(int sender_message_port_id,
                   const string16& message,
                   const std::vector<int>& sent_message_port_ids);
  void QueueMessages(int message_port_id);
  void SendQueuedMessages(int message_port_id,
                          const QueuedMessages& queued_messages);

  // Updates the information needed to reach a message port when it's sent to a
  // (possibly different) process.
  void UpdateMessagePort(
      int message_port_id,
      WorkerMessageFilter* filter,
      int routing_id);

  void OnWorkerMessageFilterClosing(WorkerMessageFilter* filter);

  // Attempts to send the queued messages for a message port.
  void SendQueuedMessagesIfPossible(int message_port_id);

 private:
  friend struct DefaultSingletonTraits<MessagePortService>;

  MessagePortService();
  ~MessagePortService();

  void PostMessageTo(int message_port_id,
                     const string16& message,
                     const std::vector<int>& sent_message_port_ids);

  // Handles the details of removing a message port id. Before calling this,
  // verify that the message port id exists.
  void Erase(int message_port_id);

  struct MessagePort {
    // |filter| and |route_id| are what we need to send messages to the port.
    // |filter| is just a weak pointer since we get notified when its process has
    // gone away and remove it.
    WorkerMessageFilter* filter;
    int route_id;
    // A globally unique id for this message port.
    int message_port_id;
    // The globally unique id of the entangled message port.
    int entangled_message_port_id;
    // If true, all messages to this message port are queued and not delivered.
    // This is needed so that when a message port is sent between processes all
    // pending message get transferred. There are two possibilities for pending
    // messages: either they are already received by the child process, or they're
    // in-flight. This flag ensures that the latter type get flushed through the
    // system.
    // This flag should only be set to true in response to
    // WorkerProcessHostMsg_QueueMessages.
    bool queue_messages;
    QueuedMessages queued_messages;
  };

  typedef std::map<int, MessagePort> MessagePorts;
  MessagePorts message_ports_;

  // We need globally unique identifiers for each message port.
  int next_message_port_id_;

  DISALLOW_COPY_AND_ASSIGN(MessagePortService);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_MESSAGE_PORT_SERVICE_H_
