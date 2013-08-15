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
package com.google.ipc.invalidation.examples.android2;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.external.client.InvalidationListener.RegistrationState;
import com.google.ipc.invalidation.external.client.contrib.AndroidListener;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.ipc.invalidation.external.client.types.Invalidation;
import com.google.ipc.invalidation.external.client.types.ObjectId;

import android.accounts.Account;
import android.accounts.AccountManager;
import android.accounts.AccountManagerFuture;
import android.accounts.AuthenticatorException;
import android.accounts.OperationCanceledException;
import android.app.PendingIntent;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.os.Bundle;
import android.util.Base64;
import android.util.Log;

import java.io.IOException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;


/**
 * Implements the service that handles invalidation client events for this application. This
 * listener registers an interest in a small number of objects and calls {@link MainActivity} with
 * any relevant status changes.
 *
 */
public final class ExampleListener extends AndroidListener {

  /** The account type value for Google accounts */
  private static final String GOOGLE_ACCOUNT_TYPE = "com.google";

  /**
   * This is the authentication token type that's used for invalidation client communication to the
   * server. For real applications, it would generally match the authorization type used by the
   * application.
   */
  private static final String AUTH_TYPE = "android";

  /** Name used for shared preferences. */
  private static final String PREFERENCES_NAME = "example_listener";

  /** Key used for listener state in shared preferences. */
  private static final String STATE_KEY = "example_listener_state";

  /** Object source for objects the client is tracking. */
  private static final int DEMO_SOURCE = 4;

  /** Prefix for object names. */
  private static final String OBJECT_ID_PREFIX = "Obj";

  /** The tag used for logging in the listener. */
  private static final String TAG = "TEA2:ExampleListener";

  /** Number of objects we're interested in tracking. */
  
  static final int NUM_INTERESTING_OBJECTS = 4;

  /** Ids for objects we want to track. */
  private final Set<ObjectId> interestingObjects;

  public ExampleListener() {
    super();
    // We're interested in objects with ids Obj1, Obj2, ...
    interestingObjects = new HashSet<ObjectId>();
    for (int i = 1; i <= NUM_INTERESTING_OBJECTS; i++) {
      interestingObjects.add(getObjectId(i));
    }
  }

  @Override
  public void informError(ErrorInfo errorInfo) {
    Log.e(TAG, "informError: " + errorInfo);

    /***********************************************************************************************
     * YOUR CODE HERE
     *
     * Handling of permanent failures is application-specific.
     **********************************************************************************************/
  }

  @Override
  public void ready(byte[] clientId) {
    Log.i(TAG, "ready()");
  }

  @Override
  public void reissueRegistrations(byte[] clientId) {
    Log.i(TAG, "reissueRegistrations()");
    register(clientId, interestingObjects);
  }

  @Override
  public void invalidate(Invalidation invalidation, byte[] ackHandle) {
    Log.i(TAG, "invalidate: " + invalidation);

    // Do real work here based upon the invalidation
    MainActivity.State.setVersion(
        invalidation.getObjectId(), "invalidate", invalidation.toString());

    acknowledge(ackHandle);
  }

  @Override
  public void invalidateUnknownVersion(ObjectId objectId, byte[] ackHandle) {
    Log.i(TAG, "invalidateUnknownVersion: " + objectId);

    // Do real work here based upon the invalidation.
    MainActivity.State.setVersion(
        objectId, "invalidateUnknownVersion", getBackendVersion(objectId));

    acknowledge(ackHandle);
  }

  @Override
  public void invalidateAll(byte[] ackHandle) {
    Log.i(TAG, "invalidateAll");

    // Do real work here based upon the invalidation.
    for (ObjectId objectId : interestingObjects) {
        MainActivity.State.setVersion(objectId, "invalidateAll", getBackendVersion(objectId));
    }

    acknowledge(ackHandle);
  }


  @Override
  public byte[] readState() {
    Log.i(TAG, "readState");
    SharedPreferences sharedPreferences = getSharedPreferences();
    String data = sharedPreferences.getString(STATE_KEY, null);
    return (data != null) ? Base64.decode(data, Base64.DEFAULT) : null;
  }

  @Override
  public void writeState(byte[] data) {
    Log.i(TAG, "writeState");
    Editor editor = getSharedPreferences().edit();
    editor.putString(STATE_KEY, Base64.encodeToString(data, Base64.DEFAULT));
    editor.commit();
  }

  @Override
  public void requestAuthToken(PendingIntent pendingIntent,
      String invalidAuthToken) {
    Log.i(TAG, "requestAuthToken");

    // In response to requestAuthToken, we need to get an auth token and inform the invalidation
    // client of the result through a call to setAuthToken. In this example, we block until a
    // result is available. It is also possible to invoke setAuthToken in a callback or when
    // handling an intent.
    AccountManager accountManager = AccountManager.get(getApplicationContext());

    // Invalidate the old token if necessary.
    if (invalidAuthToken != null) {
      accountManager.invalidateAuthToken(GOOGLE_ACCOUNT_TYPE, invalidAuthToken);
    }

    // Choose an (arbitrary in this example) account for which to retrieve an authentication token.
    Account account = getAccount(accountManager);

    try {
      // There are three possible outcomes of the call to getAuthToken:
      //
      // 1. Authentication failure (null result).
      // 2. The user needs to sign in or give permission for the account. In such cases, the result
      //    includes an intent that can be started to retrieve credentials from the user.
      // 3. The response includes the auth token, in which case we can inform the invalidation
      //    client.
      //
      // In the first case, we simply log and return. The response to such errors is application-
      // specific. For instance, the application may prompt the user to choose another account.
      //
      // In the second case, we start an intent to ask for user credentials so that they are
      // available to the application if there is a future request. An application should listen for
      // the LOGIN_ACCOUNTS_CHANGED_ACTION broadcast intent to trigger a response to the
      // invalidation client after the user has responded. Otherwise, it may take several minutes
      // for the invalidation client to start.
      //
      // In the third case, success!, we pass the authorization token and type to the invalidation
      // client using the setAuthToken method.
      AccountManagerFuture<Bundle> future = accountManager.getAuthToken(account, AUTH_TYPE,
          new Bundle(), false, null, null);
      Bundle result = future.getResult();
      if (result == null) {
        // If the result is null, it means that authentication was not possible.
        Log.w(TAG, "Auth token - getAuthToken returned null");
        return;
      }
      if (result.containsKey(AccountManager.KEY_INTENT)) {
        Log.i(TAG, "Starting intent to get auth credentials");

        // Need to start intent to get credentials.
        Intent intent = result.getParcelable(AccountManager.KEY_INTENT);
        int flags = intent.getFlags();
        flags |= Intent.FLAG_ACTIVITY_NEW_TASK;
        intent.setFlags(flags);
        getApplicationContext().startActivity(intent);
        return;
      }
      String authToken = result.getString(AccountManager.KEY_AUTHTOKEN);
      setAuthToken(getApplicationContext(), pendingIntent, authToken, AUTH_TYPE);
    } catch (OperationCanceledException e) {
      Log.w(TAG, "Auth token - operation cancelled", e);
    } catch (AuthenticatorException e) {
      Log.w(TAG, "Auth token - authenticator exception", e);
    } catch (IOException e) {
      Log.w(TAG, "Auth token - IO exception", e);
    }
  }

  /** Returns any Google account enabled on the device. */
  private static Account getAccount(AccountManager accountManager) {
    Preconditions.checkNotNull(accountManager);
    for (Account acct : accountManager.getAccounts()) {
      if (GOOGLE_ACCOUNT_TYPE.equals(acct.type)) {
        return acct;
      }
    }
    throw new RuntimeException("No google account enabled.");
  }

  @Override
  public void informRegistrationFailure(byte[] clientId, ObjectId objectId, boolean isTransient,
      String errorMessage) {
    Log.e(TAG, "Registration failure!");
    if (isTransient) {
      // Retry immediately on transient failures. The base AndroidListener will handle exponential
      // backoff if there are repeated failures.
      List<ObjectId> objectIds = new ArrayList<ObjectId>();
      objectIds.add(objectId);
      if (interestingObjects.contains(objectId)) {
        Log.i(TAG, "Retrying registration of " + objectId);
        register(clientId, objectIds);
      } else {
        Log.i(TAG, "Retrying unregistration of " + objectId);
        unregister(clientId, objectIds);
      }
    }
  }

  @Override
  public void informRegistrationStatus(byte[] clientId, ObjectId objectId,
      RegistrationState regState) {
    Log.i(TAG, "informRegistrationStatus");

    List<ObjectId> objectIds = new ArrayList<ObjectId>();
    objectIds.add(objectId);
    if (regState == RegistrationState.REGISTERED) {
      if (!interestingObjects.contains(objectId)) {
        Log.i(TAG, "Unregistering for object we're no longer interested in");
        unregister(clientId, objectIds);
      }
    } else {
      if (interestingObjects.contains(objectId)) {
        Log.i(TAG, "Registering for an object");
        register(clientId, objectIds);
      }
    }
  }

  private SharedPreferences getSharedPreferences() {
    return getApplicationContext().getSharedPreferences(PREFERENCES_NAME, MODE_PRIVATE);
  }

  private String getBackendVersion(ObjectId objectId) {
    /***********************************************************************************************
     * YOUR CODE HERE
     *
     * Invalidation client has no information about the given object. Connect with the application
     * backend to determine its current state. The implementation should be non-blocking.
     **********************************************************************************************/

    // Normally, we would connect to a real application backend. For this example, we return a fixed
    // value.

    return "some value from custom app backend";
  }

  /** Gets object ID given index. */
  private static ObjectId getObjectId(int i) {
    return ObjectId.newInstance(DEMO_SOURCE, (OBJECT_ID_PREFIX + i).getBytes());
  }
}
