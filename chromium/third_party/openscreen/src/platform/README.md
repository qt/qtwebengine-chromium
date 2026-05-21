# Platform API

The Platform API is designed to allow easy porting of the Open Screen Library
between platforms. This folder contains classes and logic used by Open Screen to
access system resources and other common OS-level facilities, such as the clock
and task runner.

*** aside
Note: utilities are generally not homed here, instead
preferring the top level util/ directory. New classes added here must NOT depend
on any other folders in the openscreen repository, excepting third party
libraries like Abseil or GTest. We include "default implementations" of many
platform features.
***

## Directory structure

- api/ - contains the Platform API which is used by the Open Screen Library.
  Some of the public API may also include adapter code that acts as a small shim
  above the C functions to be implemented by the platform. The entire Open
  Screen repository can depend on files from api/, though classes defined in
  api/ can only depend on third_party or platform/base files.

- base/ - contains declarations/definitions of API constructs. Classes homed
  here shall have no library dependencies.

- impl/ - contains a default implementation of the platform API for building
  self-contained binaries and tests in libcast.

- test/ - contains API implementations used for testing purposes, like the
  FakeClock or FakeTaskRunner.

*** aside
Note: people familiar with the old layout may notice that all files from the
`posix/`, `linux/`, and `mac/` directories have been moved under `impl/` with
an OS-specific suffix (e.g. `_mac`, `_posix`).
***

*** note
You may use the platform implementation in impl/ in your own application, but it
has not been designed for high performance and you may find performance improves
by implementing the platform API specifically for your own platform / hardware.
***
