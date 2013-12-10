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


import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;

import android.app.Service;
import android.content.Intent;

import org.json.JSONException;
import org.json.JSONObject;

/**
 * Represents a C2DM observer that gets notifications whenever the
 * application receives a C2DM message or something changes regarding C2DM registration status.
 *
 * An observer may have its own unique filter so that not all messages are dispatched to all
 * observers.
 */
public class C2DMObserver {

  /** Logger */
  private static final Logger logger = AndroidLogger.forTag("C2DMObserver");

  /** JSON key name for the observer class name string value */
  private static final String KEY_CLASS = "class";

  /** JSON key name for the filter field name string value */
  private static final String KEY_FILTER_KEY = "filterKey";

  /** JSON key name for the value field string value */
  private static final String KEY_FILTER_VALUE = "filterValue";

  /** JSON kye name for the handle wake lock boolean field */
  private static final String KEY_HANDLE_WAKE_LOCK = "handleWakeLock";

  /** Service class that handles messages for this observer */
  private final Class<? extends Service> messageService;

  /** Select field name (or {@code null} if none) */
  private final String selectKey;

  /** Select field value (or {@code null} if none) */
  private final String selectValue;

  /** {@code true} if the observer handles the wake lock */
  private final boolean handleWakeLock;

  /**
   * Creates a new observer using the state stored within the provided JSON object (normally
   * created by {@link #toJSON()}.
   */
  static C2DMObserver createFromJSON(JSONObject json) {
    try {
      // Extract instance state value from the appropriate JSON fields.
      String canonicalClassString = json.getString(KEY_CLASS);
      Class<? extends Service> clazz =
          Class.forName(canonicalClassString).asSubclass(Service.class);
      String filterKey = json.has(KEY_FILTER_KEY) ? json.getString(KEY_FILTER_KEY) : null;
      String filterValue = json.has(KEY_FILTER_VALUE) ? json.getString(KEY_FILTER_VALUE) : null;
      Boolean handleWakeLock =
          json.has(KEY_HANDLE_WAKE_LOCK) && json.getBoolean(KEY_HANDLE_WAKE_LOCK);
      return new C2DMObserver(clazz, filterKey, filterValue, handleWakeLock);
    } catch (JSONException e) {
      logger.severe("Unable to parse observer. Source: %s", json);
    } catch (ClassNotFoundException e) {
      logger.severe("Unable to parse observer. Class not found. Source: %s", json);
    }
    return null;
  }

  /**
   * Creates a new observer from the extra values contained in the provided intent.
   */
  static C2DMObserver createFromIntent(Intent intent) {
    String canonicalClassString = intent.getStringExtra(C2DMessaging.EXTRA_CANONICAL_CLASS);
    try {
      // Extract observer state from the intent built by the C2DM manager.
      Class<? extends Service> clazz =
          Class.forName(canonicalClassString).asSubclass(Service.class);
      String filterKey = intent.getStringExtra(C2DMessaging.EXTRA_FILTER_KEY);
      String filterValue = intent.getStringExtra(C2DMessaging.EXTRA_FILTER_VALUE);
      boolean handleWakeLock = intent.getBooleanExtra(C2DMessaging.EXTRA_HANDLE_WAKELOCK, false);
      return new C2DMObserver(clazz, filterKey, filterValue, handleWakeLock);
    } catch (ClassNotFoundException e) {
      logger.severe("Unable to register observer class %s", canonicalClassString);
      return null;
    }
  }

  /**
   * Creates a new observer for the provided service, selection criteria, and wake lock behavior.
   */
  
  C2DMObserver(Class<? extends Service> messageService, String selectKey, String selectValue,
      boolean handleWakeLock) {
    Preconditions.checkNotNull(messageService);
    this.messageService = messageService;
    this.selectKey = selectKey;
    this.selectValue = selectValue;
    this.handleWakeLock = handleWakeLock;
  }

  /**
   * Returns the JSON object representation of the observer state.
   */
  JSONObject toJSON() {
    try {
      JSONObject json = new JSONObject();
      json.put(KEY_CLASS, messageService.getCanonicalName());
      json.put(KEY_FILTER_KEY, selectKey);
      json.put(KEY_FILTER_VALUE, selectValue);
      json.put(KEY_HANDLE_WAKE_LOCK, handleWakeLock);
      return json;
    } catch (JSONException e) {
      logger.severe("Unable to create JSON object from observer %s", toString());
      return null;
    }
  }

  /**
   * Returns {@code true} if the provided intent matches the selection criteria for this
   * observer.
   */
  boolean matches(Intent intent) {
    if (selectKey == null) {
      return true;
    }
    if (intent.hasExtra(selectKey)) {
      return selectValue == null || selectValue.equals(intent.getStringExtra(selectKey));
    }
    return false;
  }

  Class<?> getObserverClass() {
    return messageService;
  }

  
  String getFilterKey() {
    return selectKey;
  }

  
  String getFilterValue() {
    return selectValue;
  }

  
  boolean isHandleWakeLock() {
    return handleWakeLock;
  }

  @Override
  public boolean equals(Object o) {
    if (this == o) {
      return true;
    }
    if (o == null || getClass() != o.getClass()) {
      return false;
    }
    C2DMObserver that = (C2DMObserver) o;
    if (!messageService.equals(that.messageService)) {
      return false;
    }
    if (selectKey != null ? !selectKey.equals(that.selectKey) : that.selectKey != null) {
      return false;
    }
    if (selectValue != null ? !selectValue.equals(that.selectValue) : that.selectValue
        != null) {
      return false;
    }
    return handleWakeLock == that.handleWakeLock;
  }

  @Override
  public int hashCode() {
    int result = messageService.hashCode();
    result = 31 * result + (selectKey != null ? selectKey.hashCode() : 0);
    result = 31 * result + (selectValue != null ? selectValue.hashCode() : 0);
    result = 31 * result + (handleWakeLock ? 1 : 0);
    return result;
  }

  @Override
  public String toString() {
    return "C2DMObserver{" + "mClass=" + messageService + ", mFilterKey='" + selectKey + '\''
        + ", mFilterValue='" + selectValue + '\'' + ", mHandleWakeLock=" + handleWakeLock + '}';
  }
}
