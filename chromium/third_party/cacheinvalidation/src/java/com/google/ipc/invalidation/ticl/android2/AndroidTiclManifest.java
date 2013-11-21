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

package com.google.ipc.invalidation.ticl.android2;

import com.google.common.base.Preconditions;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;

import java.util.HashMap;
import java.util.Map;


/**
 * Interface to the {@code AndroidManifest.xml} that provides access to the configuration data
 * required by the Android Ticl.
 *
 */
public class AndroidTiclManifest {
  /**
   * Name of the {@code <application>} metadata element whose value gives the Java class that
   * implements the application {@code InvalidationListener}. Must be set if
   * {@link #LISTENER_SERVICE_NAME_KEY} is not set.
   */
  private static final String LISTENER_NAME_KEY = "ipc.invalidation.ticl.listener_class";

  /**
   * Name of the {@code <application>} metadata element whose value gives the Java class that
   * implements the Ticl service. Should only be set in tests.
   */
  private static final String TICL_SERVICE_NAME_KEY = "ipc.invalidation.ticl.service_class";

  /**
   * Name of the {@code <application>} metadata element whose value gives the Java class that
   * implements the application's invalidation listener intent service.
   */
  private static final String LISTENER_SERVICE_NAME_KEY =
      "ipc.invalidation.ticl.listener_service_class";

  /** Default values returned if not overriden by the manifest file. */
  private static final Map<String, String> DEFAULTS = new HashMap<String, String>();
  static {
      DEFAULTS.put(TICL_SERVICE_NAME_KEY,
          "com.google.ipc.invalidation.ticl.android2.TiclService");
      DEFAULTS.put(LISTENER_NAME_KEY, "");
      DEFAULTS.put(LISTENER_SERVICE_NAME_KEY,
          "com.google.ipc.invalidation.ticl.android2.AndroidInvalidationListenerStub");
  }

  private final Context context;

  public AndroidTiclManifest(Context context) {
    this.context = Preconditions.checkNotNull(context);
  }

  /** Returns the name of the class implementing the Ticl service. */
  public String getTiclServiceClass() {
    return Preconditions.checkNotNull(readApplicationMetadata(TICL_SERVICE_NAME_KEY));
  }

  /** Returns the name of the class on which listener events will be invoked. */
  String getListenerClass() {
    return Preconditions.checkNotNull(readApplicationMetadata(LISTENER_NAME_KEY));
  }

  /** Returns the name of the class implementing the invalidation listener intent service. */
  public String getListenerServiceClass() {
    return Preconditions.checkNotNull(readApplicationMetadata(LISTENER_SERVICE_NAME_KEY));
  }

  /**
   * Returns the metadata-provided value for {@code key} in  {@code AndroidManifest.xml} if one
   * exists, or the value from {@link #DEFAULTS} if one does not.
   */
  private String readApplicationMetadata(String key) {
    ApplicationInfo appInfo;
    try {
      // Read the manifest-provided value.
      appInfo = context.getPackageManager().getApplicationInfo(context.getPackageName(),
          PackageManager.GET_META_DATA);
      String value = null;
      if (appInfo.metaData != null) {
        value = appInfo.metaData.getString(key);
      }
      // Return the manifest value if present or the default value if not.
      return (value != null) ?
          value : Preconditions.checkNotNull(DEFAULTS.get(key), "No default value for %s", key);
    } catch (NameNotFoundException exception) {
      throw new RuntimeException("Cannot read own application info", exception);
    }
  }
}
