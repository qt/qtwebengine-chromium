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

import android.os.Bundle;

/**
 * Creates and interprets response message bundles returned by the invalidation
 * service or application clients in response to request or event messages.
 *
 */
public final class Response extends Message {
  private static final AndroidLogger logger = AndroidLogger.forTag("Response");

  /** Contains the list of parameter names that are valid for response bundles. */
  public static class Parameter extends Message.Parameter {
    private Parameter() {} // not instantiable

    /**
     * An integer status code indicating success or failure.
     *
     * @see Status
     */
    public static final String STATUS = "status";
  }

  /** Defined values for the {@link Parameter#STATUS} parameter. */
  public static class Status {
    public static final int SUCCESS = 0;
    public static final int INVALID_CLIENT = 1;
    public static final int RUNTIME_ERROR = -1;
    public static final int UNKNOWN = -2;
  }

  /**
   * A builder class for constructing new response messages.
   *
   * @see #newBuilder
   */
  public static class Builder extends Message.Builder<Response, Builder> {

    // Instantiate using newBuilder()
    private Builder(int actionOrdinal, Bundle b) {
      super(actionOrdinal, b);
    }

    /**
     * Stores a status value within a response message.
     */
    public Builder setStatus(int status) {
      bundle.putInt(Parameter.STATUS, status);
      return this;
    }

    /**
     * Sets the status to {@link Status#RUNTIME_ERROR} and the error message to
     * the exception message within a response message.
     */
    public void setException(Exception exception) {
      if (exception instanceof AndroidClientException) {
        AndroidClientException ace = (AndroidClientException) exception;
        setStatus(ace.status);
        setError(ace.message);
      } else {
        setStatus(Status.RUNTIME_ERROR);
        setError(exception.getMessage());
      }
    }

    /**
     * Returns an response containing the set parameters.
     */
    @Override
    public Response build() {
      return new Response(bundle);
    }
  }

  /**
   * Constructs a new builder for a response associated with the provided action
   * that will store parameters into the provided bundle.
   */
  public static Builder newBuilder(int actionOrdinal, Bundle b) {
    return new Builder(actionOrdinal, b);
  }

  /**
   * Constructs a new response using the contents of the provided parameter bundle.
   */
  public Response(Bundle bundle) {
    super(bundle);
  }

  /**
   * Returns the status from a response message or {@link Status#UNKNOWN} if not
   * set by the callee (presumed failure).
   */
  public int getStatus() {
    return parameters.getInt(Parameter.STATUS, Status.UNKNOWN);
  }

  /**
   * Logs an error if the response message contains a status value other than
   * {@link Status#SUCCESS}.
   */
  public void warnOnFailure() {
    int status = getStatus();
    if (status != Status.SUCCESS) {
      String error = parameters.getString(Parameter.ERROR);
      if (error == null) {
        error = "Unexpected status value:" + status;
      }
      logger.warning("Error from AndroidInvalidationService: %s", error);
    }
  }
}
