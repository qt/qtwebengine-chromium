# Streaming Session Protocol

**Owner:** [Jordan Bayles](mailto:jophba@google.com)

---

*** promo
This document is formatted to follow [Gitiles](https://gerrit.googlesource.com/gitiles/+/HEAD/Documentation/markdown.md)
syntax, which includes several extensions (but *not* GitHub Extended Markdown).
***

[TOC]

## Objective and Requirements

This document is the reference specification for the libcast Streaming Session
Protocol. This spec was originally developed as part of the Google Cast v2
project over a decade ago, and has slowly evolved over time to meet the needs of
various Cast devices and scenarios.

With the implementation of [libcast](#libcast) and its ever increasing adoption
rate across the Cast ecosystem, this document will continue to evolve as the
official standard for the Cast Streaming protocol.

### Goals

1. Provide a reference specification for consumers and implementers of the
   libcast library, so that proposed features can be properly discussed,
   designed, implemented, and maintained.

2. Define interfaces for establishing, controlling, and terminating Cast
   streaming (mirroring, remoting, and otherwise) sessions.

3. Prioritize interoperability across the Cast ecosystem, so that Cast devices
   using the latest and greatest version of libcast are still, to the best of
   our ability, compatible with legacy devices no longer receiving updates.
   **This specification will attempt to maintain compatibility with
   legacy Cast v2 devices, but the protocol is expected to grow and change over
   time.**

### Non-Goals

Unfortunately, some important Cast APIs are still defined by closed source
specifications, and are thus out of scope of this document. Defining the
following APIs is thus out of scope:

* **Application control messaging**: Some application control messages, such as
  `LAUNCH` and `STOP`, are included. However, it is likely that there are other,
  unspecified messages used to manage higher level app state. A functional
  implementation is provided for the receiver (
  [openscreen::cast::ApplicationAgent](../receiver/application_agent.h)) and a
  reference implementation for the sender
  ([openscreen::cast::LoopingFileAgent](../standalone_sender/looping_file_cast_agent.h)).

* **Authentication messaging**: On top of TLS, Cast provides additional
  authentication through the use of certificates and private keys. The API for
  authentication is implemented in several directories of cast, especially the
  [//cast/common/channel](../common/channel/) and [//cast/common/certificate](../common/certificate/)
  folders.

* **Keep-alive behavior**: Although the control channel may (or may not) have
  more sophisticated behavior for keeping a session alive, in this specific
  protocol it is limited to a simple timeout, with [deprecated PING/PONG messages](#deprecated-messages)
  kept in the specification.

* **Flinging messaging**: there is a rich suite of messages in the `media`
  namespace used for controlling flinging messages, i.e. sessions where the
  receiver is responsible for fetching content and controlling playback. This
  document is focused on streaming, both mirroring and remoting, and leaves
  flinging for closed source documentation and closed source APIs.

## Background

### Libcast

See the [cast/README.md](../README.md) for more information about the libcast
project.

## Overview

The streaming session protocol defines how a streaming session interacts with
standard Cast messages (in the `com.google.cast` namespace),
mirroring-specific messages (in the `urn:x-cast:com.google.cast.webrtc`
namespace), and remoting-specific messages (in the
`urn:x-cast:com.google.cast.remoting` namespace).

In this context, "mirroring" refers to sending a real-time encoded stream of the
sender's screen to the receiver. "Remoting" refers to an optimization where, if
the sender's screen is primarily composed of a video that the receiver is
capable of performantly decoding, instead of transcoding the video and streaming
it to the receiver, it is instead sent to the receiver still encoded and decoded
directly by the receiver.

* **Launch and Termination Request from Sender**: streaming is initiated using a
  standard com.google.cast `LAUNCH` request with the `appId` parameter set to a
  pre-defined 8-digit alphanumeric app identifier. The full set of application
  IDs known to libcast at this time are defined in the [cast_streaming_app_ids.h](../common/public/cast_streaming_app_ids.h)
  header. Termination is via a standard `STOP` request.

* **Transport Session Negotiation**: a streaming-specific `OFFER` event is
  defined, and used by the sender to generate an offer for the receiver. The
  `ANSWER` response to this event contains the answer object. The `OFFER` must
  be sent immediately after launch, and can be sent again at any time.

* **Presentation Request from Sender**: a `PRESENTATION` request allows sender
  control over rendering transformations applied on the receiver. The
  transformation defines zoom, offset, and rotation; it enables the current
  letterboxing-elimination feature, overscan compensation (for flexibility, if
  needed), and rotation (intended for Android, not used by Chrome).

* **Keep Alive**: both sender and receiver are expected to use media-level
  activity and/or transport layer activity for keeping the connection alive.
  There's no separate application-level keep alive message.

* **Application Protocol & Other Control Messages:** the minimum set of control
  messages required for streaming is `LAUNCH`, `STOP`, and `RECEIVER_STATUS`.
  The full list is documented in the [Session Control Messages](#session-control-messages-comgooglecast)
  section.

## Detailed Design

### Protocol Messages

This section defines the JSON payloads for various messages used in the Cast
protocol.

#### Notes on Types

Message field types generally one of the following three classes: primitives,
structures, or collections of primitives or structures. The most common type
primitives are `string` and `int`. Although this spec is generally written in
C++, there is no technical reason one could not produce an implementation in
other languages. This specification's assumptions about primitive types are
defined in the below table:

| Name     | Definition                                                        |
|----------|-------------------------------------------------------------------|
| `int`    | A signed at-least-32-bit integer value (`int` in C++)             |
| `uint32` | An unsigned 32 bit integer value (`uint32_t` in C++)              |
| `string` | An array of ASCII characters (`char*` or `std::string` in C++)    |

#### Common Message Fields

It is assumed that messages generally have all of the following properties:

| Name      | Type     | Value/description                                     |
|-----------|----------|-------------------------------------------------------|
| sessionId | `int`    | Unique identifier of the session                      |
| seqNum    | `int`    | Request sequence number                               |
| type      | `string` | Represents the specific kind of message               |

#### Session Control Messages (`com.google.cast`)

Basic streaming session control occurs via `com.google.cast` messages (as
currently defined with `version=2`), as follows:

* `LAUNCH` (*request*): initiates the streaming session. For v2 streaming, the
  appId parameter must be set to `0F5096E8` if the session is audio and video,
  or `85CDB22F` if the session is audio only. For this app name, it is expected
  that the Cast receiver will run a specific built-in streaming receiver that
  implements this specification.

* `LAUNCH_STATUS` (*reply*): sent from the Cast receiver to indicate that the
  launch request succeeded.

* `LAUNCH_ERROR` (*reply*): sent from the Cast receiver to indicate that the
  launch request failed.

* `GET_APP_AVAILABILITY` (*request*, *reply*): name of both the request and
  response message for getting information about what applications are
  available.

* `GET_STATUS` (*request*): requests the status of the receiver.

* `STATUS_RESPONSE` (*reply*): response to a `GET_STATUS` request.

* `STOP` (*request*): terminates the streaming session, and must terminate all underlying
  media streams from the sender to the receiver.

* `INVALID_REQUEST` (*reply*): Optional message sent by the receiver whenever an
  invalid command is received.

* `RECEIVER_STATUS` (*reply*): response to a `GET_STATUS` request.

##### LAUNCH

This message is sent from the sender to the receiver to initiate a streaming
session.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `type` | `string` | Must be `LAUNCH`. |
| `requestId` | `int` | A unique identifier for the request. |
| `appId` | `string` | The ID of the application to launch. For streaming, this is typically `0F5096E8` for A/V or `85CDB22F` for audio-only. |
| `appParams` | `object` (optional) | An optional object containing application-specific parameters. |
| `language` | `string` (optional) | The preferred language for the application (e.g., "en-US"). |
| `supportedAppTypes` | array of `string` (optional) | A list of application types supported by the sender (e.g., "WEB", "ANDROID_TV"). |

##### LAUNCH_STATUS

This message is sent from the receiver to the sender to indicate that the
application launch was successful. Note that a full `RECEIVER_STATUS` message is typically sent immediately after this.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `responseType` | `string` | Must be `LAUNCH_STATUS`. |
| `launchRequestId` | `int` | The `requestId` from the original `LAUNCH` request. |
| `status` | `string` | A status string, must be `USER_ALLOWED`. |

##### LAUNCH_ERROR

This message is sent from the receiver to the sender if the application launch
failed.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `responseType` | `string` | Must be `LAUNCH_ERROR`. |
| `requestId` | `int` | The `requestId` from the original `LAUNCH` request. |
| `reason` | `string` | A string code indicating the reason for the failure (e.g., `NOT_FOUND`, `SYSTEM_ERROR`). |

##### GET_APP_AVAILABILITY

A request from the sender to check if specific applications can be launched on
the receiver. The response uses the same message name in its `responseType` field.

**Request:**

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `type` | `string` | Must be `GET_APP_AVAILABILITY`. |
| `requestId` | `int` | A unique identifier for the request. |
| `appId` | array of `string` | An array of application IDs to check. |

**Response:**

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `responseType` | `string` | Must be `GET_APP_AVAILABILITY`. |
| `requestId` | `int` | The `requestId` from the request. |
| `availability` | `object` | An object where keys are `appId`s and values are `APP_AVAILABLE` or `APP_UNAVAILABLE`. |

##### GET_STATUS

A request from the sender to get the receiver's current status. It has no
payload other than the common `type` and `requestId` fields.

##### RECEIVER_STATUS

A response sent by the receiver containing its current status. This can be in
response to a `GET_STATUS` request or sent unsolicited when the receiver's state
changes.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `responseType` | `string` | Must be `RECEIVER_STATUS`. |
| `requestId` | `int` | The `requestId` from the `GET_STATUS` request, or 0 if unsolicited. |
| `status` | `object` | The main status object. |
| `status.applications` | array of `object` | A list of running applications. Each object contains `appId`, `displayName`, `sessionId`, `transportId`, `isIdleScreen`, etc. |
| `status.volume` | `object` | An object describing the device's volume state, with fields like `level`, `muted`, and `controlType`. |

##### STOP

This message is sent from the sender to the receiver to terminate a running
application.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `type` | `string` | Must be `STOP`. |
| `requestId` | `int` | A unique identifier for the request. |
| `sessionId` | `string` | The ID of the session to be terminated. |

##### INVALID_REQUEST

Sent by the receiver when it receives a malformed or invalid request.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `responseType` | `string` | Must be `INVALID_REQUEST`. |
| `requestId` | `int` | The `requestId` from the invalid request, if it could be parsed. |
| `reason` | `string` | A string code for the error (e.g., `INVALID_COMMAND`). |

#### Discovery Messages (`google.cast.receiver.discovery`)

##### GET_DEVICE_INFO

A request from the sender for detailed information about the receiver device.
The response unexpectedly uses the same message `type` and is not a `responseType`.

**Request:**

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `type` | `string` | Must be `GET_DEVICE_INFO`. |
| `requestId` | `int` | A unique identifier for the request. |

**Response:**

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `type` | `string` | Must be `GET_DEVICE_INFO`. |
| `requestId` | `int` | The `requestId` from the request. |
| `deviceId` | `string` | A unique identifier for the receiver device. |
| `friendlyName` | `string` | The user-configured device name. |
| `deviceModel` | `string` | The product model name. |
| `capabilities` | `int` | A bitmask of device capabilities. |
| `controlNotifications` | `int` | A flag for control notifications. |

#### Setup Messages (`com.google.cast.setup`)

##### eureka_info

A response message providing detailed product and build information about the
receiver hardware. The request for this message is not clearly defined in the
code, but this response also uses a `type` field instead of a `responseType`.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `type` | `string` | Must be `eureka_info`. |
| `request_id` | `int` | The `request_id` from the original request. Note the underscore. |
| `response_code` | `int` | A status code (e.g., 200 for OK). |
| `response_string` | `string` | A status string (e.g., "OK"). |
| `data` | `object` | An object containing the device details. |
| `data.name` | `string` | The friendly name of the device. |
| `data.version` | `int` | The version of this info structure. |
| `data.device_info` | `object` | An object containing device hardware details like `manufacturer` and `product_name`. |
| `data.build_info` | `object` | An object containing software build details like `cast_build_revision`. |

#### Media Transport Messages (`com.google.cast.webrtc`)

A number of streaming-specific features are defined via the
`com.google.cast.webrtc` namespace, which defines the following additional
messages:

##### OFFER

This message is sent from the sender to the receiver to initiate a streaming
session.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `type` | `string` | Must be `OFFER`. |
| `offer` | `object` | [Offer object](#offer-object-definition) |

The `OFFER` request can be sent by the sender at any point in time during the
session to renegotiate parameters of the session.

If the receiver generates an error response to the initial offer, the sender
should immediately terminate the session and inform the receiver unless it's
able to generate a fallback offer.

A subsequent offer after a session is successfully established is only effective
once an "ok" response is generated by the receiver. If an "error" response is
generated, the already-established session *should* remain in effect.

###### Example `OFFER` Message

For a full example `OFFER` message, see [castv2/streaming_examples/offer.json](./castv2/streaming_examples/offer.json).

##### ANSWER

This message is sent from the receiver to the sender in response to an `OFFER`.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `type` | `string` | Must be `ANSWER`. |
| `result` | `string` | Must be `ok` or `error`. |
| `error` | `object` (optional) | Only populated if `result` is `error`. [Error object](#error-object-definition) |
| `answer` | `object` (optional) | Only populated if `result` is `ok`. [Answer object](#answer-object-definition) |

###### Example ANSWER message

For a full example `ANSWER` message, see [castv2/streaming_examples/answer.json](./castv2/streaming_examples/answer.json).

##### GET_CAPABILITIES

The "type" must be set to `GET_CAPABILITIES`, with no message body.

##### CAPABILITIES_RESPONSE

The "type" must be set to `CAPABILITIES_RESPONSE`, with the message body in a
"capabilities" object. Note that the `key_systems` field has been deprecated and
removed here.

| Name | Type | Value/description |
| :---- | :---- | :---- |
| type | `string` | Must be set to `CAPABILITIES_RESPONSE` |
| capabilities | `object` | A [Capabilities object](#capabilities-object-definition) |

### Object Definitions

#### Offer Object Definition

The "type" must be set to "OFFER", with the message body in an "offer" object.
For a living reference, see [libcast's offer_messages.h](../cast/streaming/public/offer_messages.h).

NOTE: the libcast implementation separates out `supportedStreams` into strongly
typed `audio_streams` and `video_streams` arrays, but they are collated together
when serialized to JSON.

| Name             | Type                      | Value/description             |
|------------------|---------------------------|-------------------------------|
| supportedStreams | array of `Stream` objects | An array of stream objects describing all acceptable stream formats that this endpoint supports. Sender only includes codecs it supports and the order of the stream objects shows the sender's preference. Receiver can choose any stream it prefers, or the first stream it supports if it doesn't have any preferences. Receiver informs sender about the selected stream objects in the sendIndexes of the `ANSWER` object. |
| castMode         | `string`                  | Indicates whether the offer is for "mirroring" or "remoting". See `CastMode` in [`cast/streaming/public/constants.h`](../streaming/public/constants.h). |

##### Stream object

The stream object contains a generic section common for both audio and video; it also contains an audio or video specific section based on the type specified in the generic section.

*** aside
A note on codecs:
The set of codec profiles supported for Cast playback / remoting is notably
larger than the codec profiles supported for mirroring streams. This is due to
the practical limitation that implementing decoding support for a given codec is
generally easier, more likely to have hardware support, and less likely to run
into licensing issues.
***

| Name                 | Type     | Value/description                          |
|----------------------|----------|--------------------------------------------|
| index                | `int`    | An identifier established by the initiator that MUST contain a Number. The index of the first stream object must start with 0 and each following index MUST be the previous index +1. |
| type                 | `string` | A String specifying the type of stream offered. Supported values are defined in `Stream::Type` in [`cast/streaming/public/offer_messages.h`](../streaming/public/offer_messages.h). |
| codecName            | `string` | A String specifying the codec. Supported values are defined in `AudioCodec` and `VideoCodec` in [`cast/streaming/public/constants.h`](../streaming/public/constants.h). To be compliant, cast receivers must at least implement `opus`, if they support audio, and `vp8` if they support video. Senders must implement at least the baseline codecs (`h264` or `vp8`, and `aac` or `opus`). |
| codecParameter       | `string` | A string specifying the codec parameter, in accordance with [RFC.6381](https://datatracker.ietf.org/doc/html/rfc6381#section-3.2). Also known as the "media type string" as in [**Supported Media for Google Cast**](https://developers.google.com/cast/docs/media). Examples include `avc1.64002A` for H.264 level 4.2, and `hev1.1.6.L150.B0` for H.265 main 5.0. |
| rtpProfile           | `string` | A String specifying supported RTP profile. Currently only `cast` is supported, with `codec` reserved for future interop with the intention of being used to indicate that codec-defined RTP profiles (defined by their respective RFCs) shall be used. |
| rtpPayloadType       | `int`    | A Number specifying the RTP payload type used for this stream. *Valid values are in the range [96, 127]. See [RtpPayloadType](../streaming/impl/rtp_defines.h)* |
| ssrc                 | `uint32` | A Number specifying the RTP SSRC used for this stream. Values must be unique between all streams for this sender. All values are valid. |
| targetDelay          | `int`    | Indicates the desired total end-to-end latency. |
| aesKey               | `string` | A String specifying which AES key to use. Must consist of exactly 32 hex digits. Both an AES key and initialization vector are required: if either field is missing, this stream is invalid. |
| aesIvMask            | `string` | A String specifying which initialization vector mask to use. Must consist of exactly 32 hex digits. Must be provided. |
| receiverRtcpEventLog | `boolean` (optional) | True to request receiver to send event log via RTCP. False otherwise. |
| receiverRtcpDscp     | `int` (optional) | Request receiver to send RTCP packets using DSCP value indicated. Typically this value is 46. |
| rtpExtensions        | `Array of string` (optional) | RTP extensions (Currently only `adaptive_playout_delay`) supported by the Sender. Receivers can then reply with a list of rtpExtensions from this list that it also supports. |
| timeBase             | `string` (optional) | Number specifying the time base used by this "rtpPayloadType". Default value is `1/90000`. Valid values are "1/\<sample rate\>" where sample rate is strictly positive. |

##### Audio Stream object

| Name | Type | Value/description |
| :---- | :---- | :---- |
| bitRate | `int` | A Number specifying the average bitrate in bits per second used by this "rtpPayloadType". |
| channels | `int` | A Number specifying the number of audio channels used by this "rtpPayloadType". |

##### Video Stream object

Note that additional video codec information such as codec profile and level, and video stream protection, are not implemented by any current senders or receivers. If these features need to be used in the future, they should be reimplemented.>TODO(crbug.com/471102790): The implementation includes `profile` and `level` fields, which contradicts the spec's claim that they are not implemented. The spec should be updated to reflect their presence.

| Name | Type | Value/description |
| :---- | :---- | :---- |
| maxFrameRate | `string` | Max number of frames per second used by this "rtpPayloadType". Note: Receivers may ignore this field when providing constraints in the `ANSWER` message. In this case, the sender must respect those constraints. |
| maxBitRate | `int` | Max bitrate in bits per second used by this "rtpPayloadType". Note: Receivers may ignore this field when providing constraints in the `ANSWER` message. In this case, the sender must respect those constraints. |
| resolutions | array of Video resolution objects | An array of resolutions supported by this "rtpPayloadType". Note: Receivers may ignore this field when providing constraints in the `ANSWER` message. In this case, the sender must respect those constraints. |
| errorRecoveryMode | `string` (optional) | String to indicate how video stream is encoded. Default value is **castv2.** "**castv2**" means that the receiver cannot drop any video packets. There is no key frame or intra refresh mode after the first video frame in the session. "**intra_mb_refresh**" means that frames are encoded using intra macroblock refresh mode. The receiver can drop a video frame and recover later on after receiving new key frames or intra refresh macroblocks. |

##### Video resolution object

| Name | Type | Value/description |
| :---- | :---- | :---- |
| width | `int` | Width in pixels. |
| height | `int` | Height in pixels. |

#### Error Object Definition

The "type" may be set to anything, but the "result" field must be present and
set to "error", with the message body in an "error" object.

| Name | Type | Value/description |
| :---- | :---- | :---- |
| code | `int32` | A code indicating what class of error occurred. |
| description | `string` | Description of the error. |

For a comprehensive and up-to-date list of error codes, refer to the
`openscreen::Error::Code` enum in [`platform/base/error.h`](../platform/base/error.h).

#### Answer Object Definition

The "type" must be set to "ANSWER", with the message body in an "answer" object. For a living reference, see [libcast's answer_messages.h](../cast/streaming/public/answer_messages.h).

| Name | Type | Value/description |
| :---- | :---- | :---- |
| udpPort | `int` | A Number specifying the UDP port used for all streams (RTP and RTCP) in this session. *Note: values 1 to 65535 is valid.* |
| sendIndexes | `Array of  int` | Numbers specifying the indexes chosen from the `OFFER` message. |
| ssrcs | `Array of  uint32` | Number specifying the RTP SSRC used to send the RTCP feedback of the stream indicated by the "sendIndexes" above. *Note: values 0 to 2^32 is valid.* |
| constraints | `receiver constraints object` (optional, but highly recommended) | Provides detailed maximum capabilities of the receiver for processing the streams selected in "sendIndexes" above; including audio sampling rate and number of channels, video dimensions and rates, encoded bit rates, and target latency. A sender may alter video resolution or frame rate throughout a session. The constraints here restrict how much data volume is allowed before the sender must subsample (e.g., downscale and/or reduce frame rate). |
| display | `display description object` (optional, but highly recommended) | Provides details about the display on the receiver, including dimensions (aspect ratio implied), scaling behavior, color profile, etc. |
| receiverRtcpEventLog | `Array of int` (optional) | Numbers specifying the indexes of streams that will send event log via RTCP. If this field is not present then the receiver does not support sending an event log via RTCP. |
| receiverRtcpDscp | `Array of int` (optional) | Numbers specifying the indexes of streams that will use DSCP values specified in the `OFFER` message for RTCP packets. If this field is not present then the receiver does not support DSCP. |
| rtpExtensions | `Array of string` (optional) | If this field is not present then the receiver does not support any RTP extensions. |

##### Receiver Constraints Object Definition

| Name | Type | Value/description |
| :---- | :---- | :---- |
| audio | `audio receiver constraints object` | Audio constraints. See below. |
| video | `video receiver constraints object` | Video constraints. See below. |

##### Audio Receiver Constraints Object Definition

| Name | Type | Value/description |
| :---- | :---- | :---- |
| codecName | `string` | Audio codec name. See `AudioCodec` in [`cast/streaming/public/constants.h`](../streaming/public/constants.h). |
| maxSampleRate | `int` | Maximum supported sampling frequency (not necessarily the ideal sampling frequency). |
| maxChannels | `int` |  Maximum number of audio channels supported. The number here is interpreted to relate to a standard speaker layout (e.g., 2 for left-and-right stereo, 5 for a left+center+right+left_surround+right_surround). |
| minBitRate | `int` (optional) | Minimum encoded audio data bits per second. If not specified, the sender will assume 32 kbps. Note: A receiver should never restrict the minBitRate to try to improve quality. This should reflect the true operational minimum. |
| maxBitRate | `int` | Maximum encoded audio data bits per second. This is the lower of: 1\) The maximum capability of the decoder; or 2\) The maximum sustained data transfer rate (e.g., could be limited by the CPU, RAM bandwidth, etc.). If not specified, the sender will assume no greater than 320kbps. |
| maxDelay | `int` (optional) | Maximum supported end-to-end latency, in milliseconds, for audio. This is proportional to the size of the data buffers in the receiver. Meaning, assume a very low-latency link between sender and receiver, and this value would indicate the amount of buffering that can be maintained (due to RAM capacity, etc.). If not provided, a default of 1200ms should be used. |

##### Video Receiver Constraints Object Definition

| Name | Type | Value/description |
| :---- | :---- | :---- |
| codecName | `string` (optional) | Video codec name. See `VideoCodec` in [`cast/streaming/public/constants.h`](../streaming/public/constants.h). If omitted, these constraints apply to all video codecs. |
| maxPixelsPerSecond | `double` (optional) | Maximum pixel rate (width x height x framerate). Note that this value can, and often will be, much less than multiplying the fields in maxDimensions. The purpose of this field is to limit the overall maximum processing rate. A sender will use this, in conjunction with the fields below, to trade-off between higher/lower resolution and lower/higher frame rate. *Example: A device may be capable of 62208000 pixels per second, which allows a sender to send* 1280x720@60 or 1920x1080@30*. In this example, the maxDimensions might specify {width:1920, height:1080, frameRate:60}.* |
| minResolution | `resolution object` (optional) | Minimum width and height in pixels. If not specified, the sender will assume a reasonable minimum having the same aspect ratio as maxDimensions, with an area as close to 320x180 as possible. Note: A receiver should never restrict the minResolution in an effort to improve quality. This should reflect the true operational minimum. |
| maxDimensions | `dimensions object` | Maximum width and height in pixels (not necessarily the ideal width or height), and the maximum frame rate (not necessarily the ideal frame rate). |
| minBitRate | `int` (optional) | Minimum encoded video data bits per second. If not specified, the sender will assume 300 kbps. Note: A receiver should never restrict the minBitRate in an effort to improve quality. This should reflect the true operational minimum. |
| maxBitRate | `int` | Maximum encoded video data bits per second. This is the lower of: 1\) The maximum capability of the decoder; or 2\) The maximum sustained data transfer rate (e.g., could be limited by the CPU, RAM bandwidth, etc.). |
| maxDelay | `int` (optional) | Maximum supported end-to-end latency, in milliseconds, for video. This is proportional to the size of the data buffers in the receiver. Meaning, assume a very low-latency link between sender and receiver, and this value would indicate the amount of buffering that can be maintained (due to RAM capacity, etc.). If not provided, a default of 1200ms should be used. |

##### Resolution Object Definition

| Name | Type | Value/description |
| :---- | :---- | :---- |
| width | `int` | Width, in pixels. |
| height | `int` | Height, in pixels. |

##### Dimensions Object Definition

| Name | Type | Value/description |
| :---- | :---- | :---- |
| width | `int` | Width, in pixels. |
| height | `int` | Height, in pixels. |
| frameRate | `string` | Frame rate. This should be specified as a rational decimal number (e.g., "30" or "30000/1001"). |

##### Display Description Object Definition

| Name | Type | Value/description |
| :---- | :---- | :---- |
| dimensions | `dimensions object` (optional) | If present, the receiver is attached to a fixed display having the given dimensions and frame rate (vsync) configuration. These dimensions may exceed, be the same, or be less than those mentioned in the constraints. If undefined, the receiver display is assumed to be fixed (e.g., a panel in a Hangouts UI). The sender uses this to decide the best way to sample, capture, and encode the content to optimize the user viewing experience. |
| aspectRatio | `string` (optional) | The aspect ratio, in "#:#" format, when the receiver is attached to a fixed display. When missing and dimensions are specified, the sender will assume pixels are square, and the dimensions imply the aspect ratio of the fixed display. When present and dimensions are also specified, this implies the display pixels are not square. |
| scaling | `string` (optional) | One of: "sender"  The sender must scale and letterbox the content and provide video frames of a fixed aspect ratio. "receiver"  The sender may send arbitrarily sized frames, and the receiver will handle the scaling and letterboxing as necessary for proper display. |

*** aside
Support for color balance profile, bit depth, and other properties has been
discussed in the past, but never added to the spec or finalized.
***

##### Capabilities Object Definition

| Name | Type | Value/description |
| :---- | :---- | :---- |
| mediaCaps | `array of string` | List of media capabilities of the receiver. See `AudioCapability` and `VideoCapability` in [`cast/streaming/remoting_capabilities.h`](../streaming/remoting_capabilities.h). `video` is deprecated and not used. |
| remoting | `int` | Remoting version of the receiver. |
| result | `string` | Indicates whether getting capabilities succeeded, must be either "ok" or "error." |

*** promo
`audio` is a special value that indicates support for an set of codecs that have
been defined as a "baseline" set. The current baseline set is defined in the
Chrome [RendererController](https://source.chromium.org/chromium/chromium/src/+/main:media/remoting/renderer_controller.cc;drc=7cc84add564ab18554cefa903004afca74849fe3;l=460)
as the following list:

1. **MP3**
2. **PCM** (Baseline, S16BE, S24BE, ALAW)
3. **Vorbis**
4. **FLAC** (Free Lossless Audio codec)
5. **AMR** (both narrow band and wide band)
6. **GSM MS** (a special Microsoft version of GSM Full Rate)
7. **Enhanced AC-3** (Dolby Digital Plus)
8. **ALAC** (Apple Lossless Audio Codec)
9. **AC3** (also known as Dolby Digital)
10. **DTS-HD** Master Audio
11. **DTS:X** (Profile 2, lossy)
12. **DTS** Extended Surround

***

*** aside
Some legacy receivers may report `vp9` and `hevc` in their `mediaCaps` response,
even if they cannot remote these codecs.
***

#### Remoting Messages (`com.google.cast.remoting`)

Finally, in the `com.google.cast.remoting` namespace contains the following
remoting specific messages:

##### RPC

The "type" must be set to "RPC", with the base64-encoded protobuf message stored
as a string under the "rpc" key.

Protobuf messages are complex, and defined in the [remoting.proto](../streaming/remoting.proto)
file.

#### Media Status Messages (`com.google.cast.media`)

Most of these messages are not supported in libcast, and are instead used for
controlling flinging sessions with the Cast SDKs.

*** aside
TODO(crbug.com/471102790): evaluate including more media message types in
libcast.
***

##### MEDIA_STATUS

Sent by a media application on the receiver to update the sender on the state
of media playback.

| Name | Type | Value/description |
| :--- | :--- | :--- |
| `responseType` | `string` | Must be `MEDIA_STATUS`. |
| `requestId` | `int` | An identifier for the request, or 0 if unsolicited. |
| `media` | array of `object` | An array containing one or more media status objects. |
| `media[n].mediaSessionId`| `int` | The ID of the media session. |
| `media[n].playerState` | `string` | The state of playback (e.g., `PLAYING`, `PAUSED`, `IDLE`). |
| `media[n].currentTime` | `double` | The current playback time in seconds. |
| `media[n].media` | `object` | An object with metadata about the content, such as `contentId` and `title`. |

### Reference Schemas and Examples

*** aside
TODO(crbug.com/471102790): rename castv2 folder to reflect its evergreen nature.
***

This specification is backed by two JSON Schemas:

1. [receiver_schema.json](./castv2/receiver_examples/): containing core receiver
   control and status messages, including `LAUNCH`, `STOP`,
   `GET_APP_AVAILABILITY`, `LAUNCH_STATUS`, `LAUNCH_ERROR`, `GET_STATUS`,
   `RECEIVER_STATUS`, `INVALID_REQUEST`, `GET_DEVICE_INFO`, `eureka_info`, and
   `MEDIA_STATUS`.

2. [streaming_schema.json](./castv2/streaming_schema.json): containing messages
   specific to the streaming session, such as `OFFER`, `ANSWER`,
   `GET_CAPABILITIES`, `CAPABILITIES_RESPONSE`, and `RPC`.

Examples are provided in the [castv2/receiver_examples](./castv2/receiver_examples/)
(for receiver control and media status messages) and
[castv2/streaming_examples](./castv2/streaming_examples/) (for streaming
specific messages) folders, with a C++ validation component defined in
[castv2/validation.h](./castv2/validation.h).

*** note
When adding or modifying messages in this specification, the corresponding
schema and examples should be **updated concurrently**. The syntax of these
files can be validated using `yajsv` -- see the
[castv2/README.md](./castv2/README.md) for more information.
***

### Discovering Receiver Capabilities

Prior to the offer/answer exchange, the sender may desire information about the
receiver in order to create an optimal offer. This discovery of capabilities is
currently limited to the DNS-SD [ca bit-field](https://docs.google.com/document/d/1d1wuxHioJ9cBVBQ6UwqBFVMrn48Nb3Yp5k1a_eTXsHE/edit?usp=sharing),
which indicates whether the receiver supports audio or video. Remoting specific
capabilities may be discovered by the sender using a `GET_CAPABILITIES` call, as
defined below.

### Keep Alive

There is **no** official control/application-level keep alive, and the
`PING` and `PONG` messages originally included in the protocol are now
[deprecated](#deprecated-messages). The sender and receiver are both expected to
either disconnect by inferring the session has ended through status messages, or
independently based on media-level timeouts (the media layer must thus send an
event to the application level to do the appropriate cleanup, but the exact
mechanism to this is specific to particular sender and receiver
implementations).

The default timeout is **15 seconds**. If no media packets (`RTP`, `ACK`,
`NACK`, etc.) are received from the remote peer within this duration, the sender
or receiver that failed to receive data should disconnect.

*** aside
In legacy Cast devices, an application-level "keep-alive" message was used both
by the sender and the receiver to terminate a streaming session. This was used
to handle a variety of scenarios, with the most common being the desire to
quickly end a session when a user closes their laptop (for example).

This approach was abandoned in favor of relying on ACKs, status messages, and
timeouts due to these mechanisms resulting in more robust streaming sessions,
as well as reducing unnecessary network traffic and battery drain.
***

### Security Considerations

The Cast v2 protocol includes several security mechanisms to protect the
streaming session.

#### Encryption

Media streams *must* be encrypted using [AES-128](https://nvlpubs.nist.gov/nistpubs/FIPS/NIST.FIPS.197-upd1.pdf).
The `aesKey` and `aesIvMask` fields in the `Stream` object of the `OFFER`
message are used to provide the encryption key and initialization vector mask.
Both are 32-digit hex strings. If these fields are absent, the `OFFER` message
shall be considered invalid and the session request rejected.

#### Authentication

Device authentication is performed as the first step of the connection, as shown
in [Appendix A](#appendix-a---sample-cast-channel-message-flow). This is handled
by the `com.google.cast.tp.deviceauth` namespace. The details of the
authentication protocol are outside the scope of this document.

### Deprecated Messages

Several message types were originally included in the Cast v2 specification,
but have become deprecated and may or may not be implemented on modern Cast
devices and are not required for devices to be compliant Cast senders or
receivers.

These message types are called out in the [CastMessageType](../common/channel/message_util.h)
enum in the libcast implementation, as well as listed here:

* `APPLICATION_BROADCAST` (*reply*): context is *unknown* -- lost to time.

* `INVALID_PLAYER_STATE` (*reply*): indicates that the player is in an invalid state.

* `LOAD_FAILED` (*reply*): indicates that loading a media has failed.

* `LOAD_CANCELLED` (*reply*): indicates that loading a media was cancelled.

* `MULTIZONE_STATUS` (*request*): context is *unknown* -- lost to time.

* `PRESENTATION` (*request*): controls zooming, panning, rotation.

* `PING` (*request*): sent to ask the receiver if it is currently alive.

* `PONG` (*response*): sent to inform the sender that this receiver is alive.
*
## Appendix A - Sample Cast Channel Message Flow

This appendix describes the typical sequence of JSON messages exchanged between
a Cast sender and receiver to establish, run, and terminate a streaming session.

### 1. Discovery and Handshake

Before a streaming session can be launched, the sender first discovers the
receiver on the network (e.g., via mDNS) and establishes a secure channel. The
following message exchange then occurs:

1. **Device Authentication**: The sender and receiver exchange authentication
    messages to verify their identities. This is a prerequisite for all further
    communication.
    * **Namespace**: `urn:x-cast:com.google.cast.tp.deviceauth`

2. **Virtual Connection**: The sender establishes a "virtual connection" to the
    main receiver process. This acts as the initial control channel.
    * **Sender → Receiver**: `urn:x-cast:com.google.cast.tp.connection`, type
        `CONNECT`

3. **Receiver Status Check**: The sender queries the receiver's current status.
    * **Sender → Receiver**: `urn:x-cast:com.google.cast.receiver`, type
        `GET_STATUS`
    * **Receiver → Sender**: `urn:x-cast:com.google.cast.receiver`, type
        `RECEIVER_STATUS`. The reply indicates the currently running application
        (typically the "Backdrop" idle screen) and other device state like
        volume.

4. **Application Availability**: The sender checks if the receiver supports the
    Cast streaming application.
    * **Sender → Receiver**: `urn:x-cast:com.google.cast.receiver`, type
        `GET_APP_AVAILABILITY`. The sender sends one request for the standard
        audio/video streaming app (`0F5096E8`) and another for the audio-only
        app (`85CDB22F`).
    * **Receiver → Sender**: `urn:x-cast:com.google.cast.receiver`, type
        `GET_APP_AVAILABILITY` response. The receiver confirms whether each app
        is available or not.

### 2. Streaming Session Launch

Once the user initiates streaming (e.g., by selecting "Cast..." in Chrome), the following message flow begins:

1. **Launch Request**: The sender requests the receiver to launch the streaming
   application.
    * **Sender → Receiver**: `urn:x-cast:com.google.cast.receiver`, type `LAUNCH`

    ```json
    {
      "type": "LAUNCH",
      "appId": "0F5096E8",
      "requestId": 17
    }
    ```

2. **Launch Confirmation**: The receiver confirms the launch by sending a
   `RECEIVER_STATUS` update. This crucial message contains the `transportId` and
   `sessionId` for the newly created application instance. This `transportId`
   will be used as the `destination_id` for all subsequent messages to the
   streaming app.

    ```json
    {
      "type": "RECEIVER_STATUS",
      "requestId": 17,
      "status": {
        "applications": [
          {
            "appId": "0F5096E8",
            "displayName": "Chrome Mirroring",
            "sessionId": "154d8823-...",
            "transportId": "154d8823-...",
            "isIdleScreen": false
          }
        ]
      }
    }
    ```

3. **Connect to Streaming App**: The sender establishes a new virtual
   connection, this time directly to the streaming application instance using
   its `transportId`.
    * **Sender → Streaming App**: `urn:x-cast:com.google.cast.tp.connection`,
    type `CONNECT`

4. **Media Negotiation (Offer/Answer)**: The sender and receiver negotiate
   the media format for streaming.
    * **Sender → Streaming App**: `urn:x-cast:com.google.cast.webrtc`, type
    `OFFER`. The sender proposes a set of supported audio and video streams.

    ```json
    {
      "type": "OFFER",
      "seqNum": 820263768,
      "offer": {
        "castMode": "mirroring",
        "supportedStreams": [
          {
            "index": 0,
            "type": "audio_source",
            "codecName": "opus",
            "rtpPayloadType": 127,
            "ssrc": 264890,
            "targetDelay": 400,
            "channels": 2,
            ...
          },
          {
            "index": 1,
            "type": "video_source",
            "codecName": "vp8",
            "rtpPayloadType": 96,
            "ssrc": 748229,
            "maxFrameRate": "30",
            "resolutions": [{"width": 1920, "height": 1080}],
            ...
          }
        ]
      }
    }
    ```

    * **Streaming App → Sender**: `urn:x-cast:com.google.cast.webrtc`, type
    `ANSWER`. The receiver accepts the offer, selects which streams it will
    use (via `sendIndexes`), and specifies the UDP port for media transport.

    ```json
    {
      "type": "ANSWER",
      "seqNum": 820263768,
      "result": "ok",
      "answer": {
        "udpPort": 33533,
        "sendIndexes": [0, 1],
        "ssrcs": [264891, 748230]
      }
    }
    ```

5. **Streaming Active**: With the negotiation complete, media begins to flow
   over UDP. The streaming app sends a `MEDIA_STATUS` message to confirm that
   playback has started.
    * **Streaming App → Sender**: `urn:x-cast:com.google.cast.media`,
    type `MEDIA_STATUS`

### 3. Streaming Session Termination

When the user stops the session:

1. **Stop Request**: The sender sends a `STOP` message to the main receiver
   process, referencing the `sessionId` of the streaming application.
    * **Sender → Receiver**: `urn:x-cast:com.google.cast.receiver`,
    type `STOP`

    ```json
    {
      "type": "STOP",
      "sessionId": "154d8823-...",
      "requestId": 22
    }
    ```

2. **Close Connection**: The streaming application, upon termination, sends a
   `CLOSE` message to tear down its virtual connection with the sender.
    * **Streaming App → Sender**: `urn:x-cast:com.google.cast.tp.connection`,
    type `CLOSE`

3. **Return to Idle**: The receiver returns to the idle screen and broadcasts a
   final `RECEIVER_STATUS` update, showing that "Backdrop" is now the active
   application.

