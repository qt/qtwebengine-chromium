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

package com.google.ipc.invalidation.ticl.android.c2dm;


import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;

import android.app.IntentService;
import android.content.Context;
import android.content.Intent;

/**
 * Service base class that receives events for C2DM registrations and messages.  Subclasses
 * should override the {@code onYYY} event handler methods to add appropriate logic for
 * registration or message handling.
 */
public abstract class BaseC2DMReceiver extends IntentService {
  private static final Logger logger = AndroidLogger.forTag("BaseC2DMReceiver");

  /**
   * If {@code true} indicates that the wakelock associated with messages should be automatically
   * released after the {onYYY} handler has been called. Otherwise, the subclass is responsible for
   * releasing the lock (if any).
   */
  private final boolean automaticallyReleaseWakelock;

  /**
   * Creates a new receiver instance
   *
   * @param name the name for the receiver service.  Used only for debug logging.
   * @param automaticallyReleaseWakeLock if {@code true} indicates that the wakelock associated with
   *        messages should be automatically released after the {onYYY} handler has been called.
   *        Otherwise, the subclass is responsible for releasing the lock (if any).
   */
  protected BaseC2DMReceiver(String name, boolean automaticallyReleaseWakeLock) {
    super(name);
    // Always redeliver if evicted while processing intents.
    setIntentRedelivery(true);
    this.automaticallyReleaseWakelock = automaticallyReleaseWakeLock;
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    logger.fine("Handle intent: %s", intent);
    if (intent == null) {
      logger.warning("Ignoring null intent");
      return;
    }
    try {
      // Examine the action and raise the appropriate onYYY event
      if (intent.getAction().equals(C2DMessaging.ACTION_MESSAGE)) {
        onMessage(getApplicationContext(), intent);
      } else if (intent.getAction().equals(C2DMessaging.ACTION_REGISTRATION_ERROR)) {
        onRegistrationError(getApplicationContext(),
            intent.getExtras().getString(C2DMessaging.EXTRA_REGISTRATION_ERROR));
      } else if (intent.getAction().equals(C2DMessaging.ACTION_REGISTERED)) {
        onRegistered(getApplicationContext(),
            intent.getExtras().getString(C2DMessaging.EXTRA_REGISTRATION_ID));
      } else if (intent.getAction().equals(C2DMessaging.ACTION_UNREGISTERED)) {
        onUnregistered(getApplicationContext());
      }
    } finally {
      if (automaticallyReleaseWakelock) {
        releaseWakeLock(intent);
      }
    }
  }

  /**
   * Called when a cloud message has been received.
   *
   * @param context the context the intent was received in
   * @param intent the received intent
   */
  protected abstract void onMessage(Context context, Intent intent);

  /**
   * Called on registration error. Override to provide better error messages.
   *
   * This is called in the context of a Service - no dialog or UI.
   *
   * @param context the context the intent was received in
   * @param errorId the errorId String
   */
  protected abstract void onRegistrationError(Context context, String errorId);

  /**
   * Called when a registration token has been received.
   *
   * @param context the context the intent was received in
   * @param registrationId the registration ID received from C2DM
   */
  protected abstract void onRegistered(Context context, String registrationId);

  /**
   * Called when the device has been unregistered.
   *
   * @param context the context of the received intent
   */
  protected abstract void onUnregistered(Context context);

  /**
   * Releases the WakeLock registered to the current class.
   *
   * The WakeLock is only released if the extra C2DMessaging.EXTRA_RELEASE_WAKELOCK is true.
   *
   * @param intent the intent to check for the flag to release the wakelock
   */
  protected final void releaseWakeLock(Intent intent) {
    if (intent.getBooleanExtra(C2DMessaging.EXTRA_RELEASE_WAKELOCK, false)) {
      WakeLockManager.getInstance(getApplicationContext()).release(getClass());
    }
  }
}
