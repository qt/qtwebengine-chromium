# Using the Standalone Sender and Receiver

## 1. Configure your machine

First, follow the setup steps in the root [README.md](../../README.md).

If you want to actually play back content on the Open Screen receiver, you
must make sure the FFmpeg and libSDL2 dependencies are properly configured.
Otherwise, the dummy player will be used and you will only get log statements.

If you want to use the cast sender binary at *all*, `libopus` and `libvpx` at
minimum are required to encode frames.

Instructions for setting up these dependencies are provided in
[external_libraries.md](external_libraries.md).

## 2. Build your binaries

Once your GN configuration is happy, you just need to compile your binaries.
For example, if you used `out/Default` as your build directory:

```sh
autoninja -C out/Default cast_sender cast_receiver
```

> Both the `cast_sender` and `cast_receiver` binaries have reasonably thorough
> help documentation available by invoking them with the `-h` flag.

## 3. Generate a developer certificate

To use the sender and receiver application together, a valid Cast certificate is
required. The easiest way to generate the certificate is to just run the
`cast_receiver` with `-g`, and both should be written out to files:

```sh
$ ./out/Default/cast_receiver -g
    [INFO:../../cast/receiver/channel/static_credentials.cc(161):T0] Generated new private key for session: ./generated_root_cast_receiver.key
    [INFO:../../cast/receiver/channel/static_credentials.cc(169):T0] Generated new root certificate for session: ./generated_root_cast_receiver.crt
```

## 4. Start a `cast_receiver` session

After you have a successful build working, next step is to start a test receiver
session.

First, you need to find a valid interface address to bind the `cast_receiver`
to. There are many tools available for this task, such as `ifconfig`:

```sh
$ ifconfig
eth0: flags=4163<UP,BROADCAST,RUNNING,SIMPLEX,MULTICAST> mtu 1500
        inet 192.168.1.10 netmask 255.255.255.0 broadcast 192.168.1.255
        inet6 fe80::abcd:ef01:2345:6789%eth0 prefixlen 64 scopeid 0x20<link>
        ether 00:1a:2b:3c:4d:5e txqueuelen 1000 (Ethernet)
        RX packets 92800 errors 0 dropped 0 overruns 0 frame 0
        TX packets 93810 errors 0 dropped 0 overruns 0 carrier 0 collisions 0
        collisions 0

lo: flags=73<UP,LOOPBACK,RUNNING> mtu 65536
        inet 127.0.0.1 netmask 255.0.0.0
        inet6 ::1 prefixlen 128 scopeid 0x10<host>
        loop txqueuelen 1000 (Local Loopback)
        RX packets 100 errors 0 dropped 0 overruns 0 frame 0
        TX packets 100 errors 0 dropped 0 overruns 0 carrier 0 collisions 0
        collisions 0
```

Pay attention to the `inet` address associated with your preferred interface
if you are connecting with the `cast_sender` binary -- you may need it to
connect later.

Once you have your key, certificate, and network interface, you should be ready
to start your `cast_receiver` binary. That should look something like this:

```sh
./out/Default/cast_receiver -d generated_root_cast_receiver.crt -p generated_root_cast_receiver.key eth0
```

Pay attention to the command output as it runs -- it should prompt with any
breaking errors.

### Note on discovery and macOS

Currently, the discovery component only works on Linux, as we have not written
a Bonjour integration for macOS. When running the `cast_receiver` on macOS,
you must disable mDNS using the `-x` flag. Note that this means that you
**cannot connect to `cast_receiver` instances on macOS using discovery**.

## 5. Connecting with a libcast sender

### 5a. Option 1: Using the discovery component

Assuming your `cast_receiver` instance has discovery enabled (true by default),
you can connect to it using the cast sender and specifying the network interface
and media file. It should then prompt you with a list of discovered receivers,
with indexes that allow you to connect to a specific receiver.

Example invocation:

```sh
$ ./out/Default/cast_sender -d generated_root_cast_receiver.crt eth0 ~/file_example_MP4_1920_18MG.mp4
[INFO:../../cast/standalone_sender/main.cc(198):T0] using cast trust store generated from: generated_root_cast_receiver.crt
[INFO:../../cast/standalone_sender/receiver_chooser.cc(41):T0] Starting discovery. Note that it can take dozens of seconds to detect anything on some networks!
[INFO:../../cast/standalone_sender/receiver_chooser.cc(71):T0] Discovered: Cast Standalone Receiver (id: cast_standalone_rece-4201ac134af2)

[0]: Cast Standalone Receiver @ 172.19.74.242:8010

Enter choice, or 'n' to wait longer: 0
[INFO:../../cast/standalone_sender/looping_file_cast_agent.cc(84):T0] Launching Mirroring App on the Cast Receiver...
[INFO:../../cast/standalone_sender/looping_file_cast_agent.cc(235):T0] Starting-up message routing to the Cast Receiver's Mirroring App (sessionId=streaming_receiver-10000)...
[INFO:../../cast/standalone_sender/looping_file_cast_agent.cc(249):T0] Starting streaming session...
...
```

### 5b. Option 2: Using a direct connection

If you want to connect directly to the receiver instead of using discovery, you
can pass the receiver's network address instead of a network interface:

```sh
./out/Default/cast_sender -d generated_root_cast_receiver.crt -c vp9 172.19.74.242:8010 ~/file_example_MP4_1920_18MG.mp4
```

### Specifying the video codec

To determine which video codec to use, the `-c` or `--codec` flag should be
passed. Currently supported values include `vp8`, `vp9`, and `av1`:

```bash
./out/Default/cast_sender -d generated_root_cast_receiver.crt -r 127.0.0.1 -c av1 ~/video-1080-mp4.mp4
```

## 6. Connecting with a Chrome sender

In some cases, it is desirable to directly test the Open Screen receiver using
Chrome as a receiver. This is a basic starter guide on this workflow.

### Motivation

Frequently, it can be difficult to test directly against a real, live Chromecast
device. For example, developing on a cloud-based machine or on a network with
tight access controls. Using Open Screen as a receiver allows development to
occur in these environments.

Also, sometimes you just wanna test the Open Screen receiver and make sure all
of the flows are working.

### Warning: **Linux required ahead**

You **must be running the cast receiver on a Linux device**. The libcast
standalone receiver does not have the ability to do discovery on macOS due to
the discovery code not being configured to work with Bonjour (yet). Other
platforms, like Windows, have the potential for support in the future but are
currently missing dependencies.

### Connecting with Chrome

Assuming you have followed the above steps and your `cast_receiver` binary
is happily awaiting connections on a TLS server socket, all you have to do now
is connect with Chrome.

The necessary flag is present on Chrome stable and does not require a debug
build. Any recent build of Chrome should work out of the box. In order to
connect, you just need to start Chrome from the command line, specifying the
**fully qualified path** to the developer certificate generated by the test
receiver like so:

```sh
/path/to/chrome/chrome --cast-developer-certificate-path=/fully/qualified/path/to/cert/generated_root_cast_receiver.crt
```

**Note on finding Chrome:** The path to the Chrome executable varies by platform
and channel. On Linux, you can often use `which google-chrome-stable`. On macOS,
the path is typically
`/Applications/Google Chrome.app/Contents/MacOS/Google Chrome`.

> Chrome may fail to resolve the test certificate if you do not specify the
> **fully qualified path**!
>

## The `standalone_e2e.py` script: a helpful reference

If you have any issues with the above documentation, or just want to see an
example usage of it, the [`standalone_e2e.py`](../standalone_e2e.py) script
exercises the Cast Sender sender and receiver through some basic end to
end test scenarios, using the same steps as above.
