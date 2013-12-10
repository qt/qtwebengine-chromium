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

import com.google.ipc.invalidation.external.client.InvalidationListener;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.external.client.android.service.Event;
import com.google.ipc.invalidation.external.client.android.service.Event.Action;
import com.google.ipc.invalidation.external.client.android.service.ListenerService;
import com.google.ipc.invalidation.external.client.android.service.Response;
import com.google.ipc.invalidation.external.client.types.AckHandle;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.ipc.invalidation.external.client.types.Invalidation;
import com.google.ipc.invalidation.external.client.types.ObjectId;

import android.app.Service;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;

/**
 * An abstract base class for implementing a {@link Service} component
 * that handles events from the invalidation service. This class should be
 * subclassed and concrete implementations of the {@link InvalidationListener}
 * methods added to provide application-specific handling of invalidation
 * events.
 * <p>
 * This implementing subclass should be registered in {@code
 * AndroidManifest.xml} as a service of the invalidation
 * listener binding intent, as in the following sample fragment:
 *
 * <pre>
 * {@code
 * <manifest ...>
 *   <application ...>
 *     ...
 *     service android:name="com.myco.example.AppListenerService" ...>
 *       <intent-filter>
 *         <action android:name="com.google.ipc.invalidation.LISTENER"/>
 *       </intent-filter>
 *     </receiver>
 *     ...
 *   <application>
 *   ...
 * </manifest>
 * }
 * </pre>
 *
 */
public abstract class AndroidInvalidationListener extends Service
    implements InvalidationListener {

  /** Logger */
  private static final Logger logger = AndroidLogger.forTag("InvListener");

  /**
   * Simple service stub that delegates back to methods on the service.
   */
  private final ListenerService.Stub listenerBinder = new ListenerService.Stub() {

    @Override
    public void handleEvent(Bundle input, Bundle output) {
      AndroidInvalidationListener.this.handleEvent(input, output);
    }
  };

  /** Lock over all state in this class. */
  private final Object lock = new Object();

  /** Whether the service is in the created state. */
  private boolean isCreated = false;

  @Override
  public void onCreate() {
    synchronized (lock) {
      super.onCreate();
      logger.fine("onCreate: %s", this.getClass());
      this.isCreated = true;
    }
  }

  @Override
  public void onDestroy() {
    synchronized (lock) {
      logger.fine("onDestroy: %s", this.getClass());
      this.isCreated = false;
      super.onDestroy();
    }
  }

  @Override
  public IBinder onBind(Intent arg0) {
    synchronized (lock) {
      logger.fine("Binding: %s", arg0);
      return listenerBinder;
    }
  }

  /**
   * Handles a {@link ListenerService#handleEvent} call received by the
   * listener service.
   *
   * @param input bundle containing event parameters.
   * @param output bundled used to return response to the invalidation service.
   */
  protected void handleEvent(Bundle input, Bundle output) {
    synchronized (lock) {
      if (!isCreated) {
        logger.warning("Dropping bundle since not created: %s", input);
        return;
      }
      Event event = new Event(input);
      Response.Builder response = Response.newBuilder(event.getActionOrdinal(), output);
      // All events should contain an action and client id
      Action action = event.getAction();
      String clientKey = event.getClientKey();
      logger.fine("Received %s event for %s", action, clientKey);

      AndroidInvalidationClient client = null;
      try {
        if (clientKey == null) {
          throw new IllegalStateException("Missing client id:" + event);
        }

        // Obtain the client instance for the client receiving the event. Do not attempt to load it
        // at the service: if a Ticl has been unloaded, the listener shouldn't resurrect it, because
        // that can lead to a zombie client.
        client = AndroidClientFactory.resume(this, clientKey, false);

        // Determine the event type based upon the request action, extract parameters
        // from extras, and invoke the listener event handler method.
        logger.fine("%s event for %s", action, clientKey);
        switch(action) {
          case READY:
          {
            ready(client);
            break;
          }
          case INVALIDATE:
          {
            Invalidation invalidation = event.getInvalidation();
            AckHandle ackHandle = event.getAckHandle();
            invalidate(client, invalidation, ackHandle);
            break;
          }
          case INVALIDATE_UNKNOWN:
          {
            ObjectId objectId = event.getObjectId();
            AckHandle ackHandle = event.getAckHandle();
            invalidateUnknownVersion(client, objectId, ackHandle);
            break;
          }
          case INVALIDATE_ALL:
          {
            AckHandle ackHandle = event.getAckHandle();
            invalidateAll(client, ackHandle);
            break;
          }
          case INFORM_REGISTRATION_STATUS:
          {
            ObjectId objectId = event.getObjectId();
            RegistrationState state = event.getRegistrationState();
            informRegistrationStatus(client, objectId, state);
            break;
          }
          case INFORM_REGISTRATION_FAILURE:
          {
            ObjectId objectId = event.getObjectId();
            String errorMsg = event.getError();
            boolean isTransient = event.getIsTransient();
            informRegistrationFailure(client, objectId, isTransient, errorMsg);
            break;
          }
          case REISSUE_REGISTRATIONS:
          {
            byte[] prefix = event.getPrefix();
            int prefixLength = event.getPrefixLength();
            reissueRegistrations(client, prefix, prefixLength);
            break;
          }
          case INFORM_ERROR:
          {
            ErrorInfo errorInfo = event.getErrorInfo();
            informError(client, errorInfo);
            break;
          }
          default:
            logger.warning("Urecognized event: %s", event);
        }
        response.setStatus(Response.Status.SUCCESS);
      } catch (RuntimeException re) {
        // If an exception occurs during processing, log it, and store the
        // result in the response sent back to the service.
        logger.severe("Failure in handleEvent", re);
        response.setError(re.getMessage());
      } finally {
        // Listeners will only use a client reference for the life of the event and release
        // it immediately since there is no way to know if additional events are coming.
        if (client != null) {
          client.release();
        }
      }
    }
  }
}
