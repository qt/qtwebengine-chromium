# libcast

libcast is an open source implementation of the Cast protocols that allow Cast
senders to launch Cast applications and stream real-time media to
Cast-compatible devices (aka "receivers").

Included are two applications, `cast_sender` and `cast_receiver` that
demonstrate how to send and receive media using a Cast Streaming session.

## Components

Libcast is roughly broken into components by folder, with a non-exhaustive list
of the most important listed here:

* [streaming/](streaming/README.md) - Cast Streaming (both sending and receiving
  media).

* [receiver/public/](receiver/public/README.md) - Cast server socket and a
  demonstration server (agent).

* [sender/public/](sender/public/README.md) - Cast client socket and supporting
  APIs to launch Cast applications.

* standalone_receiver/ - A reference implementation of a receiver application.

* standalone_sender/ - A reference implementation of a sender application.

* [test/](test/README.md) - Integration tests.

With all of the documentation for this implementation in the [docs](docs/)
folder. See the [architecture.md](docs/architecture.md) as a potential jumping
off point.

*** aside
The `streaming` module can be used independently of the `sender` and `receiver`
modules.
***
