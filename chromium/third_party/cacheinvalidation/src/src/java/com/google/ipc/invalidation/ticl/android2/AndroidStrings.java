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

import com.google.common.base.Joiner;
import com.google.ipc.invalidation.common.CommonProtoStrings2;
import com.google.ipc.invalidation.util.TextBuilder;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protos.ipc.invalidation.AndroidService.AndroidNetworkSendRequest;
import com.google.protos.ipc.invalidation.AndroidService.AndroidSchedulerEvent;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.AckDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.RegistrationDowncall;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.CreateClient;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.NetworkStatus;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.ServerMessage;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.ErrorUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.InvalidateUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.RegistrationFailureUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.RegistrationStatusUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.ReissueRegistrationsUpcall;
import com.google.protos.ipc.invalidation.Client.AckHandleP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;

import android.content.Intent;

import java.util.List;


/**
 * Utilities to format Android protocol buffers and intents as compact strings suitable for logging.
 * By convention, methods take a {@link TextBuilder} and the object to format and return the
 * builder. Null object arguments are permitted.
 *
 * <p>{@link #toCompactString} methods immediately append a description of the object to the given
 * {@link TextBuilder}s. {@link #toLazyCompactString} methods return an object that defers the work
 * of formatting the provided argument until {@link Object#toString} is called.
 *
 */
public class AndroidStrings {

  /**
   * String to return when the argument is unknown (suggests a new protocol field or invalid
   * proto).
   */
  static final String UNKNOWN_MESSAGE = "UNKNOWN@AndroidStrings";

  /**
   * String to return when there is an error formatting an argument.
   */
  static final String ERROR_MESSAGE = "ERROR@AndroidStrings";

  /**
   * Returns an object that lazily evaluates {@link #toCompactString} when {@link Object#toString}
   * is called.
   */
  public static Object toLazyCompactString(final Intent intent) {
    return new Object() {
      @Override
      public String toString() {
        TextBuilder builder = new TextBuilder();
        AndroidStrings.toCompactString(builder, intent);
        return builder.toString();
      }
    };
  }

  /** Appends a description of the given client {@code downcall} to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      ClientDowncall downcall) {
    if (downcall == null) {
      return builder;
    }
    builder.append(ProtocolIntents.CLIENT_DOWNCALL_KEY).append("::");
    if (downcall.hasStart()) {
      builder.append("start()");
    } else if (downcall.hasStop()) {
      builder.append("stop()");
    } else if (downcall.hasAck()) {
      toCompactString(builder, downcall.getAck());
    } else if (downcall.hasRegistrations()) {
      toCompactString(builder, downcall.getRegistrations());
    } else {
      builder.append(UNKNOWN_MESSAGE);
    }
    return builder;
  }

  /** Appends a description of the given {@code ack} downcall to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder, AckDowncall ack) {
    if (ack == null) {
      return builder;
    }
    builder.append("ack(");
    serializedAckHandleToCompactString(builder, ack.getAckHandle());
    return builder.append(")");
  }

  /**
   * Appends a description of the given {@code registration} downcall to the given {@code builder}.
   */
  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationDowncall registration) {
    if (registration == null) {
      return builder;
    }
    List<ObjectIdP> objects;
    if (registration.getRegistrationsCount() > 0) {
      builder.append("register(");
      objects = registration.getRegistrationsList();
    } else {
      builder.append("unregister(");
      objects = registration.getUnregistrationsList();
    }
    return CommonProtoStrings2.toCompactStringForObjectIds(builder, objects).append(")");
  }

  /** Appends a description of the given internal {@code downcall} to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      InternalDowncall downcall) {
    if (downcall == null) {
      return builder;
    }
    builder.append(ProtocolIntents.INTERNAL_DOWNCALL_KEY).append("::");
    if (downcall.hasServerMessage()) {
      toCompactString(builder, downcall.getServerMessage());
    } else if (downcall.hasNetworkStatus()) {
      toCompactString(builder, downcall.getNetworkStatus());
    } else if (downcall.hasNetworkAddrChange()) {
      builder.append("newtworkAddrChange()");
    } else if (downcall.hasCreateClient()) {
      toCompactString(builder, downcall.getCreateClient());
    } else {
      builder.append(UNKNOWN_MESSAGE);
    }
    return builder;
  }

  /** Appends a description of the given {@code serverMessage} to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      ServerMessage serverMessage) {
    if (serverMessage == null) {
      return builder;
    }
    return builder.append("serverMessage(").append(serverMessage.getData()).append(")");
  }

  /** Appends a description of the given {@code networkStatus} to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      NetworkStatus networkStatus) {
    if (networkStatus == null) {
      return builder;
    }
    return builder.append("networkStatus(isOnline = ").append(networkStatus.getIsOnline())
        .append(")");
  }

  /**
   * Appends a description of the given {@code createClient} command to the given {@code builder}.
   */
  public static TextBuilder toCompactString(TextBuilder builder,
      CreateClient createClient) {
    if (createClient == null) {
      return builder;
    }
    return builder.append("createClient(type = ").append(createClient.getClientType())
        .append(", name = ").append(createClient.getClientName()).append(", skipStartForTest = ")
        .append(createClient.getSkipStartForTest()).append(")");
  }

  /** Appends a description of the given listener {@code upcall} to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      ListenerUpcall upcall) {
    if (upcall == null) {
       return builder;
    }
    builder.append(ProtocolIntents.LISTENER_UPCALL_KEY).append("::");
    if (upcall.hasReady()) {
      builder.append(".ready()");
    } else if (upcall.hasInvalidate()) {
      toCompactString(builder, upcall.getInvalidate());
    } else if (upcall.hasRegistrationStatus()) {
      toCompactString(builder, upcall.getRegistrationStatus());
    } else if (upcall.hasRegistrationFailure()) {
      toCompactString(builder, upcall.getRegistrationFailure());
    } else if (upcall.hasReissueRegistrations()) {
      toCompactString(builder, upcall.getReissueRegistrations());
    } else if (upcall.hasError()) {
      toCompactString(builder, upcall.getError());
    } else {
      builder.append(UNKNOWN_MESSAGE);
    }
    return builder;
  }

  /** Appends a description of the given {@code invalidate} command to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      InvalidateUpcall invalidate) {
    if (invalidate == null) {
      return builder;
    }
    builder.append("invalidate(ackHandle = ");
    serializedAckHandleToCompactString(builder, invalidate.getAckHandle());
    builder.append(", ");
    if (invalidate.hasInvalidation()) {
      CommonProtoStrings2.toCompactString(builder, invalidate.getInvalidation());
    } else if (invalidate.getInvalidateAll()) {
      builder.append("ALL");
    } else if (invalidate.hasInvalidateUnknown()) {
      builder.append("UNKNOWN: ");
      CommonProtoStrings2.toCompactString(builder, invalidate.getInvalidateUnknown());
    }
    return builder.append(")");
  }

  /** Appends a description of the given {@code status} upcall to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationStatusUpcall status) {
    if (status == null) {
      return builder;
    }
    builder.append("registrationStatus(objectId = ");
    CommonProtoStrings2.toCompactString(builder, status.getObjectId());
    return builder.append(", isRegistered = ").append(status.getIsRegistered()).append(")");
  }

  /** Appends a description of the given {@code failure} upcall to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationFailureUpcall failure) {
    if (failure == null) {
      return builder;
    }
    builder.append("registrationFailure(objectId = ");
    CommonProtoStrings2.toCompactString(builder, failure.getObjectId());
    return builder.append(", isTransient = ").append(failure.getTransient()).append(")");
  }

  /**
   * Appends a description of the given {@code reissue} registrations upcall to the given
   * {@code builder}.
   */
  public static TextBuilder toCompactString(TextBuilder builder,
      ReissueRegistrationsUpcall reissue) {
    if (reissue == null) {
      return builder;
    }
    builder.append("reissueRegistrations(prefix = ");
    return builder.append(reissue.getPrefix()).append(", length = ").append(reissue.getLength())
        .append(")");
  }

  /** Appends a description of the given {@code error} upcall to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder, ErrorUpcall error) {
    if (error == null) {
      return builder;
    }
    return builder.append("error(code = ").append(error.getErrorCode()).append(", message = ")
        .append(error.getErrorMessage()).append(", isTransient = ").append(error.getIsTransient())
        .append(")");
  }

  /** Appends a description of the given {@code request} to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      AndroidNetworkSendRequest request) {
    if (request == null) {
      return builder;
    }
    return builder.append(ProtocolIntents.OUTBOUND_MESSAGE_KEY).append("(")
        .append(request.getMessage()).append(")");
  }

  /** Appends a description of the given (@code event} to the given {@code builder}. */
  public static TextBuilder toCompactString(TextBuilder builder,
      AndroidSchedulerEvent event) {
    if (event == null) {
      return builder;
    }
    return builder.append(ProtocolIntents.SCHEDULER_KEY).append("(eventName = ")
        .append(event.getEventName()).append(", ticlId = ").append(event.getTiclId()).append(")");
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder, AckHandleP ackHandle) {
    if (ackHandle == null) {
      return builder;
    }
    return CommonProtoStrings2.toCompactString(builder.appendFormat("AckHandle: "),
        ackHandle.getInvalidation());
  }

  /**
   * Appends a description of the given {@code intent} to the given {@code builder}. If the intent
   * includes some recognized extras, formats the extra context as well.
   */
  public static TextBuilder toCompactString(TextBuilder builder, Intent intent) {
    if (intent == null) {
      return builder;
    }
    builder.append("intent(");
    try {
      if (!tryParseExtra(builder, intent)) {
        builder.append(UNKNOWN_MESSAGE).append(", extras = ")
            .append(Joiner.on(", ").join(intent.getExtras().keySet()));
      }
    } catch (InvalidProtocolBufferException exception) {
      builder.append(ERROR_MESSAGE).append(" : ").append(exception);
    }
    return builder.append(")");
  }

  /** Appends a description of any known extra or appends 'UNKNOWN' if none are recognized. */
  private static boolean tryParseExtra(TextBuilder builder, Intent intent)
      throws InvalidProtocolBufferException {
    byte[] data;

    data = intent.getByteArrayExtra(ProtocolIntents.SCHEDULER_KEY);
    if (data != null) {
      AndroidSchedulerEvent schedulerEvent = AndroidSchedulerEvent.parseFrom(data);
      toCompactString(builder, schedulerEvent);
      return true;
    }

    data = intent.getByteArrayExtra(ProtocolIntents.OUTBOUND_MESSAGE_KEY);
    if (data != null) {
      AndroidNetworkSendRequest outboundMessage = AndroidNetworkSendRequest.parseFrom(data);
      toCompactString(builder, outboundMessage);
      return true;
    }

    data = intent.getByteArrayExtra(ProtocolIntents.LISTENER_UPCALL_KEY);
    if (data != null) {
      ListenerUpcall upcall = ListenerUpcall.parseFrom(data);
      toCompactString(builder, upcall);
      return true;
    }

    data = intent.getByteArrayExtra(ProtocolIntents.INTERNAL_DOWNCALL_KEY);
    if (data != null) {
      InternalDowncall internalDowncall = InternalDowncall.parseFrom(data);
      toCompactString(builder, internalDowncall);
      return true;
    }

    data = intent.getByteArrayExtra(ProtocolIntents.CLIENT_DOWNCALL_KEY);
    if (data != null) {
      ClientDowncall clientDowncall = ClientDowncall.parseFrom(data);
      toCompactString(builder, clientDowncall);
      return true;
    }

    // Didn't recognize any intents.
    return false;
  }

  /** Given serialized form of an ack handle, appends description to {@code builder}. */
  private static TextBuilder serializedAckHandleToCompactString(
      TextBuilder builder, ByteString serialized) {
    if (serialized == null) {
      return builder;
    }
    // The ack handle is supposed by an AckHandleP!
    try {
      AckHandleP ackHandle = AckHandleP.parseFrom(serialized);
      return toCompactString(builder, ackHandle);
    } catch (InvalidProtocolBufferException exception) {
      // But it wasn't... Just log the raw bytes.
      return builder.append(serialized);
    }
  }

  private AndroidStrings() {
    // Avoid instantiation.
  }
}
