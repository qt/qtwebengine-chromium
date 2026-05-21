# Cast Streaming

This module contains an implementation of Cast Streaming, the real-time media
streaming protocol between Cast senders and Cast receivers.

[TOC]

## Directory Structure

The `streaming` directory is organized as follows:

- `public/`: Contains the public C++ header files that form the primary API for
  the streaming library.

- `impl/`: Contains the internal implementation details of the streaming
  protocol. The components in this directory are not meant to be used directly
  by most applications.

- `testing/`: Contains testing-related utilities and mocks.

- The root directory contains build files and protocol buffer definitions, as
  well as some implementation and public specific files, although this is
  discouraged in favor of using `public/` and `impl/` where possible.

## API Overview

This document provides a code-level overview of the Cast Streaming Sender and
Receiver APIs. For a higher-level view of the entire Cast ecosystem, please see
the main [architecture document](../../cast/docs/architecture.md).

The Cast Streaming library is designed around two primary classes: the `Sender`
and the `Receiver`.

### Core Concepts: Sender and Receiver

- [**`cast::Sender`**](./sender.h): This class is used by the client application
  that wishes to stream media _to_ a Cast device. Its main responsibility is to
  take encoded media frames (e.g., from a video encoder), packetize them into
  RTP packets, and send them to a `Receiver`. It also handles feedback from the
  `Receiver`, such as acknowledgements and requests for retransmissions.

- [**`cast::Receiver`**](./receiver.h): This class runs on the Cast device that
  is the target for the media stream. It receives RTP packets, reassembles them
  into encoded media frames, and makes them available to a client application
  (e.g., a media player) for decoding and rendering.

A typical Cast session involves at least one `Sender` and `Receiver` pair. For
A/V streaming, there are usually two pairs: one for audio and one for video.

### The Offer/Answer Negotiation Flow

Before streaming can begin, the `Sender` and `Receiver` must negotiate the
parameters of the stream. This is done using an "Offer/Answer" model, similar to
WebRTC.

1. **The Sender Creates an `Offer`**: The sender application constructs an
   `Offer` object. This object describes the media stream(s) the sender wishes
   to send, including details like:
   - Codecs (e.g., VP8, Opus)
   - Resolutions and frame rates
   - Bit rates
   - Encryption parameters (`aes_key`, `aes_iv_mask`)

   ```cpp
   // Sender-side example
   openscreen::cast::Offer offer;
   offer.cast_mode = openscreen::cast::CastMode::kMirroring;

   openscreen::cast::AudioStream audio_stream;
   audio_stream.stream.index = 0;
   audio_stream.stream.type = openscreen::cast::Stream::Type::kAudioSource;
   audio_stream.codec = openscreen::cast::AudioCodec::kOpus;
   // ... and other audio parameters ...
   offer.audio_streams.push_back(audio_stream);

   openscreen::cast::VideoStream video_stream;
   video_stream.stream.index = 1;
   // ... and other video parameters ...
   offer.video_streams.push_back(video_stream);

   // The offer is then typically serialized to JSON and sent to the receiver
   // over a separate control channel (e.g., a Cast V2 message).
   ```

2. **The Receiver Responds with an `Answer`**: The receiver application receives
   the `Offer`, parses it, and determines if it can accept the proposed
   stream(s). It then constructs an `Answer` object, which specifies the
   parameters it has agreed to. Key fields in the `Answer` include:
   - `udp_port`: The port on which the `Receiver` will listen for RTP packets.
   - `send_indexes`: A list of indexes corresponding to the streams in the
     `Offer` that the receiver has chosen to accept.
   - `ssrcs`: The synchronization source identifiers the receiver will use.

   The `Answer` is serialized and sent back to the sender application.

This negotiation happens at the application level, before the `Sender` and
`Receiver` objects are instantiated. The result of this exchange is a
`SessionConfig` object that is used to initialize both sides of the stream.

### Configuration and Instantiation

Once the Offer/Answer exchange is complete, both the sender and receiver have
the necessary `SessionConfig`. They can then create the `Sender` and `Receiver`
objects.

- **Sender Instantiation**:

  ```cpp
  // Sender-side example
  #include "cast/streaming/public/sender.h"
  #include "cast/streaming/public/environment.h"
  // ...

  // The SessionConfig is created from the Offer and Answer.
  const auto session_config =
      SessionConfig::Create(offer, answer);

  // An Environment provides platform dependencies like a clock and task runner.
  openscreen::cast::Environment environment(...);
  openscreen::cast::Sender sender(environment,
                                  packet_router, // Manages network sockets
                                  session_config,
                                  rtp_payload_type);
  ```

- **Receiver Instantiation**:

  ```cpp
  // Receiver-side example
  #include "cast/streaming/public/receiver.h"
  #include "cast/streaming/public/environment.h"
  // ...

  // The SessionConfig is created from the Offer and Answer.
  const auto session_config =
      SessionConfig::Create(offer, answer);

  openscreen::cast::Environment environment(...);
  openscreen::cast::Receiver receiver(environment,
                                    packet_router, // Manages network sockets
                                    session_config);
  ```

### The Client Pattern

Both the `Sender` and `Receiver` are designed to be used with a delegate (or
client) pattern. The application provides an implementation of an interface to
receive asynchronous events and data.

- **`Sender::Observer`**: The application implements the `Sender::Observer`
  interface to handle events from the `Sender`. This is crucial for reacting to
  network conditions and receiver status.

  ```cpp
  class MySenderClient : public openscreen::cast::Sender::Observer {
   public:
    void OnPictureLost() override {
      // The receiver has indicated it needs a key frame to continue.
      // The application should request one from its encoder.
      if (sender_->NeedsKeyFrame()) {
        RequestNewKeyFrameFromEncoder();
      }
    }

    void OnFrameCanceled(openscreen::cast::FrameId frame_id) override {
      // The sender is done with this frame (it was either acknowledged or
      // skipped). The application can now free any resources associated
      // with it.
    }
    // ...
  };
  ```

- **`Receiver::Consumer`**: The application implements the `Receiver::Consumer`
  interface to be notified when new media frames are ready for consumption.

  ```cpp
  class MyPlayer : public openscreen::cast::Receiver::Consumer {
   public:
    void OnFramesReady(int next_frame_buffer_size) override {
      // The Receiver has one or more frames ready.
      std::vector<uint8_t> buffer(next_frame_buffer_size);
      openscreen::cast::EncodedFrame frame =
          receiver_->ConsumeNextFrame(std::move(buffer));

      // Pass the frame data to the decoder.
      decoder_->Decode(frame.data);
    }
    // ...
  };
  ```

### Handling Media Frames

- **Sending Frames**: The sender application gets encoded frames from its media
  source (e.g., a hardware encoder) and passes them to the `Sender`.

  ```cpp
  // Sender-side example
  openscreen::cast::EncodedFrame frame;
  frame.frame_id = sender.GetNextFrameId();
  frame.dependency = is_key_frame ?
      openscreen::cast::EncodedFrame::Dependency::kKeyFrame :
      openscreen::cast::EncodedFrame::Dependency::kDependent;
  frame.rtp_timestamp = ...;
  frame.data = ...; // Pointer to encoded frame data

  sender.EnqueueFrame(frame);
  ```

- **Receiving Frames**: The `Receiver` collects incoming RTP packets and
  reassembles them. When a full frame is ready, it notifies the application via
  the `OnFramesReady()` callback on the `Receiver::Consumer`. The application
  can then call `ConsumeNextFrame()` to retrieve the frame data for decoding and
  playback.

## Advanced Topics

### A Note on Packet Routers

The `Sender` and `Receiver` classes do not interact directly with the network.
Instead, they are connected to the network via Packet Routers:

- [`SenderPacketRouter`](./impl/sender_packet_router.h): This class manages
  packet transmission for one or more `Sender`s. It paces outbound packets in
  bursts to optimize for network conditions (especially WiFi) and can be used to
  implement congestion control. `Sender`s are registered with the router and
  request transmission slots, rather than sending packets directly.

- [`ReceiverPacketRouter`](./impl/receiver_packet_router.h): This router is
  responsible for handling all incoming network traffic from a sender. It
  dispatches packets to the appropriate `Receiver` (e.g., for an audio or video
  stream) based on the SSRC of the packet.

In a typical application, you will have one packet router on each side of the
connection, managing all the `Sender` or `Receiver` instances for that
connection.

### RTP/RTCP Implementation

The RTP/RTCP implementation in `libcast` is a **custom, from-scratch
implementation** tailored specifically for the Cast protocol. It does not use
external third-party libraries for RTP/RTCP and does not explicitly reference
specific RFCs like RFC 3550. While it follows the spirit of these standards, it
is optimized for Cast use cases.

The core implementation of the RTP and RTCP protocols, including packet parsing,
frame construction, and feedback messaging, is located in the `impl`
sub-directory. The public API abstracts away most of these details, but
developers working on the core streaming logic may need to interact with these
components.

For those looking to understand the implementation, the best places to start are
probably the [`RtpPacketizer`](./impl/rtp_packetizer.h) and the
[`CompoundRtcpBuilder`](./impl/compound_rtcp_builder.h) classes.
