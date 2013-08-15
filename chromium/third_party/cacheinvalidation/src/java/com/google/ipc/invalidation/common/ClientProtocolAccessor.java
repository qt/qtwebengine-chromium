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
// GENERATED CODE. DO NOT EDIT. (But isn't it pretty?)
package com.google.ipc.invalidation.common;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.common.ProtoValidator.Accessor;

import com.google.ipc.invalidation.common.ProtoValidator.Descriptor;

import com.google.protobuf.MessageLite;

import com.google.protos.ipc.invalidation.ClientProtocol.ApplicationClientIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientConfigP;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientHeader;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientToServerMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientVersion;
import com.google.protos.ipc.invalidation.ClientProtocol.ConfigChangeMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.ErrorMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InfoMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InfoRequestMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InitializeMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.PropertyRecord;
import com.google.protos.ipc.invalidation.ClientProtocol.ProtocolHandlerConfigP;
import com.google.protos.ipc.invalidation.ClientProtocol.ProtocolVersion;
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

import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/** Class providing access to fields of protocol buffers in a generic way without using Java reflection. */
public class ClientProtocolAccessor {
  /** Class to access fields in {@link ApplicationClientIdP} protos. */
  public static class ApplicationClientIdPAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "client_type",
        "client_name"
      ));
    
    public static final Descriptor CLIENT_TYPE = new Descriptor("client_type");
    public static final Descriptor CLIENT_NAME = new Descriptor("client_name");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ApplicationClientIdP message = (ApplicationClientIdP) rawMessage;
      if (field == CLIENT_TYPE) {
        return message.hasClientType();
      }
      if (field == CLIENT_NAME) {
        return message.hasClientName();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ApplicationClientIdP message = (ApplicationClientIdP) rawMessage;
      if (field == CLIENT_TYPE) {
        return message.getClientType();
      }
      if (field == CLIENT_NAME) {
        return message.getClientName();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ApplicationClientIdPAccessor APPLICATION_CLIENT_ID_P_ACCESSOR = new ApplicationClientIdPAccessor();
  
  /** Class to access fields in {@link ClientConfigP} protos. */
  public static class ClientConfigPAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "version",
        "network_timeout_delay_ms",
        "write_retry_delay_ms",
        "heartbeat_interval_ms",
        "perf_counter_delay_ms",
        "max_exponential_backoff_factor",
        "smear_percent",
        "is_transient",
        "initial_persistent_heartbeat_delay_ms",
        "protocol_handler_config",
        "channel_supports_offline_delivery",
        "offline_heartbeat_threshold_ms",
        "allow_suppression"
      ));
    
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor NETWORK_TIMEOUT_DELAY_MS = new Descriptor("network_timeout_delay_ms");
    public static final Descriptor WRITE_RETRY_DELAY_MS = new Descriptor("write_retry_delay_ms");
    public static final Descriptor HEARTBEAT_INTERVAL_MS = new Descriptor("heartbeat_interval_ms");
    public static final Descriptor PERF_COUNTER_DELAY_MS = new Descriptor("perf_counter_delay_ms");
    public static final Descriptor MAX_EXPONENTIAL_BACKOFF_FACTOR = new Descriptor("max_exponential_backoff_factor");
    public static final Descriptor SMEAR_PERCENT = new Descriptor("smear_percent");
    public static final Descriptor IS_TRANSIENT = new Descriptor("is_transient");
    public static final Descriptor INITIAL_PERSISTENT_HEARTBEAT_DELAY_MS = new Descriptor("initial_persistent_heartbeat_delay_ms");
    public static final Descriptor PROTOCOL_HANDLER_CONFIG = new Descriptor("protocol_handler_config");
    public static final Descriptor CHANNEL_SUPPORTS_OFFLINE_DELIVERY = new Descriptor("channel_supports_offline_delivery");
    public static final Descriptor OFFLINE_HEARTBEAT_THRESHOLD_MS = new Descriptor("offline_heartbeat_threshold_ms");
    public static final Descriptor ALLOW_SUPPRESSION = new Descriptor("allow_suppression");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientConfigP message = (ClientConfigP) rawMessage;
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == NETWORK_TIMEOUT_DELAY_MS) {
        return message.hasNetworkTimeoutDelayMs();
      }
      if (field == WRITE_RETRY_DELAY_MS) {
        return message.hasWriteRetryDelayMs();
      }
      if (field == HEARTBEAT_INTERVAL_MS) {
        return message.hasHeartbeatIntervalMs();
      }
      if (field == PERF_COUNTER_DELAY_MS) {
        return message.hasPerfCounterDelayMs();
      }
      if (field == MAX_EXPONENTIAL_BACKOFF_FACTOR) {
        return message.hasMaxExponentialBackoffFactor();
      }
      if (field == SMEAR_PERCENT) {
        return message.hasSmearPercent();
      }
      if (field == IS_TRANSIENT) {
        return message.hasIsTransient();
      }
      if (field == INITIAL_PERSISTENT_HEARTBEAT_DELAY_MS) {
        return message.hasInitialPersistentHeartbeatDelayMs();
      }
      if (field == PROTOCOL_HANDLER_CONFIG) {
        return message.hasProtocolHandlerConfig();
      }
      if (field == CHANNEL_SUPPORTS_OFFLINE_DELIVERY) {
        return message.hasChannelSupportsOfflineDelivery();
      }
      if (field == OFFLINE_HEARTBEAT_THRESHOLD_MS) {
        return message.hasOfflineHeartbeatThresholdMs();
      }
      if (field == ALLOW_SUPPRESSION) {
        return message.hasAllowSuppression();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientConfigP message = (ClientConfigP) rawMessage;
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == NETWORK_TIMEOUT_DELAY_MS) {
        return message.getNetworkTimeoutDelayMs();
      }
      if (field == WRITE_RETRY_DELAY_MS) {
        return message.getWriteRetryDelayMs();
      }
      if (field == HEARTBEAT_INTERVAL_MS) {
        return message.getHeartbeatIntervalMs();
      }
      if (field == PERF_COUNTER_DELAY_MS) {
        return message.getPerfCounterDelayMs();
      }
      if (field == MAX_EXPONENTIAL_BACKOFF_FACTOR) {
        return message.getMaxExponentialBackoffFactor();
      }
      if (field == SMEAR_PERCENT) {
        return message.getSmearPercent();
      }
      if (field == IS_TRANSIENT) {
        return message.getIsTransient();
      }
      if (field == INITIAL_PERSISTENT_HEARTBEAT_DELAY_MS) {
        return message.getInitialPersistentHeartbeatDelayMs();
      }
      if (field == PROTOCOL_HANDLER_CONFIG) {
        return message.getProtocolHandlerConfig();
      }
      if (field == CHANNEL_SUPPORTS_OFFLINE_DELIVERY) {
        return message.getChannelSupportsOfflineDelivery();
      }
      if (field == OFFLINE_HEARTBEAT_THRESHOLD_MS) {
        return message.getOfflineHeartbeatThresholdMs();
      }
      if (field == ALLOW_SUPPRESSION) {
        return message.getAllowSuppression();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ClientConfigPAccessor CLIENT_CONFIG_P_ACCESSOR = new ClientConfigPAccessor();
  
  /** Class to access fields in {@link ClientHeader} protos. */
  public static class ClientHeaderAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "protocol_version",
        "client_token",
        "registration_summary",
        "client_time_ms",
        "max_known_server_time_ms",
        "message_id",
        "client_type"
      ));
    
    public static final Descriptor PROTOCOL_VERSION = new Descriptor("protocol_version");
    public static final Descriptor CLIENT_TOKEN = new Descriptor("client_token");
    public static final Descriptor REGISTRATION_SUMMARY = new Descriptor("registration_summary");
    public static final Descriptor CLIENT_TIME_MS = new Descriptor("client_time_ms");
    public static final Descriptor MAX_KNOWN_SERVER_TIME_MS = new Descriptor("max_known_server_time_ms");
    public static final Descriptor MESSAGE_ID = new Descriptor("message_id");
    public static final Descriptor CLIENT_TYPE = new Descriptor("client_type");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientHeader message = (ClientHeader) rawMessage;
      if (field == PROTOCOL_VERSION) {
        return message.hasProtocolVersion();
      }
      if (field == CLIENT_TOKEN) {
        return message.hasClientToken();
      }
      if (field == REGISTRATION_SUMMARY) {
        return message.hasRegistrationSummary();
      }
      if (field == CLIENT_TIME_MS) {
        return message.hasClientTimeMs();
      }
      if (field == MAX_KNOWN_SERVER_TIME_MS) {
        return message.hasMaxKnownServerTimeMs();
      }
      if (field == MESSAGE_ID) {
        return message.hasMessageId();
      }
      if (field == CLIENT_TYPE) {
        return message.hasClientType();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientHeader message = (ClientHeader) rawMessage;
      if (field == PROTOCOL_VERSION) {
        return message.getProtocolVersion();
      }
      if (field == CLIENT_TOKEN) {
        return message.getClientToken();
      }
      if (field == REGISTRATION_SUMMARY) {
        return message.getRegistrationSummary();
      }
      if (field == CLIENT_TIME_MS) {
        return message.getClientTimeMs();
      }
      if (field == MAX_KNOWN_SERVER_TIME_MS) {
        return message.getMaxKnownServerTimeMs();
      }
      if (field == MESSAGE_ID) {
        return message.getMessageId();
      }
      if (field == CLIENT_TYPE) {
        return message.getClientType();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ClientHeaderAccessor CLIENT_HEADER_ACCESSOR = new ClientHeaderAccessor();
  
  /** Class to access fields in {@link ClientToServerMessage} protos. */
  public static class ClientToServerMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "header",
        "initialize_message",
        "registration_message",
        "registration_sync_message",
        "invalidation_ack_message",
        "info_message"
      ));
    
    public static final Descriptor HEADER = new Descriptor("header");
    public static final Descriptor INITIALIZE_MESSAGE = new Descriptor("initialize_message");
    public static final Descriptor REGISTRATION_MESSAGE = new Descriptor("registration_message");
    public static final Descriptor REGISTRATION_SYNC_MESSAGE = new Descriptor("registration_sync_message");
    public static final Descriptor INVALIDATION_ACK_MESSAGE = new Descriptor("invalidation_ack_message");
    public static final Descriptor INFO_MESSAGE = new Descriptor("info_message");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientToServerMessage message = (ClientToServerMessage) rawMessage;
      if (field == HEADER) {
        return message.hasHeader();
      }
      if (field == INITIALIZE_MESSAGE) {
        return message.hasInitializeMessage();
      }
      if (field == REGISTRATION_MESSAGE) {
        return message.hasRegistrationMessage();
      }
      if (field == REGISTRATION_SYNC_MESSAGE) {
        return message.hasRegistrationSyncMessage();
      }
      if (field == INVALIDATION_ACK_MESSAGE) {
        return message.hasInvalidationAckMessage();
      }
      if (field == INFO_MESSAGE) {
        return message.hasInfoMessage();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientToServerMessage message = (ClientToServerMessage) rawMessage;
      if (field == HEADER) {
        return message.getHeader();
      }
      if (field == INITIALIZE_MESSAGE) {
        return message.getInitializeMessage();
      }
      if (field == REGISTRATION_MESSAGE) {
        return message.getRegistrationMessage();
      }
      if (field == REGISTRATION_SYNC_MESSAGE) {
        return message.getRegistrationSyncMessage();
      }
      if (field == INVALIDATION_ACK_MESSAGE) {
        return message.getInvalidationAckMessage();
      }
      if (field == INFO_MESSAGE) {
        return message.getInfoMessage();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ClientToServerMessageAccessor CLIENT_TO_SERVER_MESSAGE_ACCESSOR = new ClientToServerMessageAccessor();
  
  /** Class to access fields in {@link ClientVersion} protos. */
  public static class ClientVersionAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "version",
        "platform",
        "language",
        "application_info"
      ));
    
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor PLATFORM = new Descriptor("platform");
    public static final Descriptor LANGUAGE = new Descriptor("language");
    public static final Descriptor APPLICATION_INFO = new Descriptor("application_info");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientVersion message = (ClientVersion) rawMessage;
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == PLATFORM) {
        return message.hasPlatform();
      }
      if (field == LANGUAGE) {
        return message.hasLanguage();
      }
      if (field == APPLICATION_INFO) {
        return message.hasApplicationInfo();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientVersion message = (ClientVersion) rawMessage;
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == PLATFORM) {
        return message.getPlatform();
      }
      if (field == LANGUAGE) {
        return message.getLanguage();
      }
      if (field == APPLICATION_INFO) {
        return message.getApplicationInfo();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ClientVersionAccessor CLIENT_VERSION_ACCESSOR = new ClientVersionAccessor();
  
  /** Class to access fields in {@link ConfigChangeMessage} protos. */
  public static class ConfigChangeMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "next_message_delay_ms"
      ));
    
    public static final Descriptor NEXT_MESSAGE_DELAY_MS = new Descriptor("next_message_delay_ms");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ConfigChangeMessage message = (ConfigChangeMessage) rawMessage;
      if (field == NEXT_MESSAGE_DELAY_MS) {
        return message.hasNextMessageDelayMs();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ConfigChangeMessage message = (ConfigChangeMessage) rawMessage;
      if (field == NEXT_MESSAGE_DELAY_MS) {
        return message.getNextMessageDelayMs();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ConfigChangeMessageAccessor CONFIG_CHANGE_MESSAGE_ACCESSOR = new ConfigChangeMessageAccessor();
  
  /** Class to access fields in {@link ErrorMessage} protos. */
  public static class ErrorMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "code",
        "description"
      ));
    
    public static final Descriptor CODE = new Descriptor("code");
    public static final Descriptor DESCRIPTION = new Descriptor("description");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ErrorMessage message = (ErrorMessage) rawMessage;
      if (field == CODE) {
        return message.hasCode();
      }
      if (field == DESCRIPTION) {
        return message.hasDescription();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ErrorMessage message = (ErrorMessage) rawMessage;
      if (field == CODE) {
        return message.getCode();
      }
      if (field == DESCRIPTION) {
        return message.getDescription();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ErrorMessageAccessor ERROR_MESSAGE_ACCESSOR = new ErrorMessageAccessor();
  
  /** Class to access fields in {@link InfoMessage} protos. */
  public static class InfoMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "client_version",
        "config_parameter",
        "performance_counter",
        "server_registration_summary_requested",
        "client_config"
      ));
    
    public static final Descriptor CLIENT_VERSION = new Descriptor("client_version");
    public static final Descriptor CONFIG_PARAMETER = new Descriptor("config_parameter");
    public static final Descriptor PERFORMANCE_COUNTER = new Descriptor("performance_counter");
    public static final Descriptor SERVER_REGISTRATION_SUMMARY_REQUESTED = new Descriptor("server_registration_summary_requested");
    public static final Descriptor CLIENT_CONFIG = new Descriptor("client_config");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InfoMessage message = (InfoMessage) rawMessage;
      if (field == CLIENT_VERSION) {
        return message.hasClientVersion();
      }
      if (field == CONFIG_PARAMETER) {
        return message.getConfigParameterCount() > 0;
      }
      if (field == PERFORMANCE_COUNTER) {
        return message.getPerformanceCounterCount() > 0;
      }
      if (field == SERVER_REGISTRATION_SUMMARY_REQUESTED) {
        return message.hasServerRegistrationSummaryRequested();
      }
      if (field == CLIENT_CONFIG) {
        return message.hasClientConfig();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InfoMessage message = (InfoMessage) rawMessage;
      if (field == CLIENT_VERSION) {
        return message.getClientVersion();
      }
      if (field == CONFIG_PARAMETER) {
        return message.getConfigParameterList();
      }
      if (field == PERFORMANCE_COUNTER) {
        return message.getPerformanceCounterList();
      }
      if (field == SERVER_REGISTRATION_SUMMARY_REQUESTED) {
        return message.getServerRegistrationSummaryRequested();
      }
      if (field == CLIENT_CONFIG) {
        return message.getClientConfig();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final InfoMessageAccessor INFO_MESSAGE_ACCESSOR = new InfoMessageAccessor();
  
  /** Class to access fields in {@link InfoRequestMessage} protos. */
  public static class InfoRequestMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "info_type"
      ));
    
    public static final Descriptor INFO_TYPE = new Descriptor("info_type");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InfoRequestMessage message = (InfoRequestMessage) rawMessage;
      if (field == INFO_TYPE) {
        return message.getInfoTypeCount() > 0;
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InfoRequestMessage message = (InfoRequestMessage) rawMessage;
      if (field == INFO_TYPE) {
        return message.getInfoTypeList();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final InfoRequestMessageAccessor INFO_REQUEST_MESSAGE_ACCESSOR = new InfoRequestMessageAccessor();
  
  /** Class to access fields in {@link InitializeMessage} protos. */
  public static class InitializeMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "client_type",
        "nonce",
        "application_client_id",
        "digest_serialization_type"
      ));
    
    public static final Descriptor CLIENT_TYPE = new Descriptor("client_type");
    public static final Descriptor NONCE = new Descriptor("nonce");
    public static final Descriptor APPLICATION_CLIENT_ID = new Descriptor("application_client_id");
    public static final Descriptor DIGEST_SERIALIZATION_TYPE = new Descriptor("digest_serialization_type");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InitializeMessage message = (InitializeMessage) rawMessage;
      if (field == CLIENT_TYPE) {
        return message.hasClientType();
      }
      if (field == NONCE) {
        return message.hasNonce();
      }
      if (field == APPLICATION_CLIENT_ID) {
        return message.hasApplicationClientId();
      }
      if (field == DIGEST_SERIALIZATION_TYPE) {
        return message.hasDigestSerializationType();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InitializeMessage message = (InitializeMessage) rawMessage;
      if (field == CLIENT_TYPE) {
        return message.getClientType();
      }
      if (field == NONCE) {
        return message.getNonce();
      }
      if (field == APPLICATION_CLIENT_ID) {
        return message.getApplicationClientId();
      }
      if (field == DIGEST_SERIALIZATION_TYPE) {
        return message.getDigestSerializationType();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final InitializeMessageAccessor INITIALIZE_MESSAGE_ACCESSOR = new InitializeMessageAccessor();
  
  /** Class to access fields in {@link InvalidationMessage} protos. */
  public static class InvalidationMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "invalidation"
      ));
    
    public static final Descriptor INVALIDATION = new Descriptor("invalidation");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InvalidationMessage message = (InvalidationMessage) rawMessage;
      if (field == INVALIDATION) {
        return message.getInvalidationCount() > 0;
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InvalidationMessage message = (InvalidationMessage) rawMessage;
      if (field == INVALIDATION) {
        return message.getInvalidationList();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final InvalidationMessageAccessor INVALIDATION_MESSAGE_ACCESSOR = new InvalidationMessageAccessor();
  
  /** Class to access fields in {@link InvalidationP} protos. */
  public static class InvalidationPAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "object_id",
        "is_known_version",
        "version",
        "is_trickle_restart",
        "payload",
        "bridge_arrival_time_ms_deprecated"
      ));
    
    public static final Descriptor OBJECT_ID = new Descriptor("object_id");
    public static final Descriptor IS_KNOWN_VERSION = new Descriptor("is_known_version");
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor IS_TRICKLE_RESTART = new Descriptor("is_trickle_restart");
    public static final Descriptor PAYLOAD = new Descriptor("payload");
    public static final Descriptor BRIDGE_ARRIVAL_TIME_MS_DEPRECATED = new Descriptor("bridge_arrival_time_ms_deprecated");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings({ "deprecation", "unchecked" })
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InvalidationP message = (InvalidationP) rawMessage;
      if (field == OBJECT_ID) {
        return message.hasObjectId();
      }
      if (field == IS_KNOWN_VERSION) {
        return message.hasIsKnownVersion();
      }
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == IS_TRICKLE_RESTART) {
        return message.hasIsTrickleRestart();
      }
      if (field == PAYLOAD) {
        return message.hasPayload();
      }
      if (field == BRIDGE_ARRIVAL_TIME_MS_DEPRECATED) {
        return message.hasBridgeArrivalTimeMsDeprecated();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings({ "deprecation", "unchecked" })
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InvalidationP message = (InvalidationP) rawMessage;
      if (field == OBJECT_ID) {
        return message.getObjectId();
      }
      if (field == IS_KNOWN_VERSION) {
        return message.getIsKnownVersion();
      }
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == IS_TRICKLE_RESTART) {
        return message.getIsTrickleRestart();
      }
      if (field == PAYLOAD) {
        return message.getPayload();
      }
      if (field == BRIDGE_ARRIVAL_TIME_MS_DEPRECATED) {
        return message.getBridgeArrivalTimeMsDeprecated();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final InvalidationPAccessor INVALIDATION_P_ACCESSOR = new InvalidationPAccessor();
  
  /** Class to access fields in {@link ObjectIdP} protos. */
  public static class ObjectIdPAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "source",
        "name"
      ));
    
    public static final Descriptor SOURCE = new Descriptor("source");
    public static final Descriptor NAME = new Descriptor("name");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ObjectIdP message = (ObjectIdP) rawMessage;
      if (field == SOURCE) {
        return message.hasSource();
      }
      if (field == NAME) {
        return message.hasName();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ObjectIdP message = (ObjectIdP) rawMessage;
      if (field == SOURCE) {
        return message.getSource();
      }
      if (field == NAME) {
        return message.getName();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ObjectIdPAccessor OBJECT_ID_P_ACCESSOR = new ObjectIdPAccessor();
  
  /** Class to access fields in {@link PropertyRecord} protos. */
  public static class PropertyRecordAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "name",
        "value"
      ));
    
    public static final Descriptor NAME = new Descriptor("name");
    public static final Descriptor VALUE = new Descriptor("value");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      PropertyRecord message = (PropertyRecord) rawMessage;
      if (field == NAME) {
        return message.hasName();
      }
      if (field == VALUE) {
        return message.hasValue();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      PropertyRecord message = (PropertyRecord) rawMessage;
      if (field == NAME) {
        return message.getName();
      }
      if (field == VALUE) {
        return message.getValue();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final PropertyRecordAccessor PROPERTY_RECORD_ACCESSOR = new PropertyRecordAccessor();
  
  /** Class to access fields in {@link ProtocolHandlerConfigP} protos. */
  public static class ProtocolHandlerConfigPAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "batching_delay_ms",
        "rate_limit"
      ));
    
    public static final Descriptor BATCHING_DELAY_MS = new Descriptor("batching_delay_ms");
    public static final Descriptor RATE_LIMIT = new Descriptor("rate_limit");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ProtocolHandlerConfigP message = (ProtocolHandlerConfigP) rawMessage;
      if (field == BATCHING_DELAY_MS) {
        return message.hasBatchingDelayMs();
      }
      if (field == RATE_LIMIT) {
        return message.getRateLimitCount() > 0;
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ProtocolHandlerConfigP message = (ProtocolHandlerConfigP) rawMessage;
      if (field == BATCHING_DELAY_MS) {
        return message.getBatchingDelayMs();
      }
      if (field == RATE_LIMIT) {
        return message.getRateLimitList();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ProtocolHandlerConfigPAccessor PROTOCOL_HANDLER_CONFIG_P_ACCESSOR = new ProtocolHandlerConfigPAccessor();
  
  /** Class to access fields in {@link ProtocolVersion} protos. */
  public static class ProtocolVersionAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "version"
      ));
    
    public static final Descriptor VERSION = new Descriptor("version");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ProtocolVersion message = (ProtocolVersion) rawMessage;
      if (field == VERSION) {
        return message.hasVersion();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ProtocolVersion message = (ProtocolVersion) rawMessage;
      if (field == VERSION) {
        return message.getVersion();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ProtocolVersionAccessor PROTOCOL_VERSION_ACCESSOR = new ProtocolVersionAccessor();
  
  /** Class to access fields in {@link RateLimitP} protos. */
  public static class RateLimitPAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "window_ms",
        "count"
      ));
    
    public static final Descriptor WINDOW_MS = new Descriptor("window_ms");
    public static final Descriptor COUNT = new Descriptor("count");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RateLimitP message = (RateLimitP) rawMessage;
      if (field == WINDOW_MS) {
        return message.hasWindowMs();
      }
      if (field == COUNT) {
        return message.hasCount();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RateLimitP message = (RateLimitP) rawMessage;
      if (field == WINDOW_MS) {
        return message.getWindowMs();
      }
      if (field == COUNT) {
        return message.getCount();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RateLimitPAccessor RATE_LIMIT_P_ACCESSOR = new RateLimitPAccessor();
  
  /** Class to access fields in {@link RegistrationMessage} protos. */
  public static class RegistrationMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "registration"
      ));
    
    public static final Descriptor REGISTRATION = new Descriptor("registration");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationMessage message = (RegistrationMessage) rawMessage;
      if (field == REGISTRATION) {
        return message.getRegistrationCount() > 0;
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationMessage message = (RegistrationMessage) rawMessage;
      if (field == REGISTRATION) {
        return message.getRegistrationList();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RegistrationMessageAccessor REGISTRATION_MESSAGE_ACCESSOR = new RegistrationMessageAccessor();
  
  /** Class to access fields in {@link RegistrationP} protos. */
  public static class RegistrationPAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "object_id",
        "op_type"
      ));
    
    public static final Descriptor OBJECT_ID = new Descriptor("object_id");
    public static final Descriptor OP_TYPE = new Descriptor("op_type");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationP message = (RegistrationP) rawMessage;
      if (field == OBJECT_ID) {
        return message.hasObjectId();
      }
      if (field == OP_TYPE) {
        return message.hasOpType();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationP message = (RegistrationP) rawMessage;
      if (field == OBJECT_ID) {
        return message.getObjectId();
      }
      if (field == OP_TYPE) {
        return message.getOpType();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RegistrationPAccessor REGISTRATION_P_ACCESSOR = new RegistrationPAccessor();
  
  /** Class to access fields in {@link RegistrationStatus} protos. */
  public static class RegistrationStatusAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "registration",
        "status"
      ));
    
    public static final Descriptor REGISTRATION = new Descriptor("registration");
    public static final Descriptor STATUS = new Descriptor("status");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationStatus message = (RegistrationStatus) rawMessage;
      if (field == REGISTRATION) {
        return message.hasRegistration();
      }
      if (field == STATUS) {
        return message.hasStatus();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationStatus message = (RegistrationStatus) rawMessage;
      if (field == REGISTRATION) {
        return message.getRegistration();
      }
      if (field == STATUS) {
        return message.getStatus();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RegistrationStatusAccessor REGISTRATION_STATUS_ACCESSOR = new RegistrationStatusAccessor();
  
  /** Class to access fields in {@link RegistrationStatusMessage} protos. */
  public static class RegistrationStatusMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "registration_status"
      ));
    
    public static final Descriptor REGISTRATION_STATUS = new Descriptor("registration_status");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationStatusMessage message = (RegistrationStatusMessage) rawMessage;
      if (field == REGISTRATION_STATUS) {
        return message.getRegistrationStatusCount() > 0;
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationStatusMessage message = (RegistrationStatusMessage) rawMessage;
      if (field == REGISTRATION_STATUS) {
        return message.getRegistrationStatusList();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RegistrationStatusMessageAccessor REGISTRATION_STATUS_MESSAGE_ACCESSOR = new RegistrationStatusMessageAccessor();
  
  /** Class to access fields in {@link RegistrationSubtree} protos. */
  public static class RegistrationSubtreeAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "registered_object"
      ));
    
    public static final Descriptor REGISTERED_OBJECT = new Descriptor("registered_object");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationSubtree message = (RegistrationSubtree) rawMessage;
      if (field == REGISTERED_OBJECT) {
        return message.getRegisteredObjectCount() > 0;
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationSubtree message = (RegistrationSubtree) rawMessage;
      if (field == REGISTERED_OBJECT) {
        return message.getRegisteredObjectList();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RegistrationSubtreeAccessor REGISTRATION_SUBTREE_ACCESSOR = new RegistrationSubtreeAccessor();
  
  /** Class to access fields in {@link RegistrationSummary} protos. */
  public static class RegistrationSummaryAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "num_registrations",
        "registration_digest"
      ));
    
    public static final Descriptor NUM_REGISTRATIONS = new Descriptor("num_registrations");
    public static final Descriptor REGISTRATION_DIGEST = new Descriptor("registration_digest");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationSummary message = (RegistrationSummary) rawMessage;
      if (field == NUM_REGISTRATIONS) {
        return message.hasNumRegistrations();
      }
      if (field == REGISTRATION_DIGEST) {
        return message.hasRegistrationDigest();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationSummary message = (RegistrationSummary) rawMessage;
      if (field == NUM_REGISTRATIONS) {
        return message.getNumRegistrations();
      }
      if (field == REGISTRATION_DIGEST) {
        return message.getRegistrationDigest();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RegistrationSummaryAccessor REGISTRATION_SUMMARY_ACCESSOR = new RegistrationSummaryAccessor();
  
  /** Class to access fields in {@link RegistrationSyncMessage} protos. */
  public static class RegistrationSyncMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "subtree"
      ));
    
    public static final Descriptor SUBTREE = new Descriptor("subtree");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationSyncMessage message = (RegistrationSyncMessage) rawMessage;
      if (field == SUBTREE) {
        return message.getSubtreeCount() > 0;
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationSyncMessage message = (RegistrationSyncMessage) rawMessage;
      if (field == SUBTREE) {
        return message.getSubtreeList();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RegistrationSyncMessageAccessor REGISTRATION_SYNC_MESSAGE_ACCESSOR = new RegistrationSyncMessageAccessor();
  
  /** Class to access fields in {@link RegistrationSyncRequestMessage} protos. */
  public static class RegistrationSyncRequestMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
      ));
    
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationSyncRequestMessage message = (RegistrationSyncRequestMessage) rawMessage;
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      RegistrationSyncRequestMessage message = (RegistrationSyncRequestMessage) rawMessage;
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final RegistrationSyncRequestMessageAccessor REGISTRATION_SYNC_REQUEST_MESSAGE_ACCESSOR = new RegistrationSyncRequestMessageAccessor();
  
  /** Class to access fields in {@link ServerHeader} protos. */
  public static class ServerHeaderAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "protocol_version",
        "client_token",
        "registration_summary",
        "server_time_ms",
        "message_id"
      ));
    
    public static final Descriptor PROTOCOL_VERSION = new Descriptor("protocol_version");
    public static final Descriptor CLIENT_TOKEN = new Descriptor("client_token");
    public static final Descriptor REGISTRATION_SUMMARY = new Descriptor("registration_summary");
    public static final Descriptor SERVER_TIME_MS = new Descriptor("server_time_ms");
    public static final Descriptor MESSAGE_ID = new Descriptor("message_id");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ServerHeader message = (ServerHeader) rawMessage;
      if (field == PROTOCOL_VERSION) {
        return message.hasProtocolVersion();
      }
      if (field == CLIENT_TOKEN) {
        return message.hasClientToken();
      }
      if (field == REGISTRATION_SUMMARY) {
        return message.hasRegistrationSummary();
      }
      if (field == SERVER_TIME_MS) {
        return message.hasServerTimeMs();
      }
      if (field == MESSAGE_ID) {
        return message.hasMessageId();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ServerHeader message = (ServerHeader) rawMessage;
      if (field == PROTOCOL_VERSION) {
        return message.getProtocolVersion();
      }
      if (field == CLIENT_TOKEN) {
        return message.getClientToken();
      }
      if (field == REGISTRATION_SUMMARY) {
        return message.getRegistrationSummary();
      }
      if (field == SERVER_TIME_MS) {
        return message.getServerTimeMs();
      }
      if (field == MESSAGE_ID) {
        return message.getMessageId();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ServerHeaderAccessor SERVER_HEADER_ACCESSOR = new ServerHeaderAccessor();
  
  /** Class to access fields in {@link ServerToClientMessage} protos. */
  public static class ServerToClientMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "header",
        "token_control_message",
        "invalidation_message",
        "registration_status_message",
        "registration_sync_request_message",
        "config_change_message",
        "info_request_message",
        "error_message"
      ));
    
    public static final Descriptor HEADER = new Descriptor("header");
    public static final Descriptor TOKEN_CONTROL_MESSAGE = new Descriptor("token_control_message");
    public static final Descriptor INVALIDATION_MESSAGE = new Descriptor("invalidation_message");
    public static final Descriptor REGISTRATION_STATUS_MESSAGE = new Descriptor("registration_status_message");
    public static final Descriptor REGISTRATION_SYNC_REQUEST_MESSAGE = new Descriptor("registration_sync_request_message");
    public static final Descriptor CONFIG_CHANGE_MESSAGE = new Descriptor("config_change_message");
    public static final Descriptor INFO_REQUEST_MESSAGE = new Descriptor("info_request_message");
    public static final Descriptor ERROR_MESSAGE = new Descriptor("error_message");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ServerToClientMessage message = (ServerToClientMessage) rawMessage;
      if (field == HEADER) {
        return message.hasHeader();
      }
      if (field == TOKEN_CONTROL_MESSAGE) {
        return message.hasTokenControlMessage();
      }
      if (field == INVALIDATION_MESSAGE) {
        return message.hasInvalidationMessage();
      }
      if (field == REGISTRATION_STATUS_MESSAGE) {
        return message.hasRegistrationStatusMessage();
      }
      if (field == REGISTRATION_SYNC_REQUEST_MESSAGE) {
        return message.hasRegistrationSyncRequestMessage();
      }
      if (field == CONFIG_CHANGE_MESSAGE) {
        return message.hasConfigChangeMessage();
      }
      if (field == INFO_REQUEST_MESSAGE) {
        return message.hasInfoRequestMessage();
      }
      if (field == ERROR_MESSAGE) {
        return message.hasErrorMessage();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ServerToClientMessage message = (ServerToClientMessage) rawMessage;
      if (field == HEADER) {
        return message.getHeader();
      }
      if (field == TOKEN_CONTROL_MESSAGE) {
        return message.getTokenControlMessage();
      }
      if (field == INVALIDATION_MESSAGE) {
        return message.getInvalidationMessage();
      }
      if (field == REGISTRATION_STATUS_MESSAGE) {
        return message.getRegistrationStatusMessage();
      }
      if (field == REGISTRATION_SYNC_REQUEST_MESSAGE) {
        return message.getRegistrationSyncRequestMessage();
      }
      if (field == CONFIG_CHANGE_MESSAGE) {
        return message.getConfigChangeMessage();
      }
      if (field == INFO_REQUEST_MESSAGE) {
        return message.getInfoRequestMessage();
      }
      if (field == ERROR_MESSAGE) {
        return message.getErrorMessage();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ServerToClientMessageAccessor SERVER_TO_CLIENT_MESSAGE_ACCESSOR = new ServerToClientMessageAccessor();
  
  /** Class to access fields in {@link StatusP} protos. */
  public static class StatusPAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "code",
        "description"
      ));
    
    public static final Descriptor CODE = new Descriptor("code");
    public static final Descriptor DESCRIPTION = new Descriptor("description");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      StatusP message = (StatusP) rawMessage;
      if (field == CODE) {
        return message.hasCode();
      }
      if (field == DESCRIPTION) {
        return message.hasDescription();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      StatusP message = (StatusP) rawMessage;
      if (field == CODE) {
        return message.getCode();
      }
      if (field == DESCRIPTION) {
        return message.getDescription();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final StatusPAccessor STATUS_P_ACCESSOR = new StatusPAccessor();
  
  /** Class to access fields in {@link TokenControlMessage} protos. */
  public static class TokenControlMessageAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "new_token"
      ));
    
    public static final Descriptor NEW_TOKEN = new Descriptor("new_token");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      TokenControlMessage message = (TokenControlMessage) rawMessage;
      if (field == NEW_TOKEN) {
        return message.hasNewToken();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      TokenControlMessage message = (TokenControlMessage) rawMessage;
      if (field == NEW_TOKEN) {
        return message.getNewToken();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final TokenControlMessageAccessor TOKEN_CONTROL_MESSAGE_ACCESSOR = new TokenControlMessageAccessor();
  
  /** Class to access fields in {@link Version} protos. */
  public static class VersionAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "major_version",
        "minor_version"
      ));
    
    public static final Descriptor MAJOR_VERSION = new Descriptor("major_version");
    public static final Descriptor MINOR_VERSION = new Descriptor("minor_version");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      Version message = (Version) rawMessage;
      if (field == MAJOR_VERSION) {
        return message.hasMajorVersion();
      }
      if (field == MINOR_VERSION) {
        return message.hasMinorVersion();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      Version message = (Version) rawMessage;
      if (field == MAJOR_VERSION) {
        return message.getMajorVersion();
      }
      if (field == MINOR_VERSION) {
        return message.getMinorVersion();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final VersionAccessor VERSION_ACCESSOR = new VersionAccessor();
  
}
