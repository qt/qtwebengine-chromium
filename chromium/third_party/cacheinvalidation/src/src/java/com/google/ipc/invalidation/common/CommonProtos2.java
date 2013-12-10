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

package com.google.ipc.invalidation.common;

import com.google.common.base.Preconditions;
import com.google.protobuf.ByteString;
import com.google.protos.ipc.invalidation.AndroidChannel;
import com.google.protos.ipc.invalidation.Channel.NetworkEndpointId;
import com.google.protos.ipc.invalidation.Channel.NetworkEndpointId.NetworkAddress;
import com.google.protos.ipc.invalidation.Client.AckHandleP;
import com.google.protos.ipc.invalidation.Client.PersistentStateBlob;
import com.google.protos.ipc.invalidation.Client.PersistentTiclState;
import com.google.protos.ipc.invalidation.ClientProtocol.ApplicationClientIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientVersion;
import com.google.protos.ipc.invalidation.ClientProtocol.ConfigChangeMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.ErrorMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InfoRequestMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InitializeMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InitializeMessage.DigestSerializationType;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.PropertyRecord;
import com.google.protos.ipc.invalidation.ClientProtocol.RateLimitP;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationP;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationStatus;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationStatusMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSubtree;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSummary;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSyncMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSyncRequestMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.ServerHeader;
import com.google.protos.ipc.invalidation.ClientProtocol.ServerToClientMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.StatusP;
import com.google.protos.ipc.invalidation.ClientProtocol.TokenControlMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.Version;

import java.util.Collection;
import java.util.List;


/**
 * Utilities for creating protocol buffers.
 *
 */
public class CommonProtos2 {


  /** Returns true iff status corresponds to success. */
  public static boolean isSuccess(StatusP status) {
    return status.getCode() == StatusP.Code.SUCCESS;
  }

  /** Returns true iff status corresponds to transient failure. */
  public static boolean isTransientFailure(StatusP status) {
    return status.getCode() == StatusP.Code.TRANSIENT_FAILURE;
  }

  /** Returns true iff status corresponds to permanent failure. */
  public static boolean isPermanentFailure(StatusP status) {
    return status.getCode() == StatusP.Code.PERMANENT_FAILURE;
  }

  // The following methods help in creating a protobuf object given its components. A Nullable
  // parameter implies that the particular field will not be set for that proto.
  // For the specs, please see the corresponding Proto file (to avoid inconsistency due to
  // duplication).

  public static Version newVersion(int majorVersion, int minorVersion) {
    return Version.newBuilder().setMajorVersion(majorVersion).setMinorVersion(minorVersion).build();
  }

  public static ClientVersion newClientVersion(String platform, String language,
      String applicationInfo) {
    return ClientVersion.newBuilder()
        .setVersion(CommonInvalidationConstants2.CLIENT_VERSION_VALUE)
        .setPlatform(platform)
        .setLanguage(language)
        .setApplicationInfo(applicationInfo)
        .build();
  }

  public static ObjectIdP newObjectIdP(int source, ByteString name) {
    return ObjectIdP.newBuilder().setSource(source).setName(name).build();
  }

  public static ObjectIdP newObjectIdP(int source, byte[] name) {
    return newObjectIdP(source, ByteString.copyFrom(name));
  }

  public static boolean isAllObjectId(ObjectIdP objectId) {
    return (objectId != null) &&
        (CommonInvalidationConstants2.ALL_OBJECT_ID.getSource() == objectId.getSource()) &&
        (CommonInvalidationConstants2.ALL_OBJECT_ID.getName().equals(objectId.getName()));

  }

  public static RateLimitP newRateLimitP(int windowMs, int count) {
    return RateLimitP.newBuilder()
        .setWindowMs(windowMs)
        .setCount(count)
        .build();
  }

  public static ApplicationClientIdP newApplicationClientIdP(int clientType,
      ByteString clientName) {
    return ApplicationClientIdP.newBuilder()
        .setClientType(clientType)
        .setClientName(clientName)
        .build();
  }

  public static InvalidationP newInvalidationP(ObjectIdP oid, long version,
      TrickleState trickleState) {
    return newInvalidationP(oid, version, trickleState, null);
  }

  public static InvalidationP newInvalidationP(ObjectIdP oid, long version,
      TrickleState trickleState, ByteString payload) {
    InvalidationP.Builder builder = InvalidationP.newBuilder()
        .setObjectId(oid)
        .setIsKnownVersion(true)
        .setVersion(version)
        .setIsTrickleRestart(trickleState == TrickleState.RESTART);
    if (payload != null) {
      builder.setPayload(payload);
    }
    return builder.build();
  }

  public static InvalidationP newInvalidationPForUnknownVersion(ObjectIdP oid,
      long sequenceNumber) {
    return InvalidationP.newBuilder()
        .setObjectId(oid)
        .setIsKnownVersion(false)
        .setIsTrickleRestart(true)
        .setVersion(sequenceNumber)
        .build();
  }

  /**
   * Returns an invalidation that is identical to {@code invalidation} but with the
   * {@code is_trickle_restart} flag set to true. If the input {@invalidation} is already restarted,
   * it is returned directly. Otherwise, a new invalidation is created.
   */
  public static InvalidationP toRestartedInvalidation(InvalidationP invalidation) {
    if (invalidation.hasIsTrickleRestart() && invalidation.getIsTrickleRestart()) {
      return invalidation;
    }
    return invalidation.toBuilder().setIsTrickleRestart(true).build();
  }

  /**
   * Returns an invalidation that is identical to {@code invalidation} but with the
   * {@code is_trickle_restart} flag set to false. If the input {@invalidation} is already
   * a continuous invalidation, it is returned directly. Otherwise, a new invalidation is created.
   */
  public static InvalidationP toContinuousInvalidation(InvalidationP invalidation) {
    if (invalidation.hasIsTrickleRestart() && !invalidation.getIsTrickleRestart()) {
      return invalidation;
    }
    return invalidation.toBuilder().setIsTrickleRestart(false).build();
  }

  public static RegistrationP newRegistrationP(ObjectIdP oid, boolean isReg) {
    RegistrationP registration = RegistrationP.newBuilder()
        .setObjectId(oid)
        .setOpType(isReg ? RegistrationP.OpType.REGISTER : RegistrationP.OpType.UNREGISTER)
        .build();
    return registration;
  }

  public static RegistrationP newRegistrationPForRegistration(ObjectIdP oid) {
    return newRegistrationP(oid, true);
  }

  public static RegistrationP newRegistrationPForUnregistration(ObjectIdP oid) {
    return newRegistrationP(oid, false);
  }

  public static StatusP newSuccessStatus() {
    return StatusP.newBuilder().setCode(StatusP.Code.SUCCESS).build();
  }

  public static StatusP newFailureStatus(boolean isTransient, String description) {
    return StatusP.newBuilder()
        .setCode(isTransient ? StatusP.Code.TRANSIENT_FAILURE : StatusP.Code.PERMANENT_FAILURE)
        .setDescription(description)
        .build();
  }

  public static RegistrationSummary newRegistrationSummary(int numRegistrations, byte[] regDigest) {
    RegistrationSummary regSummary = RegistrationSummary.newBuilder()
      .setNumRegistrations(numRegistrations)
      .setRegistrationDigest(ByteString.copyFrom(regDigest))
      .build();
    return regSummary;
  }

  public static RegistrationStatus newRegistrationStatus(RegistrationP registration,
      StatusP status) {
    return RegistrationStatus.newBuilder()
            .setRegistration(registration)
            .setStatus(status)
            .build();
  }

  public static RegistrationStatus newSuccessRegistrationStatus(RegistrationP registration) {
    return RegistrationStatus.newBuilder()
            .setRegistration(registration)
            .setStatus(newSuccessStatus())
            .build();
  }

  public static RegistrationStatus newTransientFailureRegistrationStatus(
      RegistrationP registration, String description) {
    return RegistrationStatus.newBuilder()
            .setRegistration(registration)
            .setStatus(newFailureStatus(true, description))
            .build();
  }

  public static PersistentTiclState newPersistentTiclState(ByteString clientToken,
      long lastMessageSendTimeMs) {
    return PersistentTiclState.newBuilder()
        .setClientToken(clientToken)
        .setLastMessageSendTimeMs(lastMessageSendTimeMs)
        .build();
  }

  public static AckHandleP newAckHandleP(InvalidationP invalidation) {
    return AckHandleP.newBuilder().setInvalidation(invalidation).build();
  }

  public static PersistentStateBlob newPersistentStateBlob(PersistentTiclState state,
      ByteString mac) {
    return PersistentStateBlob.newBuilder()
        .setTiclState(state)
        .setAuthenticationCode(mac)
        .build();
  }

  // Methods to create ClientToServerMessages.

  public static InitializeMessage newInitializeMessage(int clientType,
      ApplicationClientIdP applicationClientId, ByteString nonce,
      DigestSerializationType digestSerializationType) {
    return InitializeMessage.newBuilder()
        .setClientType(clientType)
        .setApplicationClientId(applicationClientId)
        .setDigestSerializationType(digestSerializationType)
        .setNonce(nonce)
        .build();
  }

  public static PropertyRecord newPropertyRecord(String name, int value) {
    return PropertyRecord.newBuilder().setName(name).setValue(value).build();
  }

  // Methods to create ServerToClientMessages.

  public static ServerHeader newServerHeader(ByteString clientToken, long currentTimeMs,
      RegistrationSummary registrationSummary, String messageId) {
    ServerHeader.Builder builder = ServerHeader.newBuilder()
        .setProtocolVersion(CommonInvalidationConstants2.PROTOCOL_VERSION)
        .setClientToken(clientToken)
        .setServerTimeMs(currentTimeMs);
    if (registrationSummary != null) {
      builder.setRegistrationSummary(registrationSummary);
    }
    if (messageId != null) {
      builder.setMessageId(messageId);
    }
    return builder.build();
  }

  public static ErrorMessage newErrorMessage(ErrorMessage.Code code, String description) {
    return ErrorMessage.newBuilder()
        .setCode(code)
        .setDescription(description)
        .build();
  }

  public static InvalidationMessage newInvalidationMessage(Iterable<InvalidationP> invalidations) {
    return InvalidationMessage.newBuilder().addAllInvalidation(invalidations).build();
  }

  public static RegistrationSyncRequestMessage newRegistrationSyncRequestMessage() {
    return RegistrationSyncRequestMessage.getDefaultInstance();
  }

  public static RegistrationSyncMessage newRegistrationSyncMessage(
      List<RegistrationSubtree> subtrees) {
    return RegistrationSyncMessage.newBuilder().addAllSubtree(subtrees).build();
  }

  public static RegistrationSubtree newRegistrationSubtree(List<ObjectIdP> registeredOids) {
    return RegistrationSubtree.newBuilder().addAllRegisteredObject(registeredOids).build();
  }

  public static InfoRequestMessage newPerformanceCounterRequestMessage() {
    return InfoRequestMessage.newBuilder()
        .addInfoType(InfoRequestMessage.InfoType.GET_PERFORMANCE_COUNTERS)
        .build();
  }

  public static ConfigChangeMessage newConfigChangeMessage(long nextMessageDelayMs) {
    return ConfigChangeMessage.newBuilder().setNextMessageDelayMs(nextMessageDelayMs).build();
  }

  public static ServerToClientMessage newServerToClientMessage(ServerHeader scHeader) {
    return ServerToClientMessage.newBuilder()
        .setHeader(scHeader)
        .build();
  }

  public static ServerToClientMessage.Builder newServerToClientMessage(ServerHeader scHeader,
      TokenControlMessage tokenControlMessage) {
    return ServerToClientMessage.newBuilder()
        .setHeader(scHeader)
        .setTokenControlMessage(tokenControlMessage);
  }

  public static ServerToClientMessage newServerToClientMessage(ServerHeader scHeader,
      RegistrationSyncRequestMessage syncRequestionMessage) {
    return ServerToClientMessage.newBuilder()
        .setHeader(scHeader)
        .setRegistrationSyncRequestMessage(syncRequestionMessage)
        .build();
  }

  public static ServerToClientMessage newServerToClientMessage(ServerHeader scHeader,
      InvalidationMessage invalidationMessage) {
    return ServerToClientMessage.newBuilder()
        .setHeader(scHeader)
        .setInvalidationMessage(invalidationMessage)
        .build();
  }

  public static ServerToClientMessage newServerToClientMessage(ServerHeader scHeader,
      RegistrationStatusMessage registrationStatusMessage) {
    return ServerToClientMessage.newBuilder()
        .setHeader(scHeader)
        .setRegistrationStatusMessage(registrationStatusMessage)
        .build();
  }

  public static ServerToClientMessage newServerToClientMessage(ServerHeader scHeader,
      InfoRequestMessage infoRequestMessage) {
    return ServerToClientMessage.newBuilder()
        .setHeader(scHeader)
        .setInfoRequestMessage(infoRequestMessage)
        .build();
  }

  public static ServerToClientMessage newServerToClientMessage(ServerHeader scHeader,
      ConfigChangeMessage configChangeMessage) {
    return ServerToClientMessage.newBuilder()
        .setHeader(scHeader)
        .setConfigChangeMessage(configChangeMessage)
        .build();
  }

  public static ServerToClientMessage newServerToClientMessage(ServerHeader scHeader,
      ErrorMessage errorMessage) {
    return ServerToClientMessage.newBuilder()
        .setHeader(scHeader)
        .setErrorMessage(errorMessage)
        .build();
  }

  /**
   * Constructs a network endpoint id for an Android client with the given {@code registrationId},
   * {@code clientKey}, and {@code packageName}.
   */
  public static NetworkEndpointId newAndroidEndpointId(String registrationId, String clientKey,
      String packageName, Version channelVersion) {
    Preconditions.checkNotNull(registrationId, "Null registration id");
    Preconditions.checkNotNull(clientKey, "Null client key");
    Preconditions.checkNotNull(packageName, "Null package name");
    Preconditions.checkNotNull(channelVersion, "Null channel version");

    AndroidChannel.EndpointId.Builder endpointBuilder = AndroidChannel.EndpointId.newBuilder()
        .setC2DmRegistrationId(registrationId)
        .setClientKey(clientKey)
        .setPackageName(packageName)
        .setChannelVersion(channelVersion);
    return newNetworkEndpointId(NetworkAddress.ANDROID, endpointBuilder.build().toByteString());
  }

  public static NetworkEndpointId newNetworkEndpointId(NetworkAddress networkAddr,
      ByteString clientAddr) {
    return NetworkEndpointId.newBuilder()
        .setNetworkAddress(networkAddr)
        .setClientAddress(clientAddr)
        .build();
  }

  public static TokenControlMessage newTokenControlMessage(ByteString newToken) {
    TokenControlMessage.Builder builder = TokenControlMessage.newBuilder();
    if (newToken != null) {
      builder.setNewToken(newToken);
    }
    return builder.build();
  }

  public static RegistrationStatusMessage newRegistrationStatusMessage(
      Collection<RegistrationStatus> statuses) {
    Preconditions.checkArgument(!statuses.isEmpty(), "Empty statuses");
    return RegistrationStatusMessage.newBuilder().addAllRegistrationStatus(statuses).build();
  }

  public static RegistrationMessage newRegistrationMessage(
      Collection<RegistrationP> registrations) {
    Preconditions.checkArgument(!registrations.isEmpty(), "Empty registrations");
    return RegistrationMessage.newBuilder().addAllRegistration(registrations).build();
  }

  private CommonProtos2() { // To prevent instantiation
  }
}
