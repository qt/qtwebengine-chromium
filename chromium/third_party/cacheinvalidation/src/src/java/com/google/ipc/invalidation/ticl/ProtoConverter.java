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
import com.google.ipc.invalidation.common.CommonProtos2;
import com.google.ipc.invalidation.common.TrickleState;
import com.google.ipc.invalidation.external.client.types.Invalidation;
import com.google.ipc.invalidation.external.client.types.ObjectId;
import com.google.protobuf.ByteString;
import com.google.protos.ipc.invalidation.ClientProtocol.InvalidationP;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;

/**
 * Utilities to convert between protobufs and externally-exposed types in the Ticl.
 *
 */

public class ProtoConverter {

  /**
   * Converts an object id protocol buffer {@code objectId} to the
   * corresponding external type and returns it.
   */
  public static ObjectId convertFromObjectIdProto(ObjectIdP objectIdProto) {
    Preconditions.checkNotNull(objectIdProto);
    return ObjectId.newInstance(objectIdProto.getSource(), objectIdProto.getName().toByteArray());
  }

  /**
   * Converts an object id {@code objectId} to the corresponding protocol buffer
   * and returns it.
   */
  
  public static ObjectIdP convertToObjectIdProto(ObjectId objectId) {
    Preconditions.checkNotNull(objectId);
    return CommonProtos2.newObjectIdP(objectId.getSource(),
        ByteString.copyFrom(objectId.getName()));
  }

  /**
   * Returns a list of {@link ObjectIdP} by converting each element of {@code objectIds} to
   * an {@code ObjectIdP}.
   */
  public static List<ObjectIdP> convertToObjectIdProtoList(Collection<ObjectId> objectIds) {
    List<ObjectIdP> objectIdPs = new ArrayList<ObjectIdP>(objectIds.size());
    for (ObjectId objectId : objectIds) {
      objectIdPs.add(ProtoConverter.convertToObjectIdProto(objectId));
    }
    return objectIdPs;
  }

  /**
   * Returns a list of {@link ObjectId} by converting each element of {@code oidPs} to
   * an {@code ObjectId}.
   */
  public static List<ObjectId> convertToObjectIdList(List<ObjectIdP> oidPs) {
    List<ObjectId> objects = new ArrayList<ObjectId>(oidPs.size());
    for (ObjectIdP oidP : oidPs) {
      objects.add(ObjectId.newInstance(oidP.getSource(), oidP.getName().toByteArray()));
    }
    return objects;
  }

  /**
   * Converts an invalidation protocol buffer {@code invalidation} to the
   * corresponding external object and returns it
   */
  public static Invalidation convertFromInvalidationProto(InvalidationP invalidation) {
    Preconditions.checkNotNull(invalidation);
    ObjectId objectId = convertFromObjectIdProto(invalidation.getObjectId());

    // No bridge arrival time in invalidation.
    return Invalidation.newInstance(objectId, invalidation.getVersion(),
        invalidation.hasPayload() ? invalidation.getPayload().toByteArray() : null,
            invalidation.getIsTrickleRestart());
  }

  /**
   * Converts an invalidation {@code invalidation} to the corresponding protocol
   * buffer and returns it.
   */
  public static InvalidationP convertToInvalidationProto(Invalidation invalidation) {
    Preconditions.checkNotNull(invalidation);
    ObjectIdP objectId = convertToObjectIdProto(invalidation.getObjectId());

    // Invalidations clients do not know about trickle restarts. Every invalidation is allowed
    // to suppress earlier invalidations and acks implicitly acknowledge all previous
    // invalidations. Therefore the correct semanantics are provided by setting isTrickleRestart to
    // true.
    return CommonProtos2.newInvalidationP(objectId, invalidation.getVersion(),
        TrickleState.fromBoolean(invalidation.getIsTrickleRestartForInternalUse()),
        invalidation.getPayload() == null ? null : ByteString.copyFrom(invalidation.getPayload()));
  }

  private ProtoConverter() { // To prevent instantiation.
  }
}
