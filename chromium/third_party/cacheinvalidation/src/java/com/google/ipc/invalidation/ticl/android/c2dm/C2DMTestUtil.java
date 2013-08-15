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


import android.content.Context;

/**
 * Provides utility methods that allow manipulation of underlying C2DM state for testing.
 *
 */

public class C2DMTestUtil {

  // Not instantiable
  private C2DMTestUtil() {}

  /**
   * Clears the C2DM registration ID for the application from within settings.
   */
  public static void clearRegistrationId(Context context) {
    C2DMSettings.clearC2DMRegistrationId(context);
  }

  /**
   * Sets the C2DM registration ID for the application stored within settings.
   */
  public static void setRegistrationId(Context context, String registrationId) {
    C2DMSettings.setC2DMRegistrationId(context, registrationId);
  }
}
