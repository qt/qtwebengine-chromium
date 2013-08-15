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

package com.google.ipc.invalidation.ticl.android;

import com.google.ipc.invalidation.external.client.SystemResources;
import com.google.ipc.invalidation.external.client.SystemResources.NetworkChannel;
import com.google.ipc.invalidation.external.client.SystemResources.Scheduler;
import com.google.ipc.invalidation.external.client.SystemResources.Storage;
import com.google.ipc.invalidation.external.client.SystemResourcesBuilder;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.util.NamedRunnable;

import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.TimeUnit;


/**
 * SystemResources creator for the Android system service.
 *
 */
public class AndroidResourcesFactory {

  /**
   * Implementation of {@link SystemResources.Scheduler} based on {@code ThreadExecutor}.
   *
   */
  private static class ExecutorBasedScheduler implements Scheduler {

    private SystemResources systemResources;

    /** Scheduler for running and scheduling tasks. */
    private final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();

    /** Thread that belongs to the scheduler. */
    private Thread thread = null;

    private final String threadName;

    /** Creates a scheduler with thread {@code name} for internal logging, etc. */
    private ExecutorBasedScheduler(final String name) {
      threadName = name;
    }

    @Override
    public long getCurrentTimeMs() {
      return System.currentTimeMillis();
    }

    @Override
    public void schedule(final int delayMs, final Runnable runnable) {
      // For simplicity, schedule first and then check when the event runs later if the resources
      // have been shut down.
      scheduler.schedule(new NamedRunnable("AndroidScheduler") {
        @Override
        public void run() {
          if (thread != Thread.currentThread()) {
            // Either at initialization or if the thread has been killed or restarted by the
            // Executor service.
            thread = Thread.currentThread();
            thread.setName(threadName);
          }

          if (systemResources.isStarted()) {
            runnable.run();
          } else {
            systemResources.getLogger().warning("Not running on internal thread since resources " +
              "not started %s, %s", delayMs, runnable);
          }
        }
      }, delayMs, TimeUnit.MILLISECONDS);
    }

    @Override
    public boolean isRunningOnThread() {
      return (thread == null) || (Thread.currentThread() == thread);
    }

    @Override
    public void setSystemResources(SystemResources resources) {
      this.systemResources = resources;
    }
  }

  //
  // End of nested classes.
  //

  /**
   * Constructs a {@link SystemResourcesBuilder} instance using default scheduling, Android-style
   * logging, and storage, and using {@code network} to send and receive messages.
   */
  public static SystemResourcesBuilder createResourcesBuilder(String logPrefix,
      NetworkChannel network, Storage storage) {
    SystemResourcesBuilder builder = new SystemResourcesBuilder(AndroidLogger.forPrefix(logPrefix),
          new ExecutorBasedScheduler("ticl" + logPrefix),
          new ExecutorBasedScheduler("ticl-listener" + logPrefix),
          network, storage);
    builder.setPlatform("Android-" + android.os.Build.VERSION.RELEASE);
    return builder;
  }
}
