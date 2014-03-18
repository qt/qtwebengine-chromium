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

package com.google.ipc.invalidation.external.client.android.service;

import android.os.Bundle;

/**
 * Defines the bound service interface for an Invalidation listener service.  The service 
 * exposes an intent-like model with a single {@link #handleEvent} entry point that packages
 * the event action and its parameters into a {@link Bundle} but uses a synchronous calling
 * model where a response bundle is also returned to the service containing status and/or
 * <p>
 * Having a single entry point (as compared to a interface method per action with explicit
 * parameters) will make it easier to evolve the interface over time.   New event types or
 * additional optional parameters can be added in subsequent versions without changing the
 * service interface in ways that would be incompatible with existing clients.  This is
 * important because the listener services will be packaged (and updated) independently from
 * the invalidation service.
 * <p>
 * The synchronous nature of the interface (having a response object that can indicate success
 * or failure of event handling) is important to support reliable events.  If the service
 * sends a registration request, it's important to know that it has been successfully received
 * by the local invalidation service.
 *
 * The invalidation service will bind to the invalidation listener using an intent that
 * contains the {@link Event.LISTENER} action along with the explicit listener class name
 * that was provided to {@code AndroidClientFactory.create()}.
 */
interface ListenerService {

  /**
   * Sends a request to the invalidation service and retrieves the response containing any
   * return data or status/error information.   The {@code action} parameter in the request
   * bundle will indicate the type of request to be executed and the request parameters will
   * also be stored in the bundle.   The service will acknowledge successful processing of
   * the request by returning a response bundle that contains a {@code status} parameter
   * indicating the success or failure of the request.  If successful, any other output
   * parameters will be included as values in the response bundle.  On failure, additional
   * error or debug information will be included in the response bundle.
   *
   * @see Event
   * @see Response
   */
  void handleEvent(in Bundle event, out Bundle response);
}
