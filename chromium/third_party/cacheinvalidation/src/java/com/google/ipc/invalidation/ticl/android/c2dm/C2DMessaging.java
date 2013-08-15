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

import android.app.Service;
import android.content.Context;
import android.content.Intent;


/**
 * Utilities for device registration.
 *
 *  Will keep track of the registration token in a private preference.
 *
 * This is based on the open source chrometophone project.
 */
public class C2DMessaging {
  static final String ACTION_MESSAGE = "com.google.android.c2dm.manager.intent.MESSAGE";

  static final String ACTION_REGISTER = "com.google.android.c2dm.manager.intent.REGISTER";

  static final String ACTION_UNREGISTER = "com.google.android.c2dm.manager.intent.UNREGISTER";

  static final String ACTION_REGISTERED = "com.google.android.c2dm.manager.intent.REGISTERED";

  static final String ACTION_UNREGISTERED = "com.google.android.c2dm.manager.intent.UNREGISTERED";

  static final String ACTION_REGISTRATION_ERROR =
      "com.google.android.c2dm.manager.intent.REGISTRATION_ERROR";

  static final String EXTRA_REGISTRATION_ID =
      "com.google.android.c2dm.manager.extra.REGISTRATION_ID";

  static final String EXTRA_REGISTRATION_ERROR = "com.google.android.c2dm.manager.extra.ERROR";

  static final String EXTRA_CANONICAL_CLASS =
      "com.google.android.c2dm.manager.extra.CANONICAL_CLASS";

  static final String EXTRA_FILTER_KEY = "com.google.android.c2dm.manager.extra.FILTER_KEY";

  static final String EXTRA_FILTER_VALUE = "com.google.android.c2dm.manager.extra.FILTER_VALUE";

  static final String EXTRA_HANDLE_WAKELOCK =
      "com.google.android.c2dm.manager.extra.HANDLE_WAKELOCK";

  static final String EXTRA_RELEASE_WAKELOCK =
      "com.google.android.c2dm.manager.extra.RELEASE_WAKELOCK";

  /**
   * The device can't read the response, or there was a 500/503 from the server that can be retried
   * later. The C2DMManager will automatically use exponential back off and retry.
   */
  public static final String ERR_SERVICE_NOT_AVAILABLE = "SERVICE_NOT_AVAILABLE";

  /**
   * There is no Google account on the phone. The application should ask the user to open the
   * account manager and add a Google account. Fix on the device side.
   */
  public static final String ERR_ACCOUNT_MISSING = "ACCOUNT_MISSING";

  /**
   * Bad password. The application should ask the user to enter his/her password, and let user retry
   * manually later. Fix on the device side.
   */
  public static final String ERR_AUTHENTICATION_FAILED = "AUTHENTICATION_FAILED";

  /**
   * The user has too many applications registered. The application should tell the user to
   * uninstall some other applications, let user retry manually. Fix on the device side.
   */
  public static final String ERR_TOO_MANY_REGISTRATIONS = "TOO_MANY_REGISTRATIONS";

  /**
   * Invalid parameters found in C2DM registration or message.
   */
  public static final String ERR_INVALID_PARAMETERS = "INVALID_PARAMETERS";

  /**
   * The sender account is not recognized.
   */
  public static final String ERR_INVALID_SENDER = "INVALID_SENDER";

  /** Incorrect phone registration with Google. This phone doesn't currently support C2DM. */
  public static final String ERR_PHONE_REGISTRATION_ERROR = "PHONE_REGISTRATION_ERROR";

  public static String getSenderId(Context context) {
    return C2DMManager.readSenderIdFromMetaData(context);
  }

  /**
   * Returns the current C2DM registration ID for the application or {@code null} if not yet full
   * registered.
   */
  public static String getRegistrationId(Context context) {
    return C2DMSettings.getC2DMRegistrationId(context);
  }

  /**
   * Registers a new C2DM observer service that will receive registration notifications and
   * delivered messages. Receipt of messages can be made conditional based upon the presence of a
   * particular extra in the c2dm message and optionally the value of that extra.
   *
   * @param context the current application context
   * @param clazz the service that will receive c2dm activity intents
   * @param selectKey the name of an extra that will be present in messages selected for this
   *        observer. If {@code null}, all messages are delivered.
   * @param selectValue defines a specific value that must match for the messages selected for this
   *        observer. If {@code null}, any value will match.
   * @param handleWakeLock if {@code true} indicates that a wake lock should be acquired from the
   *        {@link WakeLockManager} before messages are delivered to the observer and that the
   *        observer will be responsible for releasing the lock.
   */
  public static void register(Context context, Class<? extends Service> clazz,
      String selectKey, String selectValue, boolean handleWakeLock) {

    Intent intent = new Intent();
    intent.setAction(ACTION_REGISTER);
    intent.putExtra(EXTRA_CANONICAL_CLASS, clazz.getCanonicalName());
    intent.putExtra(EXTRA_FILTER_KEY, selectKey);
    intent.putExtra(EXTRA_FILTER_VALUE, selectValue);
    intent.putExtra(EXTRA_HANDLE_WAKELOCK, handleWakeLock);
    C2DMManager.runIntentInService(context, intent);
  }

  /**
   * Unregisters an existing C2DM observer service so it will no longer receive notifications or
   * messages (or than a final unregister notification indicating that the observer has been
   * unregistered.
   *
   * @param context the current application context
   * @param clazz the service that will receive c2dm activity intents
   * @param selectKey the name of an extra that will be present in messages selected for this
   *        observer. If {@code null}, all messages are delivered.
   * @param selectValue defines a specific value that must match for the messages selected for this
   *        observer. If {@code null}, any value will match.
   * @param handleWakeLock if {@code true} indicates that a wake lock should be acquired from the
   *        {@link WakeLockManager} before messages are delivered to the observer and that the
   *        observer will be responsible for releasing the lock.
   */
  public static void unregister(Context context, Class<?> clazz, String selectKey,
      String selectValue, boolean handleWakeLock) {
    Intent intent = new Intent();
    intent.setAction(ACTION_UNREGISTER);
    intent.putExtra(EXTRA_CANONICAL_CLASS, clazz.getCanonicalName());
    intent.putExtra(EXTRA_FILTER_KEY, selectKey);
    intent.putExtra(EXTRA_FILTER_VALUE, selectValue);
    intent.putExtra(EXTRA_HANDLE_WAKELOCK, handleWakeLock);
    C2DMManager.runIntentInService(context, intent);
  }
}
