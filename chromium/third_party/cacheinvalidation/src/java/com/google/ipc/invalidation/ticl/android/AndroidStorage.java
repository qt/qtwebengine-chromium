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

package com.google.ipc.invalidation.ticl.android;

import com.google.common.base.Preconditions;
import com.google.ipc.invalidation.external.client.SystemResources;
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.ipc.invalidation.external.client.SystemResources.Storage;
import com.google.ipc.invalidation.external.client.android.service.AndroidLogger;
import com.google.ipc.invalidation.external.client.types.Callback;
import com.google.ipc.invalidation.external.client.types.SimplePair;
import com.google.ipc.invalidation.external.client.types.Status;
import com.google.ipc.invalidation.util.NamedRunnable;
import com.google.protobuf.ByteString;
import com.google.protos.ipc.invalidation.AndroidState.ClientMetadata;
import com.google.protos.ipc.invalidation.AndroidState.ClientProperty;
import com.google.protos.ipc.invalidation.AndroidState.StoredState;
import com.google.protos.ipc.invalidation.ClientProtocol.Version;

import android.accounts.Account;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Map;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;

/**
 * Provides the storage and in-memory state model for Android client persistent state.   There is
 * one storage instance for each client instance that is responsible for loading, making state
 * available, and storing the persisted state.
 * <b>
 * The class is thread safe <b>after</b> the {@link #create} or {@link #load} method has been
 * called to populate it with initial state.
 *
 */
public class AndroidStorage implements Storage {

  /*
   * The current storage format is based upon a single file containing protocol buffer data.  Each
   * client instance will have a separate state file with a name based upon a client-key derived
   * convention.   The design could easily be evolved later to leverage a shared SQLite database
   * or other mechanisms without requiring any changes to the public interface.
   */

  /** Storage logger */
  private static final Logger logger = AndroidLogger.forTag("InvStorage");

  /** The version value that is stored within written state */
  private static final Version CURRENT_VERSION =
      Version.newBuilder().setMajorVersion(1).setMinorVersion(0).build();

  /** The name of the subdirectory in the application files store where state files are stored */
   static final String STATE_DIRECTORY = "InvalidationClient";

  /** A simple success constant */
  private static final Status SUCCESS = Status.newInstance(Status.Code.SUCCESS, "");

  /**
   * Deletes all persisted client state files stored in the state directory and then
   * the directory itself.
   */
   public static void reset(Context context) {
    File stateDir = context.getDir(STATE_DIRECTORY, Context.MODE_PRIVATE);
    for (File stateFile : stateDir.listFiles()) {
      stateFile.delete();
    }
    stateDir.delete();
  }

  /** The execution context */
   final Context context;

  /** The client key associated with this storage instance */
   final String key;

  /** the client metadata associated with the storage instance (or {@code null} if not loaded */
  private ClientMetadata metadata;

  /** Stores the client properties for a client */
  private final Map<String, byte []> properties = new ConcurrentHashMap<String, byte[]>();

  /** Executor used to schedule background reads and writes on a single shared thread */
  
  final ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();

  /**
   * Creates a new storage object for reading or writing state for the providing client key using
   * the provided execution context.
   */
  
  protected AndroidStorage(Context context, String key) {
    Preconditions.checkNotNull(context, "context");
    Preconditions.checkNotNull(key, "key");
    this.key = key;
    this.context = context;
  }

  ClientMetadata getClientMetadata() {
    return metadata;
  }

  @Override
  public void deleteKey(final String key, final Callback<Boolean> done) {
    scheduler.execute(new NamedRunnable("AndroidStorage.deleteKey") {
      @Override
      public void run() {
        properties.remove(key);
        store();
        done.accept(true);
      }
    });
  }

  @Override
  public void readAllKeys(final Callback<SimplePair<Status, String>> keyCallback) {
    scheduler.execute(new NamedRunnable("AndroidStorage.readAllKeys") {
      @Override
      public void run() {
        for (String key : properties.keySet()) {
          keyCallback.accept(SimplePair.of(SUCCESS, key));
        }
      }
    });
  }

  @Override
  public void readKey(final String key, final Callback<SimplePair<Status, byte[]>> done) {
    scheduler.execute(new NamedRunnable("AndroidStorage.readKey") {
      @Override
      public void run() {
          byte [] value = properties.get(key);
          if (value != null) {
            done.accept(SimplePair.of(SUCCESS, value));
          } else {
            Status status =
                Status.newInstance(Status.Code.PERMANENT_FAILURE, "No value in map for " + key);
            done.accept(SimplePair.of(status, (byte []) null));
          }
      }
    });
  }

  @Override
  public void writeKey(final String key, final byte[] value, final Callback<Status> done) {
    scheduler.execute(new NamedRunnable("AndroidStorage.writeKey") {
      @Override
      public void run() {
        properties.put(key, value);
        store();
        done.accept(SUCCESS);
      }
    });
  }

  @Override
  public void setSystemResources(SystemResources resources) {}

  /**
   * Returns the file where client state for this storage instance is stored.
   */
   File getStateFile() {
    File stateDir = context.getDir(STATE_DIRECTORY, Context.MODE_PRIVATE);
    return new File(stateDir, key);
  }

  /**
   * Returns the input stream that can be used to read state from the internal file storage for
   * the application.
   */
  
  protected InputStream getStateInputStream() throws FileNotFoundException {
    return new FileInputStream(getStateFile());
  }

  /**
   * Returns the output stream that can be used to write state to the internal file storage for
   * the application.
   */
  
  protected OutputStream getStateOutputStream() throws FileNotFoundException {
    return new FileOutputStream(getStateFile());
  }

  void create(int clientType, Account account, String authType,
      Intent eventIntent) {
    ComponentName component = eventIntent.getComponent();
    Preconditions.checkNotNull(component, "No component found in event intent");
    metadata = ClientMetadata.newBuilder()
        .setVersion(CURRENT_VERSION)
        .setClientKey(key)
        .setClientType(clientType)
        .setAccountName(account.name)
        .setAccountType(account.type)
        .setAuthType(authType)
        .setListenerPkg(component.getPackageName())
        .setListenerClass(component.getClassName())
        .build();
    store();
  }

  /**
   * Attempts to load any persisted client state for the stored client.
   *
   * @returns {@code true} if loaded successfully, false otherwise.
   */
  boolean load() {
    InputStream inputStream = null;
    try {
      // Load the state from internal storage and parse it the protocol
      inputStream = getStateInputStream();
      StoredState fullState = StoredState.parseFrom(inputStream);
      metadata = fullState.getMetadata();
      if (!key.equals(metadata.getClientKey())) {
        logger.severe("Unexpected client key mismatch: %s, %s", key, metadata.getClientKey());
        return false;
      }
      logger.fine("Loaded metadata: %s", metadata);

      // Unpack the client properties into a map for easy lookup / iteration / update
      for (ClientProperty clientProperty : fullState.getPropertyList()) {
        logger.fine("Loaded property: %s", clientProperty);
        properties.put(clientProperty.getKey(), clientProperty.getValue().toByteArray());
      }
      logger.fine("Loaded state for %s", key);
      return true;
    } catch (FileNotFoundException e) {
      // No state persisted on disk
    } catch (IOException exception) {
      // Log error regarding client state read and return null
      logger.severe("Error reading client state", exception);
    } finally {
      if (inputStream != null) {
        try {
          inputStream.close();
        } catch (IOException exception) {
          logger.severe("Unable to close state file", exception);
        }
      }
    }
    return false;
  }

  /**
   * Deletes all state associated with the storage instance.
   */
  void delete() {
    File stateFile = getStateFile();
    if (stateFile.exists()) {
      stateFile.delete();
      logger.info("Deleted state for %s from %s", key, stateFile.getName());
    }
  }

  /**
   * Store the current state into the persistent storage.
   */
  private void store() {
    StoredState.Builder stateBuilder =
        StoredState.newBuilder()
            .mergeMetadata(metadata);
    for (Map.Entry<String, byte []> entry : properties.entrySet()) {
      stateBuilder.addProperty(
          ClientProperty.newBuilder()
              .setKey(entry.getKey())
              .setValue(ByteString.copyFrom(entry.getValue()))
              .build());
    }
    StoredState state = stateBuilder.build();
    OutputStream outputStream = null;
    try {
      outputStream = getStateOutputStream();
      state.writeTo(outputStream);
      logger.info("State written for %s", key);
    } catch (FileNotFoundException exception) {
      // This should not happen when opening to create / replace
      logger.severe("Unable to open state file", exception);
    } catch (IOException exception) {
      logger.severe("Error writing state", exception);
    } finally {
      if (outputStream != null) {
        try {
          outputStream.close();
        } catch (IOException exception) {
          logger.warning("Unable to close state file", exception);
        }
      }
    }
  }

  /**
   * Returns the underlying properties map for direct manipulation. This is extremely
   * unsafe since it bypasses the concurrency control. It is intended only for use
   * in {@code AndroidInvalidationService#handleC2dmMessageForUnstartedClient}.
   */
  Map<String, byte[]> getPropertiesUnsafe() {
    return properties;
  }

  /**
   * Stores the properties to disk. This is extremely unsafe since it bypasses the
   * concurrency control. It is intended only for use in
   * {@code AndroidInvalidationService#handleC2dmMessageForUnstartedClient}.
   */
  void storeUnsafe() {
    store();
  }
}
