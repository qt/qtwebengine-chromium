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

package com.google.ipc.invalidation.ticl;

import com.google.common.base.Preconditions;
import com.google.protobuf.AbstractMessageLite;

import java.util.Arrays;

/**
 * Wraps a Lite protobuf type and provides appropriate {@link #equals} and {@link #hashCode}
 * implementations so the wrapped object can be stored in a Java collection and/or compared to other
 * wrapped proto instances of the same type for equality. This is necessary because protobuf classes
 * generated with the {@code LITE_RUNTIME} optimization do not have custom implementations of these
 * methods (so only support simple object equivalence).
 *
 * @param <P> the protobuf message lite type that is being wrapped.
 */
public class ProtoWrapper<P extends AbstractMessageLite> {

  /** The wrapped proto object */
  private final P proto;

  /** The serialized byte representation of the wrapped object */
  private final byte [] protoBytes;

  /** The hash code of the serialized representation */
  private final int hashCode;

  /** Returns a ProtoWrapper that wraps the provided object */
  public static <M extends AbstractMessageLite> ProtoWrapper<M> of(M proto) {
    return new ProtoWrapper<M>(proto);
  }

  // Internal constructor that savees the object and computes serialized state and hash code.
  private ProtoWrapper(P proto) {
    this.proto = Preconditions.checkNotNull(proto);
    this.protoBytes = proto.toByteArray();
    this.hashCode = Arrays.hashCode(protoBytes);
  }

  /** Returns the wrapped proto object */
  public P getProto() {
    return proto;
  }

  /** Returns the hash code of the serialized state representation of the protobuf object */
  @Override
  public int hashCode() {
    return hashCode;
  }

  /**
   * Returns {@code true} if the provided object is a proto wrapper of an object of the same
   * protobuf type that has the same serialized state representation.
   */
  @Override
  public boolean equals(Object o) {
    Class<?> msgClass = proto.getClass();
    if (!(o instanceof ProtoWrapper)) {
      return false;
    }
    @SuppressWarnings("rawtypes")
    ProtoWrapper<?> wrapper = (ProtoWrapper) o;
    if (proto.getClass() != wrapper.proto.getClass()) {
      return false;
    }
    if (hashCode != wrapper.hashCode) {
      return false;
    }
    return Arrays.equals(protoBytes, wrapper.protoBytes);
  }

  @Override
  public String toString() {
    // Don't print exactly the protocol buffer because that could be extremely confusing when
    // debugging, since this object isn't actually a protocol buffer.
    return "PW-" + proto.toString();
  }
}
