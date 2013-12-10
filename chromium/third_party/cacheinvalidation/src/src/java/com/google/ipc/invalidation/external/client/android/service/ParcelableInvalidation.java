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

import com.google.ipc.invalidation.external.client.types.Invalidation;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * Wraps an {link Invalidation} to enable writing to and reading from a
 * {@link Parcel}.
 */
class ParcelableInvalidation implements Parcelable {
  final Invalidation invalidation;
  final boolean includePayload;

  /**
   * Creator required by the Parcelable implementation conventions.
   */
  public static final Parcelable.Creator<ParcelableInvalidation> CREATOR =
      new Parcelable.Creator<ParcelableInvalidation>() {
        @Override
        public ParcelableInvalidation createFromParcel(Parcel in) {
          return new ParcelableInvalidation(in);
        }

        @Override
        public ParcelableInvalidation[] newArray(int size) {
          return new ParcelableInvalidation[size];
        }
      };

  /**
   * Creates a new wrapper around the provided invalidation
   */
  ParcelableInvalidation(Invalidation invalidation, boolean includePayload) {
    this.invalidation = invalidation;
    this.includePayload = includePayload;
  }

  /**
   * Creates a new invalidation wrapper by reading data from a parcel.
   */
  public ParcelableInvalidation(Parcel in) {
    // Read parcelable object id from parcel using the application class loader
    ParcelableObjectId objectId = in.readParcelable(getClass().getClassLoader());
    long version = in.readLong();
    boolean isTrickleRestart = in.createBooleanArray()[0];
    boolean[] values = in.createBooleanArray();
    byte[] payload = null;
    if (values[0]) { // hasPayload
      payload = in.createByteArray();
    }
    this.invalidation = Invalidation.newInstance(objectId.objectId, version, payload,
        isTrickleRestart);
    this.includePayload = payload != null;
  }

  @Override
  public int describeContents() {
    return 0;  // no special contents
  }

  @Override
  public void writeToParcel(Parcel parcel, int flags) {

    // Data written to parcel is:
    // 1. object id (as ParcelableObjectId)
    // 2. long version
    // 3. boolean [] { isTrickleRestart }
    // 4. boolean [] { hasPayload }
    // 5. byte array for payload (if hasPayload)
    parcel.writeParcelable(new ParcelableObjectId(invalidation.getObjectId()), 0);
    parcel.writeLong(invalidation.getVersion());
    parcel.writeBooleanArray(new boolean[] {invalidation.getIsTrickleRestartForInternalUse()});
    byte[] payload = invalidation.getPayload();
    if (includePayload && payload != null) {
      parcel.writeBooleanArray(new boolean[] {true});
      parcel.writeByteArray(payload);
    } else {
      parcel.writeBooleanArray(new boolean[] {false});
    }
  }

  @Override
  public boolean equals(Object object) {
    return object instanceof ParcelableInvalidation
        && invalidation.equals(((ParcelableInvalidation) object).invalidation);
  }

  @Override
  public int hashCode() {
    return invalidation.hashCode();
  }
}
