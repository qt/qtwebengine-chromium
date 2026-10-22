# Input Event API Demo Guide

This document explains how to use the Input Event API demo in the standalone
Cast reference implementations.

## Overview

The Input Event API allows a Cast Receiver to send user interactions (mouse
clicks, keyboard events, etc.) back to the Cast Sender. This demo specifically
shows mouse click events being captured on the receiver and visualized on the
sender.

## Prerequisites

- **SDL2**: The standalone receiver requires SDL2 for window management and
  input event capture.
- **FFmpeg/LibVPX/LibOpus**: Required for media encoding and decoding.

## Building the Demo

Ensure your GN args are configured for standalone build with external libraries.
Then build the following targets:

```bash
autoninja -C out/Default cast_receiver cast_sender
```

## Running the Demo

### 1. Start the Receiver

Run the receiver on a networked machine (or your local machine using the loopback
interface). You **must** opt-in using the `--enable-input-events` flag.

```bash
./out/Default/cast_receiver --enable-input-events <interface_name>
```

Replace `<interface_name>` with your network interface (e.g., `eth0`, `wlan0`,
or `lo` for local testing).

### 2. Start the Sender

Run the sender, pointing it to the receiver's address and a media file. Again,
the `--enable-input-events` flag is required.

```bash
./out/Default/cast_sender --enable-input-events <receiver_ip_or_interface> <media_file>
```

## Observing the Results

1. A window will open on the Receiver's machine showing the mirrored video.
2. **Click anywhere** inside the Receiver's video window.
3. **Visual Feedback**: An animated "ping" (an expanding white ring) will appear
    directly in the video stream at the location where you clicked. This ring is
    drawn by the **Sender** on the raw frames before they are encoded.
4. **Console Logs**: Check the Sender's terminal output. You should see logs
    indicating the received mouse events:

    ```
    [Input] Received MOUSE_DOWN at (450, 300) button=1 display=1920x1080
    [Input] Received MOUSE_UP at (450, 300) button=1 display=1920x1080
    ```

## How it Works

1. **Capture**: The `SDLEventLoopProcessor` in `cast_receiver` captures SDL
    mouse events.
2. **Mapping**: `StreamingPlaybackController` maps window coordinates to the
    video's logical coordinate space.
3. **Transport**: The events are sent back to the sender via the `InputProducer`
    using the negotiated `input_events` RTP extension.
4. **Consumption**: The `LoopingFileCastAgent` in `cast_sender` receives the
    events via `InputConsumer`.
5. **Overlay**: `LoopingFileSender` draws an animated ring directly into the YUV
    pixel data of the next video frame before encoding.
