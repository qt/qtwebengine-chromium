# Differentiated Services Code Point (DSCP) for Cast Streaming

This document describes how to use Differentiated Services Code Point (DSCP)
support in the Open Screen Library.

## Overview

DSCP is a mechanism for classifying network traffic to provide Quality of
Service (QoS). By marking packets with a DSCP value, network routers can
prioritize traffic, which is useful for latency-sensitive applications like
video streaming.

The Open Screen Library supports setting DSCP values on RTP and RTCP packets for
Cast Streaming sessions. This is based on recommendations from [RFC
8837](https://datatracker.ietf.org/doc/html/rfc8837), where video streams are
marked with `AF41` and audio-only streams with `EF`.

DSCP may improve performance in some network environments, as long as the DSCP
field is not stripped by the operating system or router. This is in line with
other network streaming implementations, like WebRTC's
[RTCRtpEncodingParameters](https://www.w3.org/TR/webrtc/#dom-rtcrtpencodingparameters)
concept of `priority` and `networkPriority`.

DSCP support in libcast is part of a larger effort to prioritize Cast traffic
on congested networks and improve performance even in the worst conditions
for streaming.

## How to Enable DSCP

DSCP is disabled by default. To enable it, set the `enable_dscp` flag to `true`
in the session configuration for both the sender and the receiver.

- **Sender:** Set `enable_dscp` in `SenderSession::Configuration`.
- **Receiver:** Set `enable_dscp` in `ReceiverConstraints`.

If both the sender and receiver have DSCP enabled, the sender session will
set the spec-recommended DSCP value in the OFFER message, and then both
the sender and receiver should set the DSCP value on their respective
UdpSockets.

## Setting DSCP Values

The `UdpSocket::SetDscp()` method is used to set the DSCP value for a socket. It
takes a `DscpMode` enum value, backed by a `uint8_t` that is valid for values
of 0-63.

```cpp
void UdpSocket::SetDscp(DscpMode mode));
```

While any valid 6-bit integer can be passed by using a `static_cast`, the
`UdpSocket::DscpMode` enum in `platform/api/udp_socket.h` provides a convenient
way to use some of the more common DSCP values:

```cpp
enum class UdpSocket::DscpMode : uint8_t {
  kBestEffort = 0,
  kAF11 = 10,
  // ...
  kEF = 46
};

// Example usage:
// Non-standard DSCP value.
socket->SetDscp(static_cast<UdpSocket::DscpMode>(55));

// Standard value.
socket->SetDscp(UdpSocket::DscpMode::kEF);
```
