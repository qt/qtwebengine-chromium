---
title: EI Protocol documentation
draft: false
archetype: "home"
alwaysopen: true
---

**libei** is a library for Emulated Input, primarily aimed at the Wayland
stack. It uses a typical client/server separation, with the two parts connected
via a UNIX socket. In libei parlance, the client-side is called **"EI client"**, the
server side, typically a Wayland compositor, is called the **"EIS Implementation"**
(Emulated Input Server). These terms are used throughout this documentation.

This documentation details the protocol to communicate between the client side
and the EIS implementation.

A typical Compositor setup using the `libei` and `libeis` [C libraries]({{% relref "libraries" %}}) looks like this:

{{< mermaid >}}
flowchart LR;
    libwayland-server --> c1
    libwayland-server --> c2
    /dev/input/event0 ---> libinput
    /dev/input/event1 ---> libinput
    libei -.-> libeis
    libinput --> inputstack
    inputstack --> libwayland-server
    libeis -.-> inputstack
    subgraph Kernel
      /dev/input/event0
      /dev/input/event1
    end
    subgraph Wayland Compositor
      libwayland-server
      inputstack[input stack]
      libinput
      libeis
    end
    subgraph EI client
      libei
    end
    subgraph Wayland client A
      c1[libwayland-client]
    end
    subgraph Wayland client B
      c2[libwayland-client]
    end
{{< /mermaid >}}

Note how the EI client is roughly equivalent to a physical input device coming
from the kernel and its events feed into the normal input stack.
However, the events are distinguishable inside the compositor to allow for
fine-grained access control on which events may be emulated and when emulation is
permitted.

Events from the EIS implementation would usually feed into the input stack in the
same way as input events from physical devices. To Wayland clients, they are
indistinguishable from real devices.

The EI client may be a Wayland client itself.

## EI Protocol

The ei protocol is a public protocol that may be used directly by clients or
EIS implementations. This documentation describes the protocol, its interfaces
and how to generate language bindings.

If you are looking for easy-to-use C libraries instead, see:

- 🥚 [libei](https://libinput.pages.freedesktop.org/libei/api/group__libei.html) for the client side
- 🍦 [libeis](https://libinput.pages.freedesktop.org/libei/api/group__libeis.html) for the EIS implementation side
- 🚌 [liboeffis](https://libinput.pages.freedesktop.org/libei/api/group__liboeffis.html) is an helper library for DBus communication with the
  XDG RemoteDesktop portal (`liboeffis`)


# Documentation

{{% children  %}}
