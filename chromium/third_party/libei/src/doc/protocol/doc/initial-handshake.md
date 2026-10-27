---
title: Initial Connection Handshake
draft: false
archetype: "chapter"
weight: 4
---

The initial connection is a two-step process:
An [ei_handshake]({{% relref "interfaces/ei_handshake" %}}) object with the special ID 0 is guaranteed to
exists. The client must send the appropriate requests to set up
its connection, followed by the `ei_handshake.finish` request. The EIS
implementation replies by creating the [ei_connection]({{% relref "interfaces/ei_connection" %}}) object with the
client-requested version (or any lower version) that is the connection for the
remainder of this client (see [version negotiation]({{% relref "doc/specification#version-negotiation" %}}).

Immediately after connecting, the EIS implementation must send the
`ei_handshake.handshake_version` event. The client replies with the
`ei_handshake.handshake_version` request to negotiate the version
of the `ei_handshake` object.

Version negotiation for other interfaces is also handled in the `ei_handshake`
object. The client announces which interfaces it supports and their
respective version, the EIS implementation should subsequently notify
the client about the interface version it supports.

Any object created by either the client or the EIS implementation must
use the lower of client and EIS implementation supported version.

The last message sent on the `ei_handshake` object is the `ei_handshake.connection` event.
This event carries the object ID and version for the newly created `ei_connection` object.
This object remains for the lifetime of the client is only destroyed when the client disconnects.

A full example sequence from socket connection to the first event sent by the client is below:

{{< mermaid >}}
sequenceDiagram
    participant client
    participant EIS

    client->>EIS: connect to socket

    Note over client, EIS: ei_handshake object 0

    EIS-->>client: ei_handshake.handshake_version(N)
    client->>EIS: ei_handshake.handshake_version(M)
    client->>EIS: ei_handshake.context_type(sender)
    client->>EIS: ei_handshake.name(some client)
    client->>EIS: ei_handshake.interface_version(ei_connection)
    client->>EIS: ei_handshake.interface_version(ei_callback)
    client->>EIS: ei_handshake.interface_version(ei_seat)
    client->>EIS: ei_handshake.interface_version(ei_device)
    client->>EIS: ei_handshake.interface_version(ei_pointer)
    client->>EIS: ei_handshake.interface_version(ei_keyboard)
    client->>EIS: ei_handshake.finish()

    EIS-->>client: ei_handshake.interface_version(ei_callback)
    EIS-->>client: ei_handshake.connection(new_id, version)

    Note over client, EIS: ei_handshake object is destroyed

    EIS-->>client: ei_connection.seat(new_id, version)
    EIS-->>client: ei_seat.name(some seat)
    EIS-->>client: ei_seat.capabilities()
    EIS-->>client: ei_seat.done()
    client->>EIS: ei_seat.bind()
    EIS-->>client: ei_seat.device(new_id, version)
    EIS-->>client: ei_device.name(some name)
    EIS-->>client: ei_device.device_type()
    EIS-->>client: ei_device.capabilities(some name)
    EIS-->>client: ei_device.interface(new_id, "ei_pointer", version)
    EIS-->>client: ei_device.interface(new_id, "ei_keyboard", version)
    EIS-->>client: ei_seat.device(new_id, version)
    EIS-->>client: ei_device.name(some name)
    EIS-->>client: ei_device.device_type()
    EIS-->>client: ei_device.capabilities(some name)
    EIS-->>client: ei_device.region()
    EIS-->>client: ei_device.interface(new_id, "ei_touchscreen", version)

    EIS-->>client: ei_device.resume()
    EIS-->>client: ei_device.resume()

    Note over client, EIS: client may emulate now

    client->>EIS: ei_pointer.start_emulating()
    client->>EIS: ei_pointer.motion()
    client->>EIS: ei_pointer.frame()
{{< /mermaid >}}
