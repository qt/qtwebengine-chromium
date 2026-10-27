---
title: "C libraries"
draft: false
archetype: "home"
alwaysopen: true
weight: 3
---

The libei project provides several C libraries that abstract the protocol and provide an easy-to-use C API
to interact with the respective EI counterpart.

## libei - the client library

This library is intended for use by EI clients. This library should be used by processes
that need to emulate devices or processes that need to receive input events from logical devices.

The interface mirrors the [libinput API](https://gitlab.freedesktop.org/libinput/libinput/).
A demo client is available in the [libei repository](https://gitlab.freedesktop.org/libinput/libei/-/blob/main/tools/ei-demo-client.c).

The C library API documentation for libei is [here](https://libinput.pages.freedesktop.org/libei/api/group__libei.html).

## libeis - the server library

This library is intended for use by EIS implementations. This library should be used by processes
that have control over input devices, e.g. Wayland compositors.

The interface mirrors the [libinput API](https://gitlab.freedesktop.org/libinput/libinput/).
A demo server is available in the [libei repository](https://gitlab.freedesktop.org/libinput/libei/-/blob/main/tools/eis-demo-server.c).

The C library API documentation for libeis is [here](https://libinput.pages.freedesktop.org/libei/api/group__libeis.html)


## liboeffis - DBus helper library

This library is a helper library for applications that do not want to or cannot
interact with the [XDG RemoteDesktop DBus portal](https://github.com/flatpak/xdg-desktop-portal)
directly.

liboeffis will:
- connect to the DBus session bus and the ``org.freedesktop.portal.Desktop`` bus name
- Start a ``org.freedesktop.portal.RemoteDesktop`` session, select the devices and invoke
  ``RemoteDesktop.ConnectToEIS()``
- Provide the returned file descriptor to the caller. This fd can be used by libei to initialize a context.
- Close everything in case of error or disconnection

The below diagram shows the simplified process:

{{< mermaid >}}
sequenceDiagram
  participant libei
  participant liboeffis
  participant portal as XDG Desktop Portal
  participant eis as EIS implementation

  liboeffis ->> portal: CreateSession
  portal ->> eis: CreateSession
  eis -->> portal: fd
  portal -->> liboeffis: fd

  liboeffis -->> libei: take fd for libei context

  libei ->> eis: setup connection
  libei ->> eis: emulate events
{{< /mermaid >}}

liboeffis is intentionally kept simple, any more complex needs should be
handled by an application talking to DBus directly.

A demo tool is available in the [libei repository](https://gitlab.freedesktop.org/libinput/libei/-/blob/main/tools/oeffis-demo-tool.c).

The C library API documentation for liboeffis is [here](https://libinput.pages.freedesktop.org/libei/api/group__liboeffis.html).
