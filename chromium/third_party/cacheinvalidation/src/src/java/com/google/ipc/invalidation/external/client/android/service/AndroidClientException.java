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

/**
 * Represents an invalidation runtime exception that will be raised to the client.
 *
 */
public class AndroidClientException extends RuntimeException {

  final int status;
  final String message;

  /**
   * Creates a new client exception with the provided status value and message
   * @param status the integer status value that describes the error.
   * @param message an error message string.
   *
   * @see Response.Status
   */
  public AndroidClientException(int status, String message) {
    super(message);
    this.status = status;
    this.message = message;
  }
}
