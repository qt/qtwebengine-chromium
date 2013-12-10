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

package com.google.ipc.invalidation.common;

import com.google.protobuf.ByteString;
import com.google.protos.ipc.invalidation.ClientProtocol.ObjectIdP;
import com.google.protos.ipc.invalidation.ClientProtocol.ProtocolVersion;
import com.google.protos.ipc.invalidation.ClientProtocol.Version;
import com.google.protos.ipc.invalidation.Types.ClientType;
import com.google.protos.ipc.invalidation.Types.ObjectSource;

/**
 * Various constants common to  clients and servers used in version 2 of the Ticl.
 *
 */
public class CommonInvalidationConstants2 {

  /** Major version of the client library. */
  public static final int CLIENT_MAJOR_VERSION = 3;

  /**
   * Minor version of the client library, defined to be equal to the datestamp of the build
   * (e.g. 20130401).
   */
  public static final int CLIENT_MINOR_VERSION = BuildConstants.BUILD_DATESTAMP;

  /** Major version of the protocol between the client and the server. */
  public static final int PROTOCOL_MAJOR_VERSION = 3;

  /** Minor version of the protocol between the client and the server. */
  public static final int PROTOCOL_MINOR_VERSION = 2;

  /** Major version of the client config. */
  public static final int CONFIG_MAJOR_VERSION = 3;

  /** Minor version of the client config. */
  public static final int CONFIG_MINOR_VERSION = 2;

  /** Version of the protocol currently being used by the client/server for v2 clients. */
  public static final ProtocolVersion PROTOCOL_VERSION = ProtocolVersion.newBuilder()
      .setVersion(CommonProtos2.newVersion(PROTOCOL_MAJOR_VERSION, PROTOCOL_MINOR_VERSION)).build();

  /** Version of the protocol currently being used by the client/server for v1 clients. */
  public static final ProtocolVersion PROTOCOL_VERSION_V1 = ProtocolVersion.newBuilder()
      .setVersion(CommonProtos2.newVersion(2, 0)).build();

  /** Version of the client currently being used by the client. */
  public static final Version CLIENT_VERSION_VALUE =
      CommonProtos2.newVersion(CLIENT_MAJOR_VERSION, CLIENT_MINOR_VERSION);

  /** The value of ObjectSource.Type from types.proto. Must be kept in sync with that file. */
  public static final int INTERNAL_OBJECT_SOURCE_TYPE = ObjectSource.Type.INTERNAL.getNumber();

  /** The value of ObjectSource.Type from types.proto. Must be kept in sync with that file. */
  public static final int TEST_OBJECT_SOURCE_TYPE = ObjectSource.Type.TEST.getNumber();

  /** The value of ClientType.Type from types.proto. Must be kept in sync with that file. */
  public static final int INTERNAL_CLIENT_TYPE = ClientType.Type.INTERNAL.getNumber();

  /** The value of ClientType.Type from types.proto. Must be kept in sync with that file. */
  public static final int TEST_CLIENT_TYPE = ClientType.Type.TEST.getNumber();

  /** Object id used to trigger a refresh of all cached objects ("invalidate-all"). */
  public static final ObjectIdP ALL_OBJECT_ID = ObjectIdP.newBuilder()
      .setName(ByteString.EMPTY)
      .setSource(INTERNAL_OBJECT_SOURCE_TYPE)
      .build();
}
