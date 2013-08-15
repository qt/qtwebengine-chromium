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

import com.google.ipc.invalidation.common.CommonProtos2;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.protobuf.ByteString;
import com.google.protos.ipc.invalidation.AndroidService.AndroidNetworkSendRequest;
import com.google.protos.ipc.invalidation.AndroidService.AndroidSchedulerEvent;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.AckDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.RegistrationDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.StartDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.StopDowncall;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.CreateClient;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.NetworkStatus;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.ServerMessage;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.ErrorUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.InvalidateUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.ReadyUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.RegistrationFailureUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.RegistrationStatusUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.ReissueRegistrationsUpcall;
import com.google.protos.ipc.invalidation.Client.AckHandleP;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientConfigP;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.Version;

import android.content.Intent;

/**
 * Factory class for {@link Intent}s used between the application, Ticl, and listener in the
 * Android Ticl.
 *
 */
public class ProtocolIntents {
  /** Version of the on-device protocol. */
  static final Version ANDROID_PROTOCOL_VERSION_VALUE = CommonProtos2.newVersion(1, 0);

  /** Key of Intent byte[] extra holding a client downcall protocol buffer. */
  public static final String CLIENT_DOWNCALL_KEY = "ipcinv-downcall";

  /** Key of Intent byte[] extra holding an internal downcall protocol buffer. */
  public static final String INTERNAL_DOWNCALL_KEY = "ipcinv-internal-downcall";

  /** Key of Intent byte[] extra holding a listener upcall protocol buffer. */
  public static final String LISTENER_UPCALL_KEY = "ipcinv-upcall";

  /** Key of Intent byte[] extra holding a schedule event protocol buffer. */
  public static final String SCHEDULER_KEY = "ipcinv-scheduler";

  /** Key of Intent byte[] extra holding an outbound message protocol buffer. */
  public static final String OUTBOUND_MESSAGE_KEY = "ipcinv-outbound-message";

  /** Intents corresponding to calls on {@code InvalidationClient}. */
  public static class ClientDowncalls {
    public static Intent newStartIntent() {
      Intent intent = new Intent();
      intent.putExtra(CLIENT_DOWNCALL_KEY, newBuilder()
          .setStart(StartDowncall.getDefaultInstance())
          .build().toByteArray());
      return intent;
    }

    public static Intent newStopIntent() {
      Intent intent = new Intent();
      intent.putExtra(CLIENT_DOWNCALL_KEY, newBuilder()
          .setStop(StopDowncall.getDefaultInstance())
          .build().toByteArray());
      return intent;
    }

    public static Intent newAcknowledgeIntent(AckHandleP ackHandle) {
      AckDowncall ackDowncall = AckDowncall.newBuilder()
          .setAckHandle(ackHandle.toByteString()).build();
      Intent intent = new Intent();
      intent.putExtra(CLIENT_DOWNCALL_KEY,
          newBuilder().setAck(ackDowncall).build().toByteArray());
      return intent;
    }

    public static Intent newRegistrationIntent(Iterable<ObjectIdP> registrations) {
      RegistrationDowncall regDowncall = RegistrationDowncall.newBuilder()
          .addAllRegistrations(registrations).build();
      Intent intent = new Intent();
      intent.putExtra(CLIENT_DOWNCALL_KEY,
          newBuilder().setRegistrations(regDowncall).build().toByteArray());
      return intent;
    }

    public static Intent newUnregistrationIntent(Iterable<ObjectIdP> unregistrations) {
      RegistrationDowncall unregDowncall = RegistrationDowncall.newBuilder()
          .addAllUnregistrations(unregistrations).build();
      Intent intent = new Intent();
      intent.putExtra(CLIENT_DOWNCALL_KEY,
          newBuilder().setRegistrations(unregDowncall).build().toByteArray());
      return intent;
    }

    private static ClientDowncall.Builder newBuilder() {
      return ClientDowncall.newBuilder().setVersion(ANDROID_PROTOCOL_VERSION_VALUE);
    }

    private ClientDowncalls() {
      // Disallow instantiation.
    }
  }

  /** Intents for non-public calls on the Ticl (currently, network-related calls. */
  public static class InternalDowncalls {
    public static Intent newServerMessageIntent(ByteString serverMessage) {
      Intent intent = new Intent();
      intent.putExtra(INTERNAL_DOWNCALL_KEY,
          newBuilder()
            .setServerMessage(ServerMessage.newBuilder().setData(serverMessage))
            .build().toByteArray());
      return intent;
    }

    public static Intent newNetworkStatusIntent(Boolean status) {
      Intent intent = new Intent();
      intent.putExtra(INTERNAL_DOWNCALL_KEY,
          newBuilder()
          .setNetworkStatus(NetworkStatus.newBuilder().setIsOnline(status))
          .build().toByteArray());
      return intent;
    }

    public static Intent newNetworkAddrChangeIntent() {
      Intent intent = new Intent();
      intent.putExtra(INTERNAL_DOWNCALL_KEY,
          newBuilder().setNetworkAddrChange(true).build().toByteArray());
      return intent;
    }

    public static Intent newCreateClientIntent(int clientType, byte[] clientName,
        ClientConfigP config, boolean skipStartForTest) {
      CreateClient createClient = CreateClient.newBuilder()
          .setClientType(clientType)
          .setClientName(ByteString.copyFrom(clientName))
          .setClientConfig(config)
          .setSkipStartForTest(skipStartForTest)
          .build();
      Intent intent = new Intent();
      intent.putExtra(INTERNAL_DOWNCALL_KEY,
          newBuilder().setCreateClient(createClient).build().toByteArray());
      return intent;
    }

    private static InternalDowncall.Builder newBuilder() {
      return InternalDowncall.newBuilder().setVersion(ANDROID_PROTOCOL_VERSION_VALUE);
    }

    private InternalDowncalls() {
      // Disallow instantiation.
    }
  }

  /** Intents corresponding to calls on {@code InvalidationListener}. */
  public static class ListenerUpcalls {
    public static Intent newReadyIntent() {
      Intent intent = new Intent();
      intent.putExtra(LISTENER_UPCALL_KEY,
          newBuilder().setReady(ReadyUpcall.getDefaultInstance()).build().toByteArray());
      return intent;
    }

    public static Intent newInvalidateIntent(InvalidationP invalidation, AckHandleP ackHandle) {
      Intent intent = new Intent();
      InvalidateUpcall invUpcall = InvalidateUpcall.newBuilder()
          .setAckHandle(ackHandle.toByteString())
          .setInvalidation(invalidation).build();
      intent.putExtra(LISTENER_UPCALL_KEY,
          newBuilder().setInvalidate(invUpcall).build().toByteArray());
      return intent;
    }

    public static Intent newInvalidateUnknownIntent(ObjectIdP object, AckHandleP ackHandle) {
      Intent intent = new Intent();
      InvalidateUpcall invUpcall = InvalidateUpcall.newBuilder()
          .setAckHandle(ackHandle.toByteString())
          .setInvalidateUnknown(object).build();
      intent.putExtra(LISTENER_UPCALL_KEY,
          newBuilder().setInvalidate(invUpcall).build().toByteArray());
      return intent;
    }

    public static Intent newInvalidateAllIntent(AckHandleP ackHandle) {
      Intent intent = new Intent();
      InvalidateUpcall invUpcall = InvalidateUpcall.newBuilder()
          .setAckHandle(ackHandle.toByteString())
          .setInvalidateAll(true).build();
      intent.putExtra(LISTENER_UPCALL_KEY,
          newBuilder().setInvalidate(invUpcall).build().toByteArray());
      return intent;
    }

    public static Intent newRegistrationStatusIntent(ObjectIdP object, boolean isRegistered) {
      Intent intent = new Intent();
      RegistrationStatusUpcall regUpcall = RegistrationStatusUpcall.newBuilder()
            .setObjectId(object)
            .setIsRegistered(isRegistered).build();
      intent.putExtra(LISTENER_UPCALL_KEY,
          newBuilder().setRegistrationStatus(regUpcall).build().toByteArray());
      return intent;
    }

    public static Intent newRegistrationFailureIntent(ObjectIdP object, boolean isTransient,
        String message) {
      Intent intent = new Intent();
      RegistrationFailureUpcall regUpcall = RegistrationFailureUpcall.newBuilder()
            .setObjectId(object)
            .setTransient(isTransient)
            .setMessage(message).build();
      intent.putExtra(LISTENER_UPCALL_KEY,
          newBuilder().setRegistrationFailure(regUpcall).build().toByteArray());
      return intent;
    }

    public static Intent newReissueRegistrationsIntent(byte[] prefix, int length) {
      Intent intent = new Intent();
      ReissueRegistrationsUpcall reissueRegistrations = ReissueRegistrationsUpcall.newBuilder()
          .setPrefix(ByteString.copyFrom(prefix))
          .setLength(length).build();
      intent.putExtra(LISTENER_UPCALL_KEY,
          newBuilder().setReissueRegistrations(reissueRegistrations).build().toByteArray());
      return intent;
    }

    public static Intent newErrorIntent(ErrorInfo errorInfo) {
      Intent intent = new Intent();
      ErrorUpcall errorUpcall = ErrorUpcall.newBuilder()
          .setErrorCode(errorInfo.getErrorReason())
          .setErrorMessage(errorInfo.getErrorMessage())
          .setIsTransient(errorInfo.isTransient())
          .build();
      intent.putExtra(LISTENER_UPCALL_KEY,
          newBuilder().setError(errorUpcall).build().toByteArray());
      return intent;
    }

    private static ListenerUpcall.Builder newBuilder() {
      return ListenerUpcall.newBuilder().setVersion(ANDROID_PROTOCOL_VERSION_VALUE);
    }

    private ListenerUpcalls() {
      // Disallow instantiation.
    }
  }

  /** Returns a new intent encoding a request to execute the scheduled action {@code eventName}. */
  public static Intent newSchedulerIntent(String eventName, long ticlId) {
    byte[] eventBytes =
        AndroidSchedulerEvent.newBuilder()
          .setVersion(ANDROID_PROTOCOL_VERSION_VALUE)
          .setEventName(eventName)
          .setTiclId(ticlId).build().toByteArray();
    return new Intent().putExtra(SCHEDULER_KEY, eventBytes);
  }

  /** Returns a new intent encoding a message to send to the data center. */
  public static Intent newOutboundMessageIntent(byte[] message) {
    byte[] payloadBytes = AndroidNetworkSendRequest.newBuilder()
        .setVersion(ANDROID_PROTOCOL_VERSION_VALUE)
        .setMessage(ByteString.copyFrom(message)).build().toByteArray();
    return new Intent().putExtra(OUTBOUND_MESSAGE_KEY, payloadBytes);
  }

  private ProtocolIntents() {
    // Disallow instantiation.
  }
}
