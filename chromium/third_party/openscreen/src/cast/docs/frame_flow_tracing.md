# Frame Flow Tracing in Open Screen

Open Screen utilizes **Flow Events** to track the lifecycle of audio and video
frames as they travel through the system. This allows developers to visualize
the journey of a single frame from generation (at the Sender) to presentation
(at the Receiver) across thread and process boundaries in the tracing tool of
the embedder's choice.

*** aside
The concept of flows in Open Screen is inspired by Perfetto, which is
the default tracing tool of the Chromium project as well as other consumers of
libcast. Flows as a concept may or may not be applicable to a given tracing
tool.
***

## The Concept

A "Flow" connects distinct trace events that share a common ID. In the context
of Cast Streaming, the natural identifier is the `FrameId`. By associating trace
events with the `FrameId`, we can visualize the latency breakdown, queueing
delays, and network transmission times for every frame.

## The Flow Lifecycle

### 1. Origin (Embedder Responsibility)

The flow typically begins when a frame is captured or encoded. Since this
happens outside the Open Screen library (in the Embedder's code), the Embedder
is responsible for providing the capture timestamps.

**Mechanism:** Provide `capture_begin_time` in `EncodedFrame`.

The Open Screen `Sender` automatically emits a `TRACE_FLOW_BEGIN` event using
the provided `capture_begin_time` as the start timestamp. This ensures the flow
visually starts at the correct moment of capture, even if the frame is enqueued
later.

```cpp
// Example: Inside your Encoder or Capture mechanism
EncodedFrame frame;
frame.capture_begin_time = my_capture_clock.now();
// ... encode ...
sender->EnqueueFrame(frame);
// The library will emit TRACE_FLOW_BEGIN("Frame.Capture", frame_id, capture_begin_time) automatically.
```

### 2. Sender Library

Once the frame is passed to the `Sender` class, the library emits steps to track
its progress within the sending queue and network stack.

* **`Frame.Capture`**: The start of the flow, back-dated to `capture_begin_time`
  provided by the Embedder.
* **`Frame.Capture.End`**: The end of capture (if `capture_end_time` provided).
* **`Frame.Encode`**: The completion of encoding (logged at Enqueue time).
* **`Frame.Enqueued`**: The frame has entered the Sender's processing queue.
* **`Frame.Acked`**: The Receiver has acknowledged receipt of this frame.
* **`Frame.Cancelled`**: The frame was dropped or replaced before it could be
  fully sent/acked.

### 3. Receiver Library

When packets arrive at the `Receiver`, the library tracks the reassembly and
availability of the frame.

* **`Frame.Complete`**: All packets for this frame have been received and
  reassembled. The frame is valid and sitting in the network queue.

* **`Frame.Ready`**: The frame is the next in sequence and has satisfied all
  dependencies. It is ready for the application to consume.

* **`Frame.Consumed`**: The application has popped the frame from the queue.

### 4. Presentation (Embedder Responsibility)

After the frame leaves the Receiver library, the Embedder (Player) takes over.
To complete the picture, Embedders should continue the flow through decoding and
rendering.

**Macro:** `TRACE_FLOW_STEP` and `TRACE_FLOW_END`

```cpp
// Example: Inside your Player's OnFramesReady()
EncodedFrame frame = receiver.ConsumeNextFrame(buffer);
// 'Frame.Consumed' is emitted by the library here.

// 1. Embedder acknowledges receipt
TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Received", frame.frame_id);

// ... Decoding ...

// 2. Rendering Start
TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Render.Begin", frame.frame_id);

// ... Rendering ...

// 3. Rendering End
TRACE_FLOW_STEP(TraceCategory::kReceiver, "Frame.Render.End", frame.frame_id);

// 4. Playout (Flow End)
// Call ReportPlayoutEvent() to notify the library. The library will emit
// TRACE_FLOW_END_WITH_TIME("Frame.PlayedOut", ..., playout_time).
receiver.ReportPlayoutEvent(frame.frame_id, frame.rtp_timestamp, playout_time);
```

## Debugging & Diagnostics

By visualizing these flows in a tool like
[ui.perfetto.dev](https://ui.perfetto.dev/), you can diagnose:

* **Sender Blocking**: Long gaps before `Frame.Enqueued`.

* **Network Latency**: Time between `Frame.Enqueued` and `Frame.Complete`
  (approximate).

* **Receiver Queueing**: Time between `Frame.Complete` and `Frame.Ready`.
  * *High duration here suggests the Receiver application is not pulling frames
    fast enough or the jitter buffer is holding them.*

* **Application Latency**: Time between `Frame.Ready` and `Frame.PlayedOut`.

## Integration Tips

1. **Use `FrameId`**: Ensure your flow IDs are derived from the `FrameId`. The
   library uses `FrameId::value()` (casted to `uint64_t`) as the flow ID.

2. **Category Consistency**: While flows can span categories, it is helpful to
   use `TraceCategory::kSender` for sender-side events and
   `TraceCategory::kReceiver` for receiver-side events.
