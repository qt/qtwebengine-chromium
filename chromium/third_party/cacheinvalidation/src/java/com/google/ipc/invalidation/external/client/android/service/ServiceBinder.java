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

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import java.util.LinkedList;
import java.util.Queue;


/**
 * Abstract base class that assists in making connections to a bound service. Subclasses can define
 * a concrete binding to a particular bound service interface by binding to an explicit type on
 * declaration, providing a public constructor, and providing an implementation of the
 * {@link #asInterface} method.
 * <p>
 * This class has two main methods: {@link #runWhenBound} and {@link #release()}.
 * {@code runWhenBound} submits a {@code receiver} to be invoked once the service is bound,
 * initiating a bind if necessary. {@code release} releases the binding if one exists.
 * <p>
 * Interestingly, invocations of receivers passed to {@code runWhenBound} and calls to
 * {@code release} will be executed in program order. I.e., a call to runWhenBound followed by
 * a call to release will result in the receiver passed to runWhenBound being invoked before the
 * release, even if the binder had to wait for the service to be bound.
 * <p>
 * It is legal to call runWhenBound after a call to release.
 *
 * @param <BoundService> the bound service interface associated with the binder.
 *
 */
public abstract class ServiceBinder<BoundService> {
  /**
   * Interface for a work unit to be executed when the service is bound.
   * @param <ServiceType> the bound service interface type
   */
  public interface BoundWork<ServiceType> {
    /** Function called with the bound service once the service is bound. */
    void run(ServiceType service);
  }
  /** Logger */
  private static final Logger logger = AndroidLogger.forTag("InvServiceBinder");

  /** Intent that can be used to bind to the service */
  private final Intent serviceIntent;

  /** Class that represents the bound service interface */
  private final Class<BoundService> serviceClass;

  /** Name of the component that implements the service interface. */
  private final String componentClassName;

  /** Work waiting to be run when the service becomes bound. */
  private final Queue<BoundWork<BoundService>> pendingWork =
      new LinkedList<BoundWork<BoundService>>();

  /** Used to synchronize. */
  private final Object lock = new Object();

  /** Bound service instance held by the binder or {@code null} if not bound. */
  private BoundService serviceInstance = null;

  /** Context to use when binding and unbinding. */
  private final Context context;

  /** Whether bindService has been called. */
  private boolean hasCalledBind = false;

  /** Whether we are currently executing an event from the queue. */
  private boolean queueHandlingInProgress = false;

  /** Number of times {@link #startBind()} has been called, for tests. */
  private int numStartBindForTest = 0;

  /**
   * Service connection implementation that handles connection/disconnection
   * events for the binder.
   */
  private final ServiceConnection serviceConnection = new ServiceConnection() {

    @Override
    public void onServiceConnected(ComponentName serviceName, IBinder binder) {
      logger.fine("onServiceConnected: %s", serviceName);
      synchronized (lock) {
        // Once the service is bound, save it and run any work that was waiting for it.
        serviceInstance = asInterface(binder);
        handleQueue();
      }
    }

    @Override
    public void onServiceDisconnected(ComponentName serviceName) {
      logger.fine("onServiceDisconnected: %s", serviceClass);
      synchronized (lock) {
        serviceInstance = null;
      }
    }
  };

  /**
   * Constructs a new ServiceBinder that uses the provided intent to bind to the service of the
   * specific type. Subclasses should expose a public constructor that passes the appropriate intent
   * and type into this constructor.
   *
   * @param context context to use for (un)binding.
   * @param serviceIntent intent that can be used to connect to the bound service.
   * @param serviceClass interface exposed by the bound service.
   * @param componentClassName name of component implementing the bound service. If non-null, then
   *        an explicit binding to the named component within the same class is guaranteed.
   */
  protected ServiceBinder(Context context, Intent serviceIntent, Class<BoundService> serviceClass,
      String componentClassName) {
    this.context = Preconditions.checkNotNull(context);
    this.serviceIntent = Preconditions.checkNotNull(serviceIntent);
    this.serviceClass = Preconditions.checkNotNull(serviceClass);
    this.componentClassName = componentClassName;
  }

  /** Returns the intent used to bind to the service */
  public Intent getIntent() {
    Intent bindIntent;
    if (componentClassName == null) {
      return serviceIntent;
    }
    bindIntent = new Intent(serviceIntent);
    bindIntent.setClassName(context, componentClassName);
    return bindIntent;
  }

  /** Runs {@code receiver} when the service becomes bound. */
  public void runWhenBound(BoundWork<BoundService> receiver) {
    synchronized (lock) {
      pendingWork.add(receiver);
      handleQueue();
    }
  }

  /** Unbinds the service associated with the binder. No-op if not bound. */
  public void release() {
    synchronized (lock) {
      if (!hasCalledBind) {
        logger.fine("Release is a no-op since not bound: %s", serviceClass);
        return;
      }
      // We need to release using a runWhenBound to avoid having a release jump ahead of
      // pending work waiting for a bind (i.e., to preserve program order).
      runWhenBound(new BoundWork<BoundService>() {
        @Override
        public void run(BoundService ignored) {
          synchronized (lock) {
            // Do the unbind.
            logger.fine("Unbinding %s from %s", serviceClass, serviceInstance);
            try {
              context.unbindService(serviceConnection);
            } catch (IllegalArgumentException exception) {
              logger.fine("Exception unbinding from %s: %s", serviceClass,
                  exception.getMessage());
            }
            // Clear the now-stale reference and reset hasCalledBind so that we will initiate a
            // bind on a subsequent call to runWhenBound.
            serviceInstance = null;
            hasCalledBind = false;

            // This isn't necessarily wrong, but it's slightly odd.
            if (!pendingWork.isEmpty()) {
              logger.info("Still have %s work items in release of %s", pendingWork.size(),
                  serviceClass);
            }
          }
        }
      });
    }
  }

  /**
   * Returns {@code true} if the service binder is currently connected to the
   * bound service.
   */
  public boolean isBoundForTest() {
    synchronized (lock) {
      return hasCalledBind;
    }
  }

  @Override
  public String toString() {
    synchronized (lock) {
      return this.getClass().getSimpleName() + "[" + serviceIntent + "]";
    }
  }

  /** Returns the number of times {@code startBind} has been called, for tests. */
  public int getNumStartBindForTest() {
    return numStartBindForTest;
  }

  /**
   * Recursively process the queue of {@link #pendingWork}. Initiates a bind to the service if
   * required. Else, if the service instance is available, removes the head of the queue and invokes
   * it with the service instance.
   * <p>
   * Note: this function differs from {@link #runWhenBound} only in that {@code runWhenBound}
   * enqueues into {@link #pendingWork}.
   */
  private void handleQueue() {
    if (queueHandlingInProgress) {
      // Someone called back into runWhenBound from receiver.accept. We don't want to start another
      // recursive call, since we're already handling the queue.
      return;
    }
    if (pendingWork.isEmpty()) {
      // Recursive base case.
      return;
    }
    if (!hasCalledBind) {
      // Initiate a bind if not bound.
      Preconditions.checkState(serviceInstance == null,
          "Bind not called but service instance is set: %s", serviceClass);

      // May fail, but does its own logging. If it fails, we will never dispatch the work in the
      // queue, but it's unclear what we can do in this case other than log.
      startBind();
      return;
    }
    if (serviceInstance == null) {
      // Wait for the service to become bound if it is not yet available and a bind is in progress.
      Preconditions.checkState(hasCalledBind, "No service instance and not waiting for bind: %s",
          serviceClass);
      return;
    }

    // Service is bound and available. Remove and invoke the head of the queue, then recurse to
    // process the rest. We recurse because the head of the queue may have been a release(), which
    // would have unbound the service, and we would need to reinvoke the binding code.
    BoundWork<BoundService> work = pendingWork.remove();
    queueHandlingInProgress = true;
    work.run(serviceInstance);
    queueHandlingInProgress = false;
    handleQueue();
  }

  /**
   * Binds to the service associated with the binder within the provided context. Returns whether
   * binding was successfully initiated.
   */
  private boolean startBind() {
    Preconditions.checkState(!hasCalledBind, "Bind already called for %s", serviceClass);
    ++numStartBindForTest;
    Intent bindIntent = getIntent();
    if (!context.bindService(bindIntent, serviceConnection, Context.BIND_AUTO_CREATE)) {
      logger.severe("Unable to bind to service: %s", bindIntent);
      return false;
    }
    hasCalledBind = true;
    return true;
  }

  /** Returns a bound service stub of the expected type. */
  protected abstract BoundService asInterface(IBinder binder);
}
