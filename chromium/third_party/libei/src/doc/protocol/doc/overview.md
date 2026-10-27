---
title: Protocol Overview
draft: false
archetype: "chapter"
weight: 1
---

## Protocol Components

The protocol is designed to connect two processes over a UNIX socket - an ei
client and an EIS implementation (typically a Wayland compositor).

{{< mermaid >}}
flowchart LR;
    subgraph EIS implementation
      socket[[ei.socket]]
    end
    c1[ei client 1] -- ei protocol --> socket
    c2[ei client 2] -- ei protocol --> socket
{{< /mermaid >}}

The protocol is asynchronous and object-oriented. Each object on the wire supports
zero or more **requests** and zero or more **events**. Requests are messages
sent from an ei client to an EIS implementation, events are messages sent from
the EIS implementation to the client.

{{< mermaid >}}
flowchart LR;
    ei -- request --> eis
    eis -- event --> ei
{{< /mermaid >}}

Objects are identified by a unique object ID, assigned at creation of the object.
The type of an object is defined by its [interface]({{% relref "interfaces" %}})
and agreed on at object creation. Each object has exactly one interface, but
there may be multiple objects with that interface. For example, a compositor
may create multiple objects with the [`ei_device`]({{% relref "interfaces/ei_device" %}})
interface.

All data on the protocol (e.g. object IDs) is private to that client's
connection.

The ei protocol is modelled closely after the Wayland protocol, but it is not
binary compatible.

## Wire Format

The wire format consists of a 3-element header comprising the `object-id` of
the object, the length of the message and the opcode representing the message
itself.

```
byte:     |0         |4         |8        |12        |16
content:  |object-id            |length   |opcode    |...
```

Where:
- `object-id` is one 64-bit unsigned integer that uniquely identifies
  the object sending the request/event. The `object-id`
  0 is reserved for the special [`ei_handshake`]({{% relref "interfaces/ei_handshake" %}}) object.
- `length` is a 32-bit integer that specifies the length of the message in
  bytes, including the 16 header bytes for `object-id`, `length` and `opcode`.
- `opcode` is a 32-bit integer that specifies the event or request-specific
  opcode, starting at 0. Requests and events have overlapping opcode ranges,
  i.e. the first request and the first event both have opcode 0.

The header is followed by the message-specific arguments (if any). All
arguments are 4 bytes or padded to a multiple of 4 bytes.

All integers are in the EIS implementation's native byte order.

## Version negotiation

For objects to be created, the EIS implementation and the client must agree on a supported
version for each object. This agreement happens during the initial setup in `ei_handshake`
- the client notifies the EIS implementation of the highest supported version for an interface,
  e.g. in the `ei_handshake.interface_version` request
- the EIS implementation responds by selecting the highest version the EIS
  implementation supports but not higher than the client version. It **may** notify the
  client of that version before `ei_handshake.connection`.

An exception to this is the `ei_handshake.handshake_version` request and event where the
EIS implementation initializes the version exchange and thus the client picks the version number.

In both cases, the version number used is simply `v = min(eis_version, client_version)`.

Whenever an object is created, the version number of that object must be sent in the corresponding
request or event.
