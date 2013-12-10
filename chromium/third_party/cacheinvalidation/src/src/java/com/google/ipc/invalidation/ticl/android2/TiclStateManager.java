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

package com.google.ipc.invalidation.ticl.android2;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.common.ObjectIdDigestUtils.Sha1DigestFunction;
import com.google.ipc.invalidation.external.client.SystemResources;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.protobuf.ByteString;
import com.google.protobuf.InvalidProtocolBufferException;
import com.google.protos.ipc.invalidation.AndroidService.AndroidTiclState;
import com.google.protos.ipc.invalidation.AndroidService.AndroidTiclState.Metadata;
import com.google.protos.ipc.invalidation.AndroidService.AndroidTiclStateWithDigest;
import com.google.protos.ipc.invalidation.ClientProtocol.ApplicationClientIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.ClientConfigP;
import com.google.protos.ipc.invalidation.JavaClient.InvalidationClientState;

import android.content.Context;

import java.io.DataInput;
import java.io.DataInputStream;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.Random;


/**
 * Class to save and restore instances of {@code InvalidationClient} to and from stable storage.
 *
 */

public class TiclStateManager {
  /** Name of the file to which Ticl state will be persisted. */
  private static final String TICL_STATE_FILENAME = "android_ticl_service_state.bin";

  /**
   * Maximum size of a Ticl state file. Files with larger size will be ignored as invalid. We use
   * this because we allocate an array of bytes of the same size as the Ticl file and want to
   * avoid accidentally allocating huge arrays.
   */
  private static final int MAX_TICL_FILE_SIZE_BYTES = 100 * 1024; // 100 kilobytes

  /** Random number generator for created Ticls. */
  private static final Random random = new Random();

  /**
   * Restores the Ticl from persistent storage if it exists. Otherwise, returns {@code null}.
   * @param context Android system context
   * @param resources resources to use for the Ticl
   */
  static AndroidInvalidationClientImpl restoreTicl(Context context,
      SystemResources resources) {
    AndroidTiclState state = readTiclState(context, resources.getLogger());
    if (state == null) {
      return null;
    }
    AndroidInvalidationClientImpl ticl = new AndroidInvalidationClientImpl(context, resources,
        random, state);
    setSchedulerId(resources, ticl);
    return ticl;
  }

  /** Creates a new Ticl. Persistent stroage must not exist. */
  static void createTicl(Context context, SystemResources resources, int clientType,
      byte[] clientName, ClientConfigP config, boolean skipStartForTest) {
    Preconditions.checkState(!doesStateFileExist(context), "Ticl already exists");
    AndroidInvalidationClientImpl ticl = new AndroidInvalidationClientImpl(context, resources,
        random, clientType, clientName, config);
    if (!skipStartForTest) {
      // Ticls are started when created unless this should be skipped for tests; we allow tests
      // to skip starting Ticls because many integration tests assume that Ticls will not be
      // started when created.
      setSchedulerId(resources, ticl);
      ticl.start();
    }
    saveTicl(context, resources.getLogger(), ticl);
  }

  /**
   * Sets the scheduling id on the scheduler in {@code resources} to {@code ticl.getSchedulingId()}.
   */
  private static void setSchedulerId(SystemResources resources,
      AndroidInvalidationClientImpl ticl) {
    AndroidInternalScheduler scheduler =
        (AndroidInternalScheduler) resources.getInternalScheduler();
    scheduler.setTiclId(ticl.getSchedulingId());
  }

  /**
   * Saves a Ticl instance to persistent storage.
   *
   * @param context Android system context
   * @param logger logger
   * @param ticl the Ticl instance to save
   */
  static void saveTicl(Context context, Logger logger, AndroidInvalidationClientImpl ticl) {
    FileOutputStream outputStream = null;
    try {

      // Create a protobuf with the Ticl state and a digest over it.
      AndroidTiclStateWithDigest digestedState = createDigestedState(ticl);
      AndroidIntentProtocolValidator validator = new AndroidIntentProtocolValidator(logger);
      Preconditions.checkState(validator.isTiclStateValid(digestedState),
          "Produced invalid digested state: %s", digestedState);

      // Write the protobuf to storage.
      outputStream = openStateFileForWriting(context);
      outputStream.write(digestedState.toByteArray());
      outputStream.close();
    } catch (FileNotFoundException exception) {
      logger.warning("Could not write Ticl state: %s", exception);
    } catch (IOException exception) {
      logger.warning("Could not write Ticl state: %s", exception);
    } finally {
      try {
        if (outputStream != null) {
          outputStream.close();
        }
      } catch (IOException exception) {
        logger.warning("Exception closing Ticl state file: %s", exception);
      }
    }
  }

  /**
   * Reads and returns the Android Ticl state from persistent storage. If the state was missing
   * or invalid, returns {@code null}.
   */
  
  static AndroidTiclState readTiclState(Context context, Logger logger) {
    FileInputStream inputStream = null;
    try {
      inputStream = openStateFileForReading(context);
      DataInput input = new DataInputStream(inputStream);
      long fileSizeBytes = inputStream.getChannel().size();
      if (fileSizeBytes > MAX_TICL_FILE_SIZE_BYTES) {
        logger.warning("Ignoring too-large Ticl state file with size %s > %s",
            fileSizeBytes, MAX_TICL_FILE_SIZE_BYTES);
      } else {
        // Cast to int must be safe due to the above size check.
        byte[] fileData = new byte[(int) fileSizeBytes];
        input.readFully(fileData);
        AndroidTiclStateWithDigest androidState = AndroidTiclStateWithDigest.parseFrom(fileData);
        AndroidIntentProtocolValidator validator = new AndroidIntentProtocolValidator(logger);

        // Check the structure of the message (required fields set).
        if (!validator.isTiclStateValid(androidState)) {
          logger.warning("Read AndroidTiclStateWithDigest with invalid structure: %s",
              androidState);
          return null;
        }
        // Validate the digest in the method.
        if (isDigestValid(androidState, logger)) {
          InvalidationClientState state = androidState.getState().getTiclState();
          return androidState.getState();
        } else {
          logger.warning("Android Ticl state failed digest check: %s", androidState);
        }
      }
    } catch (FileNotFoundException exception) {
      logger.info("Ticl state file does not exist: %s", TICL_STATE_FILENAME);
    } catch (InvalidProtocolBufferException exception) {
      logger.warning("Could not read Ticl state: %s", exception);
    } catch (IOException exception) {
      logger.warning("Could not read Ticl state: %s", exception);
    } finally {
      try {
        if (inputStream != null) {
          inputStream.close();
        }
      } catch (IOException exception) {
        logger.warning("Exception closing Ticl state file: %s", exception);
      }
    }
    return null;
  }

  /**
   * Returns a {@link AndroidTiclStateWithDigest} containing {@code ticlState} and its computed
   * digest.
   */
  private static AndroidTiclStateWithDigest createDigestedState(
      AndroidInvalidationClientImpl ticl) {
    Sha1DigestFunction digester = new Sha1DigestFunction();
    ApplicationClientIdP ticlAppId = ticl.getApplicationClientIdP();
    Metadata metaData = Metadata.newBuilder()
         .setClientConfig(ticl.getConfig())
         .setClientName(ticlAppId.getClientName())
         .setClientType(ticlAppId.getClientType())
         .setTiclId(ticl.getSchedulingId())
         .build();
    AndroidTiclState state = AndroidTiclState.newBuilder()
        .setMetadata(metaData)
        .setTiclState(ticl.marshal())
        .setVersion(ProtocolIntents.ANDROID_PROTOCOL_VERSION_VALUE).build();
    digester.update(state.toByteArray());
    AndroidTiclStateWithDigest verifiedState = AndroidTiclStateWithDigest.newBuilder()
        .setState(state)
        .setDigest(ByteString.copyFrom(digester.getDigest()))
        .build();
    return verifiedState;
  }

  /** Returns whether the digest in {@code state} is correct. */
  private static boolean isDigestValid(AndroidTiclStateWithDigest state, Logger logger) {
    Sha1DigestFunction digester = new Sha1DigestFunction();
    digester.update(state.getState().toByteArray());
    ByteString computedDigest = ByteString.copyFrom(digester.getDigest());
    if (!computedDigest.equals(state.getDigest())) {
      logger.warning("Android Ticl state digest mismatch; computed %s for %s",
          computedDigest, state);
      return false;
    }
    return true;
  }

  /** Opens {@link #TICL_STATE_FILENAME} for writing. */
  private static FileOutputStream openStateFileForWriting(Context context)
      throws FileNotFoundException {
    return context.openFileOutput(TICL_STATE_FILENAME, Context.MODE_PRIVATE);
  }

  /** Opens {@link #TICL_STATE_FILENAME} for reading. */
  private static FileInputStream openStateFileForReading(Context context)
      throws FileNotFoundException {
    return context.openFileInput(TICL_STATE_FILENAME);
  }

  /** Deletes {@link #TICL_STATE_FILENAME}. */
  
  public static void deleteStateFile(Context context) {
    context.deleteFile(TICL_STATE_FILENAME);
  }

  /** Returns whether the state file exists, for tests. */
  static boolean doesStateFileExistForTest(Context context) {
    return doesStateFileExist(context);
  }

  /** Returns whether the state file exists. */
  private static boolean doesStateFileExist(Context context) {
    return context.getFileStreamPath(TICL_STATE_FILENAME).exists();
  }

  private TiclStateManager() {
    // Disallow instantiation.
  }
}
