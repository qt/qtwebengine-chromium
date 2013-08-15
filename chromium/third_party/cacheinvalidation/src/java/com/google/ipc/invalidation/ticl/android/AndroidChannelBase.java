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
import com.google.ipc.invalidation.external.client.SystemResources.Logger;
import com.google.protos.ipc.invalidation.AndroidChannel.AddressedAndroidMessage;

import org.apache.http.client.ClientProtocolException;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpPost;
import org.apache.http.entity.ByteArrayEntity;
import org.apache.http.impl.client.BasicResponseHandler;

import java.io.IOException;


/**
 * Implementation of the HTTP communication used by {@code AndroidChannel}. Factored into
 * a separate class that can be run outside the Android environment to improve testing.
 *
 */

public abstract class AndroidChannelBase {
  /** Http client to use when making requests to . */
  HttpClient httpClient;

  /** Authentication type for  frontends. */
  private final String authType;

  /** URL of the frontends. */
  private final String channelUrl;

  /** The token that will be echoed to the data center in the headers of all HTTP requests. */
  private String echoToken = null;

  /**
   * Creates an instance that uses {@code httpClient} to send requests to {@code channelUrl}
   * using an auth type of {@code authType}.
   */
  protected AndroidChannelBase(HttpClient httpClient, String authType, String channelUrl) {
    this.httpClient = httpClient;
    this.authType = authType;
    this.channelUrl = channelUrl;
  }

  /** Sends {@code outgoingMessage} to . */
  void deliverOutboundMessage(final byte[] outgoingMessage) {
    getLogger().fine("Delivering outbound message: %s bytes", outgoingMessage.length);
    StringBuilder target = new StringBuilder();

    // Build base URL that targets the inbound request service with the encoded network endpoint id
    target.append(channelUrl);
    target.append(AndroidHttpConstants.REQUEST_URL);
    target.append(getWebEncodedEndpointId());

    // Add query parameter indicating the service to authenticate against
    target.append('?');
    target.append(AndroidHttpConstants.SERVICE_PARAMETER);
    target.append('=');
    target.append(authType);

    // Construct entity containing the outbound protobuf msg
    ByteArrayEntity contentEntity = new ByteArrayEntity(outgoingMessage);
    contentEntity.setContentType(AndroidHttpConstants.PROTO_CONTENT_TYPE);

    // Construct POST request with the entity content and appropriate authorization
    HttpPost httpPost = new HttpPost(target.toString());
    httpPost.setEntity(contentEntity);
    setPostHeaders(httpPost);
    try {
      String response = httpClient.execute(httpPost, new BasicResponseHandler());
    } catch (ClientProtocolException exception) {
      // TODO: Distinguish between key HTTP error codes and handle more specifically
      // where appropriate.
      getLogger().warning("Error from server on request: %s", exception);
    } catch (IOException exception) {
      getLogger().warning("Error writing request: %s", exception);
    } catch (RuntimeException exception) {
      getLogger().warning("Runtime exception writing request: %s", exception);
    }
  }

  /** Sets the Authorization and echo headers on {@code httpPost}. */
  private void setPostHeaders(HttpPost httpPost) {
    httpPost.setHeader("Authorization", "GoogleLogin auth=" + getAuthToken());
    if (echoToken != null) {
      // If we have a token to echo to the server, echo it.
      httpPost.setHeader(AndroidHttpConstants.ECHO_HEADER, echoToken);
    }
  }

  /**
   * If {@code echoToken} is not {@code null}, updates the token that will be sent in the header
   * of all HTTP requests.
   */
  void updateEchoToken(String echoToken) {
    if (echoToken != null) {
      this.echoToken = echoToken;
    }
  }

  /** Returns the token that will be sent in the header of all HTTP requests. */
  String getEchoTokenForTest() {
    return this.echoToken;
  }

  /** Sets the HTTP client to {@code client}. */
  void setHttpClientForTest(HttpClient client) {
    this.httpClient = Preconditions.checkNotNull(client);
  }

  /** Returns the base-64-encoded network endpoint id for the client. */
  protected abstract String getWebEncodedEndpointId();

  /** Returns the current authentication token for the client for web requests to . */
  protected abstract String getAuthToken();

  /** Returns the logger to use. */
  protected abstract Logger getLogger();

  /** Attempts to deliver a {@code message} from  to the local client. */
  protected abstract void tryDeliverMessage(AddressedAndroidMessage message);
}
