// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit test for VideoCaptureController.

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "content/browser/renderer_host/media/media_stream_provider.h"
#include "content/browser/renderer_host/media/video_capture_controller.h"
#include "content/browser/renderer_host/media/video_capture_controller_event_handler.h"
#include "content/browser/renderer_host/media/video_capture_manager.h"
#include "content/common/media/media_stream_options.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/video/capture/video_capture_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InSequence;
using ::testing::Mock;

namespace content {

class MockVideoCaptureControllerEventHandler
    : public VideoCaptureControllerEventHandler {
 public:
  explicit MockVideoCaptureControllerEventHandler(
      VideoCaptureController* controller)
      : controller_(controller) {}
  virtual ~MockVideoCaptureControllerEventHandler() {}

  // These mock methods are delegated to by our fake implementation of
  // VideoCaptureControllerEventHandler, to be used in EXPECT_CALL().
  MOCK_METHOD1(DoBufferCreated, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoBufferReady, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoFrameInfo, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoEnded, void(const VideoCaptureControllerID&));
  MOCK_METHOD1(DoError, void(const VideoCaptureControllerID&));

  virtual void OnError(const VideoCaptureControllerID& id) OVERRIDE {
    DoError(id);
  }
  virtual void OnBufferCreated(const VideoCaptureControllerID& id,
                               base::SharedMemoryHandle handle,
                               int length, int buffer_id) OVERRIDE {
    DoBufferCreated(id);
  }
  virtual void OnBufferReady(const VideoCaptureControllerID& id,
                             int buffer_id,
                             base::Time timestamp) OVERRIDE {
    DoBufferReady(id);
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&VideoCaptureController::ReturnBuffer,
                   base::Unretained(controller_), id, this, buffer_id));
  }
  virtual void OnFrameInfo(
      const VideoCaptureControllerID& id,
      const media::VideoCaptureCapability& format) OVERRIDE {
    DoFrameInfo(id);
  }
  virtual void OnEnded(const VideoCaptureControllerID& id) OVERRIDE {
    DoEnded(id);
    // OnEnded() must respond by (eventually) unregistering the client.
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(base::IgnoreResult(&VideoCaptureController::RemoveClient),
                   base::Unretained(controller_), id, this));
  }

  VideoCaptureController* controller_;
};

// Test class.
class VideoCaptureControllerTest : public testing::Test {
 public:
  VideoCaptureControllerTest() {}
  virtual ~VideoCaptureControllerTest() {}

 protected:
  static const int kPoolSize = 3;

  virtual void SetUp() OVERRIDE {
    controller_.reset(new VideoCaptureController());
    device_ = controller_->NewDeviceClient().Pass();
    client_a_.reset(new MockVideoCaptureControllerEventHandler(
        controller_.get()));
    client_b_.reset(new MockVideoCaptureControllerEventHandler(
        controller_.get()));
  }

  virtual void TearDown() OVERRIDE {
    base::RunLoop().RunUntilIdle();
  }

  TestBrowserThreadBundle bindle_;
  scoped_ptr<MockVideoCaptureControllerEventHandler> client_a_;
  scoped_ptr<MockVideoCaptureControllerEventHandler> client_b_;
  scoped_ptr<VideoCaptureController> controller_;
  scoped_ptr<media::VideoCaptureDevice::EventHandler> device_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureControllerTest);
};

// A simple test of VideoCaptureController's ability to add, remove, and keep
// track of clients.
TEST_F(VideoCaptureControllerTest, AddAndRemoveClients) {
  media::VideoCaptureParams session_100;
  session_100.session_id = 100;
  session_100.width = 320;
  session_100.height = 240;
  session_100.frame_rate = 30;

  media::VideoCaptureParams session_200 = session_100;
  session_200.session_id = 200;

  media::VideoCaptureParams session_300 = session_100;
  session_300.session_id = 300;

  media::VideoCaptureParams session_400 = session_100;
  session_400.session_id = 400;

  // Intentionally use the same route ID for two of the clients: the device_ids
  // are a per-VideoCaptureHost namespace, and can overlap across hosts.
  const VideoCaptureControllerID client_a_route_1(44);
  const VideoCaptureControllerID client_a_route_2(30);
  const VideoCaptureControllerID client_b_route_1(30);
  const VideoCaptureControllerID client_b_route_2(1);

  // Clients in controller: []
  ASSERT_EQ(0, controller_->GetClientCount())
      << "Client count should initially be zero.";
  controller_->AddClient(client_a_route_1, client_a_.get(),
                         base::kNullProcessHandle, session_100);
  // Clients in controller: [A/1]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Adding client A/1 should bump client count.";;
  controller_->AddClient(client_a_route_2, client_a_.get(),
                         base::kNullProcessHandle, session_200);
  // Clients in controller: [A/1, A/2]
  ASSERT_EQ(2, controller_->GetClientCount())
      << "Adding client A/2 should bump client count.";
  controller_->AddClient(client_b_route_1, client_b_.get(),
                         base::kNullProcessHandle, session_300);
  // Clients in controller: [A/1, A/2, B/1]
  ASSERT_EQ(3, controller_->GetClientCount())
      << "Adding client B/1 should bump client count.";
  ASSERT_EQ(200,
      controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing client A/1 should return its session_id.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2, controller_->GetClientCount());
  ASSERT_EQ(static_cast<int>(kInvalidMediaCaptureSessionId),
      controller_->RemoveClient(client_a_route_2, client_a_.get()))
      << "Removing a nonexistant client should fail.";
  // Clients in controller: [A/1, B/1]
  ASSERT_EQ(2, controller_->GetClientCount());
  ASSERT_EQ(300,
      controller_->RemoveClient(client_b_route_1, client_b_.get()))
      << "Removing client B/1 should return its session_id.";
  // Clients in controller: [A/1]
  ASSERT_EQ(1, controller_->GetClientCount());
  controller_->AddClient(client_b_route_2, client_b_.get(),
                         base::kNullProcessHandle, session_400);
  // Clients in controller: [A/1, B/2]

  EXPECT_CALL(*client_a_, DoEnded(client_a_route_1)).Times(1);
  controller_->StopSession(100);  // Session 100 == client A/1
  Mock::VerifyAndClearExpectations(client_a_.get());
  ASSERT_EQ(2, controller_->GetClientCount())
      << "Client should be closed but still exist after StopSession.";
  // Clients in controller: [A/1 (closed, removal pending), B/2]
  base::RunLoop().RunUntilIdle();
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Client A/1 should be deleted by now.";
  controller_->StopSession(200);  // Session 200 does not exist anymore
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Stopping non-existant session 200 should be a no-op.";
  controller_->StopSession(256);  // Session 256 never existed.
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Stopping non-existant session 256 should be a no-op.";
  ASSERT_EQ(static_cast<int>(kInvalidMediaCaptureSessionId),
      controller_->RemoveClient(client_a_route_1, client_a_.get()))
      << "Removing already-removed client A/1 should fail.";
  // Clients in controller: [B/2]
  ASSERT_EQ(1, controller_->GetClientCount())
      << "Removing non-existant session 200 should be a no-op.";
  ASSERT_EQ(400,
      controller_->RemoveClient(client_b_route_2, client_b_.get()))
      << "Removing client B/2 should return its session_id.";
  // Clients in controller: []
  ASSERT_EQ(0, controller_->GetClientCount())
      << "Client count should return to zero after all clients are gone.";
}

// This test will connect and disconnect several clients while simulating an
// active capture device being started and generating frames. It runs on one
// thread and is intended to behave deterministically.
TEST_F(VideoCaptureControllerTest, NormalCaptureMultipleClients) {
  media::VideoCaptureParams session_100;
  session_100.session_id = 100;
  session_100.width = 320;
  session_100.height = 240;
  session_100.frame_rate = 30;

  media::VideoCaptureParams session_200 = session_100;
  session_200.session_id = 200;

  media::VideoCaptureParams session_300 = session_100;
  session_300.session_id = 300;

  // session_id of 1 is kStartOpenSessionId, which should have special meaning
  // to VideoCaptureManager, but not to VideoCaptureController ... so test it.
  media::VideoCaptureParams session_1 = session_100;
  session_1.session_id = VideoCaptureManager::kStartOpenSessionId;

  // The device format needn't match the VideoCaptureParams (the camera can do
  // what it wants). Pick something random to use for OnFrameInfo.
  media::VideoCaptureCapability device_format(
      10, 10, 25, media::PIXEL_FORMAT_RGB24, 10, false,
      media::ConstantResolutionVideoCaptureDevice);

  const VideoCaptureControllerID client_a_route_1(0xa1a1a1a1);
  const VideoCaptureControllerID client_a_route_2(0xa2a2a2a2);
  const VideoCaptureControllerID client_b_route_1(0xb1b1b1b1);
  const VideoCaptureControllerID client_b_route_2(0xb2b2b2b2);

  // Start with two clients.
  controller_->AddClient(client_a_route_1, client_a_.get(),
                         base::kNullProcessHandle, session_100);
  controller_->AddClient(client_b_route_1, client_b_.get(),
                         base::kNullProcessHandle, session_300);
  ASSERT_EQ(2, controller_->GetClientCount());

  // The OnFrameInfo() event from the device, when processed by the controller,
  // should generate client OnFrameInfo() and OnBufferCreated() events.
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoFrameInfo(client_a_route_1)).Times(1);
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_1)).Times(kPoolSize);
  }
  {
    InSequence s;
    EXPECT_CALL(*client_b_, DoFrameInfo(client_b_route_1)).Times(1);
    EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_1)).Times(kPoolSize);
  }
  device_->OnFrameInfo(device_format);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // When a third clients is subsequently added, the frame info and buffers
  // should immediately be shared to the new clients.
  {
    InSequence s;
    EXPECT_CALL(*client_a_, DoFrameInfo(client_a_route_2)).Times(1);
    EXPECT_CALL(*client_a_, DoBufferCreated(client_a_route_2)).Times(kPoolSize);
  }
  controller_->AddClient(client_a_route_2, client_a_.get(),
                         base::kNullProcessHandle, session_200);
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Now, simulate an incoming captured frame from the capture device.
  uint8 frame_no = 1;
  scoped_refptr<media::VideoFrame> frame;
  frame = device_->ReserveOutputBuffer();
  ASSERT_TRUE(frame);
  media::FillYUV(frame, frame_no++, 0x22, 0x44);
  device_->OnIncomingCapturedVideoFrame(frame, base::Time());
  frame = NULL;

  // The buffer should be delivered to the clients in any order.
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_1)).Times(1);
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_2)).Times(1);
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_1)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Second frame. In this case pretend that the VideoFrame pointer is held
  // by the device for a long delay. This shouldn't affect anything.
  frame = device_->ReserveOutputBuffer();
  ASSERT_TRUE(frame);
  media::FillYUV(frame, frame_no++, 0x22, 0x44);
  device_->OnIncomingCapturedVideoFrame(frame, base::Time());

  // The buffer should be delivered to the clients in any order.
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_1)).Times(1);
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_2)).Times(1);
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_1)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
  frame = NULL;

  // Add a fourth client now that some frames have come through. It should get
  // the buffer info, but it won't get any frames until new ones are captured.
  {
    InSequence s;
    EXPECT_CALL(*client_b_, DoFrameInfo(client_b_route_2)).Times(1);
    EXPECT_CALL(*client_b_, DoBufferCreated(client_b_route_2)).Times(kPoolSize);
  }
  controller_->AddClient(client_b_route_2, client_b_.get(),
                         base::kNullProcessHandle, session_1);
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Third, fourth, and fifth frames. Pretend they all arrive at the same time.
  for (int i = 0; i < kPoolSize; i++) {
    frame = device_->ReserveOutputBuffer();
    ASSERT_TRUE(frame);
    ASSERT_EQ(media::VideoFrame::I420, frame->format());
    media::FillYUV(frame, frame_no++, 0x22, 0x44);
    device_->OnIncomingCapturedVideoFrame(frame, base::Time());

  }
  // ReserveOutputBuffer ought to fail now, because the pool is depleted.
  ASSERT_FALSE(device_->ReserveOutputBuffer());
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_1)).Times(kPoolSize);
  EXPECT_CALL(*client_a_, DoBufferReady(client_a_route_2)).Times(kPoolSize);
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_1)).Times(kPoolSize);
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_2)).Times(kPoolSize);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());

  // Now test the interaction of client shutdown and frame delivery.
  // Kill A1 via renderer disconnect (synchronous).
  controller_->RemoveClient(client_a_route_1, client_a_.get());
  // Kill B1 via session close (posts a task to disconnect).
  EXPECT_CALL(*client_b_, DoEnded(client_b_route_1)).Times(1);
  controller_->StopSession(300);
  // Queue up another frame.
  frame = device_->ReserveOutputBuffer();
  ASSERT_TRUE(frame);
  media::FillYUV(frame, frame_no++, 0x22, 0x44);
  device_->OnIncomingCapturedVideoFrame(frame, base::Time());
  frame = device_->ReserveOutputBuffer();
  {
    // Kill A2 via session close (posts a task to disconnect, but A2 must not
    // be sent either of these two frames)..
    EXPECT_CALL(*client_a_, DoEnded(client_a_route_2)).Times(1);
    controller_->StopSession(200);
  }
  ASSERT_TRUE(frame);
  media::FillYUV(frame, frame_no++, 0x22, 0x44);
  device_->OnIncomingCapturedVideoFrame(frame, base::Time());
  // B2 is the only client left, and is the only one that should
  // get the frame.
  EXPECT_CALL(*client_b_, DoBufferReady(client_b_route_2)).Times(2);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());
  Mock::VerifyAndClearExpectations(client_b_.get());
}

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorBeforeDeviceCreation) {
  media::VideoCaptureParams session_100;
  session_100.session_id = 100;
  session_100.width = 320;
  session_100.height = 240;
  session_100.frame_rate = 30;

  media::VideoCaptureParams session_200 = session_100;
  session_200.session_id = 200;

  const VideoCaptureControllerID route_id(0x99);

  // Start with one client.
  controller_->AddClient(route_id, client_a_.get(),
                         base::kNullProcessHandle, session_100);
  device_->OnError();
  EXPECT_CALL(*client_a_, DoError(route_id)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(*client_b_, DoError(route_id)).Times(1);
  controller_->AddClient(route_id, client_b_.get(),
                         base::kNullProcessHandle, session_200);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_b_.get());

  // OnFrameInfo from the VCD should become a no-op after the error occurs.
  media::VideoCaptureCapability device_format(
      10, 10, 25, media::PIXEL_FORMAT_ARGB, 10, false,
      media::ConstantResolutionVideoCaptureDevice);

  device_->OnFrameInfo(device_format);
  base::RunLoop().RunUntilIdle();
}

// Exercises the OnError() codepath of VideoCaptureController, and tests the
// behavior of various operations after the error state has been signalled.
TEST_F(VideoCaptureControllerTest, ErrorAfterDeviceCreation) {
  media::VideoCaptureParams session_100;
  session_100.session_id = 100;
  session_100.width = 320;
  session_100.height = 240;
  session_100.frame_rate = 30;

  media::VideoCaptureParams session_200 = session_100;
  session_200.session_id = 200;

  const VideoCaptureControllerID route_id(0x99);

  // Start with one client.
  controller_->AddClient(route_id, client_a_.get(),
                         base::kNullProcessHandle, session_100);
  // OnFrameInfo from the VCD should become a no-op after the error occurs.
  media::VideoCaptureCapability device_format(
      10, 10, 25, media::PIXEL_FORMAT_ARGB, 10, false,
      media::ConstantResolutionVideoCaptureDevice);

  // Start the device and get as far as exchanging buffers with the subprocess.
  // Then, signal an error and deliver the frame. The error should be propagated
  // to clients; the frame should not be.
  device_->OnFrameInfo(device_format);
  EXPECT_CALL(*client_a_, DoFrameInfo(route_id)).Times(1);
  EXPECT_CALL(*client_a_, DoBufferCreated(route_id)).Times(kPoolSize);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  scoped_refptr<media::VideoFrame> frame = device_->ReserveOutputBuffer();
  ASSERT_TRUE(frame);

  device_->OnError();
  device_->OnIncomingCapturedVideoFrame(frame, base::Time());
  frame = NULL;

  EXPECT_CALL(*client_a_, DoError(route_id)).Times(1);
  base::RunLoop().RunUntilIdle();
  Mock::VerifyAndClearExpectations(client_a_.get());

  // Second client connects after the error state. It also should get told of
  // the error.
  EXPECT_CALL(*client_b_, DoError(route_id)).Times(1);
  controller_->AddClient(route_id, client_b_.get(),
                         base::kNullProcessHandle, session_200);
  Mock::VerifyAndClearExpectations(client_b_.get());
}

}  // namespace content
