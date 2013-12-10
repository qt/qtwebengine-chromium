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

package com.google.ipc.invalidation.ticl.android2;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.external.client.InvalidationClient;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.types.AckHandle;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.ipc.invalidation.ticl.ProtoConverter;
import com.google.ipc.invalidation.ticl.android2.ProtocolIntents.ClientDowncalls;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protos.ipc.invalidation.Client.AckHandleP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;

import android.content.Context;
import android.content.Intent;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * Implementation of {@link InvalidationClient} that uses intents to send commands to an Android
 * service hosting the actual Ticl. This class is a proxy for the Android service.
 *
 */
class AndroidInvalidationClientStub implements InvalidationClient {
  /** Android system context. */
  private final Context context;

  /** Class implementing the Ticl service. */
  private final String serviceClass;

  /**  logger. */
  private final Logger logger;

  /** Creates an instance from {@code context} and {@code logger}. */
  AndroidInvalidationClientStub(Context context, Logger logger) {
    this.context = Preconditions.checkNotNull(context.getApplicationContext());
    this.logger = Preconditions.checkNotNull(logger);
    this.serviceClass = new AndroidTiclManifest(context).getTiclServiceClass();
  }

  @Override
  public void start() {
    throw new UnsupportedOperationException(
        "Android clients are automatically started when created");
  }

  // All calls work by marshalling the arguments to an Intent and sending the Intent to the Ticl
  // service.

  @Override
  public void stop() {
    issueIntent(ClientDowncalls.newStopIntent());
  }

  @Override
  public void register(ObjectId objectId) {
    List<ObjectIdP> objects = new ArrayList<ObjectIdP>(1);
    objects.add(ProtoConverter.convertToObjectIdProto(objectId));
    issueIntent(ClientDowncalls.newRegistrationIntent(objects));
  }

  @Override
  public void register(Collection<ObjectId> objectIds) {
    List<ObjectIdP> objectIdPs = ProtoConverter.convertToObjectIdProtoList(objectIds);
    issueIntent(ClientDowncalls.newRegistrationIntent(objectIdPs));
  }

  @Override
  public void unregister(ObjectId objectId) {
    List<ObjectIdP> objects = new ArrayList<ObjectIdP>(1);
    objects.add(ProtoConverter.convertToObjectIdProto(objectId));
    issueIntent(ClientDowncalls.newUnregistrationIntent(objects));
  }

  @Override
  public void unregister(Collection<ObjectId> objectIds) {
    List<ObjectIdP> objectIdPs = ProtoConverter.convertToObjectIdProtoList(objectIds);
    issueIntent(ClientDowncalls.newUnregistrationIntent(objectIdPs));
  }

  @Override
  public void acknowledge(AckHandle ackHandle) {
    try {
      AckHandleP ackHandleP = AckHandleP.parseFrom(ackHandle.getHandleData());
      issueIntent(ClientDowncalls.newAcknowledgeIntent(ackHandleP));
    } catch (InvalidProtocolBufferException exception) {
      logger.warning("Dropping acknowledge(); could not parse ack handle data %s",
          Arrays.toString(ackHandle.getHandleData()));
    }
  }

  /** Sends {@code intent} to the service implemented by {@link #serviceClass}. */
  private void issueIntent(Intent intent) {
    intent.setClassName(context, serviceClass);
    context.startService(intent);
  }
}
