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
package com.google.ipc.invalidation.external.client.contrib;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.ipc.invalidation.ticl.ProtoConverter;
import com.google.ipc.invalidation.ticl.TiclExponentialBackoffDelayGenerator;
import com.google.protobuf.ByteString;
import com.google.protos.ipc.invalidation.AndroidListenerProtocol.AndroidListenerState;
import com.google.protos.ipc.invalidation.AndroidListenerProtocol.AndroidListenerState.RetryRegistrationState;
import com.google.protos.ipc.invalidation.AndroidListenerProtocol.RegistrationCommand;
import com.google.protos.ipc.invalidation.AndroidListenerProtocol.StartCommand;

import java.util.Map;
import java.util.Map.Entry;

/**
 * Static helper class supporting construction of valid {code AndroidListenerProtocol} messages.
 *
 */
class AndroidListenerProtos {

  /** Creates a register command for the given objects and client. */
  static RegistrationCommand newRegisterCommand(ByteString clientId, Iterable<ObjectId> objectIds) {
    final boolean isRegister = true;
    return newRegistrationCommand(clientId, objectIds, isRegister);
  }

  /** Creates an unregister command for the given objects and client. */
  static RegistrationCommand newUnregisterCommand(ByteString clientId,
      Iterable<ObjectId> objectIds) {
    final boolean isRegister = false;
    return newRegistrationCommand(clientId, objectIds, isRegister);
  }

  /** Creates a retry register command for the given object and client. */
  static RegistrationCommand newDelayedRegisterCommand(ByteString clientId, ObjectId objectId) {
    final boolean isRegister = true;
    return newDelayedRegistrationCommand(clientId, objectId, isRegister);
  }

  /** Creates a retry unregister command for the given object and client. */
  static RegistrationCommand newDelayedUnregisterCommand(ByteString clientId, ObjectId objectId) {
    final boolean isRegister = false;
    return newDelayedRegistrationCommand(clientId, objectId, isRegister);
  }

  /** Creates proto for {@link AndroidListener} state. */
  static AndroidListenerState newAndroidListenerState(ByteString clientId, int requestCodeSeqNum,
      Map<ObjectId, TiclExponentialBackoffDelayGenerator> delayGenerators,
      Iterable<ObjectId> desiredRegistrations) {
    AndroidListenerState.Builder builder = AndroidListenerState.newBuilder()
        .setClientId(clientId)
        .setRequestCodeSeqNum(requestCodeSeqNum);
    for (ObjectId objectId : desiredRegistrations) {
      builder.addRegistration(ProtoConverter.convertToObjectIdProto(objectId));
    }
    for (Entry<ObjectId, TiclExponentialBackoffDelayGenerator> entry : delayGenerators.entrySet()) {
      builder.addRetryRegistrationState(
          newRetryRegistrationState(entry.getKey(), entry.getValue()));
    }
    return builder.build();
  }

  /** Creates proto for retry registration state. */
  static RetryRegistrationState newRetryRegistrationState(ObjectId objectId,
      TiclExponentialBackoffDelayGenerator delayGenerator) {
    return RetryRegistrationState.newBuilder()
        .setObjectId(ProtoConverter.convertToObjectIdProto(objectId))
        .setExponentialBackoffState(delayGenerator.marshal())
        .build();
  }

  /** Returns {@code true} iff the given proto is valid. */
  static boolean isValidAndroidListenerState(AndroidListenerState state) {
    return state.hasClientId() && state.hasRequestCodeSeqNum();
  }

  /** Returns {@code true} iff the given proto is valid. */
  static boolean isValidRegistrationCommand(RegistrationCommand command) {
    return command.hasIsRegister() && command.hasClientId() && command.hasIsDelayed();
  }

  /** Returns {@code true} iff the given proto is valid. */
  static boolean isValidStartCommand(StartCommand command) {
    return command.hasClientType() && command.hasClientName();
  }

  /** Creates start command proto. */
  static StartCommand newStartCommand(int clientType, ByteString clientName) {
    return StartCommand.newBuilder()
        .setClientType(clientType)
        .setClientName(clientName)
        .build();
  }

  private static RegistrationCommand newRegistrationCommand(ByteString clientId,
      Iterable<ObjectId> objectIds, boolean isRegister) {
    RegistrationCommand.Builder builder = RegistrationCommand.newBuilder()
        .setIsRegister(isRegister)
        .setClientId(clientId)
        .setIsDelayed(false);
    for (ObjectId objectId : objectIds) {
      Preconditions.checkNotNull(objectId);
      builder.addObjectId(ProtoConverter.convertToObjectIdProto(objectId));
    }
    return builder.build();
  }

  private static RegistrationCommand newDelayedRegistrationCommand(ByteString clientId,
      ObjectId objectId, boolean isRegister) {
    return RegistrationCommand.newBuilder()
        .setIsRegister(isRegister)
        .addObjectId(ProtoConverter.convertToObjectIdProto(objectId))
        .setClientId(clientId)
        .setIsDelayed(true)
        .build();
  }

  // Prevent instantiation.
  private AndroidListenerProtos() {
  }
}
