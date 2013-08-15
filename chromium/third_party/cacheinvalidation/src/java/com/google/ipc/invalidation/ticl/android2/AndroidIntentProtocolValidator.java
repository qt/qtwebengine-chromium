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

import com.google.ipc.invalidation.common.ClientProtocolAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.VersionAccessor;
import com.google.ipc.invalidation.common.ProtoValidator;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.ticl.android2.AndroidServiceAccessor.AndroidNetworkSendRequestAccessor;
import com.google.ipc.invalidation.ticl.android2.AndroidServiceAccessor.AndroidTiclStateAccessor;
import com.google.ipc.invalidation.ticl.android2.AndroidServiceAccessor.AndroidTiclStateAccessor.MetadataAccessor;
import com.google.ipc.invalidation.ticl.android2.AndroidServiceAccessor.AndroidTiclStateWithDigestAccessor;
import com.google.ipc.invalidation.ticl.android2.AndroidServiceAccessor.ClientDowncallAccessor;
import com.google.ipc.invalidation.ticl.android2.AndroidServiceAccessor.InternalDowncallAccessor;
import com.google.ipc.invalidation.ticl.android2.AndroidServiceAccessor.ListenerUpcallAccessor;
import com.google.protobuf.MessageLite;
import com.google.protos.ipc.invalidation.AndroidService.AndroidNetworkSendRequest;
import com.google.protos.ipc.invalidation.AndroidService.AndroidSchedulerEvent;
import com.google.protos.ipc.invalidation.AndroidService.AndroidTiclStateWithDigest;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall;
import com.google.protos.ipc.invalidation.ClientProtocol.Version;

/**
 * Validator for Android internal protocol intents and messages.
 * <p>
 * This class works by defining instances of {@code MessageInfo} for each protocol buffer
 * that requires validation. A {@code MessageInfo} takes two parameters: an <i>accessor</i>
 * that allows it to read the fields of an instance of the message to be validated, and a list
 * of {@code FieldInfo} objects that, for each field in the message, specify whether the field
 * is required or optional. Additionally, a {@code FieldInfo} may have a reference to a
 * {@code MessageInfo} object that specifies how to recursively validate the field.
 * <p>
 * For example, the validation for the {@code ACK} downcall protocol buffer is specified as follows:
 * <code>
 * static final MessageInfo ACK = new MessageInfo(
 *       ClientDowncallAccessor.ACK_DOWNCALL_ACCESSOR,
 *       FieldInfo.newRequired(ClientDowncallAccessor.AckDowncallAccessor.ACK_HANDLE));
 * </code>
 * This specifies that the {@code ACK_DOWNCALL_ACCESSOR} is to be used to read the fields of
 * protocol buffers to be validated, and that instances of those protocol buffers should have
 * exactly one field (ack handle) set.
 * <p>
 * For a more complicated example, the {{@link DowncallMessageInfos#REGISTRATIONS} validator
 * requires one or the other (but not both) of two fields to be set, and those fields are
 * recursively validated.
 *
 */
public final class AndroidIntentProtocolValidator extends ProtoValidator {
  /** Validation for composite (major/minor) versions. */
  static final MessageInfo VERSION = new MessageInfo(ClientProtocolAccessor.VERSION_ACCESSOR,
    FieldInfo.newRequired(VersionAccessor.MAJOR_VERSION),
    FieldInfo.newRequired(VersionAccessor.MINOR_VERSION)) {
    @Override
    public boolean postValidate(MessageLite message) {
      // Versions must be non-negative.
      Version version = (Version) message;
      if ((version.getMajorVersion() < 0) || (version.getMinorVersion() < 0)) {
        return false;
      }
      return true;
    }
  };

  /** Validation for public client downcalls. */
  static class DowncallMessageInfos {
    static final MessageInfo ACK = new MessageInfo(
        ClientDowncallAccessor.ACK_DOWNCALL_ACCESSOR,
        FieldInfo.newRequired(ClientDowncallAccessor.AckDowncallAccessor.ACK_HANDLE));

    static final MessageInfo REGISTRATIONS = new MessageInfo(
        ClientDowncallAccessor.REGISTRATION_DOWNCALL_ACCESSOR,
        FieldInfo.newOptional(ClientDowncallAccessor.RegistrationDowncallAccessor.REGISTRATIONS),
        FieldInfo.newOptional(
            ClientDowncallAccessor.RegistrationDowncallAccessor.UNREGISTRATIONS)) {
      @Override
      public boolean postValidate(MessageLite message) {
        int numSetFields = 0;
        for (FieldInfo fieldInfo : getAllFields()) {
          if (ClientDowncallAccessor.REGISTRATION_DOWNCALL_ACCESSOR.hasField(
              message, fieldInfo.getFieldDescriptor())) {
           ++numSetFields;
          }
        }
        return numSetFields == 1; // Registrations or unregistrations, but not both.
      }
    };

    static final MessageInfo DOWNCALL_MSG = new MessageInfo(
        AndroidServiceAccessor.CLIENT_DOWNCALL_ACCESSOR,
        FieldInfo.newRequired(AndroidServiceAccessor.ClientDowncallAccessor.VERSION, VERSION),
        FieldInfo.newOptional(AndroidServiceAccessor.ClientDowncallAccessor.SERIAL),
        FieldInfo.newOptional(AndroidServiceAccessor.ClientDowncallAccessor.ACK, ACK),
        FieldInfo.newOptional(
            AndroidServiceAccessor.ClientDowncallAccessor.REGISTRATIONS, REGISTRATIONS),
        FieldInfo.newOptional(AndroidServiceAccessor.ClientDowncallAccessor.START),
        FieldInfo.newOptional(AndroidServiceAccessor.ClientDowncallAccessor.STOP)) {
      @Override
      public boolean postValidate(MessageLite message) {
        int numSetFields = 0;
        for (FieldInfo fieldInfo : getAllFields()) {
          if (AndroidServiceAccessor.CLIENT_DOWNCALL_ACCESSOR.hasField(
              message, fieldInfo.getFieldDescriptor())) {
           ++numSetFields;
          }
        }
        return numSetFields == 2; // Version plus exactly one operation. Serial not currently used.
      }
    };
  }

  /** Validation for client internal downcalls. */
  static class InternalDowncallInfos {
    private static MessageInfo NETWORK_STATUS = new MessageInfo(
        InternalDowncallAccessor.NETWORK_STATUS_ACCESSOR,
        FieldInfo.newRequired(InternalDowncallAccessor.NetworkStatusAccessor.IS_ONLINE));

    private static MessageInfo SERVER_MESSAGE = new MessageInfo(
        InternalDowncallAccessor.SERVER_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(InternalDowncallAccessor.ServerMessageAccessor.DATA));

    // We do not post-validate the config in this message, since the Ticl should be doing it, and
    // it's not clear that we should be peering into Ticl protocol buffers anyway.
    private static MessageInfo CREATE_CLIENT_MESSAGE = new MessageInfo(
        InternalDowncallAccessor.CREATE_CLIENT_ACCESSOR,
        FieldInfo.newRequired(InternalDowncallAccessor.CreateClientAccessor.CLIENT_CONFIG),
        FieldInfo.newRequired(InternalDowncallAccessor.CreateClientAccessor.CLIENT_NAME),
        FieldInfo.newRequired(InternalDowncallAccessor.CreateClientAccessor.CLIENT_TYPE),
        FieldInfo.newRequired(InternalDowncallAccessor.CreateClientAccessor.SKIP_START_FOR_TEST));

    static final MessageInfo INTERNAL_DOWNCALL_MSG = new MessageInfo(
        AndroidServiceAccessor.INTERNAL_DOWNCALL_ACCESSOR,
        FieldInfo.newRequired(InternalDowncallAccessor.VERSION, VERSION),
        FieldInfo.newOptional(InternalDowncallAccessor.NETWORK_STATUS, NETWORK_STATUS),
        FieldInfo.newOptional(InternalDowncallAccessor.SERVER_MESSAGE, SERVER_MESSAGE),
        FieldInfo.newOptional(InternalDowncallAccessor.NETWORK_ADDR_CHANGE),
        FieldInfo.newOptional(InternalDowncallAccessor.CREATE_CLIENT, CREATE_CLIENT_MESSAGE)) {
      @Override
      public boolean postValidate(MessageLite message) {
        int numSetFields = 0;
        for (FieldInfo fieldInfo : getAllFields()) {
          if (AndroidServiceAccessor.INTERNAL_DOWNCALL_ACCESSOR.hasField(
              message, fieldInfo.getFieldDescriptor())) {
           ++numSetFields;
          }
        }
        return numSetFields == 2; // Version plus exactly one operation. Serial not currently used.
      }
    };
  }

  /** Validation for listener upcalls. */
  static class ListenerUpcallInfos {
    static final MessageInfo ERROR = new MessageInfo(ListenerUpcallAccessor.ERROR_UPCALL_ACCESSOR,
        FieldInfo.newRequired(ListenerUpcallAccessor.ErrorUpcallAccessor.ERROR_CODE),
        FieldInfo.newRequired(ListenerUpcallAccessor.ErrorUpcallAccessor.ERROR_MESSAGE),
        FieldInfo.newRequired(ListenerUpcallAccessor.ErrorUpcallAccessor.IS_TRANSIENT));

    // TODO: validate INVALIDATE_UNKNOWN and INVALIDATION sub-messages.
    static final MessageInfo INVALIDATE = new MessageInfo(
        ListenerUpcallAccessor.INVALIDATE_UPCALL_ACCESSOR,
        FieldInfo.newRequired(ListenerUpcallAccessor.InvalidateUpcallAccessor.ACK_HANDLE),
        FieldInfo.newOptional(ListenerUpcallAccessor.InvalidateUpcallAccessor.INVALIDATE_ALL),
        FieldInfo.newOptional(ListenerUpcallAccessor.InvalidateUpcallAccessor.INVALIDATE_UNKNOWN),
        FieldInfo.newOptional(ListenerUpcallAccessor.InvalidateUpcallAccessor.INVALIDATION)) {
      @Override
      public boolean postValidate(MessageLite message) {
        int numSetFields = 0;
        for (FieldInfo fieldInfo : getAllFields()) {
          if (ListenerUpcallAccessor.INVALIDATE_UPCALL_ACCESSOR.hasField(
              message, fieldInfo.getFieldDescriptor())) {
           ++numSetFields;
          }
        }
        return numSetFields == 2; // Handle plus exactly one operation. Serial not currently used.
      }
    };

    static final MessageInfo REGISTRATION_FAILURE = new MessageInfo(
        ListenerUpcallAccessor.REGISTRATION_FAILURE_UPCALL_ACCESSOR,
        FieldInfo.newRequired(ListenerUpcallAccessor.RegistrationFailureUpcallAccessor.MESSAGE),
        FieldInfo.newRequired(ListenerUpcallAccessor.RegistrationFailureUpcallAccessor.OBJECT_ID),
        FieldInfo.newRequired(ListenerUpcallAccessor.RegistrationFailureUpcallAccessor.TRANSIENT));

    static final MessageInfo REGISTRATION_STATUS = new MessageInfo(
        ListenerUpcallAccessor.REGISTRATION_STATUS_UPCALL_ACCESSOR,
        FieldInfo.newRequired(
            ListenerUpcallAccessor.RegistrationStatusUpcallAccessor.IS_REGISTERED),
        FieldInfo.newRequired(ListenerUpcallAccessor.RegistrationStatusUpcallAccessor.OBJECT_ID));

    static final MessageInfo REISSUE_REGISTRATIONS = new MessageInfo(
        ListenerUpcallAccessor.REISSUE_REGISTRATIONS_UPCALL_ACCESSOR,
        FieldInfo.newRequired(ListenerUpcallAccessor.ReissueRegistrationsUpcallAccessor.LENGTH),
        FieldInfo.newRequired(ListenerUpcallAccessor.ReissueRegistrationsUpcallAccessor.PREFIX));

    static final MessageInfo LISTENER_UPCALL_MESSAGE = new MessageInfo(
        AndroidServiceAccessor.LISTENER_UPCALL_ACCESSOR,
        FieldInfo.newRequired(ListenerUpcallAccessor.VERSION, VERSION),
        FieldInfo.newOptional(ListenerUpcallAccessor.SERIAL),
        FieldInfo.newOptional(ListenerUpcallAccessor.ERROR, ERROR),
        FieldInfo.newOptional(ListenerUpcallAccessor.INVALIDATE, INVALIDATE),
        FieldInfo.newOptional(ListenerUpcallAccessor.READY),
        FieldInfo.newOptional(ListenerUpcallAccessor.REGISTRATION_FAILURE, REGISTRATION_FAILURE),
        FieldInfo.newOptional(ListenerUpcallAccessor.REGISTRATION_STATUS, REGISTRATION_STATUS),
        FieldInfo.newOptional(
            ListenerUpcallAccessor.REISSUE_REGISTRATIONS, REISSUE_REGISTRATIONS)) {
      @Override
      public boolean postValidate(MessageLite message) {
        int numSetFields = 0;
        for (FieldInfo fieldInfo : getAllFields()) {
          if (AndroidServiceAccessor.LISTENER_UPCALL_ACCESSOR.hasField(
              message, fieldInfo.getFieldDescriptor())) {
           ++numSetFields;
          }
        }
        return numSetFields == 2; // Version plus exactly one operation. Serial not currently used.
      }
    };
  }

  /** Validation for internal protocol buffers. */
  static class InternalInfos {
    static final MessageInfo ANDROID_SCHEDULER_EVENT = new MessageInfo(
        AndroidServiceAccessor.ANDROID_SCHEDULER_EVENT_ACCESSOR,
        FieldInfo.newRequired(
            AndroidServiceAccessor.AndroidSchedulerEventAccessor.VERSION, VERSION),
        FieldInfo.newRequired(AndroidServiceAccessor.AndroidSchedulerEventAccessor.EVENT_NAME),
        FieldInfo.newRequired(AndroidServiceAccessor.AndroidSchedulerEventAccessor.TICL_ID));

    static final MessageInfo ANDROID_NETWORK_SEND_REQUEST = new MessageInfo(
        AndroidServiceAccessor.ANDROID_NETWORK_SEND_REQUEST_ACCESSOR,
        FieldInfo.newRequired(AndroidNetworkSendRequestAccessor.VERSION, VERSION),
        FieldInfo.newRequired(AndroidNetworkSendRequestAccessor.MESSAGE));

    // We do not post-validate the config in this message, since the Ticl should be doing it, and
    // it's not clear that we should be peering into Ticl protocol buffers anyway.
    static final MessageInfo PERSISTED_STATE_METADATA = new MessageInfo(
        AndroidTiclStateAccessor.METADATA_ACCESSOR,
        FieldInfo.newRequired(MetadataAccessor.CLIENT_CONFIG),
        FieldInfo.newRequired(MetadataAccessor.CLIENT_NAME),
        FieldInfo.newRequired(MetadataAccessor.CLIENT_TYPE),
        FieldInfo.newRequired(MetadataAccessor.TICL_ID));

    static final MessageInfo ANDROID_TICL_STATE = new MessageInfo(
        AndroidServiceAccessor.ANDROID_TICL_STATE_ACCESSOR,
        FieldInfo.newRequired(AndroidTiclStateAccessor.METADATA, PERSISTED_STATE_METADATA),
        FieldInfo.newRequired(AndroidTiclStateAccessor.TICL_STATE),
        FieldInfo.newRequired(AndroidTiclStateAccessor.VERSION));

    static final MessageInfo ANDROID_TICL_STATE_WITH_DIGEST = new MessageInfo(
        AndroidServiceAccessor.ANDROID_TICL_STATE_WITH_DIGEST_ACCESSOR,
        FieldInfo.newRequired(AndroidTiclStateWithDigestAccessor.DIGEST),
        FieldInfo.newRequired(AndroidTiclStateWithDigestAccessor.STATE, ANDROID_TICL_STATE));
  }

  /** Returns whether {@code downcall} has a valid set of fields with valid values. */
  boolean isDowncallValid(ClientDowncall downcall) {
    return checkMessage(downcall, DowncallMessageInfos.DOWNCALL_MSG);
  }

  /** Returns whether {@code downcall} has a valid set of fields with valid values. */
  boolean isInternalDowncallValid(InternalDowncall downcall) {
    return checkMessage(downcall, InternalDowncallInfos.INTERNAL_DOWNCALL_MSG);
  }

  /** Returns whether {@code upcall} has a valid set of fields with valid values. */
  boolean isListenerUpcallValid(ListenerUpcall upcall) {
    return checkMessage(upcall, ListenerUpcallInfos.LISTENER_UPCALL_MESSAGE);
  }

  /** Returns whether {@code event} has a valid set of fields with valid values. */
  boolean isSchedulerEventValid(AndroidSchedulerEvent event) {
    return checkMessage(event, InternalInfos.ANDROID_SCHEDULER_EVENT);
  }

  /** Returns whether {@code request} has a valid set of fields with valid values. */
  public boolean isNetworkSendRequestValid(AndroidNetworkSendRequest request) {
    return checkMessage(request, InternalInfos.ANDROID_NETWORK_SEND_REQUEST);
  }

  /**
   * Returns whether {@code state} has a valid set of fields with valid values. Does not
   * verify the digest.
   */
  boolean isTiclStateValid(AndroidTiclStateWithDigest state) {
    return checkMessage(state, InternalInfos.ANDROID_TICL_STATE_WITH_DIGEST);
  }

  public AndroidIntentProtocolValidator(Logger logger) {
    super(logger);
  }
}
