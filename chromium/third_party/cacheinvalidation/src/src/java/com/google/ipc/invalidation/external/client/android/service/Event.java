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

import com.google.ipc.invalidation.external.client.InvalidationListener.RegistrationState;
import com.google.ipc.invalidation.external.client.types.ErrorInfo;
import com.google.ipc.invalidation.external.client.types.Invalidation;

import android.content.Intent;
import android.os.Bundle;

/**
 * Creates and interprets event message bundles sent from the invalidation
 * service to its application clients.
 *
 */
public class Event extends Message {

  /** An intent that can be used to bind to the event listener service */
  public static final Intent LISTENER_INTENT = new Intent("com.google.ipc.invalidation.EVENT");

  /** The set of event action types */
  public static enum Action {

    /** Indicates client is ready for use */
    READY,

    /** Invalidate an object */
    INVALIDATE,

    /** Invalidation an object with an unknown version */
    INVALIDATE_UNKNOWN,

    /** Invalidate all objects */
    INVALIDATE_ALL,

    /** Registration status change notification */
    INFORM_REGISTRATION_STATUS,

    /** Registration failure */
    INFORM_REGISTRATION_FAILURE,

    /** Request to reissue registrations */
    REISSUE_REGISTRATIONS,

    /** Processing error */
    INFORM_ERROR,
  }

  /** The set of parameters that can be found in event message bundles */
  public class Parameter extends Message.Parameter {

    /**  Contains a {@link ParcelableErrorInfo} that represents an error */
    public static final String ERROR_INFO = "errorInfo";

    /** Contains an {@link ParcelableInvalidation} */
    public static final String INVALIDATION = "invalidation";

    /** Contains a boolean value indicating whether an event error is transient */
    public static final String IS_TRANSIENT = "isTransient";

    /** Contains an integer ordinal value representing a registration state */
    public static final String REGISTRATION_STATE = "registrationState";

    /** Byte array prefix of registrations requested or -1 if no prefix is present */
    public static final String PREFIX = "prefix";

    /** The integer number of bits in {@link #PREFIX} */
    public static final String PREFIX_LENGTH = "prefixLength";
  }

  /**
   * A builder class for constructing new event messages.
   *
   * @see #newBuilder
   */
  public static class Builder extends Message.Builder<Event, Builder> {

    // Use newBuilder()
    private Builder(Action action) {
      super(action.ordinal(), new Bundle());
    }

    /**
     * Stores error information within an event message.
     */
    public Builder setErrorInfo(ErrorInfo errorInfo) {
      bundle.putParcelable(Parameter.ERROR, new ParcelableErrorInfo(errorInfo));
      return this;
    }

    /**
     * Stores in invalidation within an event message.
     */
    public Builder setInvalidation(Invalidation invalidation) {
      bundle.putParcelable(Parameter.INVALIDATION, new ParcelableInvalidation(invalidation, true));
      return this;
    }

   /*
    * Stores in registrations requested prefix within an event message.
    *
    * @param prefix the registration digest prefix
    * @param prefixLength the number of significant bits in the prefix
    */
   public Builder setPrefix(byte [] prefix, int prefixLength) {
     bundle.putByteArray(Parameter.PREFIX, prefix);
     bundle.putInt(Parameter.PREFIX_LENGTH, prefixLength);
     return this;
   }

    /**
     * Stores the isTransient flag within an event message.
     */
    public Builder setIsTransient(boolean isTransient) {
      bundle.putBooleanArray(Parameter.IS_TRANSIENT, new boolean [] { isTransient });
      return this;
    }

    /**
     * Stores registration state information within an event message.
     */
    public Builder setRegistrationState(RegistrationState state) {
      bundle.putInt(Parameter.REGISTRATION_STATE, state.ordinal());
      return this;
    }

    /**
     * Returns an event containing the set parameters.
     */
    @Override
    public Event build() {
      return new Event(bundle);
    }
  }

  /**
   * Constructs a new builder for an event associated with the provided action.
   */
  public static Builder newBuilder(Action action) {
    return new Builder(action);
  }

  /**
   * Constructs a new event using the contents of the provided parameter bundle.
   */
  public Event(Bundle bundle) {
    super(bundle);
  }

  /**
   * Returns the action from an event message.
   */
  public Action getAction() {
    return Action.values()[getActionOrdinal()];
  }

  /**
   * Returns the error information from an event message, or {@code null} if not present.
   */
  public ErrorInfo getErrorInfo() {
    ParcelableErrorInfo parcelableErrorInfo = parameters.getParcelable(Parameter.ERROR);
    return parcelableErrorInfo != null ? parcelableErrorInfo.errorInfo : null;
  }

  /**
   * Returns an invalidation from an event message, or {@code null} if not present.
   */
  public Invalidation getInvalidation() {
    ParcelableInvalidation parcelableInvalidation =
        parameters.getParcelable(Parameter.INVALIDATION);
    return parcelableInvalidation != null ? parcelableInvalidation.invalidation : null;
  }

  /**
   * Returns the isTransient flag from within an event, or {@code null} if not present.
   */
  public boolean getIsTransient() {
    boolean [] isTransient = parameters.getBooleanArray(Parameter.IS_TRANSIENT);
    return isTransient != null ? isTransient[0] : null;
  }

  /**
   * Returns the registration prefix from the event, or {@code null} if not present.
   */
  public byte [] getPrefix() {
    return parameters.getByteArray(Parameter.PREFIX);
  }

  /**
   * Returns the length of the registration prefix in bits or {@code -1} if not present.
   */
  public int getPrefixLength() {
    return parameters.getInt(Parameter.PREFIX_LENGTH, -1);
  }

  /**
   * Returns the registration state from an event message, or {@code null} if not present.
   */
  public RegistrationState getRegistrationState() {
    int ordinal = parameters.getInt(Parameter.REGISTRATION_STATE, -1);
    return ordinal < 0 ? null : RegistrationState.values()[ordinal];
  }
}
