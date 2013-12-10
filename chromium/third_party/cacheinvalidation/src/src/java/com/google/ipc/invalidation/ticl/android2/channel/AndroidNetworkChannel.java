/*
 * Copyright 2011 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.ipc.invalidation.ticl.android2.channel;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.external.client.SystemResources;
import com.google.ipc.invalidation.ticl.TestableNetworkChannel;
import com.google.ipc.invalidation.ticl.android2.ProtocolIntents;
import com.google.ipc.invalidation.ticl.android2.ResourcesFactory.AndroidResources;
import com.google.protos.ipc.invalidation.Channel.NetworkEndpointId;

import android.content.Context;
import android.content.Intent;

/**
 * A network channel for Android that receives messages by GCM and that sends messages
 * using HTTP.
 *
 */
public class AndroidNetworkChannel implements TestableNetworkChannel {
  private final Context context;
  private AndroidResources resources;

  public AndroidNetworkChannel(Context context) {
    this.context = Preconditions.checkNotNull(context);
  }

  @Override
  public void sendMessage(byte[] outgoingMessage) {
    Intent intent = ProtocolIntents.newOutboundMessageIntent(outgoingMessage);
    intent.setClassName(context, AndroidMessageSenderService.class.getName());
    context.startService(intent);
  }

  @Override
  public void setListener(NetworkListener listener) {
    resources.setNetworkListener(listener);
  }

  @Override
  public void setSystemResources(SystemResources resources) {
    this.resources = (AndroidResources) Preconditions.checkNotNull(resources);
  }

  @Override
  public NetworkEndpointId getNetworkIdForTest() {
    return AndroidMessageSenderService.getNetworkEndpointId(context, resources.getLogger());
  }
}
