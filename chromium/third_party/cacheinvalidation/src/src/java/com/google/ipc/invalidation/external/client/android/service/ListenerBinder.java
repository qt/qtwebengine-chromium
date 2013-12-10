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
import android.content.Intent;
import android.os.IBinder;

/**
 * A service binder for connecting to a client {@link ListenerService}.
 *
 */
public class ListenerBinder extends ServiceBinder<ListenerService> {

  /**
   * Constructs a new ListenerBinder that connects to the ListenerService bound by the provided
   * intent. The intent should contain the component or class name of the target listener.
   */
  public ListenerBinder(Context context, Intent listenerIntent, String listenerClassName) {
    super(context, listenerIntent, ListenerService.class, listenerClassName);
  }

  @Override
  protected ListenerService asInterface(IBinder binder) {
    return ListenerService.Stub.asInterface(binder);
  }
}
