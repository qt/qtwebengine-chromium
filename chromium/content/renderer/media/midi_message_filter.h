// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_
#define CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_

#include <map>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"
#include "ipc/ipc_channel_proxy.h"
#include "media/midi/midi_port_info.h"
#include "third_party/WebKit/public/platform/WebMIDIAccessorClient.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

// MessageFilter that handles MIDI messages.
class CONTENT_EXPORT MIDIMessageFilter
    : public IPC::ChannelProxy::MessageFilter {
 public:
  explicit MIDIMessageFilter(
      const scoped_refptr<base::MessageLoopProxy>& io_message_loop);

  // Each client registers for MIDI access here.
  // If permission is granted, then the client's
  // addInputPort() and addOutputPort() methods will be called,
  // giving the client access to receive and send data.
  void StartSession(WebKit::WebMIDIAccessorClient* client);
  void RemoveClient(WebKit::WebMIDIAccessorClient* client);

  // A client will only be able to call this method if it has a suitable
  // output port (from addOutputPort()).
  void SendMIDIData(uint32 port,
                    const uint8* data,
                    size_t length,
                    double timestamp);

  // IO message loop associated with this message filter.
  scoped_refptr<base::MessageLoopProxy> io_message_loop() const {
    return io_message_loop_;
  }

 protected:
  virtual ~MIDIMessageFilter();

 private:
  // Sends an IPC message using |channel_|.
  void Send(IPC::Message* message);

  // IPC::ChannelProxy::MessageFilter override. Called on |io_message_loop|.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;
  virtual void OnFilterAdded(IPC::Channel* channel) OVERRIDE;
  virtual void OnFilterRemoved() OVERRIDE;
  virtual void OnChannelClosing() OVERRIDE;

  // Called when the browser process has approved (or denied) access to
  // MIDI hardware.
  void OnSessionStarted(int client_id,
                        bool success,
                        media::MIDIPortInfoList inputs,
                        media::MIDIPortInfoList outputs);

  // Called when the browser process has sent MIDI data containing one or
  // more messages.
  void OnDataReceived(uint32 port,
                      const std::vector<uint8>& data,
                      double timestamp);

  // From time-to-time, the browser incrementally informs us of how many bytes
  // it has successfully sent. This is part of our throttling process to avoid
  // sending too much data before knowing how much has already been sent.
  void OnAcknowledgeSentData(size_t bytes_sent);

  void HandleSessionStarted(int client_id,
                            bool success,
                            media::MIDIPortInfoList inputs,
                            media::MIDIPortInfoList outputs);

  void HandleDataReceived(uint32 port,
                          const std::vector<uint8>& data,
                          double timestamp);

  void StartSessionOnIOThread(int client_id);

  void SendMIDIDataOnIOThread(uint32 port,
                              const std::vector<uint8>& data,
                              double timestamp);

  WebKit::WebMIDIAccessorClient* GetClientFromId(int client_id);

  // IPC channel for Send(); must only be accessed on |io_message_loop_|.
  IPC::Channel* channel_;

  // Message loop on which IPC calls are driven.
  const scoped_refptr<base::MessageLoopProxy> io_message_loop_;

  // Main thread's message loop.
  scoped_refptr<base::MessageLoopProxy> main_message_loop_;

  // Keeps track of all MIDI clients.
  // We map client to "client id" used to track permission.
  // When access has been approved, we add the input and output ports to
  // the client, allowing it to actually receive and send MIDI data.
  typedef std::map<WebKit::WebMIDIAccessorClient*, int> ClientsMap;
  ClientsMap clients_;

  // Dishes out client ids.
  int next_available_id_;

  size_t unacknowledged_bytes_sent_;

  DISALLOW_COPY_AND_ASSIGN(MIDIMessageFilter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_MIDI_MESSAGE_FILTER_H_
