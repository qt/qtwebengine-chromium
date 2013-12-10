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

import com.google.ipc.invalidation.external.client.types.ObjectId;

import android.content.Intent;
import android.os.Bundle;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Creates and interprets request message bundles sent to the invalidation
 * service from its application clients.
 *
 */
public final class Request extends Message {

  /**
   * An intent that can be used to bind to the invalidation service.
   */
  public static final Intent SERVICE_INTENT = new Intent("com.google.ipc.invalidation.SERVICE");

  /**
   * Contains the list of request action names.
   */
  public static enum Action {

    /** Creates a new invalidation client instance */
    CREATE,

    /** Resumes an invalidation client instances */
    RESUME,

    /** Starts the invalidation client */
    START,

    /** Stops an invalidation client */
    STOP,

    /** Registers for invalidation notifications on an object */
    REGISTER,

    /** Unregisters for invalidation notifications on an object */
    UNREGISTER,

    /** Acknowledges an event recieved from the invalidations service */
    ACKNOWLEDGE,

    /** Destroys the client permanently */
    DESTROY;
  }

  /**
   * Contains the list of parameter names that are valid for request bundles
   */
  public static class Parameter extends Message.Parameter {
    private Parameter() {} // not instantiable

    /** Contains the integer client type */
    public static final String CLIENT_TYPE = "clientType";

    /** Contains an {@link Intent} value that can be used to bind for event delivery */
    public static final String INTENT = "intent";

    /** Contains an {@link ArrayList} of {@link ObjectId} instances */
    public static final String OBJECT_ID_LIST = "objectIdList";
  }

  /**
   * A builder class for constructing new request messages.
   *
   * @see #newBuilder
   */
  public static class Builder extends Message.Builder<Request, Builder> {

    // Constructed using newBuilder()
    private Builder(Action action) {
      super(action.ordinal(), new Bundle());
    }

    /**
     * Stores the client type within a request message.
     */
    public Builder setClientType(int clientType) {
      bundle.putInt(Parameter.CLIENT_TYPE, clientType);
      return this;
    }

    /**
     * Stores an intent within a request message.
     */
    public Builder setIntent(Intent eventIntent) {
      bundle.putParcelable(Parameter.INTENT, eventIntent);
      return this;
    }


    /** Stores a collection object IDs in the built parameters */
    public Builder setObjectIds(Collection<ObjectId> objectIds) {
      ArrayList<ParcelableObjectId> objectList =
          new ArrayList<ParcelableObjectId>(objectIds.size());
      for (ObjectId objectId : objectIds) {
        objectList.add(new ParcelableObjectId(objectId));
      }
      bundle.putParcelableArrayList(Parameter.OBJECT_ID_LIST, objectList);
      return this;
    }

    /**
     * Returns an event containing the set parameters.
     */
    @Override
    public Request build() {
      return new Request(bundle);
    }
  }

  /**
   * Constructs a new builder for a request associated with the provided action.
   */
  public static Builder newBuilder(Action action) {
    return new Builder(action);
  }

  /**
   * Constructs a new request using the contents of the provided parameter bundle.
   */
  public Request(Bundle bundle) {
    super(bundle);
  }

  /**
   * Returns the action from a request message.
   */
  public Action getAction() {
    return Action.values()[getActionOrdinal()];
  }

  /**
   * Returns the client type from a request message, or {@code -1} if not present.
   */
  public int getClientType() {
    return parameters.getInt(Parameter.CLIENT_TYPE, -1);
  }

  /**
   * Returns the intent from a request message, or {@code null} if not present.
   */
  public Intent getIntent() {
    return parameters.getParcelable(Parameter.INTENT);
  }


  /** Returns the object ID set on the message or {@code null} if not set */
  public Collection<ObjectId> getObjectIds() {
    List<ParcelableObjectId> parcelableObjectIds =
        parameters.getParcelableArrayList(Parameter.OBJECT_ID_LIST);
    if (parcelableObjectIds == null) {
      return null;
    }
    List<ObjectId> objectIds = new ArrayList<ObjectId>(parcelableObjectIds.size());
    for (ParcelableObjectId parcelableObjectId : parcelableObjectIds) {
      objectIds.add(parcelableObjectId.objectId);
    }
    return objectIds;
  }
}
