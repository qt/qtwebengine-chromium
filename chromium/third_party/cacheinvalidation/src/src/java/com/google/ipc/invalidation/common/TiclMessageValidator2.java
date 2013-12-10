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

import com.google.ipc.invalidation.common.ClientProtocolAccessor.ApplicationClientIdPAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ClientConfigPAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ClientHeaderAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ClientToServerMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ClientVersionAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ConfigChangeMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ErrorMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.InfoMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.InfoRequestMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.InitializeMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.InvalidationMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.InvalidationPAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ObjectIdPAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ProtocolHandlerConfigPAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.RateLimitPAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.RegistrationMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.RegistrationPAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.RegistrationStatusAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.RegistrationStatusMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.RegistrationSubtreeAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.RegistrationSummaryAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.RegistrationSyncMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ServerHeaderAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.ServerToClientMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.StatusPAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.TokenControlMessageAccessor;
import com.google.ipc.invalidation.common.ClientProtocolAccessor.VersionAccessor;
import com.google.ipc.invalidation.util.BaseLogger;
import com.google.protobuf.MessageLite;
import com.google.protos.ipc.invalidation.ClientProtocol.ApplicationClientIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientHeader;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientToServerMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.ConfigChangeMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InitializeMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.RateLimitP;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSummary;
import com.google.protos.ipc.invalidation.ClientProtocol.ServerHeader;
import com.google.protos.ipc.invalidation.ClientProtocol.ServerToClientMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.Version;

/**
 * Validator for v2 protocol messages.
 * <p>
 * The basic idea is to declare information about each field in each message, i.e., whether it is
 * optional or it is required. {@code FieldInfo} is a class that keeps track of information
 * about each field and {@code MessageInfo} is a class that keeps track of information about each
 * message. Given a message, we recursively descend a MessageInfo and determine if the fields
 * are as expected. Then once we are done with a message, we perform a post validation step
 * which checks constraints across fields.
 *
 */
public class TiclMessageValidator2 extends ProtoValidator {
  public TiclMessageValidator2(BaseLogger logger) {
    super(logger);
  }

  /** Describes how to validate common messages. */
  
  public class CommonMsgInfos {

    /** Validation for composite (major/minor) versions. */
    final MessageInfo VERSION = new MessageInfo(ClientProtocolAccessor.VERSION_ACCESSOR,
      FieldInfo.newRequired(VersionAccessor.MAJOR_VERSION),
      FieldInfo.newRequired(VersionAccessor.MINOR_VERSION)) {
      @Override
      public boolean postValidate(MessageLite message) {
        // Versions must be non-negative.
        Version version = (Version) message;
        if ((version.getMajorVersion() < 0) || (version.getMinorVersion() < 0)) {
          logger.info("Invalid versions: %s", version);
          return false;
        }
        return true;
      }
    };

    /** Validation for the protocol version. */
    final MessageInfo PROTOCOL_VERSION = new MessageInfo(
        ClientProtocolAccessor.PROTOCOL_VERSION_ACCESSOR,
        FieldInfo.newRequired(ClientProtocolAccessor.ProtocolVersionAccessor.VERSION, VERSION));

    /** Validation for object ids. */
    final MessageInfo OID = new MessageInfo(ClientProtocolAccessor.OBJECT_ID_P_ACCESSOR,
        FieldInfo.newRequired(ObjectIdPAccessor.NAME),
        FieldInfo.newRequired(ObjectIdPAccessor.SOURCE)) {
      @Override
      public boolean postValidate(MessageLite message) {
        // Must have non-negative source code.
        ObjectIdP oid = (ObjectIdP) message;
        if (oid.getSource() < 0) {
          logger.info("Source was negative: %s", oid);
          return false;
        }
        return true;
      }
    };

    /** Validation for invalidations. */
    final MessageInfo INVALIDATION = new MessageInfo(
        ClientProtocolAccessor.INVALIDATION_P_ACCESSOR,
        FieldInfo.newRequired(InvalidationPAccessor.OBJECT_ID, OID),
        FieldInfo.newRequired(InvalidationPAccessor.IS_KNOWN_VERSION),
        FieldInfo.newRequired(InvalidationPAccessor.VERSION),
        FieldInfo.newOptional(InvalidationPAccessor.PAYLOAD),
        FieldInfo.newOptional(InvalidationPAccessor.IS_TRICKLE_RESTART),
        FieldInfo.newOptional(InvalidationPAccessor.BRIDGE_ARRIVAL_TIME_MS_DEPRECATED)) {
      @Override
      public boolean postValidate(MessageLite message) {
        // Must have non-negative version.
        InvalidationP invalidation = (InvalidationP) message;
        if (invalidation.getVersion() < 0) {
          logger.info("Version was negative: %s", invalidation);
          return false;
        }
        boolean isUnknownVersion = !invalidation.getIsKnownVersion();
        // Note that a missing value for is_trickle_restart is treated like a true value,
        // becomes it comes from a downlevel client that uses invalidation semantics.
        boolean isTrickleRestart = !invalidation.hasIsTrickleRestart() ||
            invalidation.getIsTrickleRestart();
        if (isUnknownVersion && !isTrickleRestart) {
          logger.info(
              "if is_known_version is false, is_trickle_restart must be true or missing: %s",
              invalidation);
          return false;
        }
        return true;
      }
    };

    /** Validation for a message containing many invalidations. */
    final MessageInfo INVALIDATION_MSG;

    /** Validation for a single registration description. */
    final MessageInfo REGISTRATIONP = new MessageInfo(
        ClientProtocolAccessor.REGISTRATION_P_ACCESSOR,
        FieldInfo.newRequired(RegistrationPAccessor.OBJECT_ID, OID),
        FieldInfo.newRequired(RegistrationPAccessor.OP_TYPE));

    /** Validation for a summary of registration state. */
    final MessageInfo REGISTRATION_SUMMARY = new MessageInfo(
        ClientProtocolAccessor.REGISTRATION_SUMMARY_ACCESSOR,
        FieldInfo.newRequired(RegistrationSummaryAccessor.NUM_REGISTRATIONS),
        FieldInfo.newRequired(RegistrationSummaryAccessor.REGISTRATION_DIGEST)) {
      @Override
      public boolean postValidate(MessageLite message) {
        RegistrationSummary summary = (RegistrationSummary) message;
        return (summary.getNumRegistrations() >= 0)
            && (!summary.getRegistrationDigest().isEmpty());
      }
    };

    final MessageInfo RATE_LIMIT = new MessageInfo(
        ClientProtocolAccessor.RATE_LIMIT_P_ACCESSOR,
        FieldInfo.newRequired(RateLimitPAccessor.WINDOW_MS),
        FieldInfo.newRequired(RateLimitPAccessor.COUNT)) {
      @Override
      public boolean postValidate(MessageLite message) {
        RateLimitP rateLimit = (RateLimitP) message;
        return (rateLimit.getWindowMs() >= 1000) &&
            (rateLimit.getWindowMs() > rateLimit.getCount());
      }
    };

    final MessageInfo PROTOCOL_HANDLER_CONFIG = new MessageInfo(
        ClientProtocolAccessor.PROTOCOL_HANDLER_CONFIG_P_ACCESSOR,
        FieldInfo.newOptional(ProtocolHandlerConfigPAccessor.BATCHING_DELAY_MS),
        FieldInfo.newOptional(ProtocolHandlerConfigPAccessor.RATE_LIMIT, RATE_LIMIT)
        );

    // Validation for Client Config. */
    final MessageInfo CLIENT_CONFIG = new MessageInfo(
        ClientProtocolAccessor.CLIENT_CONFIG_P_ACCESSOR,
        FieldInfo.newRequired(ClientConfigPAccessor.VERSION, VERSION),
        FieldInfo.newOptional(ClientConfigPAccessor.NETWORK_TIMEOUT_DELAY_MS),
        FieldInfo.newOptional(ClientConfigPAccessor.WRITE_RETRY_DELAY_MS),
        FieldInfo.newOptional(ClientConfigPAccessor.HEARTBEAT_INTERVAL_MS),
        FieldInfo.newOptional(ClientConfigPAccessor.PERF_COUNTER_DELAY_MS),
        FieldInfo.newOptional(ClientConfigPAccessor.MAX_EXPONENTIAL_BACKOFF_FACTOR),
        FieldInfo.newOptional(ClientConfigPAccessor.SMEAR_PERCENT),
        FieldInfo.newOptional(ClientConfigPAccessor.IS_TRANSIENT),
        FieldInfo.newOptional(ClientConfigPAccessor.INITIAL_PERSISTENT_HEARTBEAT_DELAY_MS),
        FieldInfo.newOptional(ClientConfigPAccessor.CHANNEL_SUPPORTS_OFFLINE_DELIVERY),
        FieldInfo.newRequired(ClientConfigPAccessor.PROTOCOL_HANDLER_CONFIG,
            PROTOCOL_HANDLER_CONFIG),
        FieldInfo.newOptional(ClientConfigPAccessor.OFFLINE_HEARTBEAT_THRESHOLD_MS),
        FieldInfo.newOptional(ClientConfigPAccessor.ALLOW_SUPPRESSION)
        );

    private CommonMsgInfos() {
      // Initialize in constructor since other instance fields are referenced
      INVALIDATION_MSG = new MessageInfo(
          ClientProtocolAccessor.INVALIDATION_MESSAGE_ACCESSOR,
          FieldInfo.newRequired(InvalidationMessageAccessor.INVALIDATION,
              this.INVALIDATION));
    }
  }

  /** Describes how to validate client messages. */
  private class ClientMsgInfos {
    /** Validation for client headers. */
    final MessageInfo HEADER = new MessageInfo(
        ClientProtocolAccessor.CLIENT_HEADER_ACCESSOR,
        FieldInfo.newRequired(ClientHeaderAccessor.PROTOCOL_VERSION,
            commonMsgInfos.PROTOCOL_VERSION),
        FieldInfo.newOptional(ClientHeaderAccessor.CLIENT_TOKEN),
        FieldInfo.newOptional(ClientHeaderAccessor.REGISTRATION_SUMMARY,
            commonMsgInfos.REGISTRATION_SUMMARY),
        FieldInfo.newRequired(ClientHeaderAccessor.CLIENT_TIME_MS),
        FieldInfo.newRequired(ClientHeaderAccessor.MAX_KNOWN_SERVER_TIME_MS),
        FieldInfo.newOptional(ClientHeaderAccessor.MESSAGE_ID),
        FieldInfo.newOptional(ClientHeaderAccessor.CLIENT_TYPE)) {
      @Override
      public boolean postValidate(MessageLite message) {
        ClientHeader header = (ClientHeader) message;

        // If set, token must not be empty.
        if (header.hasClientToken() && header.getClientToken().isEmpty()) {
          logger.info("Client token was set but empty: %s", header);
          return false;
        }

        // If set, message id must not be empty.
        // Do not use String.isEmpty() here for Froyo (JDK5) compatibility.
        if (header.hasMessageId() && (header.getMessageId().length() == 0)) {
          logger.info("Message id was set but empty: %s", header);
          return false;
        }

        if (header.getClientTimeMs() < 0) {
          logger.info("Client time was negative: %s", header);
          return false;
        }

        if (header.getMaxKnownServerTimeMs() < 0) {
          logger.info("Max known server time was negative: %s", header);
          return false;
        }
        return true;
      }
    };

    /** Validation for application client ids. */
    final MessageInfo APPLICATION_CLIENT_ID = new MessageInfo(
        // Client type is optional here since the registrar needs to accept messages from
        // the ticls that do not set the client type.
        ClientProtocolAccessor.APPLICATION_CLIENT_ID_P_ACCESSOR,
        FieldInfo.newOptional(ApplicationClientIdPAccessor.CLIENT_TYPE),
        FieldInfo.newRequired(ApplicationClientIdPAccessor.CLIENT_NAME)) {
      @Override
      public boolean postValidate(MessageLite message) {
        ApplicationClientIdP applicationClientId = (ApplicationClientIdP) message;
        return !applicationClientId.getClientName().isEmpty();
      }
    };

    /** Validation for client initialization messages. */
    final MessageInfo INITIALIZE_MESSAGE = new MessageInfo(
        ClientProtocolAccessor.INITIALIZE_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(InitializeMessageAccessor.CLIENT_TYPE),
        FieldInfo.newRequired(InitializeMessageAccessor.NONCE),
        FieldInfo.newRequired(InitializeMessageAccessor.DIGEST_SERIALIZATION_TYPE),
        FieldInfo.newRequired(InitializeMessageAccessor.APPLICATION_CLIENT_ID,
            APPLICATION_CLIENT_ID)) {
      @Override
      public boolean postValidate(MessageLite message) {
        return ((InitializeMessage) message).getClientType() >= 0;
      }
    };

    /** Validation for registration requests. */
    final MessageInfo REGISTRATION = new MessageInfo(
        ClientProtocolAccessor.REGISTRATION_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(RegistrationMessageAccessor.REGISTRATION,
            commonMsgInfos.REGISTRATIONP));

    /** Validation for client versions. */
    final MessageInfo CLIENT_VERSION = new MessageInfo(
        ClientProtocolAccessor.CLIENT_VERSION_ACCESSOR,
        FieldInfo.newRequired(ClientVersionAccessor.VERSION, commonMsgInfos.VERSION),
        FieldInfo.newRequired(ClientVersionAccessor.PLATFORM),
        FieldInfo.newRequired(ClientVersionAccessor.LANGUAGE),
        FieldInfo.newRequired(ClientVersionAccessor.APPLICATION_INFO));

    /** Validation for client information messages. */
    final MessageInfo INFO = new MessageInfo(
        ClientProtocolAccessor.INFO_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(InfoMessageAccessor.CLIENT_VERSION, CLIENT_VERSION),
        FieldInfo.newOptional(InfoMessageAccessor.CONFIG_PARAMETER),
        FieldInfo.newOptional(InfoMessageAccessor.PERFORMANCE_COUNTER),
        FieldInfo.newOptional(InfoMessageAccessor.CLIENT_CONFIG, commonMsgInfos.CLIENT_CONFIG),
        FieldInfo.newOptional(InfoMessageAccessor.SERVER_REGISTRATION_SUMMARY_REQUESTED));

    /** Validation for registration subtrees. */
    final MessageInfo SUBTREE = new MessageInfo(
        ClientProtocolAccessor.REGISTRATION_SUBTREE_ACCESSOR,
        FieldInfo.newOptional(RegistrationSubtreeAccessor.REGISTERED_OBJECT));

    /** Validation for registration sync messages. */
    final MessageInfo REGISTRATION_SYNC = new MessageInfo(
        ClientProtocolAccessor.REGISTRATION_SYNC_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(RegistrationSyncMessageAccessor.SUBTREE, SUBTREE));

    /** Validation for a ClientToServerMessage. */
    final MessageInfo CLIENT_MSG = new MessageInfo(
        ClientProtocolAccessor.CLIENT_TO_SERVER_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(ClientToServerMessageAccessor.HEADER, HEADER),
        FieldInfo.newOptional(ClientToServerMessageAccessor.INFO_MESSAGE, INFO),
        FieldInfo.newOptional(ClientToServerMessageAccessor.INITIALIZE_MESSAGE, INITIALIZE_MESSAGE),
        FieldInfo.newOptional(ClientToServerMessageAccessor.INVALIDATION_ACK_MESSAGE,
            commonMsgInfos.INVALIDATION_MSG),
        FieldInfo.newOptional(ClientToServerMessageAccessor.REGISTRATION_MESSAGE, REGISTRATION),
        FieldInfo.newOptional(ClientToServerMessageAccessor.REGISTRATION_SYNC_MESSAGE,
            REGISTRATION_SYNC)) {
      @Override
      public boolean postValidate(MessageLite message) {
        ClientToServerMessage parsedMessage = (ClientToServerMessage) message;
        // The message either has an initialize request from the client or it has the client token.
        return (parsedMessage.hasInitializeMessage() ^ parsedMessage.getHeader().hasClientToken());
      }
    };
  }

  /** Describes how to validate server messages. */
  
  class ServerMsgInfos {
    /** Validation for server headers. */
    final MessageInfo HEADER = new MessageInfo(
        ClientProtocolAccessor.SERVER_HEADER_ACCESSOR,
        FieldInfo.newRequired(ServerHeaderAccessor.PROTOCOL_VERSION,
            commonMsgInfos.PROTOCOL_VERSION),
        FieldInfo.newRequired(ServerHeaderAccessor.CLIENT_TOKEN),
        FieldInfo.newOptional(ServerHeaderAccessor.REGISTRATION_SUMMARY,
            commonMsgInfos.REGISTRATION_SUMMARY),
        FieldInfo.newRequired(ServerHeaderAccessor.SERVER_TIME_MS),
        FieldInfo.newOptional(ServerHeaderAccessor.MESSAGE_ID)) {
      @Override
      public boolean postValidate(MessageLite message) {
        ServerHeader header = (ServerHeader) message;
        if (header.getClientToken().isEmpty()) {
          logger.info("Client token was empty: %s", header);
          return false;
        }
        if (header.getServerTimeMs() < 0) {
          logger.info("Server time was negative: %s", header);
          return false;
        }
        // If set, message id must not be empty.
        // Do not use String.isEmpty() here for Froyo (JDK5) compatibility.
        if (header.hasMessageId() && (header.getMessageId().length() == 0)) {
          logger.info("Message id was set but empty: %s", header);
          return false;
        }
        return true;
      }
    };

    /** Validation for server response codes. */
    final MessageInfo STATUSP = new MessageInfo(
        ClientProtocolAccessor.STATUS_P_ACCESSOR,
        FieldInfo.newRequired(StatusPAccessor.CODE),
        FieldInfo.newOptional(StatusPAccessor.DESCRIPTION));

    /** Validation for token control messages. */
    final MessageInfo TOKEN_CONTROL = new MessageInfo(
        ClientProtocolAccessor.TOKEN_CONTROL_MESSAGE_ACCESSOR,
        FieldInfo.newOptional(TokenControlMessageAccessor.NEW_TOKEN));

    /** Validation for error messages. */
    final MessageInfo ERROR = new MessageInfo(
        ClientProtocolAccessor.ERROR_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(ErrorMessageAccessor.CODE),
        FieldInfo.newRequired(ErrorMessageAccessor.DESCRIPTION));

    /** Validation for registration results. */
    final MessageInfo REGISTRATION_RESULT = new MessageInfo(
        ClientProtocolAccessor.REGISTRATION_STATUS_ACCESSOR,
        FieldInfo.newRequired(RegistrationStatusAccessor.REGISTRATION,
            commonMsgInfos.REGISTRATIONP),
        FieldInfo.newRequired(RegistrationStatusAccessor.STATUS, STATUSP));

    /** Validation for registration status messages. */
    final MessageInfo REGISTRATION_STATUS_MSG = new MessageInfo(
        ClientProtocolAccessor.REGISTRATION_STATUS_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(RegistrationStatusMessageAccessor.REGISTRATION_STATUS,
            REGISTRATION_RESULT));

    /** Validation for registration sync requests. */
    final MessageInfo REGISTRATION_SYNC_REQUEST = new MessageInfo(
        ClientProtocolAccessor.REGISTRATION_SYNC_REQUEST_MESSAGE_ACCESSOR);

    /** Validation for info requests. */
    final MessageInfo INFO_REQUEST = new MessageInfo(
        ClientProtocolAccessor.INFO_REQUEST_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(InfoRequestMessageAccessor.INFO_TYPE));

    /** Validation for config change message. */
    final MessageInfo CONFIG_CHANGE = new MessageInfo(
        ClientProtocolAccessor.CONFIG_CHANGE_MESSAGE_ACCESSOR,
        FieldInfo.newOptional(ConfigChangeMessageAccessor.NEXT_MESSAGE_DELAY_MS)) {
          @Override
          public boolean postValidate(MessageLite message) {
            ConfigChangeMessage parsedMessage = (ConfigChangeMessage) message;
            // If the message has a next_message_delay_ms value, it must be positive.
            return !parsedMessage.hasNextMessageDelayMs() ||
                (parsedMessage.getNextMessageDelayMs() > 0);
          }
        };

    /** Validation for the top-level server messages. */
    final MessageInfo SERVER_MSG = new MessageInfo(
        ClientProtocolAccessor.SERVER_TO_CLIENT_MESSAGE_ACCESSOR,
        FieldInfo.newRequired(ServerToClientMessageAccessor.HEADER, HEADER),
        FieldInfo.newOptional(ServerToClientMessageAccessor.TOKEN_CONTROL_MESSAGE, TOKEN_CONTROL),
        FieldInfo.newOptional(ServerToClientMessageAccessor.INVALIDATION_MESSAGE,
            commonMsgInfos.INVALIDATION_MSG),
        FieldInfo.newOptional(ServerToClientMessageAccessor.REGISTRATION_STATUS_MESSAGE,
            REGISTRATION_STATUS_MSG),
        FieldInfo.newOptional(ServerToClientMessageAccessor.REGISTRATION_SYNC_REQUEST_MESSAGE,
            REGISTRATION_SYNC_REQUEST),
        FieldInfo.newOptional(ServerToClientMessageAccessor.CONFIG_CHANGE_MESSAGE, CONFIG_CHANGE),
        FieldInfo.newOptional(ServerToClientMessageAccessor.INFO_REQUEST_MESSAGE, INFO_REQUEST),
        FieldInfo.newOptional(ServerToClientMessageAccessor.ERROR_MESSAGE, ERROR));
  }

  /** Common validation information */
  
  final CommonMsgInfos commonMsgInfos = new CommonMsgInfos();

  /** Client validation information */
  private final ClientMsgInfos clientMsgInfos = new ClientMsgInfos();

  /** Server validation information */
  
  final ServerMsgInfos serverMsgInfos = new ServerMsgInfos();

  /** Returns whether {@code clientMessage} is valid. */
  public boolean isValid(ClientToServerMessage clientMessage) {
    return checkMessage(clientMessage, clientMsgInfos.CLIENT_MSG);
  }

  /** Returns whether {@code serverMessage} is valid. */
  public boolean isValid(ServerToClientMessage serverMessage) {
    return checkMessage(serverMessage, serverMsgInfos.SERVER_MSG);
  }

  /** Returns whether {@code invalidation} is valid. */
  public boolean isValid(InvalidationP invalidation) {
    return checkMessage(invalidation, commonMsgInfos.INVALIDATION);
  }

  /** Returns whether {@code version} is valid. */
  public boolean isValid(Version version) {
    return checkMessage(version, commonMsgInfos.VERSION);
  }

  /** Returns the {@code MessageInfo} for a {@link ServerToClientMessage}. */
  public MessageInfo getServerToClientMessageInfo() {
    return serverMsgInfos.SERVER_MSG;
  }
}
