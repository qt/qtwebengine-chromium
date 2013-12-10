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
import com.google.ipc.invalidation.ticl.android.AndroidC2DMConstants;

import android.app.AlarmManager;
import android.app.IntentService;
import android.app.PendingIntent;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ServiceInfo;
import android.os.AsyncTask;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;

/**
 * Class for managing C2DM registration and dispatching of messages to observers.
 *
 * Requires setting the {@link #SENDER_ID_METADATA_FIELD} metadata field with the correct e-mail to
 * be used for the C2DM registration.
 *
 * This is based on the open source chrometophone project.
 */
public class C2DMManager extends IntentService {

  private static final Logger logger = AndroidLogger.forTag("C2DM");

  /** Maximum amount of time to wait for manager initialization to complete */
  private static final long MAX_INIT_SECONDS = 30;

  /** Timeout after which wakelocks will be automatically released. */
  private static final int WAKELOCK_TIMEOUT_MS = 30 * 1000;

  /**
   * The action of intents sent from the android c2dm framework regarding registration
   */
  
  public static final String REGISTRATION_CALLBACK_INTENT =
      "com.google.android.c2dm.intent.REGISTRATION";

  /**
   * The action of intents sent from the Android C2DM framework when we are supposed to retry
   * registration.
   */
  private static final String C2DM_RETRY = "com.google.android.c2dm.intent.RETRY";

  /**
   * The key in the bundle to use for the sender ID when registering for C2DM.
   *
   * The value of the field itself must be the account that the server-side pushing messages
   * towards the client is using when talking to C2DM.
   */
  private static final String EXTRA_SENDER = "sender";

  /**
   * The key in the bundle to use for boilerplate code identifying the client application towards
   * the Android C2DM framework
   */
  private static final String EXTRA_APPLICATION_PENDING_INTENT = "app";

  /**
   * The action of intents sent to the Android C2DM framework when we want to register
   */
  private static final String REQUEST_UNREGISTRATION_INTENT =
      "com.google.android.c2dm.intent.UNREGISTER";

  /**
   * The action of intents sent to the Android C2DM framework when we want to unregister
   */
  private static final String REQUEST_REGISTRATION_INTENT =
      "com.google.android.c2dm.intent.REGISTER";

  /**
   * The package for the Google Services Framework
   */
  private static final String GSF_PACKAGE = "com.google.android.gsf";

  /**
   * The action of intents sent from the Android C2DM framework when a message is received.
   */
  
  public static final String C2DM_INTENT = "com.google.android.c2dm.intent.RECEIVE";

  /**
   * The key in the bundle to use when we want to read the C2DM registration ID after a successful
   * registration
   */
  
  public static final String EXTRA_REGISTRATION_ID = "registration_id";

  /**
   * The key in the bundle to use when we want to see if we were unregistered from C2DM
   */
  
  static final String EXTRA_UNREGISTERED = "unregistered";

  /**
   * The key in the bundle to use when we want to see if there was any errors when we tried to
   * register.
   */
  
  static final String EXTRA_ERROR = "error";

  /**
   * The android:name we read from the meta-data for the C2DMManager service in the
   * AndroidManifest.xml file when we want to know which sender id we should use when registering
   * towards C2DM
   */
  
  static final String SENDER_ID_METADATA_FIELD = "sender_id";

  /**
   * If {@code true}, newly-registered observers will be informed of the current registration id
   * if one is already held. Used in  service lifecycle testing to suppress inconvenient
   * events.
   */
  public static final AtomicBoolean disableRegistrationCallbackOnRegisterForTest =
      new AtomicBoolean(false);

  /**
   * C2DMMManager is initialized asynchronously because it requires I/O that should not be done on
   * the main thread.   This latch will only be changed to zero once this initialization has been
   * completed successfully.   No intents should be handled or other work done until the latch
   * reaches the initialized state.
   */
  private final CountDownLatch initLatch = new CountDownLatch(1);

  /**
   * The sender ID we have read from the meta-data in AndroidManifest.xml for this service.
   */
  private String senderId;

  /**
   * Observers to dispatch messages from C2DM to
   */
  private Set<C2DMObserver> observers;

  /**
   * A field which is set to true whenever a C2DM registration is in progress. It is set to false
   * otherwise.
   */
  private boolean registrationInProcess;

  /**
   * The context read during onCreate() which is used throughout the lifetime of this service.
   */
  private Context context;

  /**
   * A field which is set to true whenever a C2DM unregistration is in progress. It is set to false
   * otherwise.
   */
  private boolean unregistrationInProcess;

  /**
   * A reference to our helper service for handling WakeLocks.
   */
  private WakeLockManager wakeLockManager;

  /**
   * Called from the broadcast receiver and from any observer wanting to register (observers usually
   * go through calling C2DMessaging.register(...). Will process the received intent, call
   * handleMessage(), onRegistered(), etc. in background threads, with a wake lock, while keeping
   * the service alive.
   *
   * @param context application to run service in
   * @param intent the intent received
   */
  
  static void runIntentInService(Context context, Intent intent) {
    // This is called from C2DMBroadcastReceiver and C2DMessaging, there is no init.
    WakeLockManager.getInstance(context).acquire(C2DMManager.class, WAKELOCK_TIMEOUT_MS);
    intent.setClassName(context, C2DMManager.class.getCanonicalName());
    context.startService(intent);
  }

  public C2DMManager() {
    super("C2DMManager");
    // Always redeliver intents if evicted while processing
    setIntentRedelivery(true);
  }

  @Override
  public void onCreate() {
    super.onCreate();
    // Use the mock context when testing, otherwise the service application context.
    context = getApplicationContext();
    wakeLockManager = WakeLockManager.getInstance(context);

    // Spawn an AsyncTask performing the blocking IO operations.
    new AsyncTask<Void, Void, Void>() {
      @Override
      protected Void doInBackground(Void... unused) {
        // C2DMSettings relies on SharedPreferencesImpl which performs disk access.
        C2DMManager manager = C2DMManager.this;
        manager.observers = C2DMSettings.getObservers(context);
        manager.registrationInProcess = C2DMSettings.isRegistering(context);
        manager.unregistrationInProcess = C2DMSettings.isUnregistering(context);
        return null;
      }

      @Override
      protected void onPostExecute(Void unused) {
        logger.fine("Initialized");
        initLatch.countDown();
      }
    }.execute();

    senderId = readSenderIdFromMetaData(this);
    if (senderId == null) {
      stopSelf();
    }
  }

  @Override
  public final void onHandleIntent(Intent intent) {
    if (intent == null) {
      logger.warning("Ignoring null intent");
      return;
    }
    try {
      // OK to block here (if needed) because IntentService guarantees that onHandleIntent will
      // only be called on a background thread.
      logger.fine("Handle intent = %s", intent);
      waitForInitialization();
      if (intent.getAction().equals(REGISTRATION_CALLBACK_INTENT)) {
        handleRegistration(intent);
      } else if (intent.getAction().equals(C2DM_INTENT)) {
        onMessage(intent);
      } else if (intent.getAction().equals(C2DM_RETRY)) {
        register();
      } else if (intent.getAction().equals(C2DMessaging.ACTION_REGISTER)) {
        registerObserver(intent);
      } else if (intent.getAction().equals(C2DMessaging.ACTION_UNREGISTER)) {
        unregisterObserver(intent);
      } else {
        logger.warning("Received unknown action: %s", intent.getAction());
      }
    } finally {
      // Release the power lock, so device can get back to sleep.
      // The lock is reference counted by default, so multiple
      // messages are ok, but because sometimes Android reschedules
      // services we need to handle the case that the wakelock should
      // never be underlocked.
      if (wakeLockManager.isHeld(C2DMManager.class)) {
        wakeLockManager.release(C2DMManager.class);
      }
    }
  }

  /** Returns true of the C2DMManager is fully initially */
  
  boolean isInitialized() {
    return initLatch.getCount() == 0;
  }

  /**
   * Blocks until asynchronous initialization work has been completed.
   */
  private void waitForInitialization() {
    boolean interrupted = false;
    try {
      if (initLatch.await(MAX_INIT_SECONDS, TimeUnit.SECONDS)) {
        return;
      }
      logger.warning("Initialization timeout");

    } catch (InterruptedException e) {
      // Unexpected, so to ensure a consistent state wait for initialization to complete and
      // then interrupt so higher level code can handle the interrupt.
      logger.fine("Latch wait interrupted");
      interrupted = true;
    } finally {
      if (interrupted) {
        logger.warning("Initialization interrupted");
        Thread.currentThread().interrupt();
      }
    }

    // Either an unexpected interrupt or a timeout occurred during initialization.  Set to a default
    // clean state (no registration work in progress, no observers) and proceed.
    observers = new HashSet<C2DMObserver>();
  }

  /**
   * Called when a cloud message has been received.
   *
   * @param intent the received intent
   */
  private void onMessage(Intent intent) {
    boolean matched = false;
    for (C2DMObserver observer : observers) {
      if (observer.matches(intent)) {
        Intent outgoingIntent = createOnMessageIntent(
            observer.getObserverClass(), context, intent);
        deliverObserverIntent(observer, outgoingIntent);
        matched = true;
      }
    }
    if (!matched) {
      logger.info("No receivers matched intent: %s", intent);
    }
  }

  /**
   * Returns an intent to deliver a C2DM message to {@code observerClass}.
   * @param context Android context to use to create the intent
   * @param intent the C2DM message intent to deliver
   */
  
  public static Intent createOnMessageIntent(Class<?> observerClass,
      Context context, Intent intent) {
    Intent outgoingIntent = new Intent(intent);
    outgoingIntent.setAction(C2DMessaging.ACTION_MESSAGE);
    outgoingIntent.setClass(context, observerClass);
    return outgoingIntent;
  }

  /**
   * Called on registration error. Override to provide better error messages.
   *
   * This is called in the context of a Service - no dialog or UI.
   *
   * @param errorId the errorId String
   */
  private void onRegistrationError(String errorId) {
    setRegistrationInProcess(false);
    for (C2DMObserver observer : observers) {
      deliverObserverIntent(observer,
          createOnRegistrationErrorIntent(observer.getObserverClass(),
              context, errorId));
    }
  }

  /**
   * Returns an intent to deliver the C2DM error {@code errorId} to {@code observerClass}.
   * @param context Android context to use to create the intent
   */
  
  public static Intent createOnRegistrationErrorIntent(Class<?> observerClass,
      Context context, String errorId) {
    Intent errorIntent = new Intent(context, observerClass);
    errorIntent.setAction(C2DMessaging.ACTION_REGISTRATION_ERROR);
    errorIntent.putExtra(C2DMessaging.EXTRA_REGISTRATION_ERROR, errorId);
    return errorIntent;
  }

  /**
   * Called when a registration token has been received.
   *
   * @param registrationId the registration ID received from C2DM
   */
  private void onRegistered(String registrationId) {
    setRegistrationInProcess(false);
    C2DMSettings.setC2DMRegistrationId(context, registrationId);
    try {
      C2DMSettings.setApplicationVersion(context, getCurrentApplicationVersion(this));
    } catch (NameNotFoundException e) {
      logger.severe("Unable to find our own package name when storing application version: %s",
          e.getMessage());
    }
    for (C2DMObserver observer : observers) {
      onRegisteredSingleObserver(registrationId, observer);
    }
  }

  /**
   * Informs the given observer about the registration ID
   */
  private void onRegisteredSingleObserver(String registrationId, C2DMObserver observer) {
    if (!disableRegistrationCallbackOnRegisterForTest.get()) {
      deliverObserverIntent(observer,
          createOnRegisteredIntent(observer.getObserverClass(), context, registrationId));
    }
  }

  /**
   * Returns an intent to deliver a new C2DM {@code registrationId} to {@code observerClass}.
   * @param context Android context to use to create the intent
   */
  
  public static Intent createOnRegisteredIntent(Class<?> observerClass, Context context,
      String registrationId) {
    Intent outgoingIntent = new Intent(context, observerClass);
    outgoingIntent.setAction(C2DMessaging.ACTION_REGISTERED);
    outgoingIntent.putExtra(C2DMessaging.EXTRA_REGISTRATION_ID, registrationId);
    return outgoingIntent;
  }

  /**
   * Called when the device has been unregistered.
   */
  private void onUnregistered() {
    setUnregisteringInProcess(false);
    C2DMSettings.clearC2DMRegistrationId(context);
    for (C2DMObserver observer : observers) {
      onUnregisteredSingleObserver(observer);
    }
  }

  /**
   * Informs the given observer that the application is no longer registered to C2DM
   */
  private void onUnregisteredSingleObserver(C2DMObserver observer) {
    Intent outgoingIntent = new Intent(context, observer.getObserverClass());
    outgoingIntent.setAction(C2DMessaging.ACTION_UNREGISTERED);
    deliverObserverIntent(observer, outgoingIntent);
  }

  /**
   * Starts the observer service by delivering it the provided intent. If the observer has asked us
   * to get a WakeLock for it, we do that and inform the observer that the WakeLock has been
   * acquired through the flag C2DMessaging.EXTRA_RELEASE_WAKELOCK.
   */
  private void deliverObserverIntent(C2DMObserver observer, Intent intent) {
    if (observer.isHandleWakeLock()) {
      // Set the extra so the observer knows that it needs to release the wake lock
      intent.putExtra(C2DMessaging.EXTRA_RELEASE_WAKELOCK, true);
      wakeLockManager.acquire(observer.getObserverClass(), WAKELOCK_TIMEOUT_MS);
    }
    context.startService(intent);
  }

  /**
   * Registers an observer.
   *
   *  If this was the first observer we also start registering towards C2DM. If we were already
   * registered, we do a callback to inform about the current C2DM registration ID.
   *
   * <p>We also start a registration if the application version stored does not match the
   * current version number. This leads to any observer registering after an upgrade will trigger
   * a new C2DM registration.
   */
  private void registerObserver(Intent intent) {
    C2DMObserver observer = C2DMObserver.createFromIntent(intent);
    observers.add(observer);
    C2DMSettings.setObservers(context, observers);
    if (C2DMSettings.hasC2DMRegistrationId(context)) {
      onRegisteredSingleObserver(C2DMSettings.getC2DMRegistrationId(context), observer);
      if (!isApplicationVersionCurrent() && !isRegistrationInProcess()) {
        logger.fine("Registering to C2DM since application version is not current.");
        register();
      }
    } else {
      if (!isRegistrationInProcess()) {
        logger.fine("Registering to C2DM since we have no C2DM registration.");
        register();
      }
    }
  }

  /**
   * Unregisters an observer.
   *
   *  The observer is moved to unregisteringObservers which only gets messages from C2DMManager if
   * we unregister from C2DM completely. If this was the last observer, we also start the process of
   * unregistering from C2DM.
   */
  private void unregisterObserver(Intent intent) {
    C2DMObserver observer = C2DMObserver.createFromIntent(intent);
    if (observers.remove(observer)) {
      C2DMSettings.setObservers(context, observers);
      onUnregisteredSingleObserver(observer);
    }
    if (observers.isEmpty()) {
      // No more observers, need to unregister
      if (!isUnregisteringInProcess()) {
        unregister();
      }
    }
  }

  /**
   * Called when the Android C2DM framework sends us a message regarding registration.
   *
   *  This method parses the intent from the Android C2DM framework and calls the appropriate
   * methods for when we are registered, unregistered or if there was an error when trying to
   * register.
   */
  private void handleRegistration(Intent intent) {
    String registrationId = intent.getStringExtra(EXTRA_REGISTRATION_ID);
    String error = intent.getStringExtra(EXTRA_ERROR);
    String removed = intent.getStringExtra(EXTRA_UNREGISTERED);
    logger.fine("Got registration message: registrationId = %s, error = %s, removed = %s",
                registrationId, error, removed);
    if (removed != null) {
      onUnregistered();
    } else if (error != null) {
      handleRegistrationBackoffOnError(error);
    } else {
      handleRegistration(registrationId);
    }
  }

  /**
   * Informs observers about a registration error, and schedules a registration retry if the error
   * was transient.
   */
  private void handleRegistrationBackoffOnError(String error) {
    logger.severe("Registration error %s", error);
    onRegistrationError(error);
    if (C2DMessaging.ERR_SERVICE_NOT_AVAILABLE.equals(error)) {
      long backoffTimeMs = C2DMSettings.getBackoff(context);
      createAlarm(backoffTimeMs);
      increaseBackoff(backoffTimeMs);
    }
  }

  /**
   * When C2DM registration fails, we call this method to schedule a retry in the future.
   */
  private void createAlarm(long backoffTimeMs) {
    logger.fine("Scheduling registration retry, backoff = %d", backoffTimeMs);
    Intent retryIntent = new Intent(C2DM_RETRY);
    PendingIntent retryPIntent = PendingIntent.getBroadcast(context, 0, retryIntent, 0);
    AlarmManager am = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
    am.set(AlarmManager.ELAPSED_REALTIME, backoffTimeMs, retryPIntent);
  }

  /**
   * Increases the backoff time for retrying C2DM registration
   */
  private void increaseBackoff(long backoffTimeMs) {
    backoffTimeMs *= 2;
    C2DMSettings.setBackoff(context, backoffTimeMs);
  }

  /**
   * When C2DM registration is complete, this method resets the backoff and makes sure all observers
   * are informed
   */
  private void handleRegistration(String registrationId) {
    C2DMSettings.resetBackoff(context);
    onRegistered(registrationId);
  }

  private void setRegistrationInProcess(boolean registrationInProcess) {
    C2DMSettings.setRegistering(context, registrationInProcess);
    this.registrationInProcess = registrationInProcess;
  }

  private boolean isRegistrationInProcess() {
    return registrationInProcess;
  }

  private void setUnregisteringInProcess(boolean unregisteringInProcess) {
    C2DMSettings.setUnregistering(context, unregisteringInProcess);
    this.unregistrationInProcess = unregisteringInProcess;
  }

  private boolean isUnregisteringInProcess() {
    return unregistrationInProcess;
  }

  /**
   * Initiate c2d messaging registration for the current application
   */
  private void register() {
    Intent registrationIntent = new Intent(REQUEST_REGISTRATION_INTENT);
    registrationIntent.setPackage(GSF_PACKAGE);
    registrationIntent.putExtra(
        EXTRA_APPLICATION_PENDING_INTENT, PendingIntent.getBroadcast(context, 0, new Intent(), 0));
    registrationIntent.putExtra(EXTRA_SENDER, senderId);
    setRegistrationInProcess(true);
    context.startService(registrationIntent);
  }

  /**
   * Unregister the application. New messages will be blocked by server.
   */
  private void unregister() {
    Intent regIntent = new Intent(REQUEST_UNREGISTRATION_INTENT);
    regIntent.setPackage(GSF_PACKAGE);
    regIntent.putExtra(
        EXTRA_APPLICATION_PENDING_INTENT, PendingIntent.getBroadcast(context, 0, new Intent(), 0));
    setUnregisteringInProcess(true);
    context.startService(regIntent);
  }

  /**
   * Checks if the stored application version is the same as the current application version.
   */
  private boolean isApplicationVersionCurrent() {
    try {
      String currentApplicationVersion = getCurrentApplicationVersion(this);
      if (currentApplicationVersion == null) {
        return false;
      }
      return currentApplicationVersion.equals(C2DMSettings.getApplicationVersion(context));
    } catch (NameNotFoundException e) {
      logger.fine("Unable to find our own package name when reading application version: %s",
                     e.getMessage());
      return false;
    }
  }

  /**
   * Retrieves the current application version.
   */
  
  public static String getCurrentApplicationVersion(Context context) throws NameNotFoundException {
    PackageInfo packageInfo =
        context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
    return packageInfo.versionName;
  }

  /**
   * Reads the meta-data to find the field specified in SENDER_ID_METADATA_FIELD. The value of that
   * field is used when registering towards C2DM. If no value is found,
   * {@link AndroidC2DMConstants#SENDER_ID} is returned.
   */
  static String readSenderIdFromMetaData(Context context) {
    String senderId = AndroidC2DMConstants.SENDER_ID;
    try {
      ServiceInfo serviceInfo = context.getPackageManager().getServiceInfo(
          new ComponentName(context, C2DMManager.class), PackageManager.GET_META_DATA);
      if (serviceInfo.metaData != null) {
        String manifestSenderId = serviceInfo.metaData.getString(SENDER_ID_METADATA_FIELD);
        if (manifestSenderId != null) {
          logger.fine("Using manifest-specified sender-id: %s", manifestSenderId);
          senderId = manifestSenderId;
        } else {
          logger.severe("No meta-data element with the name %s found on the service declaration",
                        SENDER_ID_METADATA_FIELD);
        }
      }
    } catch (NameNotFoundException exception) {
      logger.info("Could not find C2DMManager service info in manifest");
    }
    return senderId;
  }
}
