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

package com.google.ipc.invalidation.external.client;

import com.google.ipc.invalidation.ticl.InvalidationClientCore;
import com.google.ipc.invalidation.ticl.InvalidationClientImpl;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientConfigP;

import java.util.Random;

/**
 * Helper utility functions for testing with the invalidation client.
 *
 *
 */
public class InvalidationClientTestHelper {

  /**
   * Constructs an invalidation client library instance with parameters set for unit tests.
   *
   * @param clientType client type code as assigned by the notification system's backend
   * @param clientName id/name of the client in the application's own naming scheme
   * @param applicationName name of the application using the library (for debugging/monitoring)
   * @param listener callback object for invalidation events
   */
  public static InvalidationClient createForTest(SystemResources resources,
      int clientType, byte[] clientName, String applicationName, InvalidationListener listener) {
    ClientConfigP config = InvalidationClientCore.createConfigForTest().build();
    Random random = new Random(resources.getInternalScheduler().getCurrentTimeMs());
    return new InvalidationClientImpl(resources, random, clientType, clientName, config,
        applicationName, listener);
  }
}
