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

import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidClientException;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.external.client.android.service.Response.Status;
import com.google.ipc.invalidation.ticl.InvalidationClientCore;
import com.google.ipc.invalidation.util.TypedUtil;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientConfigP;

import android.accounts.Account;
import android.content.Context;
import android.content.Intent;

import java.util.HashMap;
import java.util.Map;


/**
 * Manages active client instances for the Android invalidation service. The client manager contains
 * the code to create, persist, load, and lookup client instances, as well as handling the
 * propagation of any C2DM registration notifications to active clients.
 *
 */
public class AndroidClientManager {

  /** Logger */
  private static final Logger logger = AndroidLogger.forTag("InvClientManager");

  /**
   * The client configuration used creating new invalidation client instances.   This is normally
   * a constant but may be varied for testing.
   */
  private static ClientConfigP clientConfig = InvalidationClientCore.createConfig().build();

  /** The invalidation service associated with this manager */
  private final AndroidInvalidationService service;

  /**
   * When set, this registration ID is used rather than the ID returned by
   * {@code GCMRegistrar.getRegistrationId()}.
   */
  private static String registrationIdForTest;

  /** A map from client key to client proxy instances for in-memory client instances */
  private final Map<String, AndroidClientProxy> clientMap =
      new HashMap<String, AndroidClientProxy>();

  /** All client manager operations are synchronized on this lock */
  private final Object lock = new Object();

  /** Creates a new client manager instance associated with the provided service */
  AndroidClientManager(AndroidInvalidationService service) {
    this.service = service;
  }

  /**
   * Returns the number of managed clients.
   */
  int getClientCount() {
    synchronized (lock) {
      return clientMap.size();
    }
  }

  /**
   * Creates a new Android client proxy with the provided attributes. Before creating, will check to
   * see if there is an existing client with attributes that match and return it if found. If there
   * is an existing client with the same key but attributes that do not match, an exception will be
   * thrown. If no client with a matching key exists, a new client proxy will be created and
   * returned.
   *
   * @param clientKey key that uniquely identifies the client on the device.
   * @param clientType client type.
   * @param account user account associated with the client.
   * @param authType authentication type for the client.
   * @param eventIntent intent that can be used to bind to an event listener for the client.
   * @return an android invalidation client instance representing the client.
   */
  AndroidClientProxy create(String clientKey, int clientType, Account account, String authType,
      Intent eventIntent) {
    synchronized (lock) {

      // First check to see if an existing client is found
      AndroidClientProxy proxy = lookup(clientKey);
      if (proxy != null) {
        if (!proxy.getAccount().equals(account) || !proxy.getAuthType().equals(authType)) {
          throw new AndroidClientException(
              Status.INVALID_CLIENT, "Account does not match existing client");
        }
        return proxy;
      }

      // If not found, create a new client proxy instance to represent the client.
      AndroidStorage store = createAndroidStorage(service, clientKey);
      store.create(clientType, account, authType, eventIntent);
      proxy = new AndroidClientProxy(service, store, clientConfig);
      if (registrationIdForTest != null) {
        proxy.getChannel().setRegistrationIdForTest(registrationIdForTest);
      }
      clientMap.put(clientKey, proxy);
      logger.fine("Client %s created", clientKey);
      return proxy;
    }
  }

  /**
   * Retrieves an existing client that matches the provided key, loading it if necessary. If no
   * matching client can be found, an exception is thrown.
   *
   * @param clientKey the client key for the client to retrieve.
   * @return the matching client instance
   */
  AndroidClientProxy get(String clientKey) {
    synchronized (lock) {
      return lookup(clientKey);
    }
  }

  /**
   * Removes any client proxy instance associated with the provided key from memory but leaves the
   * instance persisted. The client may subsequently be loaded again by calling {@code #get}.
   *
   * @param clientKey the client key of the instance to remove from memory.
   */
  void remove(String clientKey) {
    synchronized (lock) {
      // Remove the proxy from the managed set and release any associated resources
      AndroidClientProxy proxy = clientMap.remove(clientKey);
      if (proxy != null) {
        proxy.release();
      }
    }
  }

  /**
   * Looks up the client proxy instance associated with the provided key and returns it (or {@code
   * null} if not found).
   *
   * @param clientKey the client key to look up
   * @return the client instance or {@code null}.
   */
  
  AndroidClientProxy lookup(String clientKey) {
    synchronized (lock) {
      // See if the client is already resident in memory
      AndroidClientProxy client = clientMap.get(clientKey);
      if (client == null) {
        // Attempt to load the client from the store
        AndroidStorage storage = createAndroidStorage(service, clientKey);
        if (storage.load()) {
          logger.fine("Client %s loaded from disk", clientKey);
          client = new AndroidClientProxy(service, storage, clientConfig);
          clientMap.put(clientKey, client);
        }
      }
      return client;
    }
  }

  /**
   * Sets the GCM registration ID that should be used for all managed clients (new and existing).
   */
  void informRegistrationIdChanged() {
    synchronized (lock) {
      // Propagate the value to all existing clients
      for (AndroidClientProxy proxy : clientMap.values()) {
        proxy.getChannel().informRegistrationIdChanged();
      }
    }
  }

  /**
   * Releases all managed clients and drops them from the managed set.
   */
  void releaseAll() {
    synchronized (lock) {
      for (AndroidClientProxy clientProxy : clientMap.values()) {
        clientProxy.release();
      }
      clientMap.clear();
    }
  }

  /**
   * Returns an android storage instance for managing client state.
   */
  
  protected AndroidStorage createAndroidStorage(Context context, String clientKey) {
    synchronized (lock) {
      return new AndroidStorage(context, clientKey);
    }
  }

  
  static ClientConfigP setConfigForTest(ClientConfigP newConfig) {
    logger.info("Setting client configuration: %s", newConfig);
    ClientConfigP currentConfig = clientConfig;
    clientConfig = newConfig;
    return clientConfig;
  }

  
  public static void setRegistrationIdForTest(String registrationIdForTest) {
    AndroidClientManager.registrationIdForTest = registrationIdForTest;
  }

  /** Returns whether all loaded clients are stopped. */
  public boolean areAllClientsStopped() {
    synchronized (lock) {
      for (AndroidClientProxy proxy : clientMap.values()) {
        if (proxy.isStarted()) {
          return false;
        }
      }
      return true;
    }
  }

  /** Returns whether the client with key {@code clientKey} is in memory. */
  public boolean isLoadedForTest(String clientKey) {
    synchronized (lock) {
      return TypedUtil.containsKey(clientMap, clientKey);
    }
  }
}
