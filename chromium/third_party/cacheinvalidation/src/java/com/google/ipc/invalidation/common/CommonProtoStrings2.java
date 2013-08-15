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

import com.google.common.base.Receiver;
import com.google.ipc.invalidation.util.Bytes;
import com.google.ipc.invalidation.util.LazyString;
import com.google.ipc.invalidation.util.TextBuilder;
import com.google.protobuf.ByteString;
import com.google.protos.ipc.invalidation.ClientProtocol.ApplicationClientIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientHeader;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientToServerMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.ConfigChangeMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.ErrorMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InfoMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InfoRequestMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InitializeMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.PropertyRecord;
import com.google.protos.ipc.invalidation.ClientProtocol.ProtocolVersion;
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

import java.util.Collection;


/**
 * Utilities to make it easier/cleaner/shorter for printing and converting protobufs to strings in
 * . This class exposes methods to return objects such that their toString method returns a
 * compact representation of the proto. These methods can be used in the Ticl
 *
 */
public class CommonProtoStrings2 {

  //
  // Implementation notes: The following methods return the object as mentioned above for different
  // protos. Each method (except for a couple of them) essentially calls a private static method
  // that uses a TextBuilder to construct the final string. Each method has the following spec:
  // Returns a compact string representation for {@code <parameter-name>} for logging
  //

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final ByteString byteString) {
    if (byteString == null) {
      return null;
    }
    return new Object() {
      @Override
      public String toString() {
        return Bytes.toString(byteString.toByteArray());
      }
    };
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final byte[] bytes) {
    if (bytes == null) {
      return null;
    }
    return new Object() {
      @Override
      public String toString() {
        return Bytes.toString(bytes);
      }
    };
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final ObjectIdP objectId) {
    return LazyString.toLazyCompactString(objectId, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, objectId);
      }
    });
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final InvalidationP invalidation) {
    return LazyString.toLazyCompactString(invalidation, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, invalidation);
      }
    });
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final RegistrationP registration) {
    return LazyString.toLazyCompactString(registration, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, registration);
      }
    });
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final ApplicationClientIdP applicationId) {
    return LazyString.toLazyCompactString(applicationId, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, applicationId);
      }
    });
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final RegistrationSummary regSummary) {
    return LazyString.toLazyCompactString(regSummary, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, regSummary);
      }
    });
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final InfoMessage infoMessage) {
    return LazyString.toLazyCompactString(infoMessage, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, infoMessage);
      }
    });
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactString(final RegistrationSyncMessage syncMessage) {
    return LazyString.toLazyCompactString(syncMessage, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, syncMessage);
      }
    });
  }

  /** See spec in implementation notes and toCompactString for ClientToServerMessage. */
  public static Object toLazyCompactString(final ClientToServerMessage message,
      final boolean printHighFrequencyMessages) {
    return LazyString.toLazyCompactString(message, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, message, printHighFrequencyMessages);
      }
    });
  }

  /** See spec in implementation notes and toCompactString for ServerToClientMessage. */
  public static Object toLazyCompactString(final ServerToClientMessage message,
      final boolean printHighFrequencyMessages) {
    return LazyString.toLazyCompactString(message, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactString(builder, message, printHighFrequencyMessages);
      }
    });
  }


  /** See spec in implementation notes. */
  public static Object toLazyCompactStringForObjectIds(
      final Collection<ObjectIdP> objectIds) {
    return LazyString.toLazyCompactString(objectIds, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactStringForObjectIds(builder, objectIds);
      }
    });
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactStringForInvalidations(
      final Collection<InvalidationP> invalidations) {
    return LazyString.toLazyCompactString(invalidations, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactStringForInvalidations(builder, invalidations);
      }
    });
  }

  /** See spec in implementation notes. */
  public static Object toLazyCompactStringForRegistrations(
      final Collection<RegistrationP> registrations) {
    return LazyString.toLazyCompactString(registrations, new Receiver<TextBuilder>() {
      @Override
      public void accept(TextBuilder builder) {
        toCompactStringForRegistrations(builder, registrations);
      }
    });
  }

  //
  // Implementation notes: The following helper methods do the actual conversion of the proto into
  // the compact representation in the given builder. Each method has the following spec:
  // Adds a compact representation for {@code <parameter-name>} to {@code builder} and
  // returns {@code builder}.
  // TODO: Look into building indirection tables for the collections to avoid
  // code duplication.
  //

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder, ByteString byteString) {
    if (byteString == null) {
      return builder;
    }
    return Bytes.toCompactString(builder, byteString.toByteArray());
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder, ObjectIdP objectId) {
    if (objectId == null) {
      return builder;
    }
    builder.appendFormat("(Obj: %s, ", objectId.getSource());
    toCompactString(builder, objectId.getName());
    builder.append(')');
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      InvalidationP invalidation) {
    if (invalidation == null) {
      return builder;
    }
    builder.append("(Inv: ");
    toCompactString(builder, invalidation.getObjectId());
    builder.append(", ");
    if (invalidation.getIsTrickleRestart()) {
      builder.append("<");
    }
    builder.append(invalidation.getVersion());
    if (invalidation.hasPayload()) {
      builder.append(", P:");
      toCompactString(builder, invalidation.getPayload());
    }
    builder.append(')');
    return builder;
  }

 /** See spec in implementation notes. */
  public static TextBuilder toCompactStringForInvalidations(TextBuilder builder,
      final Collection<InvalidationP> invalidations) {
    if (invalidations == null) {
      return builder;
    }
    boolean first = true;
    for (InvalidationP invalidation : invalidations) {
      if (!first) {
        builder.append(", ");
      }
      toCompactString(builder, invalidation);
      first = false;
    }
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder, StatusP status) {
    if (status == null) {
      return builder;
    }
    builder.appendFormat("Status: %s", status.getCode());
    if (status.hasDescription()) {
      builder.appendFormat(", Desc: %s", status.getDescription());
    }
    return builder;
  }


  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder, RegistrationP regOp) {
    if (regOp == null) {
      return builder;
    }
    builder.appendFormat("RegOp: %s, ",
        regOp.getOpType() == RegistrationP.OpType.REGISTER ? "R" : "U");
    toCompactString(builder, regOp.getObjectId());
    return builder;
  }

  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationStatus regStatus) {
    if (regStatus == null) {
      return builder;
    }
    builder.append('<');
    toCompactString(builder, regStatus.getRegistration());
    builder.append(", ");
    toCompactString(builder, regStatus.getStatus());
    builder.append('>');
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactStringForRegistrations(TextBuilder builder,
      Collection<RegistrationP> registrations) {
    if (registrations == null) {
      return builder;
    }

    boolean first = true;
    builder.append("RegOps: ");
    for (RegistrationP registration : registrations) {
      if (!first) {
        builder.append(", ");
      }
      toCompactString(builder, registration);
      first = false;
    }
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactStringForRegistrationStatuses(TextBuilder builder,
      Collection<RegistrationStatus> registrationStatuses) {
    if (registrationStatuses == null) {
      return builder;
    }

    boolean first = true;
    builder.append("RegOps: ");
    for (RegistrationStatus registrationStatus : registrationStatuses) {
      if (!first) {
        builder.append(", ");
      }
      toCompactString(builder, registrationStatus);
      first = false;
    }
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactStringForObjectIds(TextBuilder builder,
      Collection<ObjectIdP> objectIds) {
    if (objectIds == null) {
      return builder;
    }
    boolean first = true;
    builder.append("ObjectIds: ");
    for (ObjectIdP objectId : objectIds) {
      if (!first) {
        builder.append(", ");
      }
      toCompactString(builder, objectId);
      first = false;
    }
    return builder;
  }

  /** See spec in implementation notes. */
  private static TextBuilder toCompactString(TextBuilder builder,
      ApplicationClientIdP applicationClientId) {
    if (applicationClientId == null) {
      return builder;
    }
    builder.appendFormat("(Ceid: ");
    toCompactString(builder, applicationClientId.getClientName());
    builder.append(')');
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationSummary regSummary) {
    if (regSummary == null) {
      return builder;
    }

    builder.appendFormat("<RegSummary: Num = %d, Hash = ", regSummary.getNumRegistrations());
    CommonProtoStrings2.toCompactString(builder, regSummary.getRegistrationDigest());
    builder.append('>');
    return builder;
  }

  public static TextBuilder toCompactString(TextBuilder builder,
      ProtocolVersion protocolVersion) {
    if (protocolVersion == null) {
      return builder;
    }
    builder.appendFormat("%d.%d", protocolVersion.getVersion().getMajorVersion(),
        protocolVersion.getVersion().getMinorVersion());
    return builder;
  }

  // Print methods for every client-to-server message type.

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder, ClientHeader header) {
    if (header == null) {
      return builder;
    }
    builder.append("C2S: ");
    toCompactString(builder, header.getProtocolVersion());
    builder.appendFormat(", MsgId: %s, Num regs = %s, Token = ", header.getMessageId(),
        header.getRegistrationSummary().getNumRegistrations());
    toCompactString(builder, header.getClientToken());
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      InitializeMessage initializeMessage) {
    if (initializeMessage == null) {
      return builder;
    }
    builder.appendFormat("InitMsg: Client Type: %d, ", initializeMessage.getClientType());
    toCompactString(builder, initializeMessage.getApplicationClientId());
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationMessage registrationMessage) {
    if (registrationMessage == null) {
      return builder;
    }
    builder.appendFormat("RegMsg: ");
    toCompactStringForRegistrations(builder, registrationMessage.getRegistrationList());
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationSyncMessage syncMessage) {
    if (syncMessage == null) {
      return builder;
    }
    RegistrationSubtree subtree = syncMessage.getSubtree(0);
    builder.appendFormat("RegSyncMsg: Num regs: %d, Regs: ", subtree.getRegisteredObjectCount());
    toCompactStringForObjectIds(builder, subtree.getRegisteredObjectList());
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      InfoMessage infoMessage) {
    if (infoMessage == null) {
      return builder;
    }
    builder.appendFormat("InfoMsg: Platform = %s, Is_summary_requested = %s, Perf counters: ",
        infoMessage.getClientVersion().getPlatform(),
        infoMessage.getServerRegistrationSummaryRequested());
    boolean first = true;
    for (PropertyRecord record : infoMessage.getPerformanceCounterList()) {
      if (!first) {
        builder.append(", ");
      }
      builder.appendFormat("%s = %d", record.getName(), record.getValue());
      first = false;
    }
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      InvalidationMessage invMessage) {
    if (invMessage == null) {
      return builder;
    }
    builder.appendFormat("InvMsg: ");
    toCompactStringForInvalidations(builder, invMessage.getInvalidationList());
    return builder;
  }

  // Print methods for every server-to-client message type.

  public static TextBuilder toCompactString(TextBuilder builder, ServerHeader header) {
    if (header == null) {
      return builder;
    }
    builder.append("S2C: ");
    toCompactString(builder, header.getProtocolVersion());
    builder.appendFormat(", MsgId: %s, Num regs = %s, Token = ", header.getMessageId(),
        header.getRegistrationSummary().getNumRegistrations());
    toCompactString(builder, header.getClientToken());
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      TokenControlMessage tokenControlMessage) {
    if (tokenControlMessage == null) {
      return builder;
    }
    builder.append("TokenMsg: ");
    toCompactString(builder, tokenControlMessage.getNewToken());
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationStatusMessage regStatusMessage) {
    if (regStatusMessage == null) {
      return builder;
    }
    builder.append("RegStatusMsg: ");
    toCompactStringForRegistrationStatuses(builder, regStatusMessage.getRegistrationStatusList());
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      RegistrationSyncRequestMessage regSyncRequestMessage) {
    if (regSyncRequestMessage == null) {
      return builder;
    }
    builder.append("RegSyncRequestMsg: ");
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      InfoRequestMessage infoRequestMessage) {
    if (infoRequestMessage == null) {
      return builder;
    }
    builder.append("InfoRequestMsg:");
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      ConfigChangeMessage configChangeMessage) {
    if (configChangeMessage == null) {
      return builder;
    }
    builder.appendFormat("ConfigChangeMsg: %d", configChangeMessage.getNextMessageDelayMs());
    return builder;
  }

  /** See spec in implementation notes. */
  public static TextBuilder toCompactString(TextBuilder builder,
      ErrorMessage errorMessage) {
    if (errorMessage == null) {
      return builder;
    }
    builder.appendFormat("ErrorMsg: %s, %s", errorMessage.getCode(), errorMessage.getDescription());
    return builder;
  }

  /**
   * If {@code printHighFrequencyMessages} is true, logs sub-messages that are exchanged at a high
   * frequency between the client and the registrar, e.g., invalidation ack message, heartbeat
   * message.
   */
  public static TextBuilder toCompactString(TextBuilder builder,
      ClientToServerMessage msg, boolean printHighFrequencyMessages) {
    // Print the header and any sub-messages in the message.

    toCompactString(builder, msg.getHeader());
    builder.append(',');

    if (msg.hasInitializeMessage()) {
      toCompactString(builder, msg.getInitializeMessage());
      builder.append(',');
    }
    if (msg.hasRegistrationMessage()) {
      toCompactString(builder, msg.getRegistrationMessage());
      builder.append(',');
    }
    if (msg.hasRegistrationSyncMessage()) {
      toCompactString(builder, msg.getRegistrationSyncMessage());
      builder.append(',');
    }
    if (printHighFrequencyMessages && msg.hasInvalidationAckMessage()) {
      toCompactString(builder, msg.getInvalidationAckMessage());
      builder.append(',');
    }
    if (printHighFrequencyMessages && msg.hasInfoMessage()) {
      toCompactString(builder, msg.getInfoMessage());
      builder.append(',');
    }
    return builder;
  }

  /**
   * If {@code printHighFrequencyMessages} is true, logs sub-messages that are exchanged at a high
   * frequency between the client and the registrar (if they are the only messages present),
   * e.g., invalidation message.
   */
  public static TextBuilder toCompactString(TextBuilder builder,
      ServerToClientMessage msg, boolean printHighFrequencyMessages) {
    // Print the header and any sub-messages in the message.

    toCompactString(builder, msg.getHeader());
    builder.append(',');

    if (msg.hasTokenControlMessage()) {
      toCompactString(builder, msg.getTokenControlMessage());
      builder.append(',');
    }
    if (printHighFrequencyMessages && msg.hasInvalidationMessage()) {
      toCompactString(builder, msg.getInvalidationMessage());
      builder.append(',');
    }
    if (msg.hasErrorMessage()) {
      toCompactString(builder, msg.getErrorMessage());
      builder.append(',');
    }
    if (msg.hasRegistrationSyncRequestMessage()) {
      toCompactString(builder, msg.getRegistrationSyncRequestMessage());
      builder.append(',');
    }
    if (msg.hasRegistrationStatusMessage()) {
      toCompactString(builder, msg.getRegistrationStatusMessage());
      builder.append(',');
    }
    if (msg.hasInfoRequestMessage()) {
      toCompactString(builder, msg.getInfoRequestMessage());
      builder.append(',');
    }
    if (msg.hasConfigChangeMessage()) {
      toCompactString(builder, msg.getConfigChangeMessage());
      builder.append(',');
    }
    return builder;
  }

  private CommonProtoStrings2() { // To prevent instantiation
  }
}
