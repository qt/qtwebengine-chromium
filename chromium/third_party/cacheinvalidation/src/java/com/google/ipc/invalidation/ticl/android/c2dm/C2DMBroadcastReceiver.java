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

import android.app.Activity;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/**
 * Helper class to handle BroadcastReceiver behavior. It can only run for a limited amount of time
 * so it starts the real service for longer activities (which then gets the power lock and releases
 * it when it is done).
 *
 * Based on the open source chrometophone project.
 */
public class C2DMBroadcastReceiver extends BroadcastReceiver {

    @Override
    public final void onReceive(Context context, Intent intent) {
        // To keep things in one place, we run everything in the base receiver intent service
        C2DMManager.runIntentInService(context, intent);
        setResult(Activity.RESULT_OK, null /* data */, null /* extra */);
    }

}
