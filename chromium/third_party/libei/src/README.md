libei
=====

**libei** is a library for Emulated Input, primarily aimed at the Wayland
stack. It provides three parts:
- 🥚 EI (Emulated Input) for the client side (`libei`)
- 🍦 EIS (Emulated Input Server) for the server side (`libeis`)
- 🚌 oeffis is an optional helper library for DBus communication with the
  XDG RemoteDesktop portal (`liboeffis`)

The communication between EI and EIS happens over a UNIX socket via a custom
binary protocol. See the [EI protocol documentation](https://libinput.pages.freedesktop.org/libei/)
for details.

For the purpose of this document, **libei** refers to the project,
`libei`/`libeis` to the libraries provided.

Documentation
-------------

The protocol documentation is available
[here](https://libinput.pages.freedesktop.org/libei/)

The C library API documentation is available here:
- [libei](https://libinput.pages.freedesktop.org/libei/api/group__libei.html)
- [libeis](https://libinput.pages.freedesktop.org/libei/api/group__libeis.html)
- [liboffis](https://libinput.pages.freedesktop.org/libei/api/group__liboeffis.html)

Overview
--------

In the Wayland stack, the EIS server component is part of the
compositor, the EI client component is part of the Wayland client.


```
    +--------------------+             +------------------+
    | Wayland compositor |---wayland---| Wayland client B |
    +--------------------+\            +------------------+
    | libinput | libeis  | \_wayland______
    +----------+---------+                \
        |          |           +-------+------------------+
 /dev/input/       +---brei----| libei | Wayland client A |
                               +-------+------------------+
```

The use-cases **libei** attempts to solve are:
- on-demand input device emulation, e.g. `xdotool` or more generically the
  XTEST extension
- input forwarding, e.g. `synergy`, for both client-emulated input as well as
  the forwarding of physical or logical devices.

**libei** provides three benefits:
- separation
- distinction
- control

**libei** provides **separation** of emulated input from normal input.
Emulated input is a distinct channel for the compositor and can thus be
handled accordingly. For example, the compositor may show a warning sign in
the task bar while emulated input is active.

The second benefit is **distinction**. Each **libei** client has its own
input device set, the server is always aware of which client is requesting
input at any time. It is possible for the server to treat input from
different emulated input devices differently.

The server is in **control** of emulated input - it can filter input or
discard at will. For example, if the current focus window is a password
prompt, the server can simply discard any emulated input. If the screen is
locked, the server can pause all emulated input devices.

Sender vs receiver contexts
---------------------------

As of version 0.3, libei allows a ``libei`` context to be either a sender or a
receiver. In the "sender" mode, the ``libei`` context gets a list of devices
from the EIS implementation and can emulate events on these devices. The
devices usually represent virtual devices, e.g. a generic relative pointer
corresponding to the cursor or per-screen absolute input devices.

In the "receiver" mode, the ``libei`` context gets a list of devices from the
EIS implementation and *receives* events from those. This allows for input
capture, provided the EIS implementation supports it. The devices can
correspond to virtual devices, e.g. a generic relative pointer corresponding
to the cursor. Or they may be representations of physical devices, e.g. a
tablet device getting forwarded.

A ``libei`` context may only be in one mode. It is up to the EIS
implementation to accept a sender or receiver ``libei`` context.

Why not $foo?
-------------

We start from the baseline of: "there is no emulated input in Wayland (the
protocol)".

There is emulated input in X through XTEST but it provides neither
separation, distinction nor control in a useful manner. There are however
many X clients that require XTEST to work.

There are several suggestions that overlap with **libei**, with the main
proposals being:
- a Wayland protocol for virtual input
- a (compositor-specific) DBus interface for virtual input

Emulated input is not specifically Wayland-y. Clients that emulate input
generally don't care about Wayland itself. It's not needed to emulate
events on their own surfaces and Wayland does not provide global state. The
only connection to Wayland is merely that input events are *received*
through the Wayland protocol. So a Wayland protocol for emulating input is
not a great fit, it merely ticks the convenient box of "we already have IPC
through the wayland protocol, why not just do it there".

DBus is the most prevalent generic IPC channel on the Linux desktop but it's
not available in some compositors. Any other specific side-channel requires
an IPC mechanism to be implemented in the sender and receiver.

The current situation looks like that neither proposal will be universally
available. Wayland clients (including Xwayland) would need to support any
combination of methods.

**libei** side-steps this issue by making the *communication* itself a
an implementation detail and providing different *negotiation* backends.
A client can attempt to establish a **libei** context through a Flatpak
Portal first and fall back onto a public DBus interface and then fall back
onto e.g. a named UNIX socket. All with a few lines of code only. There is
only one spot the client has to care about this, the actual emulation of input
is identical regardless of backend.

High-level summary
------------------

Simple demo implementations for server and client are available in
the [`tools/`](https://gitlab.freedesktop.org/libinput/libei/-/tree/main/tools)
directory.

The server starts a `libeis` context (which can be integrated with flatpak
portals) and uses the `libeis` file descriptor to monitor for
client requests.

A client starts a `libei` context and connects to the server - either
directly, via DBus or via a portal. The server (or the portal) approves or
denies the client. After successful authentication the server sends one or
more seats (a logical group of devices) to the client; the client can
request the creation of devices with capabilities `pointer`, `keyboard` or
`touch`, etc. in those seats.

The client triggers input events on these devices, the server receives those
as events through `libeis` and can forward them as if they were regular input
events. The server has control of the client stream. If the stream is
paused, events from the client are discarded. If the stream is resumed, the
server will receive the events (but may discard them anyway depending on
local state).

The above caters for the `xdotool` use-case.


For a `synergy` use-case, the setup requires:
- `synergy-client` on host A capturing mouse and keyboard events via an
  unspecified protocol
- `synergy-server` on host B requesting a mouse/keyboard capability device
  from the compositor
- when `synergy-client` receives events via from compositor A it
  forwards those to the remote `synergy-server` which sends them via `libei`
  to the compositor B.

**libei** does not provide a method for deciding when events should be
captured, it merely provides the transport layer for events once that decision
has been made.

Differences between XTest vs libei
----------------------------------

**libei** functionality is a superset of XTest's input emulation which
consists of a single request, `XTestFakeInput`. This request allows
emulation of button, key and motion events, including X Input 1.x events
(but not XI2). So **libei** can be a drop-in replacement since it supports
the same functionality and more.

However, XTest is an X protocol extension and users of XTest usually obtain
more information out-of-band ("out-of-band" here means "not through XTest
but instead other X protocol requests").

One example is `xdotool` which does window focus and modifier mangling (see
below). Window focus notification is not available to a pure **libei**
client and would have to be obtained or handled on a separate channel, e.g.
X or Wayland. Having said that, a Wayland client does not usually have acess
to query or modifiy the window focus.

Modifiers in `xdotool` are handled by obtaining the modifier mask from the X
server, identifying any difference to the intended mask and emulating key
events to change the modifier state to the intended one. For example, if
capslock is on, xdotool would send a capslock key event first (thus
disabling capslock) and then the actual key sequence. This is followed by
another capslock key event to restore the modifier mask.

This is not possible for a pure **libei** client as the modifier state is
maintained by the windowing system (if any). A client can obtain the
modifier state on Wayland on `wl_keyboard.enter` but when the client is
in-focus, there is rarely a need to emulate events.

Overall, it is best to think of **libei** devices as virtual equivalents to
a hardware device.

Open questions
--------------

### Flatpak integration

Where flatpak portals are in use, `libei` can communicate with
the portal through a custom backend. The above diagram modified for
Flatpak would be:

```
    +--------------------+
    | Wayland compositor |_
    +--------------------+  \
    | libinput | libeis  |   \_wayland______
    +----------+---------+                  \
        |     [eis-0.socket]                 \
 /dev/input/     /   \\       +-------+------------------+
                |      ======>| libei | Wayland client A |
                |      after    +-------+------------------+
         initial|     handover   /
      connection|               / initial request
                |              /  dbus[org.freedesktop.portal.RemoteDesktop.ConnectToEIS]
        +--------------------+
        | xdg-desktop-portal |
        +--------------------+
```

The current approach works so that
- the compositor starts an `libeis` socket backend at `$XDG_RUNTIME_DIR/eis-0`
- `xdg-desktop-portal` provides `org.freedesktop.portal.RemoteDesktop.ConnectToEIS`
- a client connects to the `xdg-desktop-portal` to request emulated input
- `xdg-desktop-portal` authenticates a client and opens the initial
  connection to the `libeis` socket.
- `xdg-desktop-portal` hands over the file descriptor to the client which
  can initialize a `libei` context
- from then on, `libei` and `libeis` talk directly to each other, the portal
  has no further influence.

This describes the **current** implementation. Changes to this approach are
likely, e.g. the portal **may** control pauseing/resuming devices (in addition to the
server). The UI for this is not yet sorted.

### Authentication

Sandboxing is addressed via flatpak portals but a further level is likely
desirable, esp. outside flatpak. The simplest solution is the client
announcing the name so the UI can be adjusted accordingly. API wise-maybe an
opaque key/value system so the exact auth can be left to the implementation.

### Capability monitoring

For the use-case of fowarding input (e.g. `synergy`) we need a method of
capturing input as well as forwarding input. An initial idea was for
**libei** to provide capability monitoring, i.e. a client requests all
events from a specific capability. As with input emulation same benefits
would apply - input can only be forwarded if the compositor explicitly does so.

However, this fails in the details. For example, for `synergy` we need
capability monitoring started by triggers, e.g. the client requests a
pointer capability monitoring when the real pointer hits
the screen edge. Or in response to a keyboard shortcut.

Some of the capabilities are distinctively display server-specific, for
example the concept of a seat and a device is different between X and
Wayland.

At this point, no implementation of capability monitoring is planned for
**libei**.

### Keyboard layouts

The emulated input may require a specific keyboard layout, for example
for softtokens (usually: constant layout "us") or for the `synergy` case
where the remote keyboard should have the same keymap as the local one, even
where the remote host is configured otherwise.

In **libei**, the server informs the client about the keymap it expects and it
is up to the client to provide the correct keyboard events.

Modifier state handling, group handling, etc. is still a private
implementation so even where the server supports individual keymaps. So it
remains to be seen if this approach is sufficient.

### Xwayland and XTEST

There are PoC implementations of using `libei` within Xwayland and
connecting it to a `libeis` context in the compositor (PoC with Weston).
This allows Xwayland to intercept XTEST events and route those through
the compositor instead.

```
    +--------------------+             +------------------+
    | Wayland compositor |---wayland---| Wayland client B |
    +--------------------+\            +------------------+
    | libinput | libeis  | \_wayland______
    +----------+---------+                \
        |          |           +-------+------------------+
 /dev/input/       +---brei----| libei |     Xwayland     |
                               +-------+------------------+
                                                |
                                                | XTEST
                                                |
                                         +-----------+
                                         |  X client |
                                         +-----------+
```
Of course, Xwayland is just another Wayland client, so the connection
between libei and libeis could be handled through a portal.

### Short-lived applications

**libei** is not designed for short-lived fire-and-forget-type applications
like `xdotool`. It provides context and device negotiation between the
server and the client - the latter must be able to adjust to limitations the
server imposes.

The current implementation of the protocol does not allow for a `libei` client
to connect, send all requests in bulk and exit. The decision on which devices
are available is made by the EIS implementation and that requires the `libei`
client to wait until such devices are available. On a technical level the
protocol is object-oriented and requests cannot be sent until the respective
device object is available.

The duration until devices become available is non-deterministic, and so is
what types of devices are available to a client. For **libei** to support a
connect-send-exit approach, it would still require the client process to stay
alive until device negotiation is complete within libei and all events have
been sent. And since the client process must stay alive, we might as well
have the device negotiation handled in the caller.

### uinput vs libei

uinput is a Linux kernel module that allows creating
`/dev/input/event`-compatible devices. Unlike XTest it is independent of a
windowing system but requires write access to `/dev/uinput`, usually limited
to root. uinput devices are effectively identical to physical devices and
will thus work on the tty and in any windowing system.

From the POV of a ``libei`` client, uinput is a server implementation
detail. The client does not need to know that the manager employs uinput to
create the devices, it merely connects to an available EIS instance.

```
    +---------+
    | server  |___auth channel__________
    +---------+                         \
    | libeis  |-             +---------------------+
    +---------+  \____brei___| libei | application |
        |                    +---------------------+
 /dev/uinput/
```

Note that the server would likely need some authentication channel
to verify which client is allowed to emulate input devices at the
kernel level (polkit? config files?). This however is out of scope for **libei**.
An example uinput server is implemented in the `eis-server-demo` in the
with libei repository.

# liboeffis

In a Wayland and/or sandboxed environment, emulating input events requires
going through the XDG RemoteDesktop portal. This portal is available on DBus
but communication with DBus is often cumbersome, especially for small tools.

`liboeffis` is an optional library that provides the communication with the
portal, sufficient to start a RemoteDesktop session and retrieve the file
descriptor to the EIS implementation.
