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

package com.google.ipc.invalidation.ticl.android;

import com.google.ipc.invalidation.common.DigestFunction;
import com.google.ipc.invalidation.common.ObjectIdDigestUtils;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.AndroidInvalidationClient;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.external.client.android.service.Request;
import com.google.ipc.invalidation.external.client.android.service.Response;
import com.google.ipc.invalidation.external.client.android.service.Response.Status;
import com.google.ipc.invalidation.external.client.contrib.MultiplexingGcmListener;
import com.google.ipc.invalidation.external.client.types.AckHandle;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.ipc.invalidation.ticl.InvalidationClientCore;
import com.google.ipc.invalidation.ticl.PersistenceUtils;
import com.google.ipc.invalidation.util.TypedUtil;
import com.google.protos.ipc.invalidation.Client.PersistentTiclState;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;
import android.os.IBinder;
import android.util.Base64;

import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;


/**
 * The AndroidInvalidationService class provides an Android service implementation that bridges
 * between the {@code InvalidationService} interface and invalidation client service instances
 * executing within the scope of that service. The invalidation service will have an associated
 * {@link AndroidClientManager} that is managing the set of active (in memory) clients associated
 * with the service.  It processes requests from invalidation applications (as invocations on
 * the {@code InvalidationService} bound service interface along with GCM registration and
 * activity (from {@link ReceiverService}).
 *
 */
public class AndroidInvalidationService extends AbstractInvalidationService {

  /**
   * Service that handles system GCM messages (with support from the base class). It receives
   * intents for GCM registration, errors and message delivery. It does some basic processing and
   * then forwards the messages to the {@link AndroidInvalidationService} for handling.
   */
  public static class ReceiverService extends MultiplexingGcmListener.AbstractListener {

    /**
     * Receiver for broadcasts by the multiplexed GCM service. It forwards them to
     * AndroidMessageReceiverService.
     */
    public static class Receiver extends MultiplexingGcmListener.AbstractListener.Receiver {
      /* This class is public so that it can be instantiated by the Android runtime. */
      @Override
      protected Class<?> getServiceClass() {
        return ReceiverService.class;
      }
    }

    public ReceiverService() {
      super("MsgRcvrSvc");
    }

    @Override
    public void onRegistered(String registrationId) {
      logger.info("GCM Registration received: %s", registrationId);

      // Upon receiving a new updated GCM ID, notify the invalidation service
      Intent serviceIntent =
          AndroidInvalidationService.createRegistrationIntent(this, registrationId);
      startService(serviceIntent);
    }

    @Override
    public void onUnregistered(String registrationId) {
      logger.info("GCM unregistered");
    }

    @Override
    protected void onMessage(Intent intent) {
      // Extract expected fields and do basic syntactic checks (but no value checking)
      // and forward the result on to the AndroidInvalidationService for processing.
      Intent serviceIntent;
      String clientKey = intent.getStringExtra(AndroidC2DMConstants.CLIENT_KEY_PARAM);
      if (clientKey == null) {
        logger.severe("GCM Intent does not contain client key value: %s", intent);
        return;
      }
      String encodedData = intent.getStringExtra(AndroidC2DMConstants.CONTENT_PARAM);
      String echoToken = intent.getStringExtra(AndroidC2DMConstants.ECHO_PARAM);
      if (encodedData != null) {
        try {
          byte [] rawData = Base64.decode(encodedData, Base64.URL_SAFE);
          serviceIntent = AndroidInvalidationService.createDataIntent(this, clientKey, echoToken,
              rawData);
        } catch (IllegalArgumentException exception) {
          logger.severe("Unable to decode intent data", exception);
          return;
        }
      } else {
        logger.severe("Received mailbox intent: %s", intent);
        return;
      }
      startService(serviceIntent);
    }

    @Override
    protected void onDeletedMessages(int total) {
      // This method must be implemented if we start using non-collapsable messages with GCM. For
      // now, there is nothing to do.
    }
  }

  /** The last created instance, for testing. */
  
  static AtomicReference<AndroidInvalidationService> lastInstanceForTest =
      new AtomicReference<AndroidInvalidationService>();

  /** For tests only, the number of C2DM errors received. */
  static final AtomicInteger numGcmErrorsForTest = new AtomicInteger(0);

  /** For tests only, the number of C2DM registration messages received. */
  static final AtomicInteger numGcmRegistrationForTest = new AtomicInteger(0);

  /** For tests only, the number of C2DM messages received. */
  static final AtomicInteger numGcmMessagesForTest = new AtomicInteger(0);

  /** For tests only, the number of onCreate calls made. */
  static final AtomicInteger numCreateForTest = new AtomicInteger(0);

  /** The client manager tracking in-memory client instances */
  
  protected static AndroidClientManager clientManager;

  private static final Logger logger = AndroidLogger.forTag("InvService");

  /** The HTTP URL of the channel service. */
  private static String channelUrl = AndroidHttpConstants.CHANNEL_URL;

  // The AndroidInvalidationService handles a set of internal intents that are used for
  // communication and coordination between the it and the GCM handling service.   These
  // are documented here with action and extra names documented with package private
  // visibility since they are not intended for use by external components.

  /**
   * Sent when a new GCM registration activity occurs for the service. This can occur the first
   * time the service is run or at any subsequent time if the Android C2DM service decides to issue
   * a new GCM registration ID.
   */
  static final String REGISTRATION_ACTION = "register";

  /**
   * The name of the String extra that contains the registration ID for a register intent.  If this
   * extra is not present, then it indicates that a C2DM notification regarding unregistration has
   * been received (not expected during normal operation conditions).
   */
  static final String REGISTER_ID = "id";

  /**
   * This intent is sent when a GCM message targeting the service is received.
   */
  static final String MESSAGE_ACTION = "message";

  /**
   * The name of the String extra that contains the client key for the GCM message.
   */
  static final String MESSAGE_CLIENT_KEY = "clientKey";

  /**
   * The name of the byte array extra that contains the encoded event for the GCM message.
   */
  static final String MESSAGE_DATA = "data";

  /** The name of the string extra that contains the echo token in the GCM message. */
  static final String MESSAGE_ECHO = "echo-token";

  /**
   * This intent is sent when GCM registration has failed irrevocably.
   */
  static final String ERROR_ACTION = "error";

  /**
   * The name of the String extra that contains the error message describing the registration
   * failure.
   */
  static final String ERROR_MESSAGE = "message";

  /** Returns the client manager for this service */
  static AndroidClientManager getClientManager() {
    return clientManager;
  }

  /**
   * Creates a new registration intent that notifies the service of a registration ID change
   */
  static Intent createRegistrationIntent(Context context, String registrationId) {
    Intent intent = new Intent(REGISTRATION_ACTION);
    intent.setClass(context, AndroidInvalidationService.class);
    if (registrationId != null) {
      intent.putExtra(AndroidInvalidationService.REGISTER_ID, registrationId);
    }
    return intent;
  }

  /**
   * Creates a new message intent to contains event data to deliver directly to a client.
   */
  static Intent createDataIntent(Context context, String clientKey, String token,
      byte [] data) {
    Intent intent = new Intent(MESSAGE_ACTION);
    intent.setClass(context, AndroidInvalidationService.class);
    intent.putExtra(MESSAGE_CLIENT_KEY, clientKey);
    intent.putExtra(MESSAGE_DATA, data);
    if (token != null) {
      intent.putExtra(MESSAGE_ECHO, token);
    }
    return intent;
  }

  /**
   * Creates a new message intent that references event data to retrieve from a mailbox.
   */
  static Intent createMailboxIntent(Context context, String clientKey, String token) {
    Intent intent = new Intent(MESSAGE_ACTION);
    intent.setClass(context, AndroidInvalidationService.class);
    intent.putExtra(MESSAGE_CLIENT_KEY, clientKey);
    if (token != null) {
      intent.putExtra(MESSAGE_ECHO, token);
    }
    return intent;
  }

  /**
   * Creates a new error intent that notifies the service of a registration failure.
   */
  static Intent createErrorIntent(Context context, String errorId) {
    Intent intent = new Intent(ERROR_ACTION);
    intent.setClass(context, AndroidInvalidationService.class);
    intent.putExtra(ERROR_MESSAGE, errorId);
    return intent;
  }

  /**
   * Overrides the channel URL set in package metadata to enable dynamic port assignment and
   * configuration during testing.
   */
  
  static void setChannelUrlForTest(String url) {
    channelUrl = url;
  }

  /**
   * Resets the state of the service to destroy any existing clients
   */
  
  static void reset() {
    if (clientManager != null) {
      clientManager.releaseAll();
    }
  }

  /** The function for computing persistence state digests when rewriting them. */
  private final DigestFunction digestFn = new ObjectIdDigestUtils.Sha1DigestFunction();

  public AndroidInvalidationService() {
    lastInstanceForTest.set(this);
  }

  @Override
  public void onCreate() {
    synchronized (lock) {
      super.onCreate();

      // Create the client manager
      if (clientManager == null) {
        clientManager = new AndroidClientManager(this);
      }
      numCreateForTest.incrementAndGet();
    }
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    // Process GCM related messages from the ReceiverService. We do not check isCreated here because
    // this is part of the stop/start lifecycle, not bind/unbind.
    synchronized (lock) {
      logger.fine("Received action = %s", intent.getAction());
      if (MESSAGE_ACTION.equals(intent.getAction())) {
        handleMessage(intent);
      } else if (REGISTRATION_ACTION.equals(intent.getAction())) {
        handleRegistration(intent);
      } else if (ERROR_ACTION.equals(intent.getAction())) {
        handleError(intent);
      }
      final int retval = super.onStartCommand(intent, flags, startId);

      // Unless we are explicitly being asked to start, stop ourselves. Request.SERVICE_INTENT
      // is the intent used by InvalidationBinder to bind the service, and
      // AndroidInvalidationClientImpl uses the intent returned by InvalidationBinder.getIntent
      // as the argument to its startService call.
      if (!Request.SERVICE_INTENT.getAction().equals(intent.getAction())) {
        stopServiceIfNoClientsRemain(intent.getAction());
      }
      return retval;
    }
  }

  @Override
  public void onDestroy() {
    synchronized (lock) {
      reset();
      super.onDestroy();
    }
  }

  @Override
  public IBinder onBind(Intent intent) {
    return super.onBind(intent);
  }

  @Override
  public boolean onUnbind(Intent intent) {
    synchronized (lock) {
      logger.fine("onUnbind");
      super.onUnbind(intent);

      if ((clientManager != null) && (clientManager.getClientCount() > 0)) {
        // This isn't wrong, per se, but it's potentially unusual.
        logger.info(" clients still active in onUnbind");
      }
      stopServiceIfNoClientsRemain("onUnbind");

      // We don't care about the onRebind event, which is what the documentation says a "true"
      // return here will get us, but if we return false then we don't get a second onUnbind() event
      // in a bind/unbind/bind/unbind cycle, which we require.
      return true;
    }
  }

  // The following protected methods are called holding "lock" by AbstractInvalidationService.

  @Override
  protected void create(Request request, Response.Builder response) {
    String clientKey = request.getClientKey();
    int clientType = request.getClientType();
    Account account = request.getAccount();
    String authType = request.getAuthType();
    Intent eventIntent = request.getIntent();
    clientManager.create(clientKey, clientType, account, authType, eventIntent);
    response.setStatus(Status.SUCCESS);
  }

  @Override
  protected void resume(Request request, Response.Builder response) {
    String clientKey = request.getClientKey();
    AndroidClientProxy client = clientManager.get(clientKey);
    if (setResponseStatus(client, request, response)) {
      response.setAccount(client.getAccount());
      response.setAuthType(client.getAuthType());
    }
  }

  @Override
  protected void start(Request request, Response.Builder response) {
    String clientKey = request.getClientKey();
    AndroidInvalidationClient client = clientManager.get(clientKey);
    if (setResponseStatus(client, request, response)) {
      client.start();
    }
  }

  @Override
  protected void stop(Request request, Response.Builder response) {
    String clientKey = request.getClientKey();
    AndroidInvalidationClient client = clientManager.get(clientKey);
    if (setResponseStatus(client, request, response)) {
      client.stop();
    }
  }

  @Override
  protected void register(Request request, Response.Builder response) {
    String clientKey = request.getClientKey();
    AndroidInvalidationClient client = clientManager.get(clientKey);
    if (setResponseStatus(client, request, response)) {
      ObjectId objectId = request.getObjectId();
      client.register(objectId);
    }
  }

  @Override
  protected void unregister(Request request, Response.Builder response) {
    String clientKey = request.getClientKey();
    AndroidInvalidationClient client = clientManager.get(clientKey);
    if (setResponseStatus(client, request, response)) {
      ObjectId objectId = request.getObjectId();
      client.unregister(objectId);
    }
  }

  @Override
  protected void acknowledge(Request request, Response.Builder response) {
    String clientKey = request.getClientKey();
    AckHandle ackHandle = request.getAckHandle();
    AndroidInvalidationClient client = clientManager.get(clientKey);
    if (setResponseStatus(client, request, response)) {
      client.acknowledge(ackHandle);
    }
  }

  @Override
  protected void destroy(Request request, Response.Builder response) {
    String clientKey = request.getClientKey();
    AndroidInvalidationClient client = clientManager.get(clientKey);
    if (setResponseStatus(client, request, response)) {
      client.destroy();
    }
  }

  /**
   * If {@code client} is {@code null}, sets the {@code response} status to an error. Otherwise,
   * sets the status to {@code success}.
   * @return whether {@code client} was non-{@code null}.   *
   */
  private boolean setResponseStatus(AndroidInvalidationClient client, Request request,
      Response.Builder response) {
    if (client == null) {
      response.setError("Client does not exist: " + request);
      response.setStatus(Status.INVALID_CLIENT);
      return false;
    } else {
      response.setStatus(Status.SUCCESS);
      return true;
    }
  }

  /** Returns the base URL used to send messages to the outbound network channel */
  String getChannelUrl() {
    synchronized (lock) {
      return channelUrl;
    }
  }

  private void handleMessage(Intent intent) {
    numGcmMessagesForTest.incrementAndGet();
    String clientKey = intent.getStringExtra(MESSAGE_CLIENT_KEY);
    AndroidClientProxy proxy = clientManager.get(clientKey);

    // Client is unknown or unstarted; we can't deliver the message, but we need to
    // remember that we dropped it if the client is known.
    if ((proxy == null) || !proxy.isStarted()) {
      logger.warning("Dropping GCM message for unknown or unstarted client: %s", clientKey);
      handleGcmMessageForUnstartedClient(proxy);
      return;
    }

    // We can deliver the message. Pass the new echo token to the channel.
    String echoToken = intent.getStringExtra(MESSAGE_ECHO);
    logger.fine("Update %s with new echo token: %s", clientKey, echoToken);
    proxy.getChannel().updateEchoToken(echoToken);

    byte [] message = intent.getByteArrayExtra(MESSAGE_DATA);
    if (message != null) {
      logger.fine("Deliver to %s message %s", clientKey, message);
      proxy.getChannel().receiveMessage(message);
    } else {
      logger.severe("Got mailbox intent: %s", intent);
    }
  }

  /**
   * Handles receipt of a GCM message for a client that was unknown or not started. If the client
   * was unknown, drops the message. If the client was not started, rewrites the client's
   * persistent state to have a last-message-sent-time of 0, ensuring that the client will
   * send a heartbeat to the server when restarted. Since we drop the received GCM message,
   * the client will be disconnected by the invalidation pusher; this heartbeat ensures a
   * timely reconnection.
   */
  private void handleGcmMessageForUnstartedClient(AndroidClientProxy proxy) {
    if (proxy == null) {
      // Unknown client; nothing to do.
      return;
    }

    // Client is not started. Open its storage. We are going to use unsafe calls here that
    // bypass the normal storage API. This is safe in this context because we hold a lock
    // that prevents anyone else from starting this client or accessing its storage. We
    // really should not be holding a lock across I/O, but at least this is only local
    // file I/O, and we're only writing a few bytes. Additionally, since we currently only
    // have one Ticl, we should only ever enter this function if we're not being used for
    // anything else.
    final String clientKey = proxy.getClientKey();
    logger.info("Received message for unloaded client; rewriting state file: %s", clientKey);

    // This storage must have been loaded, because we got this proxy from the client manager,
    // which always ensures that its entries have that property.
    AndroidStorage storageForClient = proxy.getStorage();
    PersistentTiclState clientState = decodeTiclState(clientKey, storageForClient);
    if (clientState == null) {
      // Logging done in decodeTiclState.
      return;
    }

    // Rewrite the last message sent time.
    PersistentTiclState newState = PersistentTiclState.newBuilder(clientState)
        .setLastMessageSendTimeMs(0).build();

    // Serialize the new state.
    byte[] newClientState = PersistenceUtils.serializeState(newState, digestFn);

    // Write it out.
    storageForClient.getPropertiesUnsafe().put(InvalidationClientCore.CLIENT_TOKEN_KEY,
        newClientState);
    storageForClient.storeUnsafe();
  }

  private void handleRegistration(Intent intent) {
    // Notify the client manager of the updated registration ID
    String id = intent.getStringExtra(REGISTER_ID);
    clientManager.informRegistrationIdChanged();
    numGcmRegistrationForTest.incrementAndGet();
  }

  private void handleError(Intent intent) {
    logger.severe("Unable to perform GCM registration: %s", intent.getStringExtra(ERROR_MESSAGE));
    numGcmErrorsForTest.incrementAndGet();
  }

  /**
   * Stops the service if there are no clients in the client manager.
   * @param debugInfo short string describing why the check was made
   */
  private void stopServiceIfNoClientsRemain(String debugInfo) {
    if ((clientManager == null) || clientManager.areAllClientsStopped()) {
      logger.info("Stopping AndroidInvalidationService since no clients remain: %s", debugInfo);
      stopSelf();
    } else {
      logger.fine("Not stopping service since %s clients remain (%s)",
          clientManager.getClientCount(), debugInfo);
    }
  }

  /**
   * Returns the persisted state for the client with key {@code clientKey} in
   * {@code storageForClient}, or {@code null} if no valid state could be found.
   * <p>
   * REQUIRES: {@code storageForClient}.load() has been called successfully.
   */
  
  
  PersistentTiclState decodeTiclState(final String clientKey, AndroidStorage storageForClient) {
    synchronized (lock) {
      // Retrieve the serialized state.
      final Map<String, byte[]> properties = storageForClient.getPropertiesUnsafe();
      byte[] clientStateBytes = TypedUtil.mapGet(properties,
          InvalidationClientCore.CLIENT_TOKEN_KEY);
      if (clientStateBytes == null) {
        logger.warning("No client state found in storage for %s: %s", clientKey,
            properties.keySet());
        return null;
      }

      // Deserialize it.
      PersistentTiclState clientState =
          PersistenceUtils.deserializeState(logger, clientStateBytes, digestFn);
      if (clientState == null) {
        logger.warning("Invalid client state found in storage for %s", clientKey);
        return null;
      }
      return clientState;
    }
  }

  /**
   * Returns whether the client with {@code clientKey} is loaded in the client manager.
   */
  public static boolean isLoadedForTest(String clientKey) {
    return (getClientManager() != null) && getClientManager().isLoadedForTest(clientKey);
  }
}
