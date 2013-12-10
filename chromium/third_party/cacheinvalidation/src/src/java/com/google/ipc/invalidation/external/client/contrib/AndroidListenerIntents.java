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
package com.google.ipc.invalidation.external.client.contrib;

import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.external.client.contrib.AndroidListener.AlarmReceiver;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.ipc.invalidation.ticl.android2.AndroidClock;
import com.google.ipc.invalidation.ticl.android2.AndroidTiclManifest;
import com.google.ipc.invalidation.ticl.android2.channel.AndroidChannelConstants.AuthTokenConstants;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protos.ipc.invalidation.AndroidListenerProtocol.RegistrationCommand;
import com.google.protos.ipc.invalidation.AndroidListenerProtocol.StartCommand;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.app.PendingIntent.CanceledException;
import android.content.Context;
import android.content.Intent;


/**
 * Static helper class supporting construction and decoding of intents issued and handled by the
 * {@link AndroidListener}.
 *
 */
class AndroidListenerIntents {

  /** The logger. */
  private static final Logger logger = AndroidLogger.forPrefix("");

  /** Key of Intent byte[] holding a {@link RegistrationCommand} protocol buffer. */
  static final String EXTRA_REGISTRATION =
      "com.google.ipc.invalidation.android_listener.REGISTRATION";

  /** Key of Intent byte[] holding a {@link StartCommand} protocol buffer. */
  static final String EXTRA_START =
      "com.google.ipc.invalidation.android_listener.START";

  /** Key of Intent extra indicating that the client should stop. */
  static final String EXTRA_STOP =
      "com.google.ipc.invalidation.android_listener.STOP";

  /** Key of Intent extra holding a byte[] that is ack handle data. */
  static final String EXTRA_ACK =
      "com.google.ipc.invalidation.android_listener.ACK";

  /**
   * Issues the given {@code intent} to the TICL service class registered in the {@code context}.
   */
  static void issueTiclIntent(Context context, Intent intent) {
    context.startService(intent.setClassName(context,
        new AndroidTiclManifest(context).getTiclServiceClass()));
  }

  /**
   * Issues the given {@code intent} to the {@link AndroidListener} class registered in the
   * {@code context}.
   */
  static void issueAndroidListenerIntent(Context context, Intent intent) {
    context.startService(setAndroidListenerClass(context, intent));
  }

  /**
   * Returns the ack handle from the given intent if it has the appropriate extra. Otherwise,
   * returns {@code null}.
   */
  static byte[] findAckHandle(Intent intent) {
    return intent.getByteArrayExtra(EXTRA_ACK);
  }

  /**
   * Returns {@link RegistrationCommand} extra from the given intent or null if no valid
   * registration command exists.
   */
  static RegistrationCommand findRegistrationCommand(Intent intent) {
    // Check that the extra exists.
    byte[] data = intent.getByteArrayExtra(EXTRA_REGISTRATION);
    if (null == data) {
      return null;
    }

    // Attempt to parse the extra.
    try {
      return RegistrationCommand.parseFrom(data);
    } catch (InvalidProtocolBufferException exception) {
      logger.warning("Received invalid proto: %s", exception);
      return null;
    }
  }

  /**
   * Returns {@link StartCommand} extra from the given intent or null if no valid start command
   * exists.
   */
  static StartCommand findStartCommand(Intent intent) {
    // Check that the extra exists.
    byte[] data = intent.getByteArrayExtra(EXTRA_START);
    if (null == data) {
      return null;
    }

    // Attempt to parse the extra.
    try {
      return StartCommand.parseFrom(data);
    } catch (InvalidProtocolBufferException exception) {
      logger.warning("Received invalid proto: %s", exception);
      return null;
    }
  }

  /** Returns {@code true} if the intent has the 'stop' extra. */
  static boolean isStopIntent(Intent intent) {
    return intent.hasExtra(EXTRA_STOP);
  }

  /** Issues a registration retry with delay. */
  static void issueDelayedRegistrationIntent(Context context, AndroidClock clock,
      ByteString clientId, ObjectId objectId, boolean isRegister, int delayMs, int requestCode) {
    RegistrationCommand command = isRegister ?
        AndroidListenerProtos.newDelayedRegisterCommand(clientId, objectId) :
            AndroidListenerProtos.newDelayedUnregisterCommand(clientId, objectId);
    Intent intent = new Intent()
        .putExtra(EXTRA_REGISTRATION, command.toByteArray())
        .setClass(context, AlarmReceiver.class);

    // Create a pending intent that will cause the AlarmManager to fire the above intent.
    PendingIntent pendingIntent = PendingIntent.getBroadcast(context, requestCode, intent,
        PendingIntent.FLAG_ONE_SHOT);

    // Schedule the pending intent after the appropriate delay.
    AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
    long executeMs = clock.nowMs() + delayMs;
    alarmManager.set(AlarmManager.RTC, executeMs, pendingIntent);
  }

  /** Creates a 'start-client' intent. */
  static Intent createStartIntent(Context context, int clientType, byte[] clientName) {
    Intent intent = new Intent();
    // Create proto for the start command.
    StartCommand command = AndroidListenerProtos.newStartCommand(clientType,
        ByteString.copyFrom(clientName));
    intent.putExtra(EXTRA_START, command.toByteArray());
    return setAndroidListenerClass(context, intent);
  }

  /** Creates a 'stop-client' intent. */
  static Intent createStopIntent(Context context) {
    // Stop command just has the extra (its content doesn't matter).
    Intent intent = new Intent();
    intent.putExtra(EXTRA_STOP, true);
    return setAndroidListenerClass(context, intent);
  }

  /** Create an ack intent. */
  static Intent createAckIntent(Context context, byte[] ackHandle) {
    // Ack intent has an extra containing the ack handle data.
    Intent intent = new Intent();
    intent.putExtra(EXTRA_ACK, ackHandle);
    return setAndroidListenerClass(context, intent);
  }

  /** Constructs an intent with {@link RegistrationCommand} proto. */
  static Intent createRegistrationIntent(Context context, byte[] clientId,
      Iterable<ObjectId> objectIds, boolean isRegister) {
    // Registration intent has an extra containing the RegistrationCommand proto.
    Intent intent = new Intent();
    RegistrationCommand command = isRegister
        ? AndroidListenerProtos.newRegisterCommand(ByteString.copyFrom(clientId), objectIds)
            : AndroidListenerProtos.newUnregisterCommand(ByteString.copyFrom(clientId), objectIds);
    intent.putExtra(EXTRA_REGISTRATION, command.toByteArray());
    return setAndroidListenerClass(context, intent);
  }

  /** Sets the appropriate class for {@link AndroidListener} service intents. */
  static Intent setAndroidListenerClass(Context context, Intent intent) {
    String simpleListenerClass = new AndroidTiclManifest(context).getListenerServiceClass();
    return intent.setClassName(context, simpleListenerClass);
  }

  /** Returns {@code true} iff the given intent is an authorization token request. */
  static boolean isAuthTokenRequest(Intent intent) {
    return AuthTokenConstants.ACTION_REQUEST_AUTH_TOKEN.equals(intent.getAction());
  }

  /**
   * Given an authorization token request intent and authorization information ({@code authToken}
   * and {@code authType}) issues a response.
   */
  static void issueAuthTokenResponse(Context context, PendingIntent pendingIntent, String authToken,
      String authType) {
    Intent responseIntent = new Intent()
        .putExtra(AuthTokenConstants.EXTRA_AUTH_TOKEN, authToken)
        .putExtra(AuthTokenConstants.EXTRA_AUTH_TOKEN_TYPE, authType);
    try {
      pendingIntent.send(context, 0, responseIntent);
    } catch (CanceledException exception) {
      logger.warning("Canceled auth request: %s", exception);
    }
  }

  // Prevent instantiation.
  private AndroidListenerIntents() {
  }
}
