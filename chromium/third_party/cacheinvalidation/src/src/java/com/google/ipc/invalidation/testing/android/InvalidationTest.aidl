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

package com.google.ipc.invalidation.testing.android;

import android.os.Bundle;

interface InvalidationTest {

  /**
   * Used to set whether the invalidation test service should store incoming
   * actions and outgoing events respectively by {@link getActionEvents()}
   * and {@link getEventIntents()}.  If {@code false}, they will be processed
   * and forgotten.
   */
  void setCapture(boolean captureActions, boolean captureEvents);

  /**
   * Returns an array of bundle containing the set of invalidation requests
   * received by the test service since the last call to this method.
   */
  Bundle [] getRequests();

  /**
   * Returns an array of intents containing the set of invalidation event
   * bundles sent by the test service since the last call to this method.
   */
  Bundle [] getEvents();

  /**
   * Instructs the test service to send an event back to the client to support
   * testing of listener functionality.
   */
  void sendEvent(in Bundle eventBundle);
  
  /**
   * Reset all state for the invalidation test service.  This will clear all
   * current clients and drop and disable any captured action or event bundles.
   */
  void reset();
}
