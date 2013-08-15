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

import com.google.android.gcm.GCMRegistrar;
import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.common.CommonProtos2;
import com.google.ipc.invalidation.external.client.SystemResources;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.SystemResources.NetworkChannel;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.ticl.TestableNetworkChannel;
import com.google.ipc.invalidation.util.ExponentialBackoffDelayGenerator;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protos.ipc.invalidation.AndroidChannel.AddressedAndroidMessage;
import com.google.protos.ipc.invalidation.AndroidChannel.MajorVersion;
import com.google.protos.ipc.invalidation.Channel.NetworkEndpointId;
import com.google.protos.ipc.invalidation.ClientProtocol.Version;

import android.accounts.AccountManager;
import android.accounts.AccountManagerCallback;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.content.Context;
import android.net.http.AndroidHttpClient;
import android.os.Build;
import android.os.Bundle;
import android.util.Base64;

import org.apache.http.client.HttpClient;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.Random;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicInteger;


/**
 * Provides a bidirectional channel for Android devices using GCM (data center to device) and the
 * Android HTTP frontend (device to data center). The android channel computes a network endpoint id
 * based upon the GCM registration ID for the containing application ID and the client key of the
 * client using the channel. If an attempt is made to send messages on the channel before a GCM
 * registration ID has been assigned, it will temporarily buffer the outbound messages and send them
 * when the registration ID is eventually assigned.
 *
 */
class AndroidChannel extends AndroidChannelBase implements TestableNetworkChannel {

  private static final Logger logger = AndroidLogger.forTag("InvChannel");

  /**
   * The maximum number of outbound messages that will be buffered while waiting for async delivery
   * of the GCM registration ID and authentication token.   The general flow for a new client is
   * that an 'initialize' message is sent (and resent on a timed interval) until a session token is
   * sent back and this just prevents accumulation a large number of initialize messages (and
   * consuming memory) in a long delay or failure scenario.
   */
  private static final int MAX_BUFFERED_MESSAGES = 10;

  /** The channel version expected by this channel implementation */
  
  static final Version CHANNEL_VERSION =
      CommonProtos2.newVersion(MajorVersion.INITIAL.getNumber(), 0);

  /** How to long to wait initially before retrying a failed auth token request. */
  private static final int INITIAL_AUTH_TOKEN_RETRY_DELAY_MS = 1 * 1000;  // 1 second

  /** Largest exponential backoff factor to use for auth token retries. */
  private static final int MAX_AUTH_TOKEN_RETRY_FACTOR = 60 * 60 * 12; // 12 hours

  /** Number of C2DM messages for unknown clients. */
  
  static final AtomicInteger numGcmInvalidClients = new AtomicInteger();

  /** Invalidation client proxy using the channel. */
  private final AndroidClientProxy proxy;

  /** Android context used to retrieve registration IDs. */
  private final Context context;

  /** System resources for this channel */
  private SystemResources resources;

  /**
   * When set, this registration ID is used rather than checking
   * {@link GCMRegistrar#getRegistrationId}. It should not be read directly: call
   * {@link #getRegistrationId} instead.
   */
  private String registrationIdForTest;

  /** The authentication token that can be used in channel requests to the server */
  private String authToken;

  /** Listener for network events. */
  private NetworkChannel.NetworkListener listener;

  // TODO: Add code to track time of last network activity (in either direction)
  // so inactive clients can be detected and periodically flushed from memory.

  /**
   * List that holds outbound messages while waiting for a registration ID.   Allocated on
   * demand since it is only needed when there is no registration id.
   */
  private List<byte[]> pendingMessages = null;

  /**
   * Testing only flag that disables interactions with the AcccountManager for mock tests.
   */
   static boolean disableAccountManager = false;

  /**
   * Returns the default HTTP client to use for requests from the channel based upon its execution
   * context.  The format of the User-Agent string is "<application-pkg>(<android-release>)".
   */
  static AndroidHttpClient getDefaultHttpClient(Context context) {
    return AndroidHttpClient.newInstance(
       context.getApplicationInfo().className + "(" + Build.VERSION.RELEASE + ")");
  }

  /** Executor used for HTTP calls to send messages to . */
  
  final ExecutorService scheduler = Executors.newSingleThreadExecutor();

  /**
   * Creates a new AndroidChannel.
   *
   * @param proxy the client proxy associated with the channel
   * @param httpClient the HTTP client to use to communicate with the Android invalidation frontend
   * @param context Android context
   */
  AndroidChannel(AndroidClientProxy proxy, HttpClient httpClient, Context context) {
    super(httpClient, proxy.getAuthType(), proxy.getService().getChannelUrl());
    this.proxy = Preconditions.checkNotNull(proxy);
    this.context = Preconditions.checkNotNull(context);
  }

  /**
   * Returns the GCM registration ID associated with the channel. Checks the {@link GCMRegistrar}
   * unless {@link #setRegistrationIdForTest} has been called.
   */
  String getRegistrationId() {
    String registrationId = (registrationIdForTest != null) ? registrationIdForTest :
        GCMRegistrar.getRegistrationId(context);

    // Callers check for null registration ID rather than "null or empty", so replace empty strings
    // with null here.
    if ("".equals(registrationId)) {
      registrationId = null;
    }
    return registrationId;
  }

  /** Returns the client proxy that is using the channel */
   AndroidClientProxy getClientProxy() {
    return proxy;
  }

  /**
   * Retrieves the list of pending messages in the channel (or {@code null} if there are none).
   */
   List<byte[]> getPendingMessages() {
    return pendingMessages;
  }

  @Override
  
  protected String getAuthToken() {
    return authToken;
  }

  /** A completion callback for an asynchronous operation. */
  interface CompletionCallback {
    void success();
    void failure();
  }

  /** An asynchronous runnable that calls a completion callback. */
  interface AsyncRunnable {
    void run(CompletionCallback callback);
  }

  /**
   * A utility function to run an async runnable with exponential backoff after failures.
   * @param runnable the asynchronous runnable.
   * @param scheduler used to schedule retries.
   * @param backOffGenerator a backoff generator that returns how to long to wait between retries.
   *     The client must pass a new instance or reset the backoff generator before calling this
   *     method.
   */
  
  static void retryUntilSuccessWithBackoff(final SystemResources.Scheduler scheduler,
      final ExponentialBackoffDelayGenerator backOffGenerator, final AsyncRunnable runnable) {
    logger.fine("Running %s", runnable);
    runnable.run(new CompletionCallback() {
        @Override
        public void success() {
          logger.fine("%s succeeded", runnable);
        }

        @Override
        public void failure() {
          int nextDelay = backOffGenerator.getNextDelay();
          logger.fine("%s failed, retrying after %s ms", nextDelay);
          scheduler.schedule(nextDelay, new Runnable() {
            @Override
            public void run() {
              retryUntilSuccessWithBackoff(scheduler, backOffGenerator, runnable);
            }
          });
        }
    });
  }

  /**
   * Initiates acquisition of an authentication token that can be used with channel HTTP requests.
   * Android token acquisition is asynchronous since it may require HTTP interactions with the
   * ClientLogin servers to obtain the token.
   */
  @SuppressWarnings("deprecation")
  
  synchronized void requestAuthToken(final CompletionCallback callback) {
    // If there is currently no token and no pending request, initiate one.
    if (disableAccountManager) {
      logger.fine("Not requesting auth token since account manager disabled");
      return;
    }
    if (authToken == null) {
      // Ask the AccountManager for the token, with a pending future to store it on the channel
      // once available.
      final AndroidChannel theChannel = this;
      AccountManager accountManager = AccountManager.get(proxy.getService());
      accountManager.getAuthToken(proxy.getAccount(), proxy.getAuthType(), true,
          new AccountManagerCallback<Bundle>() {
            @Override
            public void run(AccountManagerFuture<Bundle> future) {
              try {
                Bundle result = future.getResult();
                if (result.containsKey(AccountManager.KEY_INTENT)) {
                  // TODO: Handle case where there are no authentication
                  // credentials associated with the client account
                  logger.severe("Token acquisition requires user login");
                  callback.success(); // No further retries.
                }
                setAuthToken(result.getString(AccountManager.KEY_AUTHTOKEN));
              } catch (OperationCanceledException exception) {
                logger.warning("Auth cancelled", exception);
                // TODO: Send error to client
              } catch (AuthenticatorException exception) {
                logger.warning("Auth error acquiring token", exception);
                callback.failure();
              } catch (IOException exception) {
                logger.warning("IO Exception acquiring token", exception);
                callback.failure();
              }
            }
      }, null);
    } else {
      logger.fine("Auth token request already pending");
      callback.success();
    }
  }

  /*
   * Updates the registration ID for this channel, flushing any pending outbound messages that
   * were waiting for an id.
   */
  synchronized void setRegistrationIdForTest(String updatedRegistrationId) {
    // Synchronized to avoid concurrent access to pendingMessages
    if (registrationIdForTest != updatedRegistrationId) {
      logger.fine("Setting registration ID for test for client key %s", proxy.getClientKey());
      registrationIdForTest = updatedRegistrationId;
      informRegistrationIdChanged();
    }
  }

  /**
   * Call to inform the Android channel that the registration ID has changed. May kick loose some
   * pending outbound messages.
   */
  synchronized void informRegistrationIdChanged() {
    checkReady();
  }

  /**
   * Sets the authentication token to use for HTTP requests to the invalidation frontend and
   * flushes any pending messages (if appropriate).
   *
   * @param authToken the authentication token
   */
  synchronized void setAuthToken(String authToken) {
    logger.fine("Auth token received fo %s", proxy.getClientKey());
    this.authToken = authToken;
    checkReady();
  }

  @Override
  public void setListener(NetworkChannel.NetworkListener listener) {
    this.listener = Preconditions.checkNotNull(listener);
  }

  @Override
  public synchronized void sendMessage(final byte[] outgoingMessage) {
    // synchronized to avoid concurrent access to pendingMessages

    // If there is no registration id, we cannot compute a network endpoint id. If there is no
    // auth token, then we cannot authenticate the send request.  Defer sending messages until both
    // are received.
    String registrationId = getRegistrationId();
    if ((registrationId == null) || (authToken == null)) {
      if (pendingMessages == null) {
        pendingMessages = new ArrayList<byte[]>();
      }
      logger.fine("Buffering outbound message: hasRegId: %s, hasAuthToken: %s",
          registrationId != null, authToken != null);
      if (pendingMessages.size() < MAX_BUFFERED_MESSAGES) {
        pendingMessages.add(outgoingMessage);
      } else {
        logger.warning("Exceeded maximum number of buffered messages, dropping outbound message");
      }
      return;
    }

    // Do the actual HTTP I/O on a separate thread, since we may be called on the main
    // thread for the application.
    scheduler.execute(new Runnable() {
      @Override
      public void run() {
        if (resources.isStarted()) {
          deliverOutboundMessage(outgoingMessage);
        } else {
          logger.warning("Dropping outbound messages because resources are stopped");
        }
      }
    });
  }

  /**
   * Called when either the registration or authentication token has been received to check to
   * see if channel is ready for network activity.  If so, the status receiver is notified and
   * any pending messages are flushed.
   */
  private synchronized void checkReady() {
    String registrationId = getRegistrationId();
    if ((registrationId != null) && (authToken != null)) {

      logger.fine("Enabling network endpoint: %s", getWebEncodedEndpointId());

      // Notify the network listener that we are now network enabled
      if (listener != null) {
        listener.onOnlineStatusChange(true);
      }

      // Flush any pending messages
      if (pendingMessages != null) {
        for (byte [] message : pendingMessages) {
          sendMessage(message);
        }
        pendingMessages = null;
      }
    }
  }

  void receiveMessage(byte[] inboundMessage) {
    try {
      AddressedAndroidMessage addrMessage = AddressedAndroidMessage.parseFrom(inboundMessage);
      tryDeliverMessage(addrMessage);
    } catch (InvalidProtocolBufferException exception) {
      logger.severe("Failed decoding AddressedAndroidMessage as C2DM payload", exception);
    }
  }

  /**
   * Delivers the payload of {@code addrMessage} to the {@code callbackReceiver} if the client key
   * of the addressed message matches that of the {@link #proxy}.
   */
  @Override
  protected void tryDeliverMessage(AddressedAndroidMessage addrMessage) {
    String clientKey = proxy.getClientKey();
    if (addrMessage.getClientKey().equals(clientKey)) {
      logger.fine("Deliver to %s message %s", clientKey, addrMessage);
      listener.onMessageReceived(addrMessage.getMessage().toByteArray());
    } else {
      logger.severe("Not delivering message due to key mismatch: %s vs %s",
          addrMessage.getClientKey(), clientKey);
      numGcmInvalidClients.incrementAndGet();
    }
  }

  /** Returns the web encoded version of the channel network endpoint ID for HTTP requests. */
  @Override
  protected String getWebEncodedEndpointId() {
    NetworkEndpointId networkEndpointId = getNetworkId();
    return Base64.encodeToString(networkEndpointId.toByteArray(),
        Base64.URL_SAFE | Base64.NO_WRAP  | Base64.NO_PADDING);
  }

  @Override
  public void setSystemResources(SystemResources resources) {
    this.resources = resources;

    // Prefetch the auth sub token.  Since this might require an HTTP round trip, we do this
    // as soon as the resources are available.
    // TODO: Find a better place to fetch the auth token; this method
    // doesn't sound like one that should be doing work.
    retryUntilSuccessWithBackoff(resources.getInternalScheduler(),
        new ExponentialBackoffDelayGenerator(
            new Random(), INITIAL_AUTH_TOKEN_RETRY_DELAY_MS, MAX_AUTH_TOKEN_RETRY_FACTOR),
        new AsyncRunnable() {
          @Override
          public void run(CompletionCallback callback) {
            requestAuthToken(callback);
          }
        });
  }

  @Override
  public NetworkEndpointId getNetworkIdForTest() {
    return getNetworkId();
  }

  @Override
  protected Logger getLogger() {
    return resources.getLogger();
  }

  private NetworkEndpointId getNetworkId() {
    String registrationId = getRegistrationId();
    return CommonProtos2.newAndroidEndpointId(registrationId, proxy.getClientKey(),
        proxy.getService().getPackageName(), CHANNEL_VERSION);
  }

  ExecutorService getExecutorServiceForTest() {
    return scheduler;
  }

  @Override
  void setHttpClientForTest(HttpClient client) {
    if (this.httpClient instanceof AndroidHttpClient) {
      // Release the previous client if any.
      ((AndroidHttpClient) this.httpClient).close();
    }
    super.setHttpClientForTest(client);
  }
}
