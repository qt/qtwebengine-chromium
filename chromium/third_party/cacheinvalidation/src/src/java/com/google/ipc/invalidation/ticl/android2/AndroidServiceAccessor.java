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
package com.google.ipc.invalidation.ticl.android2;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.common.ProtoValidator.Accessor;

import com.google.ipc.invalidation.common.ProtoValidator.Descriptor;

import com.google.protobuf.MessageLite;

import com.google.protos.ipc.invalidation.AndroidService.AndroidNetworkSendRequest;
import com.google.protos.ipc.invalidation.AndroidService.AndroidSchedulerEvent;
import com.google.protos.ipc.invalidation.AndroidService.AndroidTiclState;
import com.google.protos.ipc.invalidation.AndroidService.AndroidTiclState.Metadata;
import com.google.protos.ipc.invalidation.AndroidService.AndroidTiclStateWithDigest;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.RegistrationDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.AckDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.StopDowncall;
import com.google.protos.ipc.invalidation.AndroidService.ClientDowncall.StartDowncall;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.CreateClient;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.NetworkStatus;
import com.google.protos.ipc.invalidation.AndroidService.InternalDowncall.ServerMessage;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.ErrorUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.ReissueRegistrationsUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.RegistrationFailureUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.RegistrationStatusUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.InvalidateUpcall;
import com.google.protos.ipc.invalidation.AndroidService.ListenerUpcall.ReadyUpcall;

import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Set;

/** Class providing access to fields of protocol buffers in a generic way without using Java reflection. */
public class AndroidServiceAccessor {
  /** Class to access fields in {@link AndroidNetworkSendRequest} protos. */
  public static class AndroidNetworkSendRequestAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "version",
        "message"
      ));
    
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor MESSAGE = new Descriptor("message");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      AndroidNetworkSendRequest message = (AndroidNetworkSendRequest) rawMessage;
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == MESSAGE) {
        return message.hasMessage();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      AndroidNetworkSendRequest message = (AndroidNetworkSendRequest) rawMessage;
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == MESSAGE) {
        return message.getMessage();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final AndroidNetworkSendRequestAccessor ANDROID_NETWORK_SEND_REQUEST_ACCESSOR = new AndroidNetworkSendRequestAccessor();
  
  /** Class to access fields in {@link AndroidSchedulerEvent} protos. */
  public static class AndroidSchedulerEventAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "version",
        "event_name",
        "ticl_id"
      ));
    
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor EVENT_NAME = new Descriptor("event_name");
    public static final Descriptor TICL_ID = new Descriptor("ticl_id");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      AndroidSchedulerEvent message = (AndroidSchedulerEvent) rawMessage;
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == EVENT_NAME) {
        return message.hasEventName();
      }
      if (field == TICL_ID) {
        return message.hasTiclId();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      AndroidSchedulerEvent message = (AndroidSchedulerEvent) rawMessage;
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == EVENT_NAME) {
        return message.getEventName();
      }
      if (field == TICL_ID) {
        return message.getTiclId();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final AndroidSchedulerEventAccessor ANDROID_SCHEDULER_EVENT_ACCESSOR = new AndroidSchedulerEventAccessor();
  
  /** Class to access fields in {@link AndroidTiclState} protos. */
  public static class AndroidTiclStateAccessor implements Accessor {
    /** Class to access fields in {@link Metadata} protos. */
    public static class MetadataAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "client_type",
          "client_name",
          "ticl_id",
          "client_config"
        ));
      
      public static final Descriptor CLIENT_TYPE = new Descriptor("client_type");
      public static final Descriptor CLIENT_NAME = new Descriptor("client_name");
      public static final Descriptor TICL_ID = new Descriptor("ticl_id");
      public static final Descriptor CLIENT_CONFIG = new Descriptor("client_config");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        Metadata message = (Metadata) rawMessage;
        if (field == CLIENT_TYPE) {
          return message.hasClientType();
        }
        if (field == CLIENT_NAME) {
          return message.hasClientName();
        }
        if (field == TICL_ID) {
          return message.hasTiclId();
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
        Metadata message = (Metadata) rawMessage;
        if (field == CLIENT_TYPE) {
          return message.getClientType();
        }
        if (field == CLIENT_NAME) {
          return message.getClientName();
        }
        if (field == TICL_ID) {
          return message.getTiclId();
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
    public static final MetadataAccessor METADATA_ACCESSOR = new MetadataAccessor();
    
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "version",
        "ticl_state",
        "metadata"
      ));
    
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor TICL_STATE = new Descriptor("ticl_state");
    public static final Descriptor METADATA = new Descriptor("metadata");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      AndroidTiclState message = (AndroidTiclState) rawMessage;
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == TICL_STATE) {
        return message.hasTiclState();
      }
      if (field == METADATA) {
        return message.hasMetadata();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      AndroidTiclState message = (AndroidTiclState) rawMessage;
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == TICL_STATE) {
        return message.getTiclState();
      }
      if (field == METADATA) {
        return message.getMetadata();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final AndroidTiclStateAccessor ANDROID_TICL_STATE_ACCESSOR = new AndroidTiclStateAccessor();
  
  /** Class to access fields in {@link AndroidTiclStateWithDigest} protos. */
  public static class AndroidTiclStateWithDigestAccessor implements Accessor {
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "state",
        "digest"
      ));
    
    public static final Descriptor STATE = new Descriptor("state");
    public static final Descriptor DIGEST = new Descriptor("digest");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      AndroidTiclStateWithDigest message = (AndroidTiclStateWithDigest) rawMessage;
      if (field == STATE) {
        return message.hasState();
      }
      if (field == DIGEST) {
        return message.hasDigest();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      AndroidTiclStateWithDigest message = (AndroidTiclStateWithDigest) rawMessage;
      if (field == STATE) {
        return message.getState();
      }
      if (field == DIGEST) {
        return message.getDigest();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final AndroidTiclStateWithDigestAccessor ANDROID_TICL_STATE_WITH_DIGEST_ACCESSOR = new AndroidTiclStateWithDigestAccessor();
  
  /** Class to access fields in {@link ClientDowncall} protos. */
  public static class ClientDowncallAccessor implements Accessor {
    /** Class to access fields in {@link RegistrationDowncall} protos. */
    public static class RegistrationDowncallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "registrations",
          "unregistrations"
        ));
      
      public static final Descriptor REGISTRATIONS = new Descriptor("registrations");
      public static final Descriptor UNREGISTRATIONS = new Descriptor("unregistrations");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        RegistrationDowncall message = (RegistrationDowncall) rawMessage;
        if (field == REGISTRATIONS) {
          return message.getRegistrationsCount() > 0;
        }
        if (field == UNREGISTRATIONS) {
          return message.getUnregistrationsCount() > 0;
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        RegistrationDowncall message = (RegistrationDowncall) rawMessage;
        if (field == REGISTRATIONS) {
          return message.getRegistrationsList();
        }
        if (field == UNREGISTRATIONS) {
          return message.getUnregistrationsList();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final RegistrationDowncallAccessor REGISTRATION_DOWNCALL_ACCESSOR = new RegistrationDowncallAccessor();
    
    /** Class to access fields in {@link AckDowncall} protos. */
    public static class AckDowncallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "ack_handle"
        ));
      
      public static final Descriptor ACK_HANDLE = new Descriptor("ack_handle");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        AckDowncall message = (AckDowncall) rawMessage;
        if (field == ACK_HANDLE) {
          return message.hasAckHandle();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        AckDowncall message = (AckDowncall) rawMessage;
        if (field == ACK_HANDLE) {
          return message.getAckHandle();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final AckDowncallAccessor ACK_DOWNCALL_ACCESSOR = new AckDowncallAccessor();
    
    /** Class to access fields in {@link StopDowncall} protos. */
    public static class StopDowncallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
        ));
      
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        StopDowncall message = (StopDowncall) rawMessage;
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        StopDowncall message = (StopDowncall) rawMessage;
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final StopDowncallAccessor STOP_DOWNCALL_ACCESSOR = new StopDowncallAccessor();
    
    /** Class to access fields in {@link StartDowncall} protos. */
    public static class StartDowncallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
        ));
      
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        StartDowncall message = (StartDowncall) rawMessage;
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        StartDowncall message = (StartDowncall) rawMessage;
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final StartDowncallAccessor START_DOWNCALL_ACCESSOR = new StartDowncallAccessor();
    
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "serial",
        "version",
        "start",
        "stop",
        "ack",
        "registrations"
      ));
    
    public static final Descriptor SERIAL = new Descriptor("serial");
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor START = new Descriptor("start");
    public static final Descriptor STOP = new Descriptor("stop");
    public static final Descriptor ACK = new Descriptor("ack");
    public static final Descriptor REGISTRATIONS = new Descriptor("registrations");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientDowncall message = (ClientDowncall) rawMessage;
      if (field == SERIAL) {
        return message.hasSerial();
      }
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == START) {
        return message.hasStart();
      }
      if (field == STOP) {
        return message.hasStop();
      }
      if (field == ACK) {
        return message.hasAck();
      }
      if (field == REGISTRATIONS) {
        return message.hasRegistrations();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ClientDowncall message = (ClientDowncall) rawMessage;
      if (field == SERIAL) {
        return message.getSerial();
      }
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == START) {
        return message.getStart();
      }
      if (field == STOP) {
        return message.getStop();
      }
      if (field == ACK) {
        return message.getAck();
      }
      if (field == REGISTRATIONS) {
        return message.getRegistrations();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ClientDowncallAccessor CLIENT_DOWNCALL_ACCESSOR = new ClientDowncallAccessor();
  
  /** Class to access fields in {@link InternalDowncall} protos. */
  public static class InternalDowncallAccessor implements Accessor {
    /** Class to access fields in {@link CreateClient} protos. */
    public static class CreateClientAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "client_type",
          "client_name",
          "client_config",
          "skip_start_for_test"
        ));
      
      public static final Descriptor CLIENT_TYPE = new Descriptor("client_type");
      public static final Descriptor CLIENT_NAME = new Descriptor("client_name");
      public static final Descriptor CLIENT_CONFIG = new Descriptor("client_config");
      public static final Descriptor SKIP_START_FOR_TEST = new Descriptor("skip_start_for_test");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        CreateClient message = (CreateClient) rawMessage;
        if (field == CLIENT_TYPE) {
          return message.hasClientType();
        }
        if (field == CLIENT_NAME) {
          return message.hasClientName();
        }
        if (field == CLIENT_CONFIG) {
          return message.hasClientConfig();
        }
        if (field == SKIP_START_FOR_TEST) {
          return message.hasSkipStartForTest();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        CreateClient message = (CreateClient) rawMessage;
        if (field == CLIENT_TYPE) {
          return message.getClientType();
        }
        if (field == CLIENT_NAME) {
          return message.getClientName();
        }
        if (field == CLIENT_CONFIG) {
          return message.getClientConfig();
        }
        if (field == SKIP_START_FOR_TEST) {
          return message.getSkipStartForTest();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final CreateClientAccessor CREATE_CLIENT_ACCESSOR = new CreateClientAccessor();
    
    /** Class to access fields in {@link NetworkStatus} protos. */
    public static class NetworkStatusAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "is_online"
        ));
      
      public static final Descriptor IS_ONLINE = new Descriptor("is_online");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        NetworkStatus message = (NetworkStatus) rawMessage;
        if (field == IS_ONLINE) {
          return message.hasIsOnline();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        NetworkStatus message = (NetworkStatus) rawMessage;
        if (field == IS_ONLINE) {
          return message.getIsOnline();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final NetworkStatusAccessor NETWORK_STATUS_ACCESSOR = new NetworkStatusAccessor();
    
    /** Class to access fields in {@link ServerMessage} protos. */
    public static class ServerMessageAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "data"
        ));
      
      public static final Descriptor DATA = new Descriptor("data");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        ServerMessage message = (ServerMessage) rawMessage;
        if (field == DATA) {
          return message.hasData();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        ServerMessage message = (ServerMessage) rawMessage;
        if (field == DATA) {
          return message.getData();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final ServerMessageAccessor SERVER_MESSAGE_ACCESSOR = new ServerMessageAccessor();
    
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "version",
        "server_message",
        "network_status",
        "network_addr_change",
        "create_client"
      ));
    
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor SERVER_MESSAGE = new Descriptor("server_message");
    public static final Descriptor NETWORK_STATUS = new Descriptor("network_status");
    public static final Descriptor NETWORK_ADDR_CHANGE = new Descriptor("network_addr_change");
    public static final Descriptor CREATE_CLIENT = new Descriptor("create_client");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InternalDowncall message = (InternalDowncall) rawMessage;
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == SERVER_MESSAGE) {
        return message.hasServerMessage();
      }
      if (field == NETWORK_STATUS) {
        return message.hasNetworkStatus();
      }
      if (field == NETWORK_ADDR_CHANGE) {
        return message.hasNetworkAddrChange();
      }
      if (field == CREATE_CLIENT) {
        return message.hasCreateClient();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      InternalDowncall message = (InternalDowncall) rawMessage;
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == SERVER_MESSAGE) {
        return message.getServerMessage();
      }
      if (field == NETWORK_STATUS) {
        return message.getNetworkStatus();
      }
      if (field == NETWORK_ADDR_CHANGE) {
        return message.getNetworkAddrChange();
      }
      if (field == CREATE_CLIENT) {
        return message.getCreateClient();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final InternalDowncallAccessor INTERNAL_DOWNCALL_ACCESSOR = new InternalDowncallAccessor();
  
  /** Class to access fields in {@link ListenerUpcall} protos. */
  public static class ListenerUpcallAccessor implements Accessor {
    /** Class to access fields in {@link ErrorUpcall} protos. */
    public static class ErrorUpcallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "error_code",
          "error_message",
          "is_transient"
        ));
      
      public static final Descriptor ERROR_CODE = new Descriptor("error_code");
      public static final Descriptor ERROR_MESSAGE = new Descriptor("error_message");
      public static final Descriptor IS_TRANSIENT = new Descriptor("is_transient");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        ErrorUpcall message = (ErrorUpcall) rawMessage;
        if (field == ERROR_CODE) {
          return message.hasErrorCode();
        }
        if (field == ERROR_MESSAGE) {
          return message.hasErrorMessage();
        }
        if (field == IS_TRANSIENT) {
          return message.hasIsTransient();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        ErrorUpcall message = (ErrorUpcall) rawMessage;
        if (field == ERROR_CODE) {
          return message.getErrorCode();
        }
        if (field == ERROR_MESSAGE) {
          return message.getErrorMessage();
        }
        if (field == IS_TRANSIENT) {
          return message.getIsTransient();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final ErrorUpcallAccessor ERROR_UPCALL_ACCESSOR = new ErrorUpcallAccessor();
    
    /** Class to access fields in {@link ReissueRegistrationsUpcall} protos. */
    public static class ReissueRegistrationsUpcallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "prefix",
          "length"
        ));
      
      public static final Descriptor PREFIX = new Descriptor("prefix");
      public static final Descriptor LENGTH = new Descriptor("length");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        ReissueRegistrationsUpcall message = (ReissueRegistrationsUpcall) rawMessage;
        if (field == PREFIX) {
          return message.hasPrefix();
        }
        if (field == LENGTH) {
          return message.hasLength();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        ReissueRegistrationsUpcall message = (ReissueRegistrationsUpcall) rawMessage;
        if (field == PREFIX) {
          return message.getPrefix();
        }
        if (field == LENGTH) {
          return message.getLength();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final ReissueRegistrationsUpcallAccessor REISSUE_REGISTRATIONS_UPCALL_ACCESSOR = new ReissueRegistrationsUpcallAccessor();
    
    /** Class to access fields in {@link RegistrationFailureUpcall} protos. */
    public static class RegistrationFailureUpcallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "object_id",
          "transient",
          "message"
        ));
      
      public static final Descriptor OBJECT_ID = new Descriptor("object_id");
      public static final Descriptor TRANSIENT = new Descriptor("transient");
      public static final Descriptor MESSAGE = new Descriptor("message");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        RegistrationFailureUpcall message = (RegistrationFailureUpcall) rawMessage;
        if (field == OBJECT_ID) {
          return message.hasObjectId();
        }
        if (field == TRANSIENT) {
          return message.hasTransient();
        }
        if (field == MESSAGE) {
          return message.hasMessage();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        RegistrationFailureUpcall message = (RegistrationFailureUpcall) rawMessage;
        if (field == OBJECT_ID) {
          return message.getObjectId();
        }
        if (field == TRANSIENT) {
          return message.getTransient();
        }
        if (field == MESSAGE) {
          return message.getMessage();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final RegistrationFailureUpcallAccessor REGISTRATION_FAILURE_UPCALL_ACCESSOR = new RegistrationFailureUpcallAccessor();
    
    /** Class to access fields in {@link RegistrationStatusUpcall} protos. */
    public static class RegistrationStatusUpcallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "object_id",
          "is_registered"
        ));
      
      public static final Descriptor OBJECT_ID = new Descriptor("object_id");
      public static final Descriptor IS_REGISTERED = new Descriptor("is_registered");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        RegistrationStatusUpcall message = (RegistrationStatusUpcall) rawMessage;
        if (field == OBJECT_ID) {
          return message.hasObjectId();
        }
        if (field == IS_REGISTERED) {
          return message.hasIsRegistered();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        RegistrationStatusUpcall message = (RegistrationStatusUpcall) rawMessage;
        if (field == OBJECT_ID) {
          return message.getObjectId();
        }
        if (field == IS_REGISTERED) {
          return message.getIsRegistered();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final RegistrationStatusUpcallAccessor REGISTRATION_STATUS_UPCALL_ACCESSOR = new RegistrationStatusUpcallAccessor();
    
    /** Class to access fields in {@link InvalidateUpcall} protos. */
    public static class InvalidateUpcallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
          "ack_handle",
          "invalidation",
          "invalidate_unknown",
          "invalidate_all"
        ));
      
      public static final Descriptor ACK_HANDLE = new Descriptor("ack_handle");
      public static final Descriptor INVALIDATION = new Descriptor("invalidation");
      public static final Descriptor INVALIDATE_UNKNOWN = new Descriptor("invalidate_unknown");
      public static final Descriptor INVALIDATE_ALL = new Descriptor("invalidate_all");
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        InvalidateUpcall message = (InvalidateUpcall) rawMessage;
        if (field == ACK_HANDLE) {
          return message.hasAckHandle();
        }
        if (field == INVALIDATION) {
          return message.hasInvalidation();
        }
        if (field == INVALIDATE_UNKNOWN) {
          return message.hasInvalidateUnknown();
        }
        if (field == INVALIDATE_ALL) {
          return message.hasInvalidateAll();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        InvalidateUpcall message = (InvalidateUpcall) rawMessage;
        if (field == ACK_HANDLE) {
          return message.getAckHandle();
        }
        if (field == INVALIDATION) {
          return message.getInvalidation();
        }
        if (field == INVALIDATE_UNKNOWN) {
          return message.getInvalidateUnknown();
        }
        if (field == INVALIDATE_ALL) {
          return message.getInvalidateAll();
        }
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final InvalidateUpcallAccessor INVALIDATE_UPCALL_ACCESSOR = new InvalidateUpcallAccessor();
    
    /** Class to access fields in {@link ReadyUpcall} protos. */
    public static class ReadyUpcallAccessor implements Accessor {
      private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
        Arrays.<String>asList(
        ));
      
      
      /** Returns whether {@code field} is present in {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public boolean hasField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        ReadyUpcall message = (ReadyUpcall) rawMessage;
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      /** Returns the {@code field} from {@code message}. */
      @Override
      @SuppressWarnings("unchecked")
      public Object getField(MessageLite rawMessage, Descriptor field) {
        Preconditions.checkNotNull(rawMessage);
        Preconditions.checkNotNull(field);
        ReadyUpcall message = (ReadyUpcall) rawMessage;
        throw new IllegalArgumentException("Bad descriptor: " + field);
      }
      
      @Override
      public Set<String> getAllFieldNames() {
        return ALL_FIELD_NAMES;
      }
    }
    public static final ReadyUpcallAccessor READY_UPCALL_ACCESSOR = new ReadyUpcallAccessor();
    
    private static final Set<String> ALL_FIELD_NAMES = new HashSet<String>(
      Arrays.<String>asList(
        "serial",
        "version",
        "ready",
        "invalidate",
        "registration_status",
        "registration_failure",
        "reissue_registrations",
        "error"
      ));
    
    public static final Descriptor SERIAL = new Descriptor("serial");
    public static final Descriptor VERSION = new Descriptor("version");
    public static final Descriptor READY = new Descriptor("ready");
    public static final Descriptor INVALIDATE = new Descriptor("invalidate");
    public static final Descriptor REGISTRATION_STATUS = new Descriptor("registration_status");
    public static final Descriptor REGISTRATION_FAILURE = new Descriptor("registration_failure");
    public static final Descriptor REISSUE_REGISTRATIONS = new Descriptor("reissue_registrations");
    public static final Descriptor ERROR = new Descriptor("error");
    
    /** Returns whether {@code field} is present in {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public boolean hasField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ListenerUpcall message = (ListenerUpcall) rawMessage;
      if (field == SERIAL) {
        return message.hasSerial();
      }
      if (field == VERSION) {
        return message.hasVersion();
      }
      if (field == READY) {
        return message.hasReady();
      }
      if (field == INVALIDATE) {
        return message.hasInvalidate();
      }
      if (field == REGISTRATION_STATUS) {
        return message.hasRegistrationStatus();
      }
      if (field == REGISTRATION_FAILURE) {
        return message.hasRegistrationFailure();
      }
      if (field == REISSUE_REGISTRATIONS) {
        return message.hasReissueRegistrations();
      }
      if (field == ERROR) {
        return message.hasError();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    /** Returns the {@code field} from {@code message}. */
    @Override
    @SuppressWarnings("unchecked")
    public Object getField(MessageLite rawMessage, Descriptor field) {
      Preconditions.checkNotNull(rawMessage);
      Preconditions.checkNotNull(field);
      ListenerUpcall message = (ListenerUpcall) rawMessage;
      if (field == SERIAL) {
        return message.getSerial();
      }
      if (field == VERSION) {
        return message.getVersion();
      }
      if (field == READY) {
        return message.getReady();
      }
      if (field == INVALIDATE) {
        return message.getInvalidate();
      }
      if (field == REGISTRATION_STATUS) {
        return message.getRegistrationStatus();
      }
      if (field == REGISTRATION_FAILURE) {
        return message.getRegistrationFailure();
      }
      if (field == REISSUE_REGISTRATIONS) {
        return message.getReissueRegistrations();
      }
      if (field == ERROR) {
        return message.getError();
      }
      throw new IllegalArgumentException("Bad descriptor: " + field);
    }
    
    @Override
    public Set<String> getAllFieldNames() {
      return ALL_FIELD_NAMES;
    }
  }
  public static final ListenerUpcallAccessor LISTENER_UPCALL_ACCESSOR = new ListenerUpcallAccessor();
  
}
