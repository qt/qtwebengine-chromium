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

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.external.client.android.service.Event;
import com.google.ipc.invalidation.external.client.android.service.ListenerBinder;
import com.google.ipc.invalidation.external.client.android.service.ListenerService;
import com.google.ipc.invalidation.external.client.android.service.Request;
import com.google.ipc.invalidation.external.client.android.service.Request.Action;
import com.google.ipc.invalidation.external.client.android.service.Request.Parameter;
import com.google.ipc.invalidation.external.client.android.service.Response;
import com.google.ipc.invalidation.external.client.android.service.ServiceBinder.BoundWork;
import com.google.ipc.invalidation.ticl.android.AbstractInvalidationService;
import com.google.ipc.invalidation.util.TypedUtil;

import android.accounts.Account;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;

import junit.framework.Assert;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * A stub invalidation service implementation that can be used to test the
 * client library or invalidation applications. The test service will validate
 * all incoming events sent by the client. It also supports the ability to store
 * all incoming action intents and outgoing event intents and make them
 * available for retrieval via the {@link InvalidationTest} interface.
 * <p>
 * The implementation of service intent handling will simply log the invocation
 * and do nothing else.
 *
 */
public class InvalidationTestService extends AbstractInvalidationService {

  private static class ClientState {
    final Account account;
    final String authType;
    final Intent eventIntent;

    private ClientState(Account account, String authType, Intent eventIntent) {
      this.account = account;
      this.authType = authType;
      this.eventIntent = eventIntent;
    }
  }

  /**
   * Intent that can be used to bind to the InvalidationTest service.
   */
  public static final Intent TEST_INTENT = new Intent("com.google.ipc.invalidation.TEST");

  /** Logger */
  private static final Logger logger = AndroidLogger.forTag("InvTestService");

  /** Map of currently active clients from key to {@link ClientState} */
  private static Map<String, ClientState> clientMap = new HashMap<String, ClientState>();

  /** {@code true} the test service should capture actions */
  private static boolean captureActions;

  /** The stored actions that are available for retrieval */
  private static List<Bundle> actions = new ArrayList<Bundle>();

  /** {@code true} if the client should capture events */
  private static boolean captureEvents;

  /** The stored events that are available for retrieval */
  private static List<Bundle> events = new ArrayList<Bundle>();

  /** Lock over all state in all instances. */
  private static final Object LOCK = new Object();

  /**
   * InvalidationTest stub to handle calls from clients.
   */
  private final InvalidationTest.Stub testBinder = new InvalidationTest.Stub() {

    @Override
    public void setCapture(boolean captureActions, boolean captureEvents) {
      synchronized (LOCK) {
        InvalidationTestService.captureActions = captureActions;
        InvalidationTestService.captureEvents = captureEvents;
      }
    }

    @Override
    public Bundle[] getRequests() {
      synchronized (LOCK) {
        logger.fine("Reading actions from %s:%d", actions, actions.size());
        Bundle[] value = new Bundle[actions.size()];
        actions.toArray(value);
        actions.clear();
        return value;
      }
    }

    @Override
    public Bundle[] getEvents() {
      synchronized (LOCK) {
        Bundle[] value = new Bundle[events.size()];
        events.toArray(value);
        events.clear();
        return value;
      }
    }

    @Override
    public void sendEvent(final Bundle eventBundle) {
      synchronized (LOCK) {
        // Retrive info for that target client
        String clientKey = eventBundle.getString(Parameter.CLIENT);
        ClientState state = clientMap.get(clientKey);
        Preconditions.checkNotNull(state, "No state for %s in %s", clientKey, clientMap.keySet());

        // Bind to the listener associated with the client and send the event
        ListenerBinder binder = new ListenerBinder(getBaseContext(), state.eventIntent,
            InvalidationTestListener.class.getName());
        binder.runWhenBound(new BoundWork<ListenerService>() {
          @Override
          public void run(ListenerService service) {
            InvalidationTestService.this.sendEvent(service, new Event(eventBundle));
          }
        });

        // Will happen after the runWhenBound invokes the receiver. Could also be done inside
        // the receiver.
        binder.release();
      }
    }

    @Override
    public void reset() {
      synchronized (LOCK) {
        logger.info("Resetting test service");
        captureActions = false;
        captureEvents = false;
        clientMap.clear();
        actions.clear();
        events.clear();
      }
    }
  };

  @Override
  public void onCreate() {
    synchronized (LOCK) {
      logger.info("onCreate");
      super.onCreate();
    }
  }

  @Override
  public void onDestroy() {
    synchronized (LOCK) {
      logger.info("onDestroy");
      super.onDestroy();
    }
  }

  @Override
  public int onStartCommand(Intent intent, int flags, int startId) {
    synchronized (LOCK) {
      logger.info("onStart");
      return super.onStartCommand(intent, flags, startId);
    }
  }

  @Override
  public IBinder onBind(Intent intent) {
    synchronized (LOCK) {
      logger.info("onBind");

      // For InvalidationService binding, delegate to the superclass
      if (Request.SERVICE_INTENT.getAction().equals(intent.getAction())) {
        return super.onBind(intent);
      }

      // Otherwise, return the test interface binder
      return testBinder;
    }
  }

  @Override
  public boolean onUnbind(Intent intent) {
    synchronized (LOCK) {
      logger.info("onUnbind");
      return super.onUnbind(intent);
    }
  }

  @Override
  protected void handleRequest(Bundle input, Bundle output) {
    synchronized (LOCK) {
      super.handleRequest(input, output);
      if (captureActions) {
        actions.add(input);
      }
      validateResponse(input, output);
    }
  }

  @Override
  protected void sendEvent(ListenerService listenerService, Event event) {
    synchronized (LOCK) {
      if (captureEvents) {
        events.add(event.getBundle());
      }
      super.sendEvent(listenerService, event);
    }
  }


  @Override
  protected void create(Request request, Response.Builder response) {
    synchronized (LOCK) {
      validateRequest(request, Action.CREATE, Parameter.ACTION, Parameter.CLIENT,
          Parameter.CLIENT_TYPE, Parameter.ACCOUNT, Parameter.AUTH_TYPE, Parameter.INTENT);
      logger.info("Creating client %s:%s", request.getClientKey(), clientMap.keySet());
      if (!TypedUtil.containsKey(clientMap, request.getClientKey())) {
        // If no client exists with this key, create one.
        clientMap.put(
            request.getClientKey(), new ClientState(request.getAccount(), request.getAuthType(),
                request.getIntent()));
      } else {
        // Otherwise, verify that the existing client has the same account / auth type / intent.
        ClientState existingState = TypedUtil.mapGet(clientMap, request.getClientKey());
        Preconditions.checkState(request.getAccount().equals(existingState.account));
        Preconditions.checkState(request.getAuthType().equals(existingState.authType));
      }
      response.setStatus(Response.Status.SUCCESS);
    }
  }

  @Override
  protected void resume(Request request, Response.Builder response) {
    synchronized (LOCK) {
      validateRequest(
          request, Action.RESUME, Parameter.ACTION, Parameter.CLIENT);
      ClientState state = clientMap.get(request.getClientKey());
      if (state != null) {
        logger.info("Resuming client %s:%s", request.getClientKey(), clientMap.keySet());
        response.setStatus(Response.Status.SUCCESS);
        response.setAccount(state.account);
        response.setAuthType(state.authType);
      } else {
        logger.warning("Cannot resume client %s:%s", request.getClientKey(), clientMap.keySet());
        response.setStatus(Response.Status.INVALID_CLIENT);
      }
    }
  }

  @Override
  protected void register(Request request, Response.Builder response) {
    synchronized (LOCK) {
      // Ensure that one (and only one) of the variant object id forms is used
      String objectParam =
        request.getBundle().containsKey(Parameter.OBJECT_ID) ?
            Parameter.OBJECT_ID : Parameter.OBJECT_ID_LIST;
      validateRequest(request, Action.REGISTER, Parameter.ACTION, Parameter.CLIENT, objectParam);
      if (!validateClient(request)) {
        response.setStatus(Response.Status.INVALID_CLIENT);
        return;
      }
      response.setStatus(Response.Status.SUCCESS);
    }
  }

  @Override
  protected void unregister(Request request, Response.Builder response) {
    synchronized (LOCK) {
      // Ensure that one (and only one) of the variant object id forms is used
      String objectParam =
        request.getBundle().containsKey(Parameter.OBJECT_ID) ?
            Parameter.OBJECT_ID :
            Parameter.OBJECT_ID_LIST;
      validateRequest(request, Action.UNREGISTER, Parameter.ACTION,
          Parameter.CLIENT, objectParam);
      if (!validateClient(request)) {
        response.setStatus(Response.Status.INVALID_CLIENT);
        return;
      }
      response.setStatus(Response.Status.SUCCESS);
    }
  }

  @Override
  protected void start(Request request, Response.Builder response) {
    synchronized (LOCK) {
      validateRequest(
          request, Action.START, Parameter.ACTION, Parameter.CLIENT);
      if (!validateClient(request)) {
        response.setStatus(Response.Status.INVALID_CLIENT);
        return;
      }
      response.setStatus(Response.Status.SUCCESS);
    }
  }

  @Override
  protected void stop(Request request, Response.Builder response) {
    synchronized (LOCK) {
      validateRequest(request, Action.STOP, Parameter.ACTION, Parameter.CLIENT);
      if (!validateClient(request)) {
        response.setStatus(Response.Status.INVALID_CLIENT);
        return;
      }
      response.setStatus(Response.Status.SUCCESS);
    }
  }

  @Override
  protected void acknowledge(Request request, Response.Builder response) {
    synchronized (LOCK) {
      validateRequest(request, Action.ACKNOWLEDGE, Parameter.ACTION, Parameter.CLIENT,
          Parameter.ACK_TOKEN);
      if (!validateClient(request)) {
        response.setStatus(Response.Status.INVALID_CLIENT);
        return;
      }
      response.setStatus(Response.Status.SUCCESS);
    }
  }

  @Override
  protected void destroy(Request request, Response.Builder response) {
    synchronized (LOCK) {
      validateRequest(request, Action.DESTROY, Parameter.ACTION, Parameter.CLIENT);
      if (!validateClient(request)) {
        response.setStatus(Response.Status.INVALID_CLIENT);
        return;
      }
      response.setStatus(Response.Status.SUCCESS);
    }
  }

  /**
   * Validates that the client associated with the request is one that has
   * previously been created or resumed on the test service.
   */
  private boolean validateClient(Request request) {
    if (!clientMap.containsKey(request.getClientKey())) {
      logger.warning("Client %s is not an active client: %s",
                     request.getClientKey(), clientMap.keySet());
      return false;
    }
    return true;
  }

  /**
   * Validates that the request contains exactly the set of parameters expected.
   *
   * @param request request to validate
   * @param action expected action
   * @param parameters expected parameters
   */
  private void validateRequest(Request request, Action action, String... parameters) {
    Assert.assertEquals(action, request.getAction());
    List<String> expectedParameters = new ArrayList<String>(Arrays.asList(parameters));
    Bundle requestBundle = request.getBundle();
    for (String parameter : requestBundle.keySet()) {
      Assert.assertTrue("Unexpected parameter: " + parameter, expectedParameters.remove(parameter));

      // Validate the value
      Object value = requestBundle.get(parameter);
      Assert.assertNotNull(value);
    }
    Assert.assertTrue("Missing parameter:" + expectedParameters, expectedParameters.isEmpty());
  }

  /**
   * Validates a response bundle being returned to a client contains valid
   * success response.
   */
  protected void validateResponse(Bundle input, Bundle output) {
    synchronized (LOCK) {
      int status = output.getInt(Response.Parameter.STATUS, Response.Status.UNKNOWN);
      Assert.assertEquals("Unexpected failure for input = " + input + "; output = " + output,
          Response.Status.SUCCESS, status);
      String error = output.getString(Response.Parameter.ERROR);
      Assert.assertNull(error);
    }
  }

  /** Returns whether a client with key {@code clientKey} is known to the service. */
  public static boolean clientExists(String clientKey) {
    synchronized (LOCK) {
      return TypedUtil.containsKey(clientMap, clientKey);
    }
  }
}
