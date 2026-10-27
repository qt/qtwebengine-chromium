---
title: Protocol Types
draft: false
archetype: "chapter"
weight: 2
---

## Protocol Types

The protocol types are described as used in the [protocol specification](https://gitlab.freedesktop.org/libinput/libei/-/tree/main/proto/protocol.xml).

All types are encoded in the EIS implementation's native byte order.


| Type   | Bits  |Description             | C type     | Notes  |
| ------ | ----- | ---------------------- | ---------- | ------ |
| uint32 | 32    | unsigned integer       | `uint32_t` |        |
| int32  | 32    | signed integer         | `int32_t`  |        |
| uint64 | 64    | unsigned integer       | `uint64_t` |        |
| int64  | 64    | signed integer         | `int64_t`  |        |
| float  | 32    | IEEE-754 float         | `float`    |        |
| fd     | 0     | file descriptor        | `int`      | see [^1] |
| string | 32 + N| length + string        |  `int`, `char[]` | see [String Encoding]({{% ref "#string-encoding" %}}) |
| new_id | 64    | object id allocated by the caller | `uint64_t` | see [Object IDs]({{% ref "#object-ids" %}}) |
| object_id | 64    | previously allocated object id | `uint64_t` | see [Object IDs]({{% ref "#object-ids" %}}) |


[^1]: zero bytes in the message itself, transmitted in the overhead

### String encoding

Strings are encoded as a length-prefixed zero-terminated UTF-8 string.
Each string is prefixed by one 32-bit unsigned integer that specifies the length
of the string *including the trailing null byte*. Then follows the string itself,
zero-padded to the nearest multiple of 4 bytes.

A NULL-string is thus 4 bytes long in the protocol, with a length of zero and no string:
```
[0x00, 0x00, 0x00, 0x00]
```

The empty string (`""`) is 8 bytes long, with a length of 1 and 4 bytes of zeros for
the string null byte and 3 bytes of padding. Full (le) encoding:

```
[0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
```

The string "hello" is of length 6 but is zero-padded to 8 bytes and with the 4-byte
length field thus takes 12 bytes in total. Full (le) encoding:

```
[0x06, 0x00, 0x00, 0x00, 'h', 'e', 'l', 'l', 'o', '\0', '\0\, '\0']
```

### Object IDs

Object IDs are unique, monotonically increasing 64-bit integers that must not
be re-used by the client or the EIS implementation. Object IDs created by the
client start at 1 and must be less than `0xff00000000000000`. Object IDs allocated by
the EIS implementation start at and must not be less than `0xff00000000000000`.

Where a request or event specifies a `new_id` argument, it is the caller's responsibility
to create a new Object ID and map that to the corresponding object. For example, in the
`ei_handshake.connection` event, the EIS implementation will allocate an object in the
permitted range and send it to the client. Both client and EIS implementation must
refer to this object with this allocated ID. Once the object is destroyed, the ID is discarded
and must not be re-used.

## Special items in the protocol

### Interface names

Some requests and events, for example `ei_handshake.interface_version` require arguments
that represent interface names. Interface names on the wire are always in the
form "`ei_foo`", i.e. lowercase snake_case spelling, as used in the [protocol specification](https://gitlab.freedesktop.org/libinput/libei/-/tree/main/proto/protocol.xml).

### Serial numbers

Some events have a `serial` argument corresponding to a serial number assigned
by the EIS implementation. The serial number is a monotonically increasing
unsigned 32-bit number (that wraps, clients must handle this). Some requests
have a `last_serial` argument that clients must set to the  most recently seen
EIS serial number.

The serial number aims to help a client track failure cases and the EIS
implementation to detect errors caused by the protocol's asynchronicity.
For example, if the EIS implementation pauses a device at the serial number N and
subsequently receives an event with a `last_serial` of N-1, the EIS implementation
knows that the events were caused by the client lagging behind and are not
a protocol violation. The events can thus be discarded but the client does not
need to be disconnected.
