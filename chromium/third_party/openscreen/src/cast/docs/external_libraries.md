# Standalone Sender and Receiver Dependencies

Currently, external libraries are used exclusively by the standalone sender and
receiver applications, for compiling in dependencies used for video decoding and
playback.

The decision to link external libraries is made manually by setting the GN args.

> NOTE: The build currently defaults to using a sysroot on some platforms, such
> as Linux. You will likely find it helpful to disable the `use_sysroot` GN arg
> in order to properly use system libraries and avoid linker issues.

For example, a developer wanting to link the necessary libraries for the
standalone sender and receiver executables might add the following to `gn args
out/Default`:

```python
is_debug=true
have_ffmpeg=true
have_libsdl2=true
have_libopus=true
have_libvpx=true
use_sysroot=false
```

Or on the command line as:

```bash
gn gen --args="is_debug=true have_ffmpeg=true have_libsdl2=true have_libopus=true have_libvpx=true use_sysroot=false" out/Default
```

## Linux

As of ==December 22nd, 2025==, the below command installs all of the needed
external libraries on gLinux Rodete. Your mileage may vary on other Debian
based OSes.

```sh
sudo apt install libsdl2-2.0-0 libsdl2-dev libavcodec61 libavcodec-dev libavformat61 libavformat-dev libavutil59 libavutil-dev libswresample5 libswresample-dev libopus0 libopus-dev libvpx11 libvpx-dev
```

Note: release of these operating systems may require slightly different
packages, so these `sh` commands are merely a potential starting point.

Also note that generally the headers for packages must also be installed.
In Debian Linux flavors, this usually means that the `*-dev` version of each
package must also be installed. In the example above, this looks like having
both `libavcodec61` and `libavcodec-dev`.

Finally, sometimes header resolution can fail. If that occurs for you, please
specific the header include dirs. For example, if your build complains about
`SDL2/SDL.h` and related headers missing, the GN argument to fix it may look
something like this (adjusted for your specific system):

```sh
libsdl2_include_dirs = [ "/usr/include", "/usr/include/x64_64-linux-gnu" ]
```

A more extensive example is provided in the below
[Library specific include paths](#library-specific-include-paths) section. For a
full list of potential GN arguments, see
[external_libaries.gni](../streaming/external_libraries.gni).

## MacOS (Homebrew)

You can use [Homebrew](https://brew.sh/) to install the libraries needed to compile the
standalone sender and receiver applications.

```sh
brew install ffmpeg sdl2 opus libvpx aom
```

To compile and link against these libraries, set the path arguments as follows
in your `gn args`.

### Library specific include paths

**Important**: Using Homebrew's top-level include directory
(`/opt/homebrew/include`) can cause header conflicts with the project's internal
BoringSSL. You must use the specific include paths for each library from
Homebrew's `Cellar` directory.

Homebrew libraries are often built for the latest version of macOS. You may need
to set the `mac_deployment_target` to match the version of macOS you are running
to avoid linker errors. For example, on macOS Sonoma (version 14):

You will need to replace the `<version>` placeholders with the actual versions
installed on your system. You can find these in `/opt/homebrew/Cellar/`.

```python
mac_deployment_target="14.0"

have_ffmpeg=true
have_libsdl2=true
have_libopus=true
have_libvpx=true
have_libaom=true

# Homebrew on Apple Silicon installs to /opt/homebrew.
# On Intel macs, it's /usr/local.
external_lib_dirs=["/opt/homebrew/lib"]

ffmpeg_include_dirs=["/opt/homebrew/Cellar/ffmpeg/<version>/include"]
libsdl2_include_dirs=["/opt/homebrew/Cellar/sdl2/<version>/include"]
libopus_include_dirs=["/opt/homebrew/Cellar/opus/<version>/include"]
libvpx_include_dirs=["/opt/homebrew/Cellar/libvpx/<version>/include"]
libaom_include_dirs=["/opt/homebrew/Cellar/aom/<version>/include"]
```

## libaom

For AV1 support, it is advised that most Linux users compile and install
`libaom` from source, using the instructions at
https://aomedia.googlesource.com/aom/ Older versions found in many package
management systems are not compatible with the Open Screen Library because of
API compatibility and performance issues.

To to enable AV1 support, also add the following to your GN args:

```python
have_libaom=true
```

Note that AV1 support is configured separately from the other standalone
libraries and the `have_libaom` flag is not necessary to run the standalone
demo.

Similar to other libraries, you may need to set `libaom_include_dirs` to the
location of the libaom header files and `libaom_lib_dirs` to the location of
the linkable libaom libraries.

## Standalone Sender

The standalone sender uses `ffmpeg`, `libopus`, and `libvpx` for encoding video
and audio for sending. When the build has determined that
[have_external_libs](../standalone_sender/BUILD.gn) is set to true, meaning that
all of these libraries are installed, then the VP8 and Opus encoders are enabled
and actual video files can be sent to standalone receiver instances. Without
these dependencies, the standalone sender cannot properly function (contrasted
with the standalone receiver, which can use a dummy player).

## Standalone Receiver

The standalone receiver also uses `ffmpeg`, for decoding the video stream
encoded by the sender, and also uses `libsdl2` to create a surface for decoding
video.  Unlike the sender, the standalone receiver can work without having its
[have_external_libs](../standalone_receiver/BUILD.gn) set to true, through the
use of its [Dummy Player](../standalone_receiver/dummy_player.h) that does not
perform any actual decoding or playback.
