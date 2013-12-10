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

package com.google.ipc.invalidation.ticl;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.common.CommonInvalidationConstants2;
import com.google.ipc.invalidation.common.CommonProtoStrings2;
import com.google.ipc.invalidation.common.CommonProtos2;
import com.google.ipc.invalidation.common.TiclMessageValidator2;
import com.google.ipc.invalidation.external.client.SystemResources;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.SystemResources.NetworkChannel;
import com.google.ipc.invalidation.external.client.SystemResources.Scheduler;
import com.google.ipc.invalidation.external.client.types.SimplePair;
import com.google.ipc.invalidation.ticl.InvalidationClientCore.BatchingTask;
import com.google.ipc.invalidation.ticl.Statistics.ClientErrorType;
import com.google.ipc.invalidation.ticl.Statistics.ReceivedMessageType;
import com.google.ipc.invalidation.ticl.Statistics.SentMessageType;
import com.google.ipc.invalidation.util.InternalBase;
import com.google.ipc.invalidation.util.Marshallable;
import com.google.ipc.invalidation.util.Smearer;
import com.google.ipc.invalidation.util.TextBuilder;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
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
import com.google.protos.ipc.invalidation.ClientProtocol.InitializeMessage.DigestSerializationType;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.PropertyRecord;
import com.google.protos.ipc.invalidation.ClientProtocol.ProtocolHandlerConfigP;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationP;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationP.OpType;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationStatusMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSubtree;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSummary;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSyncMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSyncRequestMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.ServerHeader;
import com.google.protos.ipc.invalidation.ClientProtocol.ServerToClientMessage;
import com.google.protos.ipc.invalidation.ClientProtocol.TokenControlMessage;
import com.google.protos.ipc.invalidation.JavaClient.BatcherState;
import com.google.protos.ipc.invalidation.JavaClient.ProtocolHandlerState;

import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;


/**
 * A layer for interacting with low-level protocol messages.  Parses messages from the server and
 * calls appropriate functions on the {@code ProtocolListener} to handle various types of message
 * content.  Also buffers message data from the client and constructs and sends messages to the
 * server.
 * <p>
 * This class implements {@link Marshallable}, so its state can be written to a protocol buffer,
 * and instances can be restored from such protocol buffers. Additionally, the nested class
 * {@link Batcher} also implements {@code Marshallable} for the same reason.
 * <p>
 * Note that while we talk about "marshalling," in this context we mean marshalling to protocol
 * buffers, not raw bytes.
 *
 */
class ProtocolHandler implements Marshallable<ProtocolHandlerState> {
  /** Class that batches messages to the server. */
  private static class Batcher implements Marshallable<BatcherState> {
    /** Statistics to be updated when messages are created. */
    private final Statistics statistics;

    /** Resources used for logging and thread assertions. */
    private final SystemResources resources;

    /** Set of pending registrations stored as a map for overriding later operations. */
    private final Map<ProtoWrapper<ObjectIdP>, RegistrationP.OpType> pendingRegistrations =
        new HashMap<ProtoWrapper<ObjectIdP>, RegistrationP.OpType>();

    /** Set of pending invalidation acks. */
    private final Set<ProtoWrapper<InvalidationP>> pendingAckedInvalidations =
        new HashSet<ProtoWrapper<InvalidationP>>();

    /** Set of pending registration sub trees for registration sync. */
    private final Set<ProtoWrapper<RegistrationSubtree>> pendingRegSubtrees =
        new HashSet<ProtoWrapper<RegistrationSubtree>>();

    /** Pending initialization message to send to the server, if any. */
    private InitializeMessage pendingInitializeMessage = null;

    /** Pending info message to send to the server, if any. */
    private InfoMessage pendingInfoMessage = null;

    /** Creates a batcher. */
    Batcher(SystemResources resources, Statistics statistics) {
      this.resources = resources;
      this.statistics = statistics;
    }

    /** Creates a batcher from {@code marshalledState}. */
    Batcher(SystemResources resources, Statistics statistics, BatcherState marshalledState) {
      this(resources, statistics);
      for (ObjectIdP registration : marshalledState.getRegistrationList()) {
        pendingRegistrations.put(ProtoWrapper.of(registration), RegistrationP.OpType.REGISTER);
      }
      for (ObjectIdP unregistration : marshalledState.getUnregistrationList()) {
        pendingRegistrations.put(ProtoWrapper.of(unregistration), RegistrationP.OpType.UNREGISTER);
      }
      for (InvalidationP ack : marshalledState.getAcknowledgementList()) {
        pendingAckedInvalidations.add(ProtoWrapper.of(ack));
      }
      for (RegistrationSubtree subtree : marshalledState.getRegistrationSubtreeList()) {
        pendingRegSubtrees.add(ProtoWrapper.of(subtree));
      }
      if (marshalledState.hasInitializeMessage()) {
        pendingInitializeMessage = marshalledState.getInitializeMessage();
      }
      if (marshalledState.hasInfoMessage()) {
        pendingInfoMessage = marshalledState.getInfoMessage();
      }
    }

    /** Sets the initialize message to be sent. */
    void setInitializeMessage(InitializeMessage msg) {
      pendingInitializeMessage = msg;
    }

    /** Sets the info message to be sent. */
    void setInfoMessage(InfoMessage msg) {
      pendingInfoMessage = msg;
    }

    /** Adds a registration on {@code oid} of {@code opType} to the registrations to be sent. */
    void addRegistration(ObjectIdP oid, RegistrationP.OpType opType) {
      pendingRegistrations.put(ProtoWrapper.of(oid), opType);
    }

    /** Adds {@code ack} to the set of acknowledgements to be sent. */
    void addAck(InvalidationP ack) {
      pendingAckedInvalidations.add(ProtoWrapper.of(ack));
    }

    /** Adds {@code subtree} to the set of registration subtrees to be sent. */
    void addRegSubtree(RegistrationSubtree subtree) {
      pendingRegSubtrees.add(ProtoWrapper.of(subtree));
    }

    /**
     * Returns a builder for a {@link ClientToServerMessage} to be sent to the server. Crucially,
     * the builder does <b>NOT</b> include the message header.
     * @param hasClientToken whether the client currently holds a token
     */
    ClientToServerMessage.Builder toBuilder(boolean hasClientToken) {
      ClientToServerMessage.Builder builder = ClientToServerMessage.newBuilder();
      if (pendingInitializeMessage != null) {
        statistics.recordSentMessage(SentMessageType.INITIALIZE);
        builder.setInitializeMessage(pendingInitializeMessage);
        pendingInitializeMessage = null;
      }

      // Note: Even if an initialize message is being sent, we can send additional
      // messages such as regisration messages, etc to the server. But if there is no token
      // and an initialize message is not being sent, we cannot send any other message.

      if (!hasClientToken && !builder.hasInitializeMessage()) {
        // Cannot send any message
        resources.getLogger().warning(
            "Cannot send message since no token and no initialize msg: %s", builder);
        statistics.recordError(ClientErrorType.TOKEN_MISSING_FAILURE);
        return null;
      }

      // Check for pending batched operations and add to message builder if needed.

      // Add reg, acks, reg subtrees - clear them after adding.
      if (!pendingAckedInvalidations.isEmpty()) {
        builder.setInvalidationAckMessage(createInvalidationAckMessage());
        statistics.recordSentMessage(SentMessageType.INVALIDATION_ACK);
      }

      // Check regs.
      if (!pendingRegistrations.isEmpty()) {
        builder.setRegistrationMessage(createRegistrationMessage());
        statistics.recordSentMessage(SentMessageType.REGISTRATION);
      }

      // Check reg substrees.
      if (!pendingRegSubtrees.isEmpty()) {
        for (ProtoWrapper<RegistrationSubtree> subtree : pendingRegSubtrees) {
          builder.setRegistrationSyncMessage(RegistrationSyncMessage.newBuilder()
              .addSubtree(subtree.getProto()));
        }
        pendingRegSubtrees.clear();
        statistics.recordSentMessage(SentMessageType.REGISTRATION_SYNC);
      }

      // Check if an info message has to be sent.
      if (pendingInfoMessage != null) {
        statistics.recordSentMessage(SentMessageType.INFO);
        builder.setInfoMessage(pendingInfoMessage);
        pendingInfoMessage = null;
      }
      return builder;
    }

    /**
     * Creates a registration message based on registrations from {@code pendingRegistrations}
     * and returns it.
     * <p>
     * REQUIRES: pendingRegistrations.size() > 0
     */
    private RegistrationMessage createRegistrationMessage() {
      Preconditions.checkState(!pendingRegistrations.isEmpty());
      RegistrationMessage.Builder regMessage = RegistrationMessage.newBuilder();

      // Run through the pendingRegistrations map.
      for (Map.Entry<ProtoWrapper<ObjectIdP>, RegistrationP.OpType> entry :
           pendingRegistrations.entrySet()) {
        RegistrationP reg = CommonProtos2.newRegistrationP(entry.getKey().getProto(),
            entry.getValue() == RegistrationP.OpType.REGISTER);
        regMessage.addRegistration(reg);
      }
      pendingRegistrations.clear();
      return regMessage.build();
    }

    /**
     * Creates an invalidation ack message based on acks from {@code pendingAckedInvalidations} and
     * returns it.
     * <p>
     * REQUIRES: pendingAckedInvalidations.size() > 0
     */
    private InvalidationMessage createInvalidationAckMessage() {
      Preconditions.checkState(!pendingAckedInvalidations.isEmpty());
      InvalidationMessage.Builder ackMessage = InvalidationMessage.newBuilder();
      for (ProtoWrapper<InvalidationP> wrapper : pendingAckedInvalidations) {
        ackMessage.addInvalidation(wrapper.getProto());
      }
      pendingAckedInvalidations.clear();
      return ackMessage.build();
    }

    @Override
    public BatcherState marshal() {
      BatcherState.Builder builder = BatcherState.newBuilder();

      // Marshall (un)registrations.
      for (Map.Entry<ProtoWrapper<ObjectIdP>, RegistrationP.OpType> entry :
          pendingRegistrations.entrySet()) {
        OpType opType = entry.getValue();
        ObjectIdP oid = entry.getKey().getProto();
        switch (opType) {
          case REGISTER:
            builder.addRegistration(oid);
            break;
          case UNREGISTER:
            builder.addUnregistration(oid);
            break;
          default:
            throw new IllegalArgumentException(opType.toString());
        }
      }

      // Marshall acks.
      for (ProtoWrapper<InvalidationP> ack : pendingAckedInvalidations) {
        builder.addAcknowledgement(ack.getProto());
      }

      // Marshall registration subtrees.
      for (ProtoWrapper<RegistrationSubtree> subtree : pendingRegSubtrees) {
        builder.addRegistrationSubtree(subtree.getProto());
      }

      // Marshall initialize and info messages if present.
      if (pendingInitializeMessage != null) {
        builder.setInitializeMessage(pendingInitializeMessage);
      }
      if (pendingInfoMessage != null) {
        builder.setInfoMessage(pendingInfoMessage);
      }
      return builder.build();
    }
  }

  /** Representation of a message header for use in a server message. */
  static class ServerMessageHeader extends InternalBase {
    /**
     * Constructs an instance.
     *
     * @param token server-sent token
     * @param registrationSummary summary over server registration state
     */
    ServerMessageHeader(ByteString token, RegistrationSummary registrationSummary) {
      this.token = token;
      this.registrationSummary = registrationSummary;
    }

    /** Server-sent token. */
    ByteString token;

    /** Summary of the client's registration state at the server. */
    RegistrationSummary registrationSummary;

    @Override
    public void toCompactString(TextBuilder builder) {
      builder.appendFormat("Token: %s, Summary: %s", CommonProtoStrings2.toLazyCompactString(token),
          registrationSummary);
    }
  }

  /**
   * Representation of a message receiver for the server. Such a message is guaranteed to be
   * valid (i.e. checked by {@link TiclMessageValidator2}, but the session token is <b>not</b>
   * checked.
   */
  static class ParsedMessage {
    /*
     * Each of these fields corresponds directly to a field in the ServerToClientMessage protobuf.
     * It is non-null iff the correspondig hasYYY method in the protobuf would return true.
     */
    final ServerMessageHeader header;
    final TokenControlMessage tokenControlMessage;
    final InvalidationMessage invalidationMessage;
    final RegistrationStatusMessage registrationStatusMessage;
    final RegistrationSyncRequestMessage registrationSyncRequestMessage;
    final ConfigChangeMessage configChangeMessage;
    final InfoRequestMessage infoRequestMessage;
    final ErrorMessage errorMessage;

    /** Constructs an instance from a {@code rawMessage}. */
    ParsedMessage(ServerToClientMessage rawMessage) {
      // For each field, assign it to the corresponding protobuf field if present, else null.
      ServerHeader messageHeader = rawMessage.getHeader();
      header = new ServerMessageHeader(messageHeader.getClientToken(),
          messageHeader.hasRegistrationSummary() ? messageHeader.getRegistrationSummary() : null);
      tokenControlMessage = rawMessage.hasTokenControlMessage() ?
          rawMessage.getTokenControlMessage() : null;
      invalidationMessage = rawMessage.hasInvalidationMessage() ?
          rawMessage.getInvalidationMessage() : null;
      registrationStatusMessage = rawMessage.hasRegistrationStatusMessage() ?
          rawMessage.getRegistrationStatusMessage() : null;
      registrationSyncRequestMessage = rawMessage.hasRegistrationSyncRequestMessage() ?
          rawMessage.getRegistrationSyncRequestMessage() : null;
      configChangeMessage = rawMessage.hasConfigChangeMessage() ?
          rawMessage.getConfigChangeMessage() : null;
      infoRequestMessage = rawMessage.hasInfoRequestMessage() ?
          rawMessage.getInfoRequestMessage() : null;
      errorMessage = rawMessage.hasErrorMessage() ? rawMessage.getErrorMessage() : null;
    }
  }

  /**
   * Listener for protocol events. The handler guarantees that the call will be made on the internal
   * thread that the SystemResources provides.
   */
  interface ProtocolListener {
    /** Records that a message was sent to the server at the current time. */
    void handleMessageSent();

    /** Returns a summary of the current desired registrations. */
    RegistrationSummary getRegistrationSummary();

    /** Returns the current server-assigned client token, if any. */
    ByteString getClientToken();
  }

  /** Information about the client, e.g., application name, OS, etc. */
  private final ClientVersion clientVersion;

  /** A logger. */
  private final Logger logger;

  /** Scheduler for the client's internal processing. */
  private final Scheduler internalScheduler;

  /** Network channel for sending and receiving messages to and from the server. */
  private final NetworkChannel network;

  /** The protocol listener. */
  private final ProtocolListener listener;

  /** Checks that messages (inbound and outbound) conform to basic validity constraints. */
  private final TiclMessageValidator2 msgValidator;

  /** Batches messages to the server. */
  private final Batcher batcher;

  /** A debug message id that is added to every message to the server. */
  private int messageId = 1;

  // State specific to a client. If we want to support multiple clients, this could
  // be in a map or could be eliminated (e.g., no batching).

  /** The last known time from the server. */
  private long lastKnownServerTimeMs = 0;

  /**
   * The next time before which a message cannot be sent to the server. If this is less than current
   * time, a message can be sent at any time.
   */
  private long nextMessageSendTimeMs = 0;

  /** Statistics objects to track number of sent messages, etc. */
  private final Statistics statistics;

  /** Client type for inclusion in headers. */
  private final int clientType;

  /**
   * Creates an instance.
   *
   * @param config configuration for the client
   * @param resources resources to use
   * @param smearer a smearer to randomize delays
   * @param statistics track information about messages sent/received, etc
   * @param applicationName name of the application using the library (for debugging/monitoring)
   * @param listener callback for protocol events
   */
  ProtocolHandler(ProtocolHandlerConfigP config, final SystemResources resources,
      Smearer smearer, Statistics statistics, int clientType, String applicationName,
      ProtocolListener listener, TiclMessageValidator2 msgValidator,
      ProtocolHandlerState marshalledState) {
    this.logger = resources.getLogger();
    this.statistics = statistics;
    this.internalScheduler = resources.getInternalScheduler();
    this.network = resources.getNetwork();
    this.listener = listener;
    this.msgValidator = msgValidator;
    this.clientVersion = CommonProtos2.newClientVersion(resources.getPlatform(), "Java",
        applicationName);
    this.clientType = clientType;
    if (marshalledState == null) {
      // If there is no marshalled state, construct a clean batcher.
      this.batcher = new Batcher(resources, statistics);
    } else {
      // Otherwise, restore the batcher from the marshalled state.
      this.batcher = new Batcher(resources, statistics, marshalledState.getBatcherState());
      this.messageId = marshalledState.getMessageId();
      this.lastKnownServerTimeMs = marshalledState.getLastKnownServerTimeMs();
      this.nextMessageSendTimeMs = marshalledState.getNextMessageSendTimeMs();
    }
    logger.info("Created protocol handler for application %s, platform %s", applicationName,
        resources.getPlatform());
  }

  /** Returns a default config for the protocol handler. */
  static ProtocolHandlerConfigP.Builder createConfig() {
    // Allow at most 3 messages every 5 seconds.
    int windowMs = 5 * 1000;
    int numMessagesPerWindow = 3;

    return ProtocolHandlerConfigP.newBuilder()
        .addRateLimit(CommonProtos2.newRateLimitP(windowMs, numMessagesPerWindow));
  }

  /** Returns a configuration object with parameters set for unit tests. */
  static ProtocolHandlerConfigP.Builder createConfigForTest() {
    // No rate limits
    int smallBatchDelayForTest = 200;
    return ProtocolHandlerConfigP.newBuilder().setBatchingDelayMs(smallBatchDelayForTest);
  }

  /**
   * Returns the next time a message is allowed to be sent to the server.  Typically, this will be
   * in the past, meaning that the client is free to send a message at any time.
   */
  public long getNextMessageSendTimeMsForTest() {
    return nextMessageSendTimeMs;
  }

  /**
   * Handles a message from the server. If the message can be processed (i.e., is valid, is
   * of the right version, and is not a silence message), returns a {@link ParsedMessage}
   * representing it. Otherwise, returns {@code null}.
   * <p>
   * This class intercepts and processes silence messages. In this case, it will discard any other
   * data in the message.
   * <p>
   * Note that this method does <b>not</b> check the session token of any message.
   */
  ParsedMessage handleIncomingMessage(byte[] incomingMessage) {
    Preconditions.checkState(internalScheduler.isRunningOnThread(), "Not on internal thread");
    ServerToClientMessage message;
    try {
      message = ServerToClientMessage.parseFrom(incomingMessage);
    } catch (InvalidProtocolBufferException exception) {
      logger.warning("Incoming message is unparseable: %s",
          CommonProtoStrings2.toLazyCompactString(incomingMessage));
      return null;
    }

    // Validate the message. If this passes, we can blindly assume valid messages from here on.
    logger.fine("Incoming message: %s", message);
    if (!msgValidator.isValid(message)) {
      statistics.recordError(ClientErrorType.INCOMING_MESSAGE_FAILURE);
      logger.severe("Received invalid message: %s", message);
      return null;
    }

    // Check the version of the message.
    if (message.getHeader().getProtocolVersion().getVersion().getMajorVersion() !=
        CommonInvalidationConstants2.PROTOCOL_MAJOR_VERSION) {
      statistics.recordError(ClientErrorType.PROTOCOL_VERSION_FAILURE);
      logger.severe("Dropping message with incompatible version: %s", message);
      return null;
    }

    // Check if it is a ConfigChangeMessage which indicates that messages should no longer be
    // sent for a certain duration. Perform this check before the token is even checked.
    if (message.hasConfigChangeMessage()) {
      ConfigChangeMessage configChangeMsg = message.getConfigChangeMessage();
      statistics.recordReceivedMessage(ReceivedMessageType.CONFIG_CHANGE);
      if (configChangeMsg.hasNextMessageDelayMs()) {  // Validator has ensured that it is positive.
        nextMessageSendTimeMs =
            internalScheduler.getCurrentTimeMs() + configChangeMsg.getNextMessageDelayMs();
      }
      return null;  // Ignore all other messages in the envelope.
    }

    lastKnownServerTimeMs = Math.max(lastKnownServerTimeMs, message.getHeader().getServerTimeMs());
    return new ParsedMessage(message);
  }

  /**
   * Sends a message to the server to request a client token.
   *
   * @param applicationClientId application-specific client id
   * @param nonce nonce for the request
   * @param debugString information to identify the caller
   */
  void sendInitializeMessage(ApplicationClientIdP applicationClientId, ByteString nonce,
      BatchingTask batchingTask, String debugString) {
    Preconditions.checkState(internalScheduler.isRunningOnThread(), "Not on internal thread");
    if (applicationClientId.getClientType() != clientType) {
      // This condition is not fatal, but it probably represents a bug somewhere if it occurs.
      logger.warning(
          "Client type in application id does not match constructor-provided type: %s vs %s",
          applicationClientId, clientType);
    }

    // Simply store the message in pendingInitializeMessage and send it when the batching task runs.
    InitializeMessage initializeMsg = CommonProtos2.newInitializeMessage(clientType,
        applicationClientId, nonce, DigestSerializationType.BYTE_BASED);
    batcher.setInitializeMessage(initializeMsg);
    logger.info("Batching initialize message for client: %s, %s", debugString, initializeMsg);
    batchingTask.ensureScheduled(debugString);
  }

  /**
   * Sends an info message to the server with the performance counters supplied
   * in {@code performanceCounters} and the config supplies in
   * {@code configParams}.
   *
   * @param requestServerRegistrationSummary indicates whether to request the
   *        server's registration summary
   */
  void sendInfoMessage(List<SimplePair<String, Integer>> performanceCounters,
      ClientConfigP clientConfig, boolean requestServerRegistrationSummary,
      BatchingTask batchingTask) {
    Preconditions.checkState(internalScheduler.isRunningOnThread(), "Not on internal thread");
    InfoMessage.Builder infoMessage = InfoMessage.newBuilder()
        .setClientVersion(clientVersion);

    // Add configuration parameters.
    if (clientConfig != null) {
      infoMessage.setClientConfig(clientConfig);
    }

    // Add performance counters.
    for (SimplePair<String, Integer> performanceCounter : performanceCounters) {
      PropertyRecord counter =
          CommonProtos2.newPropertyRecord(performanceCounter.first, performanceCounter.second);
      infoMessage.addPerformanceCounter(counter);
    }

    // Indicate whether we want the server's registration summary sent back.
    infoMessage.setServerRegistrationSummaryRequested(requestServerRegistrationSummary);

    // Simply store the message in pendingInfoMessage and send it when the batching task runs.
    batcher.setInfoMessage(infoMessage.build());
    batchingTask.ensureScheduled("Send-info");
  }

  /**
   * Sends a registration request to the server.
   *
   * @param objectIds object ids on which to (un)register
   * @param regOpType whether to register or unregister
   */
  void sendRegistrations(Collection<ObjectIdP> objectIds, RegistrationP.OpType regOpType,
      BatchingTask batchingTask) {
    Preconditions.checkState(internalScheduler.isRunningOnThread(), "Not on internal thread");
    for (ObjectIdP objectId : objectIds) {
      batcher.addRegistration(objectId, regOpType);
    }
    batchingTask.ensureScheduled("Send-registrations");
  }

  /** Sends an acknowledgement for {@code invalidation} to the server. */
  void sendInvalidationAck(InvalidationP invalidation, BatchingTask batchingTask) {
    Preconditions.checkState(internalScheduler.isRunningOnThread(), "Not on internal thread");
    // We could do squelching - we don't since it is unlikely to be too beneficial here.
    logger.fine("Sending ack for invalidation %s", invalidation);
    batcher.addAck(invalidation);
    batchingTask.ensureScheduled("Send-Ack");
  }

  /**
   * Sends a single registration subtree to the server.
   *
   * @param regSubtree subtree to send
   */
  void sendRegistrationSyncSubtree(RegistrationSubtree regSubtree, BatchingTask batchingTask) {
    Preconditions.checkState(internalScheduler.isRunningOnThread(), "Not on internal thread");
    batcher.addRegSubtree(regSubtree);
    logger.info("Adding subtree: %s", regSubtree);
    batchingTask.ensureScheduled("Send-reg-sync");
  }

  /** Sends pending data to the server (e.g., registrations, acks, registration sync messages). */
  void sendMessageToServer() {
    Preconditions.checkState(internalScheduler.isRunningOnThread(), "Not on internal thread");
    if (nextMessageSendTimeMs > internalScheduler.getCurrentTimeMs()) {
      logger.warning("In quiet period: not sending message to server: %s > %s",
          nextMessageSendTimeMs, internalScheduler.getCurrentTimeMs());
      return;
    }

    // Create the message from the batcher.
    ClientToServerMessage.Builder msgBuilder =
        batcher.toBuilder(listener.getClientToken() != null);
    if (msgBuilder == null) {
      // Happens when we don't have a token and are not sending an initialize message. Logged
      // in batcher.toBuilder().
      return;
    }
    msgBuilder.setHeader(createClientHeader());
    ++messageId;

    // Validate the message and send it.
    ClientToServerMessage message = msgBuilder.build();
    if (!msgValidator.isValid(message)) {
      logger.severe("Tried to send invalid message: %s", message);
      statistics.recordError(ClientErrorType.OUTGOING_MESSAGE_FAILURE);
      return;
    }

    statistics.recordSentMessage(SentMessageType.TOTAL);
    logger.fine("Sending message to server: {0}",
        CommonProtoStrings2.toLazyCompactString(message, true));
    network.sendMessage(message.toByteArray());

    // Record that the message was sent. We're invoking the listener directly, rather than
    // scheduling a new work unit to do it. It would be safer to do a schedule, but that's hard to
    // do in Android, we wrote this listener (it's InvalidationClientCore, so we know what it does),
    // and it's the last line of this function.
    listener.handleMessageSent();
  }

  /** Returns the header to include on a message to the server. */
  private ClientHeader.Builder createClientHeader() {
    Preconditions.checkState(internalScheduler.isRunningOnThread(), "Not on internal thread");
    ClientHeader.Builder builder = ClientHeader.newBuilder()
        .setProtocolVersion(CommonInvalidationConstants2.PROTOCOL_VERSION)
        .setClientTimeMs(internalScheduler.getCurrentTimeMs())
        .setMessageId(Integer.toString(messageId))
        .setMaxKnownServerTimeMs(lastKnownServerTimeMs)
        .setRegistrationSummary(listener.getRegistrationSummary())
        .setClientType(clientType);
    ByteString clientToken = listener.getClientToken();
    if (clientToken != null) {
      logger.fine("Sending token on client->server message: %s",
          CommonProtoStrings2.toLazyCompactString(clientToken));
      builder.setClientToken(clientToken);
    }
    return builder;
  }

  @Override
  public ProtocolHandlerState marshal() {
    ProtocolHandlerState.Builder builder = ProtocolHandlerState.newBuilder();
    builder.setLastKnownServerTimeMs(lastKnownServerTimeMs);
    builder.setMessageId(messageId);
    builder.setNextMessageSendTimeMs(nextMessageSendTimeMs);
    builder.setBatcherState(batcher.marshal());
    return builder.build();
  }
}
