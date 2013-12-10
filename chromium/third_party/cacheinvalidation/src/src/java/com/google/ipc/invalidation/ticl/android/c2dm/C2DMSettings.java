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

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.HashSet;
import java.util.Set;

/**
 * Stores and provides access to private settings used by the C2DM manager.
 */
public class C2DMSettings {

  private static final Logger logger = AndroidLogger.forTag("C2DMSettings");

  
  static final String PREFERENCE_PACKAGE = "com.google.android.c2dm.manager";

  
  static final String REGISTRATION_ID = "registrationId";

  private static final String APPLICATION_VERSION = "applicationVersion";

  private static final String BACKOFF = "c2dm_backoff";

  private static final long BACKOFF_DEFAULT = 30000;

  private static final String OBSERVERS = "observers";

  private static final String REGISTERING = "registering";

  private static final String UNREGISTERING = "unregistering";

  /**
   * Sets the C2DM registration ID.
   */
  static void setC2DMRegistrationId(Context context, String registrationId) {
    storeField(context, REGISTRATION_ID, registrationId);
  }

  /**
   * Clears the C2DM registration ID.
   */
  static void clearC2DMRegistrationId(Context context) {
    storeField(context, REGISTRATION_ID, null);
  }

  /**
   * Retrieves the C2DM registration ID (or {@code null} if not stored).
   */
  static String getC2DMRegistrationId(Context context) {
    return retrieveField(context, REGISTRATION_ID, null);
  }

  /**
   * Returns {@code true} if there is a C2DM registration ID stored.
   */
  static boolean hasC2DMRegistrationId(Context context) {
    return getC2DMRegistrationId(context) != null;
  }

  /**
   * Sets the application version.
   */
  static void setApplicationVersion(Context context, String applicationVersion) {
    storeField(context, APPLICATION_VERSION, applicationVersion);
  }

  /**
   * Retrieves the application version (or {@code null} if not stored).
   */
  static String getApplicationVersion(Context context) {
    return retrieveField(context, APPLICATION_VERSION, null);
  }

  /**
   * Returns the backoff setting.
   */
  static long getBackoff(Context context) {
    return retrieveField(context, BACKOFF, BACKOFF_DEFAULT);
  }

  /**
   * Sets the backoff setting.
   * @param context
   * @param backoff
   */
  static void setBackoff(Context context, long backoff) {
    storeField(context, BACKOFF, backoff);
  }

  /**
   * Resets the backoff setting to the default value.
   */
  static void resetBackoff(Context context) {
    setBackoff(context, BACKOFF_DEFAULT);
  }

  /**
   * Sets the boolean flag indicating C2DM registration is in process.
   */
  static void setRegistering(Context context, boolean registering) {
    storeField(context, REGISTERING, registering);
  }

  /**
   * Returns {@code true} if C2DM registration is in process.
   */
  static boolean isRegistering(Context context) {
    return retrieveField(context, REGISTERING, false);
  }

  /**
   * Sets the boolean flag indicating C2DM unregistration is in process.
   */
  static void setUnregistering(Context context, boolean registering) {
    storeField(context, UNREGISTERING, registering);
  }

  /**
   * Returns the boolean flag indicating C2DM unregistration is in process.
   */
  static boolean isUnregistering(Context context) {
    return retrieveField(context, UNREGISTERING, false);
  }

  /**
   * Returns the set of stored observers.
   */
  static Set<C2DMObserver> getObservers(Context context) {
    return createC2DMObserversFromJSON(retrieveField(context, OBSERVERS, null));
  }

  /**
   * Sets the set of stored observers.
   */
  static void setObservers(Context context, Set<C2DMObserver> observers) {
    storeField(context, OBSERVERS, createJsonObserversFromC2DMObservers(observers));
  }

  private static Set<C2DMObserver> createC2DMObserversFromJSON(String jsonString) {
    // The observer set is stored in a json array of objects that contain the
    // observer json representation produced by C2DMObserver.toJSON.   Iterate over
    // this array and recreate observers from the objects.
    Set<C2DMObserver> observers = new HashSet<C2DMObserver>();
    if (jsonString == null) {
      return observers;
    }
    try {
      JSONArray array = new JSONArray(jsonString);
      for (int i = 0; i < array.length(); i++) {
        JSONObject jsonObserver = array.getJSONObject(i);
        C2DMObserver observer = C2DMObserver.createFromJSON(jsonObserver);
        if (observer != null) {
          observers.add(observer);
        }
      }
    } catch (JSONException e) {
      logger.severe("Unable to parse observers. Source: %s", jsonString);
      observers.clear(); // No partial result
    }
    return observers;
  }

  private static String createJsonObserversFromC2DMObservers(Set<C2DMObserver> observers) {
    // Stores the observers as an array of json objects in the format produced by
    // C2DMObserver.toJSON
    JSONArray array = new JSONArray();
    for (C2DMObserver observer : observers) {
      JSONObject json = observer.toJSON();
      if (json != null) {
        array.put(json);
      }
    }
    return array.toString();
  }

  private static boolean retrieveField(Context context, String field, boolean defaultValue) {
    SharedPreferences preferences = getPreferences(context);
    return preferences.getBoolean(field, defaultValue);
  }

  private static void storeField(Context context, String field, boolean value) {
    SharedPreferences preferences = getPreferences(context);
    SharedPreferences.Editor editor = preferences.edit();
    editor.putBoolean(field, value);
    editor.commit();
  }

  private static long retrieveField(Context context, String field, long defaultValue) {
    SharedPreferences preferences = getPreferences(context);
    return preferences.getLong(field, defaultValue);
  }

  private static void storeField(Context context, String field, long value) {
    SharedPreferences preferences = getPreferences(context);
    SharedPreferences.Editor editor = preferences.edit();
    editor.putLong(field, value);
    editor.commit();
  }

  private static String retrieveField(Context context, String field, String defaultValue) {
    SharedPreferences preferences = getPreferences(context);
    return preferences.getString(field, defaultValue);
  }

  private static void storeField(Context context, String field, String value) {
    SharedPreferences preferences = getPreferences(context);
    SharedPreferences.Editor editor = preferences.edit();
    editor.putString(field, value);
    editor.commit();
    if (value == null) {
      logger.fine("Cleared field %s", field);
    }
  }

  private static SharedPreferences getPreferences(Context context) {
    return context.getSharedPreferences(PREFERENCE_PACKAGE, Context.MODE_PRIVATE);
  }

  /** Sets the C2DM registration id to {@code registrationId}. */
  public static void setC2DMRegistrationIdForTest(Context context, String registrationId) {
    setC2DMRegistrationId(context, registrationId);
  }

  /** Sets the C2DM application version to {@code version}. */
  public static void setApplicationVersionForTest(Context context, String version) {
    setApplicationVersion(context, version);
  }
}
