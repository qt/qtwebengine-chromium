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

import android.content.Context;
import android.os.IBinder;

/**
 * A service binder implementation for connecting to the {@link InvalidationService}.
 *
 */
public class InvalidationBinder extends ServiceBinder<InvalidationService> {

  /**
   * Contains the name of the service implementation class that will be explicit bound to or
   * {@code null} if implicitly binding
   */
  private static String serviceClassName;

  static {
    try {
      // If the expected service class if found in the current application, use an explicit binding
      // otherwise an implicit, intent-based binding will be used.  The latter capability is
      // generally only to support mock and test service bindings.
      Class<?> serviceClass =
          Class.forName("com.google.ipc.invalidation.ticl.android.AndroidInvalidationService");
      serviceClassName = serviceClass.getName();
    } catch (ClassNotFoundException e) {
      serviceClassName = null;
    }
  }

  /**
   * Constructs a new InvalidationBinder that connects to the invalidation service.
   */
  public InvalidationBinder(Context context) {
    super(context, Request.SERVICE_INTENT, InvalidationService.class, serviceClassName);
  }

  @Override
  protected InvalidationService asInterface(IBinder binder) {
    return InvalidationService.Stub.asInterface(binder);
  }
}
