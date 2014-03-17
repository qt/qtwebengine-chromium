// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_message_filter.h"

#include "base/bind.h"
#include "base/logging.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/resource_reply_thread_registrar.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/resource_tracker.h"

namespace ppapi {
namespace proxy {

PluginMessageFilter::PluginMessageFilter(
    std::set<PP_Instance>* seen_instance_ids,
    scoped_refptr<ResourceReplyThreadRegistrar> registrar)
    : seen_instance_ids_(seen_instance_ids),
      resource_reply_thread_registrar_(registrar),
      channel_(NULL) {
}

PluginMessageFilter::~PluginMessageFilter() {
}

void PluginMessageFilter::OnFilterAdded(IPC::Channel* channel) {
  channel_ = channel;
}

void PluginMessageFilter::OnFilterRemoved() {
  channel_ = NULL;
}

bool PluginMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PluginMessageFilter, message)
    IPC_MESSAGE_HANDLER(PpapiMsg_ReserveInstanceId, OnMsgReserveInstanceId)
    IPC_MESSAGE_HANDLER(PpapiPluginMsg_ResourceReply, OnMsgResourceReply)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool PluginMessageFilter::Send(IPC::Message* msg) {
  if (channel_)
    return channel_->Send(msg);
  delete msg;
  return false;
}

// static
void PluginMessageFilter::DispatchResourceReplyForTest(
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  DispatchResourceReply(reply_params, nested_msg);
}

void PluginMessageFilter::OnMsgReserveInstanceId(PP_Instance instance,
                                                 bool* usable) {
  // If |seen_instance_ids_| is set to NULL, we are not supposed to see this
  // message.
  CHECK(seen_instance_ids_);
  // See the message definition for how this works.
  if (seen_instance_ids_->find(instance) != seen_instance_ids_->end()) {
    // Instance ID already seen, reject it.
    *usable = false;
    return;
  }

  // This instance ID is new so we can return that it's usable and mark it as
  // used for future reference.
  seen_instance_ids_->insert(instance);
  *usable = true;
}

void PluginMessageFilter::OnMsgResourceReply(
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  scoped_refptr<base::MessageLoopProxy> target =
      resource_reply_thread_registrar_->GetTargetThreadAndUnregister(
          reply_params.pp_resource(), reply_params.sequence());

  target->PostTask(
      FROM_HERE,
      base::Bind(&DispatchResourceReply, reply_params, nested_msg));
}

// static
void PluginMessageFilter::DispatchResourceReply(
    const ResourceMessageReplyParams& reply_params,
    const IPC::Message& nested_msg) {
  ProxyAutoLock lock;
  Resource* resource = PpapiGlobals::Get()->GetResourceTracker()->GetResource(
      reply_params.pp_resource());
  if (!resource) {
    DVLOG_IF(1, reply_params.sequence() != 0)
        << "Pepper resource reply message received but the resource doesn't "
           "exist (probably has been destroyed).";
    return;
  }
  resource->OnReplyReceived(reply_params, nested_msg);
}

}  // namespace proxy
}  // namespace ppapi
