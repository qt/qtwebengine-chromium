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

package com.google.ipc.invalidation.external.client.android;

import com.google.common.base.Preconditions;

import android.accounts.Account;
import android.content.Context;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * Factory for obtaining an {@code InvalidationClient} for the Android platform. The {@link #create}
 * method will create a invalidation client associated with a particular application and user
 * account.
 * <p>
 * Applications should persist the unique client key for the new client so invalidation activity can
 * restart later if the application is removed from memory. An application can obtain an
 * invalidation client instance to resume activity by calling the {@link #resume} method with the
 * same application id that was originally passed to {@link #create}.
 *
 */
public class AndroidClientFactory {
  /**
   * A mapping of application id to invalidation client instances that can be used to
   * resume/reassociate an existing invalidation client. Client instances are not guaranteed (nor
   * required) to be reused.
   */
  private static final Map<String, AndroidInvalidationClientImpl> clientMap =
      new ConcurrentHashMap<String, AndroidInvalidationClientImpl>();

  /**
   * Starts a new invalidation client for the provided application and account token that will
   * deliver invalidation events to an instance of the provided listener component.
   * <p>
   * The implementation of this method is idempotent. If you call {@link #create} more than once
   * with the same application id, account, and listenerName values, all calls after the first one
   * are equivalent to just calling {@link #resume} with the same application id.
   *
   * @param context the context for the client.
   * @param clientKey a unique id that identifies the created client within the scope of the
   *        application.
   * @param account user account that is registering the invalidations.
   * @param authType the authentication token type that should be used to authenticate the client.
   * @param listenerClass the {@link AndroidInvalidationListener} subclass that is registered to
   *        receive the broadcast intents for invalidation events.
   */
  public static AndroidInvalidationClient create(Context context, String clientKey, int clientType,
      Account account, String authType,
      Class<? extends AndroidInvalidationListener> listenerClass) {
    Preconditions.checkNotNull(context, "context");
    Preconditions.checkNotNull(account, "account");
    Preconditions.checkArgument((authType != null) && (authType.length() != 0),
        "authType must be a non-empty string value");
    Preconditions.checkNotNull(listenerClass, "listenerClass");

    AndroidInvalidationClientImpl client = null;
    if (clientMap.containsKey(clientKey)) {
      return resume(context, clientKey);
    }
    if (client == null) {
      client = new AndroidInvalidationClientImpl(context, clientKey, clientType, account, authType,
          listenerClass);
      client.initialize();
      clientMap.put(clientKey, client);
    }
    return client;
  }

  /**
   * Creates a new AndroidInvalidationClient instance that is resuming processing for an existing
   * application id.
   * <p>
   * Use of this method is not recommended: use {@link #create} instead.
   *
   * @param context the context for the client.
   * @param clientKey a unique key that identifies the created client within the scope of the
   *        device.
   */
  public static AndroidInvalidationClient resume(Context context, String clientKey) {
    return resume(context, clientKey, true);
  }

  /**
   * Creates a new AndroidInvalidationClient instance that is resuming processing for an existing
   * application id.
   * <p>
   * Use of this method is not recommended: use {@link #create} instead.
   *
   * @param context the context for the client.
   * @param clientKey a unique key that identifies the created client within the scope of the
   *        device.
   * @param sendTiclResumeRequest whether to send a request to the service to resume the Ticl. If
   *        {@code false}, assumes the Ticl is already loaded at the service. This is used in the
   *         listener implementation and should not be set by clients.
   */
  public static AndroidInvalidationClient resume(Context context, String clientKey,
      boolean sendTiclResumeRequest) {
    Preconditions.checkNotNull(context, "context");

    // See if a cached entry is available with a matching application id
    AndroidInvalidationClientImpl client = clientMap.get(clientKey);
    if (client != null) {
      // Notify the client instance that it has multiple references.
      client.addReference();
    } else {
      // Attempt to resume the client using the invalidation service
      client = new AndroidInvalidationClientImpl(context, clientKey);
      client.initResumed(sendTiclResumeRequest);
    }
    return client;
  }

  /**
   * Release the client instance associated with the provided key.   Any transient resources
   * associated with the client in the factory will be released.
   *
   * @param clientKey the client to remove
   * @return {@code true} if a client with the provided key was found and releasedUUUU  .
   */
  static boolean release(String clientKey) {
    return clientMap.remove(clientKey) != null;
  }

  /**
   * Resets the state of the factory by dropping all cached client references.
   */
  
  static void resetForTest() {
    clientMap.clear();
  }
}
