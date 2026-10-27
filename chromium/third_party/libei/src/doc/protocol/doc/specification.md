---
title: Protocol Specification
draft: false
archetype: "chapter"
weight: 3
---

The current protocol specification is available [in XML format here](https://gitlab.freedesktop.org/libinput/libei/-/tree/main/proto/protocol.xml) and the corresponding [XML DTD here](https://gitlab.freedesktop.org/libinput/libei/-/tree/main/proto/protocol.dtd).


In that protocol specification:
- a request or event with the XML attribute `type="destructor"` marks the message as [destructor]({{% relref "#destructors" %}}).
- an argument with an XML attribute `enum` carries a value of the corresponding enum
- an argument with an XML attribute `interface` attribute indicates that an object in the same message is
  of that interface type
- an argument with an XML attribute `interface_arg` attribute indicates that an object in the same message is
  of the interface given in the named argument.
- an enum with an XML attribute `bitfield` indicates a single-bit value.
- each request, event or enum has an XML attribute `since` to indicate the
  interface version this request, event or enum was introduced in
- each interface has an XML attribute `version` indicate the current version of this interface

## Protocol violations

The term "Protocol Violation" is used to indicate that the client or the EIS
implementation have sent a message that is not allowed by the protocol specification. A protocol
violation must result in immediate disconnection. In the case of the EIS implementation it
is permitted to send the `ei_connection.disconnect` event to the client to notify them of the
protocol violation.

## Destructors

A request or event marked as destructor causes the object to be destroyed by the sender
immediately after sending the request or event. On the receiving side the object must thus be
treated as defunct and no further interaction with the object is permitted.

Note that due to the asynchronous nature of the protocol, some messages may
have been sent before the destructor message was received. Both client and EIS implementation
must handle this case gracefully. In the EIS implementation for example,
it is usually enough to reply with the `ei_connection.invalid_object` event.
