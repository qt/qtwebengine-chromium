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
import com.google.ipc.invalidation.external.client.SystemResources;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.SystemResources.Scheduler;
import com.google.ipc.invalidation.ticl.RecurringTask;
import com.google.ipc.invalidation.util.NamedRunnable;
import com.google.ipc.invalidation.util.TypedUtil;
import com.google.protos.ipc.invalidation.AndroidService.AndroidSchedulerEvent;

import android.app.AlarmManager;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import java.util.HashMap;
import java.util.Map;


/**
 * Scheduler for controlling access to internal Ticl state in Android.
 * <p>
 * This class maintains a map from recurring task names to the recurring task instances in the
 * associated Ticl. To schedule a recurring task, it uses the {@link AlarmManager} to schedule
 * an intent to itself at the appropriate time in the future. This intent contains the name of
 * the task to run; when it is received, this class looks up the appropriate recurring task
 * instance and runs it.
 * <p>
 * Note that this class only supports scheduling recurring tasks, not ordinary runnables. In
 * order for it to be used, the application must declare the AlarmReceiver of the scheduler
 * in the application's manifest file; see the implementation comment in AlarmReceiver for
 * details.
 *
 */
public final class AndroidInternalScheduler implements Scheduler {
  /** Class that receives AlarmManager broadcasts and reissues them as intents for this service. */
  public static final class AlarmReceiver extends BroadcastReceiver {
    /*
     * This class needs to be public so that it can be instantiated by the Android runtime.
     * Additionally, it should be declared as a broadcast receiver in the application manifest:
     * <receiver android:name="com.google.ipc.invalidation.ticl.android2.\
     *  AndroidInternalScheduler$AlarmReceiver" android:enabled="true"/>
     */

    @Override
    public void onReceive(Context context, Intent intent) {
      // Resend the intent to the service so that it's processed on the handler thread and with
      // the automatic shutdown logic provided by IntentService.
      intent.setClassName(context, new AndroidTiclManifest(context).getTiclServiceClass());
      context.startService(intent);
    }
  }

  /**
   * If {@code true}, {@link #isRunningOnThread} will verify that calls are being made from either
   * the {@link TiclService} or the {@link TestableTiclService.TestableClient}.
   */
  public static boolean checkStackForTest = false;

  /** Class name of the testable client class, for checking call stacks in tests. */
  private static final String TESTABLE_CLIENT_CLASSNAME_FOR_TEST =
      "com.google.ipc.invalidation.ticl.android2.TestableTiclService$TestableClient";

  /**
   * {@link RecurringTask}-created runnables that can be executed by this instance, by their names.
   */
  private final Map<String, Runnable> registeredTasks = new HashMap<String, Runnable>();

  /** Android system context. */
  private final Context context;

  /** Source of time for computing scheduling delays. */
  private final AndroidClock clock;

  private Logger logger;

  /** Id of the Ticl for which this scheduler will process events. */
  private long ticlId = -1;

  AndroidInternalScheduler(Context context, AndroidClock clock) {
    this.context = Preconditions.checkNotNull(context);
    this.clock = Preconditions.checkNotNull(clock);
  }

  @Override
  public void setSystemResources(SystemResources resources) {
    this.logger = Preconditions.checkNotNull(resources.getLogger());
  }

  @Override
  public void schedule(int delayMs, Runnable runnable) {
    if (!(runnable instanceof NamedRunnable)) {
      throw new RuntimeException("Unsupported: can only schedule named runnables, not " + runnable);
    }
    // Create an intent that will cause the service to run the right recurring task. We explicitly
    // target it to our AlarmReceiver so that no other process in the system can receive it and so
    // that our AlarmReceiver will not be able to receive events from any other broadcaster (which
    // it would be if we used action-based targeting).
    String taskName = ((NamedRunnable) runnable).getName();
    Intent eventIntent = ProtocolIntents.newSchedulerIntent(taskName, ticlId);
    eventIntent.setClass(context, AlarmReceiver.class);

    // Create a pending intent that will cause the AlarmManager to fire the above intent.
    PendingIntent sender = PendingIntent.getBroadcast(context,
        (int) (Integer.MAX_VALUE * Math.random()), eventIntent, PendingIntent.FLAG_ONE_SHOT);

    // Schedule the pending intent after the appropriate delay.
    AlarmManager alarmManager = (AlarmManager) context.getSystemService(Context.ALARM_SERVICE);
    long executeMs = clock.nowMs() + delayMs;
    alarmManager.set(AlarmManager.RTC, executeMs, sender);
  }

  /**
   * Handles an event intent created in {@link #schedule} by running the corresponding recurring
   * task.
   * <p>
   * REQUIRES: a recurring task with the name in the intent be present in {@link #registeredTasks}.
   */
  void handleSchedulerEvent(AndroidSchedulerEvent event) {
    Runnable recurringTaskRunnable = Preconditions.checkNotNull(
        TypedUtil.mapGet(registeredTasks, event.getEventName()),
        "No task registered for %s", event.getEventName());
    if (ticlId != event.getTiclId()) {
      logger.warning("Ignoring event with wrong ticl id (not %s): %s", ticlId, event);
      return;
    }
    recurringTaskRunnable.run();
  }

  /**
   * Registers {@code task} so that it can be subsequently run by the scheduler.
   * <p>
   * REQUIRES: no recurring task with the same name be already present in {@link #registeredTasks}.
   */
  void registerTask(String name, Runnable runnable) {
    Runnable previous = registeredTasks.put(name, runnable);
    Preconditions.checkState(previous == null,
        "Cannot overwrite task registered on %s, %s; tasks = %s",
        name, this, registeredTasks.keySet());
  }

  @Override
  public boolean isRunningOnThread() {
    if (!checkStackForTest) {
      return true;
    }
    // If requested, check that the current stack looks legitimate.
    for (StackTraceElement stackElement : Thread.currentThread().getStackTrace()) {
      if (stackElement.getMethodName().equals("onHandleIntent") &&
          stackElement.getClassName().contains("TiclService")) {
        // Called from the TiclService.
        return true;
      }
      if (stackElement.getClassName().equals(TESTABLE_CLIENT_CLASSNAME_FOR_TEST)) {
        // Called from the TestableClient.
        return true;
      }
    }
    return false;
  }

  @Override
  public long getCurrentTimeMs() {
    return clock.nowMs();
  }

  /** Removes the registered tasks. */
  void reset() {
    logger.fine("Clearing registered tasks on %s", this);
    registeredTasks.clear();
  }

  /**
   * Sets the id of the ticl for which this scheduler will process events. We do not know the
   * Ticl id until done constructing the Ticl, and we need the scheduler to construct a Ticl. This
   * method breaks what would otherwise be a dependency cycle on getting the Ticl id.
   */
  void setTiclId(long ticlId) {
    this.ticlId = ticlId;
  }
}
