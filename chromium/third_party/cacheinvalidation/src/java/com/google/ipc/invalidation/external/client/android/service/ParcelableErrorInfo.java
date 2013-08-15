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

import com.google.ipc.invalidation.external.client.types.ErrorInfo;

import android.os.Parcel;
import android.os.Parcelable;

/**
 * Wraps an {link ErrorInfo} to enable writing to and reading from a
 * {@link Parcel}.
 */
class ParcelableErrorInfo implements Parcelable {
  final ErrorInfo errorInfo;

  /**
   * Creator required by the Parcelable implementation conventions.
   */
  public static final Parcelable.Creator<ParcelableErrorInfo> CREATOR =
      new Parcelable.Creator<ParcelableErrorInfo>() {
        @Override
        public ParcelableErrorInfo createFromParcel(Parcel in) {
          return new ParcelableErrorInfo(in);
        }

        @Override
        public ParcelableErrorInfo[] newArray(int size) {
          return new ParcelableErrorInfo[size];
        }
      };

  /**
   * Creates a new wrapper around the provided error info.
   */
  ParcelableErrorInfo(ErrorInfo errorInfo) {
    this.errorInfo = errorInfo;
  }

  /**
   * Creates a new ErrorInfo wrapper by reading data from a parcel.
   */
  public ParcelableErrorInfo(Parcel in) {
    int reason = in.readInt();
    boolean isTransient = in.createBooleanArray()[0];
    String message = in.readString();
    this.errorInfo = ErrorInfo.newInstance(reason, isTransient, message, null);
  }

  @Override
  public int describeContents() {
    return 0;  // no special contents
  }

  @Override
  public void writeToParcel(Parcel parcel, int flags) {

    // Data written to parcel is:
    // 1. int errorReason
    // 2. boolean [] { isTransient }
    // 3. String error message
    // TODO: Add support for object marshaling when needed
    parcel.writeInt(errorInfo.getErrorReason());
    parcel.writeBooleanArray(new boolean[]{errorInfo.isTransient()});
    parcel.writeString(errorInfo.getErrorMessage());
  }

  @Override
  public boolean equals(Object object) {
    return object instanceof ParcelableErrorInfo
        && errorInfo.equals(((ParcelableErrorInfo) object).errorInfo);
  }

  @Override
  public int hashCode() {
    return errorInfo.hashCode();
  }
}
