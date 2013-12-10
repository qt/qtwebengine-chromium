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

import com.google.ipc.invalidation.external.client.android.service.Request.Action;
import com.google.ipc.invalidation.external.client.types.AckHandle;
import com.google.ipc.invalidation.external.client.types.ObjectId;

import android.accounts.Account;
import android.os.Bundle;

/**
 * A base class for message bundles sent to/from the invalidation service. This
 * class provides the declaration of the base parameter names, a basic builder
 * class for storing them in messages, and the accessor methods to read them.
 *
 */
public abstract class Message {
  protected final Bundle parameters;

  /** The list of parameter names that are common across message types */
  public static class Parameter {
    protected Parameter() {} // can subclass but not instantiate

    public static final String ACTION = "action";

    public static final String ACCOUNT = "account";

    public static final String AUTH_TYPE = "authType";

    public static final String ACK_TOKEN = "ackToken";

    public static final String CLIENT = "client";

    public static final String OBJECT_ID = "objectId";

    /** A string value describing any error in processing */
    public static final String ERROR = "error";
  }

  /**
   * A base builder class for constructing new messages and populating
   * them with parameter values.
   *
   * @param <MessageType> the message type constructed by the builder
   * @param <BuilderType> the concrete builder subtype
   */
  protected abstract static class Builder<MessageType extends Message,
                                          BuilderType extends Builder<?, ?>> {
    protected final Bundle bundle;

    // Typed pointer to 'this' that's set once in constructor for return by setters,
    // to avoid unchecked warning suppression throughout the code
    private BuilderType thisInstance;

    @SuppressWarnings("unchecked")
    protected Builder(int actionOrdinal, Bundle b) {
      this.bundle = b;
      this.thisInstance = (BuilderType) this; // requires unchecked but is safe
      b.putInt(Parameter.ACTION, actionOrdinal);
    }

    /** Stores an account in the built parameters. */
    public BuilderType setAccount(Account account) {
      bundle.putParcelable(Parameter.ACCOUNT, account);
      return thisInstance;
    }

    /** Stores an acknowledgement handle in the built parameters. */
    public BuilderType setAckHandle(AckHandle ackHandle) {
      bundle.putByteArray(Parameter.ACK_TOKEN, ackHandle.getHandleData());
      return thisInstance;
    }

    /** Stores the authentication type in the built parameters */
    public BuilderType setAuthType(String authType) {
      bundle.putString(Parameter.AUTH_TYPE, authType);
      return thisInstance;
    }

    /** Stores a client key in the built parameters. */
    public BuilderType setClientKey(String clientKey) {
      bundle.putString(Parameter.CLIENT, clientKey);
      return thisInstance;
    }


    /** Stores an error message value within a response message.*/
    public BuilderType setError(String message) {
      bundle.putString(Parameter.ERROR, message);
      return thisInstance;
    }

    /** Stores an object ID in the built parameters. */
    public BuilderType setObjectId(ObjectId objectId) {
      bundle.putParcelable(Parameter.OBJECT_ID, new ParcelableObjectId(objectId));
      return thisInstance;
    }

    /**
     * Returns the message associated with the builder.  Concrete subclasses
     * will override to return a concrete message instance.
     */
    public abstract MessageType build();
  }

  /**
   * Constructs a new message containing the the parameters in the provide bundle.
   */
  protected Message(Bundle bundle) {
    this.parameters = bundle;
  }

  /**
   * Returns the parameter bundle associated with the message.
   */
  public Bundle getBundle() {
    return parameters;
  }

  /**
   * Returns the action set on the message or {@code null} if not set. For
   * request or event messages, this will be the action associated with the
   * message. For response messages, it will be the action associated with the
   * original request or event that is being responded to.
   */
  public int getActionOrdinal() {
    return parameters.getInt(Parameter.ACTION);
  }

  /** Returns the account set on the message or {@code null} if not set. */
  public Account getAccount() {
    return parameters.getParcelable(Parameter.ACCOUNT);
  }

  /** Returns the acknowledgement handle set on the message or {@code null} if not set */
  public AckHandle getAckHandle() {
    byte [] tokenData = parameters.getByteArray(Parameter.ACK_TOKEN);
    return tokenData != null ? AckHandle.newInstance(tokenData) : null;
  }

  /** Returns the authentication type set on the message or {@code null} if not set */
  public String getAuthType() {
    return parameters.getString(Parameter.AUTH_TYPE);
  }

  /** Returns the client key set on the message or {@code null} if not set */
  public String getClientKey() {
    return parameters.getString(Parameter.CLIENT);
  }

  /**
   * Returns the error message string or {@code null} if not present.
   */
  public String getError() {
    return parameters.getString(Parameter.ERROR);
  }

  /** Returns the object id set on the message or {@code null} if not set */
  public ObjectId getObjectId() {
    ParcelableObjectId parcelableObjectId = parameters.getParcelable(Parameter.OBJECT_ID);
    return parcelableObjectId != null ? parcelableObjectId.objectId : null;
  }

  @Override
  public String toString() {
    String actionStr = (getActionOrdinal() < Action.values().length) ?
        Action.values()[getActionOrdinal()].toString() : "invalid";
    return "Message ACTION = " + actionStr + " CLIENT = " + getClientKey();
  }
}
