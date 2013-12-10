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

package com.google.ipc.invalidation.testing.android;

import com.google.ipc.invalidation.external.client.InvalidationClient;
import com.google.ipc.invalidation.external.client.InvalidationListener;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.AndroidInvalidationClient;
import com.google.ipc.invalidation.external.client.android.AndroidInvalidationListener;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.external.client.android.service.Event;
import com.google.ipc.invalidation.external.client.android.service.Response;
import com.google.ipc.invalidation.external.client.types.AckHandle;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.ipc.invalidation.external.client.types.Invalidation;
import com.google.ipc.invalidation.external.client.types.ObjectId;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;

import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;

/**
 * TestListener service maintains a mapping of listeners by client key and forwards received events
 * to an InvalidationListener instance.   The listener should be registered in
 * {@code Android-Manifest.xml} as follows:
 *
 * {@code
 * <service
 *   android:name="com.google.ipc.invalidation.testing.android.InvalidationTestListener">
 *  <intent-filter>
 *     <action android:name="com.google.ipc.invalidation.EVENTS"/>
 *  </intent-filter>
 * </service>
 * }
 *
 */
public class InvalidationTestListener extends AndroidInvalidationListener {

  /** Logger */
  private static final Logger logger = AndroidLogger.forTag("InvTestListener");

  private static final Map<String, InvalidationListener> listenerMap =
      new ConcurrentHashMap<String, InvalidationListener>();

  /**
   * Creates and returns an intent that is valid for use in creating a new invalidation client
   * that will deliver events to the test listener.
   */
  public static Intent getEventIntent(Context context) {
    Intent eventIntent = new Intent(Event.LISTENER_INTENT);
    ComponentName component = new ComponentName(context.getPackageName(),
        InvalidationTestListener.class.getName());
    eventIntent.setComponent(component);
    return eventIntent;
  }

  /**
   * Sets the invalidation listener delegate to receive events for a given clientKey.
   */
  public static void setInvalidationListener(String clientKey, InvalidationListener listener) {
    logger.fine("setListener %s for %s", listener, clientKey);
    listenerMap.put(clientKey, listener);
  }

  /**
   * Removes the invalidation listener delegate to receive events for a given clientKey.
   */
  public static void removeInvalidationListener(String clientKey) {
    listenerMap.remove(clientKey);
  }


  @Override
  protected void handleEvent(Bundle input, Bundle output) {

    // Ignore events that target a client key where there is no listener registered
    // It's likely that these are late-delivered events for an earlier test case.
    Event event = new Event(input);
    String clientKey = event.getClientKey();
    if (!listenerMap.containsKey(clientKey)) {
      logger.fine("Ignoring %s event to %s", event.getAction(), clientKey);
      Response.Builder response = Response.newBuilder(event.getActionOrdinal(), output);
      response.setStatus(Response.Status.SUCCESS);
      return;
    }

    super.handleEvent(input, output);
  }

  @Override
  public void ready(InvalidationClient client) {
    InvalidationListener listener = getListener(client);
    logger.fine("Received READY for %s: %s", getClientKey(client), listener);
    if (listener != null) {
      listener.ready(client);
    }
  }

  @Override
  public void invalidate(
      InvalidationClient client, Invalidation invalidation, AckHandle ackHandle) {
    InvalidationListener listener = getListener(client);
    logger.fine("Received INVALIDATE for %s: %s", getClientKey(client), listener);
    if (listener != null) {
      listener.invalidate(client, invalidation, ackHandle);
    }
  }

  @Override
  public void invalidateUnknownVersion(
      InvalidationClient client, ObjectId objectId, AckHandle ackHandle) {
    InvalidationListener listener = getListener(client);
    logger.fine("Received INVALIDATE_UNKNOWN_VERSION for %s: %s", getClientKey(client), listener);
    if (listener != null) {
      listener.invalidateUnknownVersion(client, objectId, ackHandle);
    }
  }

  @Override
  public void invalidateAll(InvalidationClient client, AckHandle ackHandle) {
    InvalidationListener listener = getListener(client);
    logger.fine("Received INVALIDATE_ALL for %s: %s", getClientKey(client), listener);
    if (listener != null) {
      listener.invalidateAll(client, ackHandle);
    }
  }

  @Override
  public void informRegistrationStatus(
      InvalidationClient client, ObjectId objectId, RegistrationState regState) {
    InvalidationListener listener = getListener(client);
    logger.fine("Received INFORM_REGISTRATION_STATUS for %s: %s", getClientKey(client), listener);
    if (listener != null) {
      listener.informRegistrationStatus(client, objectId, regState);
    }
  }

  @Override
  public void informRegistrationFailure(
      InvalidationClient client, ObjectId objectId, boolean isTransient, String errorMessage) {
    InvalidationListener listener = getListener(client);
    logger.fine("Received INFORM_REGISTRATION_FAILURE for %s: %s", getClientKey(client), listener);
    if (listener != null) {
      listener.informRegistrationFailure(client, objectId, isTransient, errorMessage);
    }
  }

  @Override
  public void reissueRegistrations(InvalidationClient client, byte[] prefix, int prefixLength) {
    InvalidationListener listener = getListener(client);
    logger.fine("Received REISSUE_REGISTRATIONS for %s: %s", getClientKey(client), listener);
    if (listener != null) {
      listener.reissueRegistrations(client, prefix, prefixLength);
    }
  }

  @Override
  public void informError(InvalidationClient client, ErrorInfo errorInfo) {
    InvalidationListener listener = getListener(client);
    logger.fine("Received INFORM_ERROR for %s: %s", getClientKey(client), listener);
    if (listener != null) {
      listener.informError(client, errorInfo);
    }
  }

  private String getClientKey(InvalidationClient client) {
    return ((AndroidInvalidationClient) client).getClientKey();
  }

  private InvalidationListener getListener(InvalidationClient client) {
    String clientKey = getClientKey(client);
    return listenerMap.get(clientKey);
  }
}
