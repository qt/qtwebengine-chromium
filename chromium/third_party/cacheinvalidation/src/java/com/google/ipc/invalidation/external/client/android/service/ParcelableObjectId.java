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

import android.os.Parcel;
import android.os.Parcelable;

/**
 * Wraps an {link ObjectId} to enable writing to and reading from a
 * {@link Parcel}.
 */
class ParcelableObjectId implements Parcelable {
  final ObjectId objectId;

  /**
   * Creator required by the Parcelable implementation conventions.
   */
  public static final Parcelable.Creator<ParcelableObjectId> CREATOR =
      new Parcelable.Creator<ParcelableObjectId>() {
        @Override
        public ParcelableObjectId createFromParcel(Parcel in) {
          return new ParcelableObjectId(in);
        }

        @Override
        public ParcelableObjectId[] newArray(int size) {
          return new ParcelableObjectId[size];
        }
      };

  /**
   * Creates a new wrapper around the provided object id.
   */
  ParcelableObjectId(ObjectId objectId) {
    this.objectId = objectId;
  }

  /**
   * Creates a new wrapper and object id by reading data from a parcel.
   */
  private ParcelableObjectId(Parcel in) {
    int source = in.readInt();
    byte[] value = in.createByteArray();
    objectId = ObjectId.newInstance(source, value);
  }

  @Override
  public int describeContents() {
    return 0;  // no special contents
  }

  @Override
  public void writeToParcel(Parcel parcel, int flags) {

    // Data written to parcel is:
    // 1. numeric value of source type
    // 2. byte array for name
    parcel.writeInt(objectId.getSource());
    parcel.writeByteArray(objectId.getName());
  }

  @Override
  public boolean equals(Object object) {
    return object instanceof ParcelableObjectId
        && objectId.equals(((ParcelableObjectId) object).objectId);
  }

  @Override
  public int hashCode() {
    return objectId.hashCode();
  }
}
