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

import com.google.ipc.invalidation.common.CommonProtoStrings2;
import com.google.ipc.invalidation.common.CommonProtos2;
import com.google.ipc.invalidation.common.DigestFunction;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.ticl.Statistics.ClientErrorType;
import com.google.ipc.invalidation.ticl.TestableInvalidationClient.RegistrationManagerState;
import com.google.ipc.invalidation.util.InternalBase;
import com.google.ipc.invalidation.util.Marshallable;
import com.google.ipc.invalidation.util.TextBuilder;
import com.google.ipc.invalidation.util.TypedUtil;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationP;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationP.OpType;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationStatus;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSubtree;
import com.google.protos.ipc.invalidation.ClientProtocol.RegistrationSummary;
import com.google.protos.ipc.invalidation.JavaClient.RegistrationManagerStateP;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;


/**
 * Object to track desired client registrations. This class belongs to caller (e.g.,
 * InvalidationClientImpl) and is not thread-safe - the caller has to use this class in a
 * thread-safe manner.
 *
 */
class RegistrationManager extends InternalBase implements Marshallable<RegistrationManagerStateP> {

  /** Prefix used to request all registrations. */
  static final byte[] EMPTY_PREFIX = new byte[]{};

  /** The set of regisrations that the application has requested for. */
  private DigestStore<ObjectIdP> desiredRegistrations;

  /** Statistics objects to track number of sent messages, etc. */
  private final Statistics statistics;

  /** Latest known server registration state summary. */
  private ProtoWrapper<RegistrationSummary> lastKnownServerSummary;

  /**
   * Map of object ids and operation types for which we have not yet issued any registration-status
   * upcall to the listener. We need this so that we can synthesize success upcalls if registration
   * sync, rather than a server message, communicates to us that we have a successful
   * (un)registration.
   * <p>
   * This is a map from object id to type, rather than a set of {@code RegistrationP}, because
   * a set of {@code RegistrationP} would assume that we always get a response for every operation
   * we issue, which isn't necessarily true (i.e., the server might send back an unregistration
   * status in response to a registration request).
   */
  private final Map<ProtoWrapper<ObjectIdP>, RegistrationP.OpType> pendingOperations =
      new HashMap<ProtoWrapper<ObjectIdP>, RegistrationP.OpType>();

  private final Logger logger;

  public RegistrationManager(Logger logger, Statistics statistics, DigestFunction digestFn,
      RegistrationManagerStateP registrationManagerState) {
    this.logger = logger;
    this.statistics = statistics;
    this.desiredRegistrations = new SimpleRegistrationStore(digestFn);

    if (registrationManagerState == null) {
      // Initialize the server summary with a 0 size and the digest corresponding
      // to it.  Using defaultInstance would wrong since the server digest will
      // not match unnecessarily and result in an info message being sent.
      this.lastKnownServerSummary = ProtoWrapper.of(getRegistrationSummary());
    } else {
      this.lastKnownServerSummary =
        ProtoWrapper.of(registrationManagerState.getLastKnownServerSummary());
      desiredRegistrations.add(registrationManagerState.getRegistrationsList());
      for (RegistrationP regOp : registrationManagerState.getPendingOperationsList()) {
        pendingOperations.put(ProtoWrapper.of(regOp.getObjectId()), regOp.getOpType());
      }
    }
  }

  /**
   * Returns a copy of the registration manager's state
   * <p>
   * Direct test code MUST not call this method on a random thread. It must be called on the
   * InvalidationClientImpl's internal thread.
   */
  
  RegistrationManagerState getRegistrationManagerStateCopyForTest(DigestFunction digestFunction) {
    List<ObjectIdP> registeredObjects = new ArrayList<ObjectIdP>();
    for (ObjectIdP oid : desiredRegistrations.getElements(EMPTY_PREFIX, 0)) {
      registeredObjects.add(oid);
    }
    return new RegistrationManagerState(
        RegistrationSummary.newBuilder(getRegistrationSummary()).build(),
        RegistrationSummary.newBuilder(lastKnownServerSummary.getProto()).build(),
        registeredObjects);
  }

  /**
   * Sets the digest store to be {@code digestStore} for testing purposes.
   * <p>
   * REQUIRES: This method is called before the Ticl has done any operations on this object.
   */
  
  void setDigestStoreForTest(DigestStore<ObjectIdP> digestStore) {
    this.desiredRegistrations = digestStore;
    this.lastKnownServerSummary = ProtoWrapper.of(getRegistrationSummary());
  }

  
  Collection<ObjectIdP> getRegisteredObjectsForTest() {
    return desiredRegistrations.getElements(EMPTY_PREFIX, 0);
  }

  /** Perform registration/unregistation for all objects in {@code objectIds}. */
  Collection<ObjectIdP> performOperations(Collection<ObjectIdP> objectIds,
      RegistrationP.OpType regOpType) {
    // Record that we have pending operations on the objects.
    for (ObjectIdP objectId : objectIds) {
      pendingOperations.put(ProtoWrapper.of(objectId), regOpType);
    }
    // Update the digest appropriately.
    if (regOpType == RegistrationP.OpType.REGISTER) {
      return desiredRegistrations.add(objectIds);
    } else {
      return desiredRegistrations.remove(objectIds);
    }
  }

  /**
   * Returns a registration subtree for registrations where the digest of the object id begins with
   * the prefix {@code digestPrefix} of {@code prefixLen} bits. This method may also return objects
   * whose digest prefix does not match {@code digestPrefix}.
   */
  RegistrationSubtree getRegistrations(byte[] digestPrefix, int prefixLen) {
    RegistrationSubtree.Builder builder = RegistrationSubtree.newBuilder();
    for (ObjectIdP objectId : desiredRegistrations.getElements(digestPrefix, prefixLen)) {
      builder.addRegisteredObject(objectId);
    }
    return builder.build();
  }

  /**
   * Handles registration operation statuses from the server. Returns a list of booleans, one per
   * registration status, that indicates whether the registration operation was both successful and
   * agreed with the desired client state (i.e., for each registration status,
   * (status.optype == register) == desiredRegistrations.contains(status.objectid)).
   * <p>
   * REQUIRES: the caller subsequently make an informRegistrationStatus or informRegistrationFailure
   * upcall on the listener for each registration in {@code registrationStatuses}.
   */
  List<Boolean> handleRegistrationStatus(List<RegistrationStatus> registrationStatuses) {
    // Local-processing result code for each element of registrationStatuses.
    List<Boolean> localStatuses = new ArrayList<Boolean>(registrationStatuses.size());
    for (RegistrationStatus registrationStatus : registrationStatuses) {
      ObjectIdP objectIdProto = registrationStatus.getRegistration().getObjectId();

      // The object is no longer pending, since we have received a server status for it, so
      // remove it from the pendingOperations map. (It may or may not have existed in the map,
      // since we can receive spontaneous status messages from the server.)
      TypedUtil.remove(pendingOperations, ProtoWrapper.of(objectIdProto));

      // We start off with the local-processing set as success, then potentially fail.
      boolean isSuccess = true;

      // if the server operation succeeded, then local processing fails on "incompatibility" as
      // defined above.
      if (CommonProtos2.isSuccess(registrationStatus.getStatus())) {
        boolean appWantsRegistration = desiredRegistrations.contains(objectIdProto);
        boolean isOpRegistration =
            registrationStatus.getRegistration().getOpType() == RegistrationP.OpType.REGISTER;
        boolean discrepancyExists = isOpRegistration ^ appWantsRegistration;
        if (discrepancyExists) {
          // Remove the registration and set isSuccess to false, which will cause the caller to
          // issue registration-failure to the application.
          desiredRegistrations.remove(objectIdProto);
          statistics.recordError(ClientErrorType.REGISTRATION_DISCREPANCY);
          logger.info("Ticl discrepancy detected: registered = %s, requested = %s. " +
              "Removing %s from requested",
              isOpRegistration, appWantsRegistration,
              CommonProtoStrings2.toLazyCompactString(objectIdProto));
          isSuccess = false;
        }
      } else {
        // If the server operation failed, then also local processing fails.
        desiredRegistrations.remove(objectIdProto);
        logger.fine("Removing %s from committed",
            CommonProtoStrings2.toLazyCompactString(objectIdProto));
        isSuccess = false;
      }
      localStatuses.add(isSuccess);
    }
    return localStatuses;
  }

  /**
   * Removes all desired registrations and pending operations. Returns all object ids
   * that were affected.
   * <p>
   * REQUIRES: the caller issue a permanent failure upcall to the listener for all returned object
   * ids.
   */
  Collection<ProtoWrapper<ObjectIdP>> removeRegisteredObjects() {
    int numObjects = desiredRegistrations.size() + pendingOperations.size();
    Set<ProtoWrapper<ObjectIdP>> failureCalls = new HashSet<ProtoWrapper<ObjectIdP>>(numObjects);
    for (ObjectIdP objectId : desiredRegistrations.removeAll()) {
      failureCalls.add(ProtoWrapper.of(objectId));
    }
    failureCalls.addAll(pendingOperations.keySet());
    pendingOperations.clear();
    return failureCalls;
  }

  //
  // Digest-related methods
  //

  /** Returns a summary of the desired registrations. */
  RegistrationSummary getRegistrationSummary() {
    return CommonProtos2.newRegistrationSummary(desiredRegistrations.size(),
        desiredRegistrations.getDigest());
  }

  /**
   * Informs the manager of a new registration state summary from the server.
   * Returns a possibly-empty map of <object-id, reg-op-type>. For each entry in the map,
   * the caller should make an inform-registration-status upcall on the listener.
   */
  Set<ProtoWrapper<RegistrationP>> informServerRegistrationSummary(
      RegistrationSummary regSummary) {
    if (regSummary != null) {
      this.lastKnownServerSummary = ProtoWrapper.of(regSummary);
    }
    if (isStateInSyncWithServer()) {
      // If we are now in sync with the server, then the caller should make inform-reg-status
      // upcalls for all operations that we had pending, if any; they are also no longer pending.
      Set<ProtoWrapper<RegistrationP>> upcallsToMake =
          new HashSet<ProtoWrapper<RegistrationP>>(pendingOperations.size());
      for (Map.Entry<ProtoWrapper<ObjectIdP>, RegistrationP.OpType> entry :
          pendingOperations.entrySet()) {
        ObjectIdP objectId = entry.getKey().getProto();
        boolean isReg = entry.getValue() == OpType.REGISTER;
        upcallsToMake.add(ProtoWrapper.of(CommonProtos2.newRegistrationP(objectId, isReg)));
      }
      pendingOperations.clear();
      return upcallsToMake;
    } else {
      // If we are not in sync with the server, then the caller should make no upcalls.
      return Collections.emptySet();
    }
  }

  /**
   * Returns whether the local registration state and server state agree, based on the last
   * received server summary (from {@link #informServerRegistrationSummary}).
   */
  boolean isStateInSyncWithServer() {
    return TypedUtil.equals(lastKnownServerSummary, ProtoWrapper.of(getRegistrationSummary()));
  }

  @Override
  public void toCompactString(TextBuilder builder) {
    builder.appendFormat("Last known digest: %s, Requested regs: %s", lastKnownServerSummary,
        desiredRegistrations);
  }

  @Override
  public RegistrationManagerStateP marshal() {
    RegistrationManagerStateP.Builder builder = RegistrationManagerStateP.newBuilder();
    builder.setLastKnownServerSummary(lastKnownServerSummary.getProto());
    builder.addAllRegistrations(desiredRegistrations.getElements(EMPTY_PREFIX, 0));
    for (Map.Entry<ProtoWrapper<ObjectIdP>, RegistrationP.OpType> pendingOp :
        pendingOperations.entrySet()) {
      ObjectIdP objectId = pendingOp.getKey().getProto();
      boolean isReg = pendingOp.getValue() == OpType.REGISTER;
      builder.addPendingOperations(CommonProtos2.newRegistrationP(objectId, isReg));
    }
    return builder.build();
  }
}
