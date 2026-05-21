# Cast Protocol Flow Overview

## Introduction

The Cast protocol involves a sequence of interactions between a Sender device
and a Receiver device to establish a connection, launch an application, and
stream media. This document provides a high-level overview of this flow.

The key stages are:

1. **Discovery:** The Sender finds the Receiver on the local network.
2. **Connection Establishment:** A secure channel is created between the two
    devices.
3. **Application Launch/Session Establishment:** The Sender requests the
    Receiver to launch a specific application.
4. **Streaming Setup:** Media streaming parameters are negotiated.
5. **Media Streaming:** Media is transported from Sender to Receiver.
6. **Teardown:** The session is terminated.

## Sequence Diagram

The following diagram illustrates the typical message flow.

```ascii
+--------------+                                    +----------------+
| Sender Device|                                    | Receiver Device|
+--------------+                                    +----------------+
       |                                                    |
       |------------- 1. Discovery (mDNS) ----------------->|
       |          (sends mDNS query for Cast devices)       |
       |                                                    |
       |<--------------- (responds with mDNS record) -------|
       |                                                    |
       |------------ 2. Connection Establishment ---------->|
       |             (establishes TCP connection)           |
       |                                                    |
       |--------------- (TLS Handshake) ------------------->|
       |<-------------- (TLS Handshake) --------------------|
       |                                                    |
       |---------- (CastV2 Channel Handshake) ------------->|
       |           (Auth, Device Capabilities)              |
       |                                                    |
       |--------- 3. Application Launch ------------------->|
       |          (sends LAUNCH message)                    |
       |                                                +---|
       |                                     (launches app) |
       |                                                +-->|
       |<----------- (reports App status, session ID) ------|
       |                                                    |
       |------------ 4. Streaming Setup ------------------->|
       |             (sends OFFER message)                  |
       |                                                    |
       |<----------- (responds with ANSWER message) --------|
       |                                                    |
       |-------------- 5. Media Streaming ----------------->|
       |          (streams RTP/RTCP packets via UDP)        |
       |                                                    |
       |                                                    |
       |<-------------- (sends RTCP feedback) --------------|
       |                                                    |
       |------------ (Ongoing Control Messages) ----------->|
       |          (e.g., PAUSE)                             |
       |                                                    |
       |----------------- 6. Teardown --------------------->|
       |          (sends STOP message or disconnects)       |
       |                                                +---|
       |                                     (ends session) |
       |                                                +---|
```

## Key Messages and States

### Discovery

- The Sender broadcasts a Multicast DNS (mDNS) query for services of type
  `_googlecast._tcp.local`.
- Cast Receivers on the network respond with their IP address, port, and
  device information.

A minimal mDNS record for a Cast device, named `Living Room`, is shown below.
For more details on how this record is parsed, see
`DnsSdInstanceEndpointToReceiverInfo()` in
`cast/common/public/receiver_info.cc`.

```bash
_googlecast._tcp.local. 86400 IN PTR LivingRoom.local.
LivingRoom.local. 86400 IN SRV 0 0 8009 LivingRoom.local.
LivingRoom.local. 86400 IN A 192.168.1.100
LivingRoom.local. 86400 IN TXT "id=<unique_id>" "ve=02" "ca=5" "fn=Living Room" "st=0" "md=Chromecast"
```

### Connection Establishment

- The Sender establishes a TCP connection to the Receiver's IP address and
  port.
- A TLS handshake is performed to create a secure channel.
- The CastV2 protocol handshake occurs over this secure channel, where the
  sender authenticates that the receiver is an official Cast receiver device.

### Application Launch

- The Sender sends a `LAUNCH` message to the Receiver, requesting it to start
  a specific application (identified by an App ID).
- The Receiver launches the application (e.g., a web-based media player) in
  its own environment. Once ready, the Receiver informs the Sender of the
  application's status and provides a unique `sessionId`.

### Streaming Setup

- Using the Cast Streaming Control Protocol (CSCP), the Sender and Receiver
  negotiate streaming parameters via an `OFFER`/`ANSWER` exchange.
- This negotiation determines codecs, resolutions, bitrates, and the UDP ports
  to be used for media transport.

### Media Streaming

- The Sender begins encoding media and streaming it to the Receiver as a
  sequence of RTP (Real-time Transport Protocol) packets over UDP.
- The Receiver receives, decodes, and renders the media.
- RTCP (RTP Control Protocol) packets are exchanged periodically to provide
  feedback on stream quality, synchronization, and to handle packet loss.

### Teardown

- When the streaming session is finished, the Sender sends a `STOP` message to
  terminate the application on the Receiver, or simply closes the connection.
